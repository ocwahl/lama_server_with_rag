#include "utils.hpp"

#include "arg.h"
#include "common.h"
#include "json-schema-to-grammar.h"
#include "llama.h"
#include "log.h"
#include "sampling.h"
#include "speculative.h"
#include "mtmd.h"

// Change JSON_ASSERT from assert() to GGML_ASSERT:
#define JSON_ASSERT GGML_ASSERT
#include "json.hpp"
// mime type for sending response
#define MIMETYPE_JSON "application/json; charset=utf-8"

// auto generated files (see README.md for details)
#include "index.html.gz.hpp"
#include "loading.html.hpp"

#include <unistd.h> // Required for getpid()
#include <iostream>
#include <fstream> // For reading files
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cinttypes>
#include <deque>
#include <memory>
#include <mutex>
#include <signal.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

#include "rag_database.h"
#include "postgres_client.h"
#include "self_signed.h"

namespace fs = std::filesystem;

std::shared_ptr<postgres_client> rag_db_ = nullptr;
std::shared_ptr<rag_database> create_rag_database(const std::string & host_name, int port, const std::string & db_name)
{
    if(!rag_db_)
        return rag_db_ = std::make_shared<postgres_client>(host_name, port, db_name);
    if((rag_db_->get_host_name() == host_name) && (rag_db_->get_port() == port) && (rag_db_->get_name() == db_name))
        return rag_db_;
    std::cerr<<"rebuilding database";
    return rag_db_ = std::make_shared<postgres_client>(host_name, port, db_name);
}
using json = nlohmann::ordered_json;

constexpr int HTTP_POLLING_SECONDS = 1;

enum stop_type {
    STOP_TYPE_NONE,
    STOP_TYPE_EOS,
    STOP_TYPE_WORD,
    STOP_TYPE_LIMIT,
};

// state diagram: https://github.com/ggml-org/llama.cpp/pull/9283
enum slot_state {
    SLOT_STATE_IDLE,
    SLOT_STATE_STARTED, // TODO: this state is only used for setting up the initial prompt processing; maybe merge it with launch_slot_with_task in the future
    SLOT_STATE_PROCESSING_PROMPT,
    SLOT_STATE_DONE_PROMPT,
    SLOT_STATE_GENERATING,
};

enum server_state {
    SERVER_STATE_LOADING_MODEL,  // Server is starting up, model not fully loaded yet
    SERVER_STATE_READY,          // Server is ready and model is loaded
};

enum server_task_type {
    SERVER_TASK_TYPE_COMPLETION,
    SERVER_TASK_TYPE_EMBEDDING,
    SERVER_TASK_TYPE_RERANK,
    SERVER_TASK_TYPE_INFILL,
    SERVER_TASK_TYPE_CANCEL,
    SERVER_TASK_TYPE_NEXT_RESPONSE,
    SERVER_TASK_TYPE_METRICS,
    SERVER_TASK_TYPE_SLOT_SAVE,
    SERVER_TASK_TYPE_SLOT_RESTORE,
    SERVER_TASK_TYPE_SLOT_ERASE,
    SERVER_TASK_TYPE_SET_LORA,
};

enum oaicompat_type {
    OAICOMPAT_TYPE_NONE,
    OAICOMPAT_TYPE_CHAT,
    OAICOMPAT_TYPE_COMPLETION,
    OAICOMPAT_TYPE_EMBEDDING,
};

// https://community.openai.com/t/openai-chat-list-of-error-codes-and-types/357791/11
enum error_type {
    ERROR_TYPE_INVALID_REQUEST,
    ERROR_TYPE_AUTHENTICATION,
    ERROR_TYPE_SERVER,
    ERROR_TYPE_NOT_FOUND,
    ERROR_TYPE_PERMISSION,
    ERROR_TYPE_UNAVAILABLE, // custom error
    ERROR_TYPE_NOT_SUPPORTED, // custom error
};

struct slot_params {
    bool stream        = true;
    bool cache_prompt  = true; // remember the prompt to avoid reprocessing all prompt
    bool return_tokens = false;

    int32_t n_keep    =  0; // number of tokens to keep from initial prompt
    int32_t n_discard =  0; // number of tokens after n_keep that may be discarded when shifting context, 0 defaults to half
    int32_t n_predict = -1; // new tokens to predict
    int32_t n_indent  =  0; // mininum line indentation for the generated text in number of whitespace characters

    int64_t t_max_prompt_ms  = -1; // TODO: implement
    int64_t t_max_predict_ms = -1; // if positive, limit the generation phase to this time limit

    std::vector<common_adapter_lora_info> lora;

    std::vector<std::string> antiprompt;
    std::vector<std::string> response_fields;
    bool timings_per_token = false;
    bool post_sampling_probs = false;
    bool ignore_eos = false;

    struct common_params_sampling sampling;
    struct common_params_speculative speculative;

    // OAI-compat fields
    bool                  verbose                   = false;
    oaicompat_type        oaicompat                 = OAICOMPAT_TYPE_NONE;
    std::string           oaicompat_model;
    std::string           oaicompat_cmpl_id;
    common_chat_format    oaicompat_chat_format     = COMMON_CHAT_FORMAT_CONTENT_ONLY;

    json to_json() const {
        std::vector<std::string> samplers;
        samplers.reserve(sampling.samplers.size());
        for (const auto & sampler : sampling.samplers) {
            samplers.emplace_back(common_sampler_type_to_str(sampler));
        }

        json lora = json::array();
        for (size_t i = 0; i < this->lora.size(); ++i) {
            lora.push_back({{"id", i}, {"scale", this->lora[i].scale}});
        }

        auto grammar_triggers = json::array();
        for (const auto & trigger : sampling.grammar_triggers) {
            server_grammar_trigger ct(std::move(trigger));
            grammar_triggers.push_back(ct.to_json());
        }

        return json {
            {"n_predict",                 n_predict},     // Server configured n_predict
            {"seed",                      sampling.seed},
            {"temperature",               sampling.temp},
            {"dynatemp_range",            sampling.dynatemp_range},
            {"dynatemp_exponent",         sampling.dynatemp_exponent},
            {"top_k",                     sampling.top_k},
            {"top_p",                     sampling.top_p},
            {"min_p",                     sampling.min_p},
            {"top_n_sigma",               sampling.top_n_sigma},
            {"xtc_probability",           sampling.xtc_probability},
            {"xtc_threshold",             sampling.xtc_threshold},
            {"typical_p",                 sampling.typ_p},
            {"repeat_last_n",             sampling.penalty_last_n},
            {"repeat_penalty",            sampling.penalty_repeat},
            {"presence_penalty",          sampling.penalty_present},
            {"frequency_penalty",         sampling.penalty_freq},
            {"dry_multiplier",            sampling.dry_multiplier},
            {"dry_base",                  sampling.dry_base},
            {"dry_allowed_length",        sampling.dry_allowed_length},
            {"dry_penalty_last_n",        sampling.dry_penalty_last_n},
            {"dry_sequence_breakers",     sampling.dry_sequence_breakers},
            {"mirostat",                  sampling.mirostat},
            {"mirostat_tau",              sampling.mirostat_tau},
            {"mirostat_eta",              sampling.mirostat_eta},
            {"stop",                      antiprompt},
            {"max_tokens",                n_predict}, // User configured n_predict
            {"n_keep",                    n_keep},
            {"n_discard",                 n_discard},
            {"ignore_eos",                sampling.ignore_eos},
            {"stream",                    stream},
            {"logit_bias",                format_logit_bias(sampling.logit_bias)},
            {"n_probs",                   sampling.n_probs},
            {"min_keep",                  sampling.min_keep},
            {"grammar",                   sampling.grammar},
            {"grammar_lazy",              sampling.grammar_lazy},
            {"grammar_triggers",          grammar_triggers},
            {"preserved_tokens",          sampling.preserved_tokens},
            {"chat_format",               common_chat_format_name(oaicompat_chat_format)},
            {"samplers",                  samplers},
            {"speculative.n_max",         speculative.n_max},
            {"speculative.n_min",         speculative.n_min},
            {"speculative.p_min",         speculative.p_min},
            {"timings_per_token",         timings_per_token},
            {"post_sampling_probs",       post_sampling_probs},
            {"lora",                      lora},
        };
    }
};

struct server_task {
    int id    = -1; // to be filled by server_queue
    int index = -1; // used when there are multiple prompts (batch request)

    server_task_type type;

    // used by SERVER_TASK_TYPE_CANCEL
    int id_target = -1;

    // used by SERVER_TASK_TYPE_INFERENCE
    slot_params   params;
    server_tokens prompt_tokens;
    int id_selected_slot = -1;

    // used by SERVER_TASK_TYPE_SLOT_SAVE, SERVER_TASK_TYPE_SLOT_RESTORE, SERVER_TASK_TYPE_SLOT_ERASE
    struct slot_action {
        int slot_id;
        std::string filename;
        std::string filepath;
    };
    slot_action slot_action;

    // used by SERVER_TASK_TYPE_METRICS
    bool metrics_reset_bucket = false;

    // used by SERVER_TASK_TYPE_SET_LORA
    std::vector<common_adapter_lora_info> set_lora;

    server_task(server_task_type type) : type(type) {}

    static slot_params params_from_json_cmpl(
            const llama_context * ctx,
            const common_params & params_base,
            const json & data) {
        const llama_model * model = llama_get_model(ctx);
        const llama_vocab * vocab = llama_model_get_vocab(model);

        slot_params params;

        // Sampling parameter defaults are loaded from the global server context (but individual requests can still override them)
        slot_params defaults;
        defaults.sampling    = params_base.sampling;
        defaults.speculative = params_base.speculative;

        // enabling this will output extra debug information in the HTTP responses from the server
        params.verbose           = params_base.verbosity > 9;
        params.timings_per_token = json_value(data, "timings_per_token", false);

        params.stream           = json_value(data, "stream",             false);
        params.cache_prompt     = json_value(data, "cache_prompt",       true);
        params.return_tokens    = json_value(data, "return_tokens",      false);
        params.n_predict        = json_value(data, "n_predict",          json_value(data, "max_tokens", defaults.n_predict));
        params.n_indent         = json_value(data, "n_indent",           defaults.n_indent);
        params.n_keep           = json_value(data, "n_keep",             defaults.n_keep);
        params.n_discard        = json_value(data, "n_discard",          defaults.n_discard);
      //params.t_max_prompt_ms  = json_value(data, "t_max_prompt_ms",    defaults.t_max_prompt_ms); // TODO: implement
        params.t_max_predict_ms = json_value(data, "t_max_predict_ms",   defaults.t_max_predict_ms);
        params.response_fields  = json_value(data, "response_fields",   std::vector<std::string>());

        params.sampling.top_k              = json_value(data, "top_k",              defaults.sampling.top_k);
        params.sampling.top_p              = json_value(data, "top_p",              defaults.sampling.top_p);
        params.sampling.min_p              = json_value(data, "min_p",              defaults.sampling.min_p);
        params.sampling.top_n_sigma        = json_value(data, "top_n_sigma",        defaults.sampling.top_n_sigma);
        params.sampling.xtc_probability    = json_value(data, "xtc_probability",    defaults.sampling.xtc_probability);
        params.sampling.xtc_threshold      = json_value(data, "xtc_threshold",      defaults.sampling.xtc_threshold);
        params.sampling.typ_p              = json_value(data, "typical_p",          defaults.sampling.typ_p);
        params.sampling.temp               = json_value(data, "temperature",        defaults.sampling.temp);
        params.sampling.dynatemp_range     = json_value(data, "dynatemp_range",     defaults.sampling.dynatemp_range);
        params.sampling.dynatemp_exponent  = json_value(data, "dynatemp_exponent",  defaults.sampling.dynatemp_exponent);
        params.sampling.penalty_last_n     = json_value(data, "repeat_last_n",      defaults.sampling.penalty_last_n);
        params.sampling.penalty_repeat     = json_value(data, "repeat_penalty",     defaults.sampling.penalty_repeat);
        params.sampling.penalty_freq       = json_value(data, "frequency_penalty",  defaults.sampling.penalty_freq);
        params.sampling.penalty_present    = json_value(data, "presence_penalty",   defaults.sampling.penalty_present);
        params.sampling.dry_multiplier     = json_value(data, "dry_multiplier",     defaults.sampling.dry_multiplier);
        params.sampling.dry_base           = json_value(data, "dry_base",           defaults.sampling.dry_base);
        params.sampling.dry_allowed_length = json_value(data, "dry_allowed_length", defaults.sampling.dry_allowed_length);
        params.sampling.dry_penalty_last_n = json_value(data, "dry_penalty_last_n", defaults.sampling.dry_penalty_last_n);
        params.sampling.mirostat           = json_value(data, "mirostat",           defaults.sampling.mirostat);
        params.sampling.mirostat_tau       = json_value(data, "mirostat_tau",       defaults.sampling.mirostat_tau);
        params.sampling.mirostat_eta       = json_value(data, "mirostat_eta",       defaults.sampling.mirostat_eta);
        params.sampling.seed               = json_value(data, "seed",               defaults.sampling.seed);
        params.sampling.n_probs            = json_value(data, "n_probs",            defaults.sampling.n_probs);
        params.sampling.min_keep           = json_value(data, "min_keep",           defaults.sampling.min_keep);
        params.post_sampling_probs         = json_value(data, "post_sampling_probs", defaults.post_sampling_probs);

        params.speculative.n_min = json_value(data, "speculative.n_min", defaults.speculative.n_min);
        params.speculative.n_max = json_value(data, "speculative.n_max", defaults.speculative.n_max);
        params.speculative.p_min = json_value(data, "speculative.p_min", defaults.speculative.p_min);

        params.speculative.n_min = std::min(params.speculative.n_max, params.speculative.n_min);
        params.speculative.n_min = std::max(params.speculative.n_min, 0);
        params.speculative.n_max = std::max(params.speculative.n_max, 0);

        // Use OpenAI API logprobs only if n_probs wasn't provided
        if (data.contains("logprobs") && params.sampling.n_probs == defaults.sampling.n_probs){
            params.sampling.n_probs = json_value(data, "logprobs", defaults.sampling.n_probs);
        }

        if (data.contains("lora")) {
            if (data.at("lora").is_array()) {
                params.lora = parse_lora_request(params_base.lora_adapters, data.at("lora"));
            } else {
                throw std::runtime_error("Error: 'lora' must be an array of objects with 'id' and 'scale' fields");
            }
        } else {
            params.lora = params_base.lora_adapters;
        }

        // TODO: add more sanity checks for the input parameters

        if (params.sampling.penalty_last_n < -1) {
            throw std::runtime_error("Error: repeat_last_n must be >= -1");
        }

        if (params.sampling.dry_penalty_last_n < -1) {
            throw std::runtime_error("Error: dry_penalty_last_n must be >= -1");
        }

        if (params.sampling.penalty_last_n == -1) {
            // note: should be the slot's context and not the full context, but it's ok
            params.sampling.penalty_last_n = llama_n_ctx(ctx);
        }

        if (params.sampling.dry_penalty_last_n == -1) {
            params.sampling.dry_penalty_last_n = llama_n_ctx(ctx);
        }

        if (params.sampling.dry_base < 1.0f) {
            params.sampling.dry_base = defaults.sampling.dry_base;
        }

        // sequence breakers for DRY
        {
            // Currently, this is not compatible with TextGen WebUI, Koboldcpp and SillyTavern format
            // Ref: https://github.com/oobabooga/text-generation-webui/blob/d1af7a41ade7bd3c3a463bfa640725edb818ebaf/extensions/openai/typing.py#L39

            if (data.contains("dry_sequence_breakers")) {
                params.sampling.dry_sequence_breakers = json_value(data, "dry_sequence_breakers", std::vector<std::string>());
                if (params.sampling.dry_sequence_breakers.empty()) {
                    throw std::runtime_error("Error: dry_sequence_breakers must be a non-empty array of strings");
                }
            }
        }

        // process "json_schema" and "grammar"
        if (data.contains("json_schema") && !data.contains("grammar")) {
            try {
                auto schema                  = json_value(data, "json_schema", json::object());
                SRV_DBG("JSON schema: %s\n", schema.dump(2).c_str());
                params.sampling.grammar      = json_schema_to_grammar(schema);
                SRV_DBG("Converted grammar: %s\n", params.sampling.grammar.c_str());
            } catch (const std::exception & e) {
                throw std::runtime_error(std::string("\"json_schema\": ") + e.what());
            }
        } else {
            params.sampling.grammar      = json_value(data, "grammar", defaults.sampling.grammar);
            SRV_DBG("Grammar: %s\n", params.sampling.grammar.c_str());
            params.sampling.grammar_lazy = json_value(data, "grammar_lazy", defaults.sampling.grammar_lazy);
            SRV_DBG("Grammar lazy: %s\n", params.sampling.grammar_lazy ? "true" : "false");
        }

        {
            auto it = data.find("chat_format");
            if (it != data.end()) {
                params.oaicompat_chat_format = static_cast<common_chat_format>(it->get<int>());
                SRV_INF("Chat format: %s\n", common_chat_format_name(params.oaicompat_chat_format).c_str());
            } else {
                params.oaicompat_chat_format = defaults.oaicompat_chat_format;
            }
        }

        {
            const auto preserved_tokens = data.find("preserved_tokens");
            if (preserved_tokens != data.end()) {
                for (const auto & t : *preserved_tokens) {
                    auto ids = common_tokenize(vocab, t.get<std::string>(), /* add_special= */ false, /* parse_special= */ true);
                    if (ids.size() == 1) {
                        SRV_DBG("Preserved token: %d\n", ids[0]);
                        params.sampling.preserved_tokens.insert(ids[0]);
                    } else {
                        // This may happen when using a tool call style meant for a model with special tokens to preserve on a model without said tokens.
                        SRV_DBG("Not preserved because more than 1 token: %s\n", t.get<std::string>().c_str());
                    }
                }
            }
            const auto grammar_triggers = data.find("grammar_triggers");
            if (grammar_triggers != data.end()) {
                for (const auto & t : *grammar_triggers) {
                    server_grammar_trigger ct(t);
                    if (ct.value.type == COMMON_GRAMMAR_TRIGGER_TYPE_WORD) {
                        const auto & word = ct.value.value;
                        auto ids = common_tokenize(vocab, word, /* add_special= */ false, /* parse_special= */ true);
                        if (ids.size() == 1) {
                            auto token = ids[0];
                            if (std::find(params.sampling.preserved_tokens.begin(), params.sampling.preserved_tokens.end(), (llama_token) token) == params.sampling.preserved_tokens.end()) {
                                throw std::runtime_error("Grammar trigger word should be marked as preserved token: " + word);
                            }
                            SRV_DBG("Grammar trigger token: %d (`%s`)\n", token, word.c_str());
                            common_grammar_trigger trigger;
                            trigger.type = COMMON_GRAMMAR_TRIGGER_TYPE_TOKEN;
                            trigger.value = word;
                            trigger.token = token;
                            params.sampling.grammar_triggers.push_back(std::move(trigger));
                        } else {
                            SRV_DBG("Grammar trigger word: `%s`\n", word.c_str());
                            params.sampling.grammar_triggers.push_back({COMMON_GRAMMAR_TRIGGER_TYPE_WORD, word});
                        }
                    } else {
                        params.sampling.grammar_triggers.push_back(std::move(ct.value));
                    }
                }
            }
            if (params.sampling.grammar_lazy && params.sampling.grammar_triggers.empty()) {
                throw std::runtime_error("Error: no triggers set for lazy grammar!");
            }
        }

        {
            params.sampling.logit_bias.clear();
            params.ignore_eos = json_value(data, "ignore_eos", false);

            const auto & logit_bias = data.find("logit_bias");
            if (logit_bias != data.end() && logit_bias->is_array()) {
                const int n_vocab = llama_vocab_n_tokens(vocab);
                for (const auto & el : *logit_bias) {
                    // TODO: we may want to throw errors here, in case "el" is incorrect
                    if (el.is_array() && el.size() == 2) {
                        float bias;
                        if (el[1].is_number()) {
                            bias = el[1].get<float>();
                        } else if (el[1].is_boolean() && !el[1].get<bool>()) {
                            bias = -INFINITY;
                        } else {
                            continue;
                        }

                        if (el[0].is_number_integer()) {
                            llama_token tok = el[0].get<llama_token>();
                            if (tok >= 0 && tok < n_vocab) {
                                params.sampling.logit_bias.push_back({tok, bias});
                            }
                        } else if (el[0].is_string()) {
                            auto toks = common_tokenize(vocab, el[0].get<std::string>(), false);
                            for (auto tok : toks) {
                                params.sampling.logit_bias.push_back({tok, bias});
                            }
                        }
                    }
                }
            }
        }

        {
            params.antiprompt.clear();

            const auto & stop = data.find("stop");
            if (stop != data.end() && stop->is_array()) {
                for (const auto & word : *stop) {
                    if (!word.empty()) {
                        params.antiprompt.push_back(word);
                    }
                }
            }
        }

        {
            const auto samplers = data.find("samplers");
            if (samplers != data.end()) {
                if (samplers->is_array()) {
                    params.sampling.samplers = common_sampler_types_from_names(*samplers, false);
                } else if (samplers->is_string()){
                    params.sampling.samplers = common_sampler_types_from_chars(samplers->get<std::string>());
                }
            } else {
                params.sampling.samplers = defaults.sampling.samplers;
            }
        }

        std::string model_name = params_base.model_alias.empty() ? DEFAULT_OAICOMPAT_MODEL : params_base.model_alias;
        params.oaicompat_model = json_value(data, "model", model_name);

        return params;
    }

    // utility function
    static std::unordered_set<int> get_list_id(const std::vector<server_task> & tasks) {
        std::unordered_set<int> ids(tasks.size());
        for (size_t i = 0; i < tasks.size(); i++) {
            ids.insert(tasks[i].id);
        }
        return ids;
    }
};

struct result_timings {
    int32_t prompt_n = -1;
    double prompt_ms;
    double prompt_per_token_ms;
    double prompt_per_second;

    int32_t predicted_n = -1;
    double predicted_ms;
    double predicted_per_token_ms;
    double predicted_per_second;

    // Optional speculative metrics - only included when > 0
    int32_t draft_n = 0;
    int32_t draft_n_accepted = 0;

    json to_json() const {
        json base = {
            {"prompt_n",               prompt_n},
            {"prompt_ms",              prompt_ms},
            {"prompt_per_token_ms",    prompt_per_token_ms},
            {"prompt_per_second",      prompt_per_second},

            {"predicted_n",            predicted_n},
            {"predicted_ms",           predicted_ms},
            {"predicted_per_token_ms", predicted_per_token_ms},
            {"predicted_per_second",   predicted_per_second},
        };

        if (draft_n > 0) {
            base["draft_n"] = draft_n;
            base["draft_n_accepted"] = draft_n_accepted;
        }

        return base;
    }
};

struct server_task_result {
    int id           = -1;
    int id_slot      = -1;
    virtual bool is_error() {
        // only used by server_task_result_error
        return false;
    }
    virtual bool is_stop() {
        // only used by server_task_result_cmpl_*
        return false;
    }
    virtual int get_index() {
        return -1;
    }
    virtual json to_json() = 0;
    virtual ~server_task_result() = default;
};

// using shared_ptr for polymorphism of server_task_result
using server_task_result_ptr = std::unique_ptr<server_task_result>;

inline std::string stop_type_to_str(stop_type type) {
    switch (type) {
        case STOP_TYPE_EOS:   return "eos";
        case STOP_TYPE_WORD:  return "word";
        case STOP_TYPE_LIMIT: return "limit";
        default:              return "none";
    }
}

struct completion_token_output {
    llama_token tok;
    float prob;
    std::string text_to_send;
    struct prob_info {
        llama_token tok;
        std::string txt;
        float prob;
    };
    std::vector<prob_info> probs;

    json to_json(bool post_sampling_probs) const {
        json probs_for_token = json::array();
        for (const auto & p : probs) {
            std::string txt(p.txt);
            txt.resize(validate_utf8(txt));
            probs_for_token.push_back(json {
                {"id",      p.tok},
                {"token",   txt},
                {"bytes",   str_to_bytes(p.txt)},
                {
                    post_sampling_probs ? "prob" : "logprob",
                    post_sampling_probs ? p.prob : logarithm(p.prob)
                },
            });
        }
        return probs_for_token;
    }

    static json probs_vector_to_json(const std::vector<completion_token_output> & probs, bool post_sampling_probs) {
        json out = json::array();
        for (const auto & p : probs) {
            std::string txt(p.text_to_send);
            txt.resize(validate_utf8(txt));
            out.push_back(json {
                {"id",           p.tok},
                {"token",        txt},
                {"bytes",        str_to_bytes(p.text_to_send)},
                {
                    post_sampling_probs ? "prob" : "logprob",
                    post_sampling_probs ? p.prob : logarithm(p.prob)
                },
                {
                    post_sampling_probs ? "top_probs" : "top_logprobs",
                    p.to_json(post_sampling_probs)
                },
            });
        }
        return out;
    }

    static float logarithm(float x) {
        // nlohmann::json converts -inf to null, so we need to prevent that
        return x == 0.0f ? std::numeric_limits<float>::lowest() : std::log(x);
    }

    static std::vector<unsigned char> str_to_bytes(const std::string & str) {
        std::vector<unsigned char> bytes;
        for (unsigned char c : str) {
            bytes.push_back(c);
        }
        return bytes;
    }
};

struct server_task_result_cmpl_final : server_task_result {
    int index = 0;

    std::string content;
    llama_tokens tokens;

    bool stream;
    result_timings timings;
    std::string prompt;

    bool truncated;
    int32_t n_decoded;
    int32_t n_prompt_tokens;
    int32_t n_tokens_cached;
    bool has_new_line;
    std::string stopping_word;
    stop_type stop = STOP_TYPE_NONE;

    bool post_sampling_probs;
    std::vector<completion_token_output> probs_output;
    std::vector<std::string>  response_fields;

    slot_params generation_params;

    // OAI-compat fields
    bool                  verbose                  = false;
    oaicompat_type        oaicompat                = OAICOMPAT_TYPE_NONE;
    std::string           oaicompat_model;
    std::string           oaicompat_cmpl_id;
    common_chat_format    oaicompat_chat_format    = COMMON_CHAT_FORMAT_CONTENT_ONLY;

    virtual int get_index() override {
        return index;
    }

    virtual bool is_stop() override {
        return true; // in stream mode, final responses are considered stop
    }

    virtual json to_json() override {
        switch (oaicompat) {
            case OAICOMPAT_TYPE_NONE:
                return to_json_non_oaicompat();
            case OAICOMPAT_TYPE_COMPLETION:
                return to_json_oaicompat();
            case OAICOMPAT_TYPE_CHAT:
                return stream ? to_json_oaicompat_chat_stream() : to_json_oaicompat_chat();
            default:
                GGML_ASSERT(false && "Invalid oaicompat_type");
        }
    }

    json to_json_non_oaicompat() {
        json res = json {
            {"index",               index},
            {"content",             stream ? "" : content}, // in stream mode, content is already in last partial chunk
            {"tokens",              stream ? llama_tokens {} : tokens},
            {"id_slot",             id_slot},
            {"stop",                true},
            {"model",               oaicompat_model},
            {"tokens_predicted",    n_decoded},
            {"tokens_evaluated",    n_prompt_tokens},
            {"generation_settings", generation_params.to_json()},
            {"prompt",              prompt},
            {"has_new_line",        has_new_line},
            {"truncated",           truncated},
            {"stop_type",           stop_type_to_str(stop)},
            {"stopping_word",       stopping_word},
            {"tokens_cached",       n_tokens_cached},
            {"timings",             timings.to_json()},
        };
        if (!stream && !probs_output.empty()) {
            res["completion_probabilities"] = completion_token_output::probs_vector_to_json(probs_output, post_sampling_probs);
        }
        return response_fields.empty() ? res : json_get_nested_values(response_fields, res);
    }

    json to_json_oaicompat() {
        std::time_t t = std::time(0);
        json logprobs = json(nullptr); // OAI default to null
        if (!stream && probs_output.size() > 0) {
            logprobs = json{
                {"content", completion_token_output::probs_vector_to_json(probs_output, post_sampling_probs)},
            };
        }
        json finish_reason = "length";
        if (stop == STOP_TYPE_WORD || stop == STOP_TYPE_EOS) {
            finish_reason = "stop";
        }
        json res = json {
            {"choices",            json::array({
                json{
                    {"text",          stream ? "" : content}, // in stream mode, content is already in last partial chunk
                    {"index",         index},
                    {"logprobs",      logprobs},
                    {"finish_reason", finish_reason},
                }
            })},
            {"created",            t},
            {"model",              oaicompat_model},
            {"system_fingerprint", build_info},
            {"object",             "text_completion"},
            {"usage", json {
                {"completion_tokens", n_decoded},
                {"prompt_tokens",     n_prompt_tokens},
                {"total_tokens",      n_decoded + n_prompt_tokens}
            }},
            {"id", oaicompat_cmpl_id}
        };

        // extra fields for debugging purposes
        if (verbose) {
            res["__verbose"] = to_json_non_oaicompat();
        }
        if (timings.prompt_n >= 0) {
            res.push_back({"timings", timings.to_json()});
        }

        return res;
    }

    json to_json_oaicompat_chat() {
        std::string finish_reason = "length";
        common_chat_msg msg;
        if (stop == STOP_TYPE_WORD || stop == STOP_TYPE_EOS) {
            SRV_DBG("Parsing chat message: %s\n", content.c_str());
            msg = common_chat_parse(content, oaicompat_chat_format);
            finish_reason = msg.tool_calls.empty() ? "stop" : "tool_calls";
        } else {
            msg.content = content;
        }

        json message {
            {"role", "assistant"},
        };
        if (!msg.reasoning_content.empty()) {
            message["reasoning_content"] = msg.reasoning_content;
        }
        if (msg.content.empty() && !msg.tool_calls.empty()) {
            message["content"] = json();
        } else {
            message["content"] = msg.content;
        }
        if (!msg.tool_calls.empty()) {
            auto tool_calls = json::array();
            for (const auto & tc : msg.tool_calls) {
                tool_calls.push_back({
                    {"type", "function"},
                    {"function", {
                        {"name", tc.name},
                        {"arguments", tc.arguments},
                    }},
                    // Some templates generate and require an id (sometimes in a very specific format, e.g. Mistral Nemo).
                    // We only generate a random id for the ones that don't generate one by themselves
                    // (they also won't get to see it as their template likely doesn't use it, so it's all for the client)
                    {"id", tc.id.empty() ? gen_tool_call_id() : tc.id},
                });
            }
            message["tool_calls"] = tool_calls;
        }

        json choice {
            {"finish_reason", finish_reason},
            {"index", 0},
            {"message", message},
        };

        if (!stream && probs_output.size() > 0) {
            choice["logprobs"] = json{
                {"content", completion_token_output::probs_vector_to_json(probs_output, post_sampling_probs)},
            };
        }

        std::time_t t = std::time(0);

        json res = json {
            {"choices",            json::array({choice})},
            {"created",            t},
            {"model",              oaicompat_model},
            {"system_fingerprint", build_info},
            {"object",             "chat.completion"},
            {"usage", json {
                {"completion_tokens", n_decoded},
                {"prompt_tokens",     n_prompt_tokens},
                {"total_tokens",      n_decoded + n_prompt_tokens}
            }},
            {"id", oaicompat_cmpl_id}
        };

        // extra fields for debugging purposes
        if (verbose) {
            res["__verbose"] = to_json_non_oaicompat();
        }
        if (timings.prompt_n >= 0) {
            res.push_back({"timings", timings.to_json()});
        }

        return res;
    }

    json to_json_oaicompat_chat_stream() {
        std::time_t t = std::time(0);
        std::string finish_reason = "length";
        if (stop == STOP_TYPE_WORD || stop == STOP_TYPE_EOS) {
            finish_reason = "stop";
        }

        json choice = json {
            {"finish_reason", finish_reason},
            {"index", 0},
            {"delta", json::object()}
        };

        json ret = json {
            {"choices",            json::array({choice})},
            {"created",            t},
            {"id",                 oaicompat_cmpl_id},
            {"model",              oaicompat_model},
            {"system_fingerprint", build_info},
            {"object",             "chat.completion.chunk"},
            {"usage", json {
                {"completion_tokens", n_decoded},
                {"prompt_tokens",     n_prompt_tokens},
                {"total_tokens",      n_decoded + n_prompt_tokens},
            }},
        };

        if (timings.prompt_n >= 0) {
            ret.push_back({"timings", timings.to_json()});
        }

        // extra fields for debugging purposes
        if (verbose) {
            ret["__verbose"] = to_json_non_oaicompat();
        }

        return ret;
    }
};

struct server_task_result_cmpl_partial : server_task_result {
    int index = 0;

    std::string  content;
    llama_tokens tokens;

    int32_t n_decoded;
    int32_t n_prompt_tokens;

    bool post_sampling_probs;
    completion_token_output prob_output;
    result_timings timings;

    // OAI-compat fields
    bool           verbose   = false;
    oaicompat_type oaicompat = OAICOMPAT_TYPE_NONE;
    std::string    oaicompat_model;
    std::string    oaicompat_cmpl_id;

    virtual int get_index() override {
        return index;
    }

    virtual bool is_stop() override {
        return false; // in stream mode, partial responses are not considered stop
    }

    virtual json to_json() override {
        switch (oaicompat) {
            case OAICOMPAT_TYPE_NONE:
                return to_json_non_oaicompat();
            case OAICOMPAT_TYPE_COMPLETION:
                return to_json_oaicompat();
            case OAICOMPAT_TYPE_CHAT:
                return to_json_oaicompat_chat();
            default:
                GGML_ASSERT(false && "Invalid oaicompat_type");
        }
    }

    json to_json_non_oaicompat() {
        // non-OAI-compat JSON
        json res = json {
            {"index",            index},
            {"content",          content},
            {"tokens",           tokens},
            {"stop",             false},
            {"id_slot",          id_slot},
            {"tokens_predicted", n_decoded},
            {"tokens_evaluated", n_prompt_tokens},
        };
        // populate the timings object when needed (usually for the last response or with timings_per_token enabled)
        if (timings.prompt_n > 0) {
            res.push_back({"timings", timings.to_json()});
        }
        if (!prob_output.probs.empty()) {
            res["completion_probabilities"] = completion_token_output::probs_vector_to_json({prob_output}, post_sampling_probs);
        }
        return res;
    }

    json to_json_oaicompat() {
        std::time_t t = std::time(0);
        json logprobs = json(nullptr); // OAI default to null
        if (prob_output.probs.size() > 0) {
            logprobs = json{
                {"content", completion_token_output::probs_vector_to_json({prob_output}, post_sampling_probs)},
            };
        }
        json res = json {
            {"choices",            json::array({
                json{
                    {"text",          content},
                    {"index",         index},
                    {"logprobs",      logprobs},
                    {"finish_reason", nullptr},
                }
            })},
            {"created",            t},
            {"model",              oaicompat_model},
            {"system_fingerprint", build_info},
            {"object",             "text_completion"},
            {"id",                 oaicompat_cmpl_id}
        };

        // extra fields for debugging purposes
        if (verbose) {
            res["__verbose"] = to_json_non_oaicompat();
        }
        if (timings.prompt_n >= 0) {
            res.push_back({"timings", timings.to_json()});
        }

        return res;
    }

    json to_json_oaicompat_chat() {
        bool first = n_decoded == 0;
        std::time_t t = std::time(0);
        json choices;

        if (first) {
            if (content.empty()) {
                choices = json::array({json{{"finish_reason", nullptr},
                                            {"index", 0},
                                            {"delta", json{{"role", "assistant"}}}}});
            } else {
                // We have to send this as two updates to conform to openai behavior
                json initial_ret = json{{"choices", json::array({json{
                                        {"finish_reason", nullptr},
                                        {"index", 0},
                                        {"delta", json{
                                            {"role", "assistant"}
                                        }}}})},
                            {"created", t},
                            {"id", oaicompat_cmpl_id},
                            {"model", oaicompat_model},
                            {"object", "chat.completion.chunk"}};

                json second_ret = json{
                            {"choices", json::array({json{{"finish_reason", nullptr},
                                                            {"index", 0},
                                                            {"delta", json {
                                                            {"content", content}}}
                                                            }})},
                            {"created", t},
                            {"id", oaicompat_cmpl_id},
                            {"model", oaicompat_model},
                            {"object", "chat.completion.chunk"}};

                return std::vector<json>({initial_ret, second_ret});
            }
        } else {
            choices = json::array({json{
                {"finish_reason", nullptr},
                {"index", 0},
                {"delta",
                json {
                    {"content", content},
                }},
            }});
        }

        GGML_ASSERT(choices.size() >= 1);

        if (prob_output.probs.size() > 0) {
            choices[0]["logprobs"] = json{
                {"content", completion_token_output::probs_vector_to_json({prob_output}, post_sampling_probs)},
            };
        }

        json ret = json {
            {"choices",            choices},
            {"created",            t},
            {"id",                 oaicompat_cmpl_id},
            {"model",              oaicompat_model},
            {"system_fingerprint", build_info},
            {"object",             "chat.completion.chunk"}
        };

        if (timings.prompt_n >= 0) {
            ret.push_back({"timings", timings.to_json()});
        }

        return std::vector<json>({ret});
    }
};

struct server_task_result_embd : server_task_result {
    int index = 0;
    std::vector<std::vector<float>> embedding;

    int32_t n_tokens;

    // OAI-compat fields
    oaicompat_type oaicompat = OAICOMPAT_TYPE_NONE;

    virtual int get_index() override {
        return index;
    }

    virtual json to_json() override {
        return oaicompat == OAICOMPAT_TYPE_EMBEDDING
            ? to_json_oaicompat()
            : to_json_non_oaicompat();
    }

    json to_json_non_oaicompat() {
        return json {
            {"index",     index},
            {"embedding", embedding},
        };
    }

    json to_json_oaicompat() {
        return json {
            {"index",            index},
            {"embedding",        embedding[0]},
            {"tokens_evaluated", n_tokens},
        };
    }
};


//OWL BEGIN
std::vector<uint8_t> hexToBytes(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        throw std::runtime_error("Hex string length must be even.");
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned int byte = 0;
        std::istringstream(byteString) >> std::hex >> byte;
        bytes.push_back(static_cast<uint8_t>(byte));
    }
    return bytes;
}
std::string dehex_string(const std::string& hex)
{
    auto v = hexToBytes(hex);
    return std::string(std::begin(v),std::end(v));
}
//OWL END

//OWL BEGIN
struct server_task_result_chunking : server_task_result {
    int index = 0;
    llama_tokens tokens;//will originally stay empty, to be added later
    std::string contents;//will originally stay empty, to be added later
    std::vector<float> embedding;
    int32_t n_tokens;

    virtual int get_index() override {
        return index;
    }

    virtual json to_json() override {
        return json {
            {"index",            index},
            {"tokens",            tokens},
            {"content",            contents},
            {"embedding",        embedding},
            {"tokens_evaluated", n_tokens},
        };
    }
    json to_json_simple() {
        return json {
            {"index",            index},
            {"tokens_evaluated", n_tokens},
        };
    }

    /***
     * method that chunks an server_task_result_embd into equal sizes of n_tokens, each chunk overlapping with the previous and next one
     * the overlaps are equal, except when n_tokens is odd, in whcih case the overlaps will fluctuate by more or less one
     * a remainder may be included in the end
     * the embedding of a chunk is the average of individual token embedding
    */
    static std::vector<std::unique_ptr<server_task_result_chunking>> get_brutal_chunking(const struct llama_context * ctx, const std::vector<server_task_result_embd*> & embeds, const llama_tokens& toks, int32_t chunk_size, int32_t chunk_overlap)
    {
        std::vector<std::unique_ptr<server_task_result_chunking>> res;
        if (embeds.empty() || chunk_size <= 0) {
            return res;
        }
        std::vector<std::vector<float>> all_embedding;
        for(const auto* p_embeds :  embeds)
        {
            all_embedding.insert(all_embedding.end(),std::begin(p_embeds->embedding),std::end(p_embeds->embedding));
        }
        const int32_t num_embeds = all_embedding.size();

        for (int32_t i = 0; i < num_embeds; i += (chunk_size - chunk_overlap)) {
            auto chunk = std::make_unique<server_task_result_chunking>();
            chunk->index = i;
            chunk->n_tokens = std::min((int32_t)chunk_size, num_embeds - i);
            chunk->tokens.assign(toks.begin() + i, toks.begin() + i + chunk->n_tokens);
            chunk->contents = common_detokenize(ctx,chunk->tokens,false);

            chunk->embedding.resize(all_embedding[0].size());
            for (int32_t j = 0; j < chunk->n_tokens; ++j)
                for(uint32_t dim = 0; dim < chunk->embedding.size(); ++dim)
                    chunk->embedding[dim] += all_embedding[i + j][dim];
            for(uint32_t dim = 0; dim < chunk->embedding.size(); ++dim)
                chunk->embedding[dim] /= (float)chunk->n_tokens;
            res.push_back(std::move(chunk));

            if (i + chunk_size >= num_embeds) {
                break; // Avoid going beyond the bounds
            }
        }

        return res;
    }

};
//OWL END

struct server_task_result_rerank : server_task_result {
    int index = 0;
    float score = -1e6;

    int32_t n_tokens;

    virtual int get_index() override {
        return index;
    }

    virtual json to_json() override {
        return json {
            {"index",            index},
            {"score",            score},
            {"tokens_evaluated", n_tokens},
        };
    }
};

// this function maybe used outside of server_task_result_error
static json format_error_response(const std::string & message, const enum error_type type) {
    std::string type_str;
    int code = 500;
    switch (type) {
        case ERROR_TYPE_INVALID_REQUEST:
            type_str = "invalid_request_error";
            code = 400;
            break;
        case ERROR_TYPE_AUTHENTICATION:
            type_str = "authentication_error";
            code = 401;
            break;
        case ERROR_TYPE_NOT_FOUND:
            type_str = "not_found_error";
            code = 404;
            break;
        case ERROR_TYPE_SERVER:
            type_str = "server_error";
            code = 500;
            break;
        case ERROR_TYPE_PERMISSION:
            type_str = "permission_error";
            code = 403;
            break;
        case ERROR_TYPE_NOT_SUPPORTED:
            type_str = "not_supported_error";
            code = 501;
            break;
        case ERROR_TYPE_UNAVAILABLE:
            type_str = "unavailable_error";
            code = 503;
            break;
    }
    return json {
        {"code", code},
        {"message", message},
        {"type", type_str},
    };
}

struct server_task_result_error : server_task_result {
    int index = 0;
    error_type err_type = ERROR_TYPE_SERVER;
    std::string err_msg;

    virtual bool is_error() override {
        return true;
    }

    virtual json to_json() override {
        return format_error_response(err_msg, err_type);
    }
};

struct server_task_result_metrics : server_task_result {
    int n_idle_slots;
    int n_processing_slots;
    int n_tasks_deferred;
    int64_t t_start;

    int32_t kv_cache_tokens_count;
    int32_t kv_cache_used_cells;

    // TODO: somehow reuse server_metrics in the future, instead of duplicating the fields
    uint64_t n_prompt_tokens_processed_total = 0;
    uint64_t t_prompt_processing_total       = 0;
    uint64_t n_tokens_predicted_total        = 0;
    uint64_t t_tokens_generation_total       = 0;

    uint64_t n_prompt_tokens_processed = 0;
    uint64_t t_prompt_processing       = 0;

    uint64_t n_tokens_predicted  = 0;
    uint64_t t_tokens_generation = 0;

    uint64_t n_decode_total     = 0;
    uint64_t n_busy_slots_total = 0;

    // while we can also use std::vector<server_slot> this requires copying the slot object which can be quite messy
    // therefore, we use json to temporarily store the slot.to_json() result
    json slots_data = json::array();

    virtual json to_json() override {
        return json {
            { "idle",                            n_idle_slots },
            { "processing",                      n_processing_slots },
            { "deferred",                        n_tasks_deferred },
            { "t_start",                         t_start },

            { "n_prompt_tokens_processed_total", n_prompt_tokens_processed_total },
            { "t_tokens_generation_total",       t_tokens_generation_total },
            { "n_tokens_predicted_total",        n_tokens_predicted_total },
            { "t_prompt_processing_total",       t_prompt_processing_total },

            { "n_prompt_tokens_processed",       n_prompt_tokens_processed },
            { "t_prompt_processing",             t_prompt_processing },
            { "n_tokens_predicted",              n_tokens_predicted },
            { "t_tokens_generation",             t_tokens_generation },

            { "n_decode_total",                  n_decode_total },
            { "n_busy_slots_total",              n_busy_slots_total },

            { "kv_cache_tokens_count",           kv_cache_tokens_count },
            { "kv_cache_used_cells",             kv_cache_used_cells },

            { "slots",                           slots_data },
        };
    }
};

struct server_task_result_slot_save_load : server_task_result {
    std::string filename;
    bool is_save; // true = save, false = load

    size_t n_tokens;
    size_t n_bytes;
    double t_ms;

    virtual json to_json() override {
        if (is_save) {
            return json {
                { "id_slot",   id_slot },
                { "filename",  filename },
                { "n_saved",   n_tokens },
                { "n_written", n_bytes },
                { "timings", {
                    { "save_ms", t_ms }
                }},
            };
        } else {
            return json {
                { "id_slot",    id_slot },
                { "filename",   filename },
                { "n_restored", n_tokens },
                { "n_read",     n_bytes },
                { "timings", {
                    { "restore_ms", t_ms }
                }},
            };
        }
    }
};

struct server_task_result_slot_erase : server_task_result {
    size_t n_erased;

    virtual json to_json() override {
        return json {
            { "id_slot",  id_slot },
            { "n_erased", n_erased },
        };
    }
};

struct server_task_result_apply_lora : server_task_result {
    virtual json to_json() override {
        return json {{ "success", true }};
    }
};

struct server_slot {
    int id;
    int id_task = -1;

    // only used for completion/embedding/infill/rerank
    server_task_type task_type = SERVER_TASK_TYPE_COMPLETION;

    llama_batch batch_spec = {};

    llama_context * ctx = nullptr;
    llama_context * ctx_dft = nullptr;

    // multimodal
    mtmd_context * mctx = nullptr;

    common_speculative * spec = nullptr;

    std::vector<common_adapter_lora_info> lora;

    // the index relative to completion multi-task request
    size_t index = 0;

    struct slot_params params;

    slot_state state = SLOT_STATE_IDLE;

    // used to determine the slot that has been used the longest
    int64_t t_last_used = -1;

    // generation props
    int32_t n_ctx       = 0;  // context size per slot
    int32_t n_past      = 0;
    int32_t n_decoded   = 0;
    int32_t n_remaining = -1;
    int32_t i_batch     = -1;
    int32_t n_predict   = -1; // TODO: disambiguate from params.n_predict

    // n_prompt_tokens may not be equal to prompt_tokens.size(), because prompt maybe truncated
    int32_t n_prompt_tokens           = 0;
    int32_t n_prompt_tokens_processed = 0;

    // input prompt tokens
    server_tokens prompt_tokens;

    size_t last_nl_pos = 0;

    std::string  generated_text;
    llama_tokens generated_tokens;

    server_tokens cache_tokens;

    std::vector<completion_token_output> generated_token_probs;

    bool has_next_token = true;
    bool has_new_line   = false;
    bool truncated      = false;
    stop_type stop;

    std::string stopping_word;

    // sampling
    json json_schema;

    struct common_sampler * smpl = nullptr;

    llama_token sampled;

    common_chat_format chat_format = COMMON_CHAT_FORMAT_CONTENT_ONLY;

    // stats
    size_t n_sent_text        = 0; // number of sent text character

    int64_t t_start_process_prompt;
    int64_t t_start_generation;

    double t_prompt_processing; // ms
    double t_token_generation;  // ms

    std::function<void(int)> callback_on_release;

    // Speculative decoding stats
    int32_t n_draft_total = 0;      // Total draft tokens generated
    int32_t n_draft_accepted = 0;   // Draft tokens actually accepted

    void reset() {
        SLT_DBG(*this, "%s", "\n");

        n_prompt_tokens    = 0;
        last_nl_pos        = 0;
        generated_text     = "";
        has_new_line       = false;
        truncated          = false;
        stop               = STOP_TYPE_NONE;
        stopping_word      = "";
        n_past             = 0;
        n_sent_text        = 0;
        task_type          = SERVER_TASK_TYPE_COMPLETION;

        generated_tokens.clear();
        generated_token_probs.clear();

        // clear speculative decoding stats
        n_draft_total = 0;
        n_draft_accepted = 0;
    }

    bool is_non_causal() const {
        return task_type == SERVER_TASK_TYPE_EMBEDDING || task_type == SERVER_TASK_TYPE_RERANK;
    }

    bool can_batch_with(server_slot & other_slot) const {
        return is_non_causal() == other_slot.is_non_causal()
            && are_lora_equal(lora, other_slot.lora);
    }

    bool has_budget(const common_params & global_params) {
        if (params.n_predict == -1 && global_params.n_predict == -1) {
            return true; // limitless
        }

        n_remaining = -1;

        if (params.n_predict != -1) {
            n_remaining = params.n_predict - n_decoded;
        } else if (global_params.n_predict != -1) {
            n_remaining = global_params.n_predict - n_decoded;
        }

        return n_remaining > 0; // no budget
    }

    bool is_processing() const {
        return state != SLOT_STATE_IDLE;
    }

    bool can_speculate() const {
        return ctx_dft && params.speculative.n_max > 0 && params.cache_prompt;
    }

    void add_token(const completion_token_output & token) {
        if (!is_processing()) {
            SLT_WRN(*this, "%s", "slot is not processing\n");
            return;
        }
        generated_token_probs.push_back(token);
    }

    void release() {
        if (is_processing()) {
            SLT_INF(*this, "stop processing: n_past = %d, truncated = %d\n", n_past, truncated);

            t_last_used = ggml_time_us();
            t_token_generation = (ggml_time_us() - t_start_generation) / 1e3;
            state = SLOT_STATE_IDLE;
            callback_on_release(id);
        }
    }

    result_timings get_timings() const {
        result_timings timings;
        timings.prompt_n = n_prompt_tokens_processed;
        timings.prompt_ms = t_prompt_processing;
        timings.prompt_per_token_ms = t_prompt_processing / n_prompt_tokens_processed;
        timings.prompt_per_second = 1e3 / t_prompt_processing * n_prompt_tokens_processed;

        timings.predicted_n = n_decoded;
        timings.predicted_ms = t_token_generation;
        timings.predicted_per_token_ms = t_token_generation / n_decoded;
        timings.predicted_per_second = 1e3 / t_token_generation * n_decoded;

        // Add speculative metrics
        if (n_draft_total > 0) {
            timings.draft_n = n_draft_total;
            timings.draft_n_accepted = n_draft_accepted;
        }

        return timings;
    }

    size_t find_stopping_strings(const std::string & text, const size_t last_token_size, bool is_full_stop) {
        size_t stop_pos = std::string::npos;

        for (const std::string & word : params.antiprompt) {
            size_t pos;

            if (is_full_stop) {
                const size_t tmp      = word.size() + last_token_size;
                const size_t from_pos = text.size() > tmp ? text.size() - tmp : 0;

                pos = text.find(word, from_pos);
            } else {
                // otherwise, partial stop
                pos = find_partial_stop_string(word, text);
            }

            if (pos != std::string::npos && (stop_pos == std::string::npos || pos < stop_pos)) {
                if (is_full_stop) {
                    stop           = STOP_TYPE_WORD;
                    stopping_word  = word;
                    has_next_token = false;
                }
                stop_pos = pos;
            }
        }

        return stop_pos;
    }

    void print_timings() const {
        const double t_prompt        =       t_prompt_processing / n_prompt_tokens_processed;
        const double n_prompt_second = 1e3 / t_prompt_processing * n_prompt_tokens_processed;

        const double t_gen        =       t_token_generation / n_decoded;
        const double n_gen_second = 1e3 / t_token_generation * n_decoded;

        SLT_INF(*this,
                "\n"
                "prompt eval time = %10.2f ms / %5d tokens (%8.2f ms per token, %8.2f tokens per second)\n"
                "       eval time = %10.2f ms / %5d tokens (%8.2f ms per token, %8.2f tokens per second)\n"
                "      total time = %10.2f ms / %5d tokens\n",
                t_prompt_processing, n_prompt_tokens_processed, t_prompt, n_prompt_second,
                t_token_generation, n_decoded, t_gen, n_gen_second,
                t_prompt_processing + t_token_generation, n_prompt_tokens_processed + n_decoded);

        if (n_draft_total > 0) {
            const float draft_ratio = (float) n_draft_accepted / n_draft_total;
            SLT_INF(*this,
                    "\n"
                    "draft acceptance rate = %0.5f (%5d accepted / %5d generated)\n",
                    draft_ratio, n_draft_accepted, n_draft_total
            );
        }
    }

    json to_json() const {
        return json {
            {"id",            id},
            {"id_task",       id_task},
            {"n_ctx",         n_ctx},
            {"speculative",   can_speculate()},
            {"is_processing", is_processing()},
            {"non_causal",    is_non_causal()},
            {"params",        params.to_json()},
            {"prompt",        prompt_tokens.detokenize(ctx, true)},
            {"next_token",
                {
                    {"has_next_token", has_next_token},
                    {"has_new_line",   has_new_line},
                    {"n_remain",       n_remaining},
                    {"n_decoded",      n_decoded},
                    {"stopping_word",  stopping_word},
                }
            },
        };
    }
};

struct server_metrics {
    int64_t t_start = 0;

    uint64_t n_prompt_tokens_processed_total = 0;
    uint64_t t_prompt_processing_total       = 0;
    uint64_t n_tokens_predicted_total        = 0;
    uint64_t t_tokens_generation_total       = 0;

    uint64_t n_prompt_tokens_processed = 0;
    uint64_t t_prompt_processing       = 0;

    uint64_t n_tokens_predicted  = 0;
    uint64_t t_tokens_generation = 0;

    uint64_t n_decode_total     = 0;
    uint64_t n_busy_slots_total = 0;

    void init() {
        t_start = ggml_time_us();
    }

    void on_prompt_eval(const server_slot & slot) {
        n_prompt_tokens_processed_total += slot.n_prompt_tokens_processed;
        n_prompt_tokens_processed       += slot.n_prompt_tokens_processed;
        t_prompt_processing             += slot.t_prompt_processing;
        t_prompt_processing_total       += slot.t_prompt_processing;
    }

    void on_prediction(const server_slot & slot) {
        n_tokens_predicted_total   += slot.n_decoded;
        n_tokens_predicted         += slot.n_decoded;
        t_tokens_generation        += slot.t_token_generation;
        t_tokens_generation_total  += slot.t_token_generation;
    }

    void on_decoded(const std::vector<server_slot> & slots) {
        n_decode_total++;
        for (const auto & slot : slots) {
            if (slot.is_processing()) {
                n_busy_slots_total++;
            }
        }
    }

    void reset_bucket() {
        n_prompt_tokens_processed = 0;
        t_prompt_processing       = 0;
        n_tokens_predicted        = 0;
        t_tokens_generation       = 0;
    }
};

struct server_queue {
    int id = 0;
    bool running;

    // queues
    std::deque<server_task> queue_tasks;
    std::deque<server_task> queue_tasks_deferred;

    std::mutex mutex_tasks;
    std::condition_variable condition_tasks;

    // callback functions
    std::function<void(server_task &&)> callback_new_task;
    std::function<void(void)>           callback_update_slots;

    // Add a new task to the end of the queue
    int post(server_task && task, bool front = false) {
        std::unique_lock<std::mutex> lock(mutex_tasks);
        GGML_ASSERT(task.id != -1);
        // if this is cancel task make sure to clean up pending tasks
        if (task.type == SERVER_TASK_TYPE_CANCEL) {
            cleanup_pending_task(task.id_target);
        }
        const int task_id = task.id;
        QUE_DBG("new task, id = %d, front = %d\n", task_id, front);
        if (front) {
            queue_tasks.push_front(std::move(task));
        } else {
            queue_tasks.push_back(std::move(task));
        }
        condition_tasks.notify_one();
        return task_id;
    }

    // multi-task version of post()
    int post(std::vector<server_task> && tasks, bool front = false) {
        std::unique_lock<std::mutex> lock(mutex_tasks);
        for (auto & task : tasks) {
            if (task.id == -1) {
                task.id = id++;
            }
            // if this is cancel task make sure to clean up pending tasks
            if (task.type == SERVER_TASK_TYPE_CANCEL) {
                cleanup_pending_task(task.id_target);
            }
            QUE_DBG("new task, id = %d/%d, front = %d\n", task.id, (int) tasks.size(), front);
            if (front) {
                queue_tasks.push_front(std::move(task));
            } else {
                queue_tasks.push_back(std::move(task));
            }
        }
        condition_tasks.notify_one();
        return 0;
    }

    // Add a new task, but defer until one slot is available
    void defer(server_task && task) {
        std::unique_lock<std::mutex> lock(mutex_tasks);
        QUE_DBG("defer task, id = %d\n", task.id);
        queue_tasks_deferred.push_back(std::move(task));
        condition_tasks.notify_one();
    }

    // Get the next id for creating a new task
    int get_new_id() {
        std::unique_lock<std::mutex> lock(mutex_tasks);
        int new_id = id++;
        return new_id;
    }

    // Register function to process a new task
    void on_new_task(std::function<void(server_task &&)> callback) {
        callback_new_task = std::move(callback);
    }

    // Register the function to be called when all slots data is ready to be processed
    void on_update_slots(std::function<void(void)> callback) {
        callback_update_slots = std::move(callback);
    }

    // Call when the state of one slot is changed, it will move one task from deferred to main queue
    void pop_deferred_task() {
        std::unique_lock<std::mutex> lock(mutex_tasks);
        if (!queue_tasks_deferred.empty()) {
            queue_tasks.emplace_back(std::move(queue_tasks_deferred.front()));
            queue_tasks_deferred.pop_front();
        }
        condition_tasks.notify_one();
    }

    // end the start_loop routine
    void terminate() {
        std::unique_lock<std::mutex> lock(mutex_tasks);
        running = false;
        condition_tasks.notify_all();
    }

    /**
     * Main loop consists of these steps:
     * - Wait until a new task arrives
     * - Process the task (i.e. maybe copy data into slot)
     * - Check if multitask is finished
     * - Update all slots
     */
    void start_loop() {
        running = true;

        while (true) {
            QUE_DBG("%s", "processing new tasks\n");

            while (true) {
                std::unique_lock<std::mutex> lock(mutex_tasks);
                if (!running) {
                    QUE_DBG("%s", "terminate\n");
                    return;
                }
                if (queue_tasks.empty()) {
                    lock.unlock();
                    break;
                }
                server_task task = std::move(queue_tasks.front());
                queue_tasks.pop_front();
                lock.unlock();

                QUE_DBG("processing task, id = %d\n", task.id);
                callback_new_task(std::move(task));
            }

            // all tasks in the current loop is processed, slots data is now ready
            QUE_DBG("%s", "update slots\n");

            callback_update_slots();

            QUE_DBG("%s", "waiting for new tasks\n");
            {
                std::unique_lock<std::mutex> lock(mutex_tasks);
                if (!running) {
                    QUE_DBG("%s", "terminate\n");
                    return;
                }
                if (queue_tasks.empty()) {
                    condition_tasks.wait(lock, [&]{
                        return (!queue_tasks.empty() || !running);
                    });
                }
            }
        }
    }

private:
    void cleanup_pending_task(int id_target) {
        // no need lock because this is called exclusively by post()
        auto rm_func = [id_target](const server_task & task) {
            return task.id_target == id_target;
        };
        queue_tasks.erase(
            std::remove_if(queue_tasks.begin(),          queue_tasks.end(),          rm_func),
            queue_tasks.end());
        queue_tasks_deferred.erase(
            std::remove_if(queue_tasks_deferred.begin(), queue_tasks_deferred.end(), rm_func),
            queue_tasks_deferred.end());
    }
};

struct server_response {
    bool running = true;

    // for keeping track of all tasks waiting for the result
    std::unordered_set<int> waiting_task_ids;

    // the main result queue (using ptr for polymorphism)
    std::vector<server_task_result_ptr> queue_results;

    std::mutex mutex_results;
    std::condition_variable condition_results;

    // add the id_task to the list of tasks waiting for response
    void add_waiting_task_id(int id_task) {
        SRV_DBG("add task %d to waiting list. current waiting = %d (before add)\n", id_task, (int) waiting_task_ids.size());

        std::unique_lock<std::mutex> lock(mutex_results);
        waiting_task_ids.insert(id_task);
    }

    void add_waiting_tasks(const std::vector<server_task> & tasks) {
        std::unique_lock<std::mutex> lock(mutex_results);

        for (const auto & task : tasks) {
            SRV_DBG("add task %d to waiting list. current waiting = %d (before add)\n", task.id, (int) waiting_task_ids.size());
            waiting_task_ids.insert(task.id);
        }
    }

    // when the request is finished, we can remove task associated with it
    void remove_waiting_task_id(int id_task) {
        SRV_DBG("remove task %d from waiting list. current waiting = %d (before remove)\n", id_task, (int) waiting_task_ids.size());

        std::unique_lock<std::mutex> lock(mutex_results);
        waiting_task_ids.erase(id_task);
        // make sure to clean up all pending results
        queue_results.erase(
            std::remove_if(queue_results.begin(), queue_results.end(), [id_task](const server_task_result_ptr & res) {
                return res->id == id_task;
            }),
            queue_results.end());
    }

    void remove_waiting_task_ids(const std::unordered_set<int> & id_tasks) {
        std::unique_lock<std::mutex> lock(mutex_results);

        for (const auto & id_task : id_tasks) {
            SRV_DBG("remove task %d from waiting list. current waiting = %d (before remove)\n", id_task, (int) waiting_task_ids.size());
            waiting_task_ids.erase(id_task);
        }
    }

    // This function blocks the thread until there is a response for one of the id_tasks
    server_task_result_ptr recv(const std::unordered_set<int> & id_tasks) {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_results);
            condition_results.wait(lock, [&]{
                if (!running) {
                    SRV_DBG("%s : queue result stop\n", __func__);
                    std::terminate(); // we cannot return here since the caller is HTTP code
                }
                return !queue_results.empty();
            });

            for (size_t i = 0; i < queue_results.size(); i++) {
                if (id_tasks.find(queue_results[i]->id) != id_tasks.end()) {
                    server_task_result_ptr res = std::move(queue_results[i]);
                    queue_results.erase(queue_results.begin() + i);
                    return res;
                }
            }
        }

        // should never reach here
    }

    // same as recv(), but have timeout in seconds
    // if timeout is reached, nullptr is returned
    server_task_result_ptr recv_with_timeout(const std::unordered_set<int> & id_tasks, int timeout) {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_results);

            for (int i = 0; i < (int) queue_results.size(); i++) {
                if (id_tasks.find(queue_results[i]->id) != id_tasks.end()) {
                    server_task_result_ptr res = std::move(queue_results[i]);
                    queue_results.erase(queue_results.begin() + i);
                    return res;
                }
            }

            std::cv_status cr_res = condition_results.wait_for(lock, std::chrono::seconds(timeout));
            if (!running) {
                SRV_DBG("%s : queue result stop\n", __func__);
                std::terminate(); // we cannot return here since the caller is HTTP code
            }
            if (cr_res == std::cv_status::timeout) {
                return nullptr;
            }
        }

        // should never reach here
    }

    // single-task version of recv()
    server_task_result_ptr recv(int id_task) {
        std::unordered_set<int> id_tasks = {id_task};
        return recv(id_tasks);
    }

    // Send a new result to a waiting id_task
    void send(server_task_result_ptr && result) {
        SRV_DBG("sending result for task id = %d\n", result->id);

        std::unique_lock<std::mutex> lock(mutex_results);
        for (const auto & id_task : waiting_task_ids) {
            if (result->id == id_task) {
                SRV_DBG("task id = %d pushed to result queue\n", result->id);

                queue_results.emplace_back(std::move(result));
                condition_results.notify_all();
                return;
            }
        }
    }

    // terminate the waiting loop
    void terminate() {
        running = false;
        condition_results.notify_all();
    }
};

struct server_context {
    common_params params_base;

    // note: keep these alive - they determine the lifetime of the model, context, etc.
    common_init_result llama_init;
    common_init_result llama_init_dft;

    llama_model * model = nullptr;
    llama_context * ctx = nullptr;

    // multimodal
    mtmd_context * mctx = nullptr;

    const llama_vocab * vocab = nullptr;

    llama_model * model_dft = nullptr;

    llama_context_params cparams_dft;

    llama_batch batch {};

    bool clean_kv_cache = true;
    bool add_bos_token  = true;
    bool has_eos_token  = false;

    int32_t n_ctx; // total context for all clients / slots

    // slots / clients
    std::vector<server_slot> slots;
    json default_generation_settings_for_props;

    server_queue    queue_tasks;
    server_response queue_results;

    server_metrics metrics;

    // Necessary similarity of prompt for slot selection
    float slot_prompt_similarity = 0.0f;

    common_chat_templates_ptr chat_templates;

    ~server_context() {
        mtmd_free(mctx);

        // Clear any sampling context
        for (server_slot & slot : slots) {
            common_sampler_free(slot.smpl);
            slot.smpl = nullptr;

            llama_free(slot.ctx_dft);
            slot.ctx_dft = nullptr;

            common_speculative_free(slot.spec);
            slot.spec = nullptr;

            llama_batch_free(slot.batch_spec);
        }

        llama_batch_free(batch);
    }

    bool load_model(const common_params & params) {
        SRV_INF("loading model '%s'\n", params.model.path.c_str());

        params_base = params;

        llama_init = common_init_from_params(params_base);

        model = llama_init.model.get();
        ctx   = llama_init.context.get();

        if (model == nullptr) {
            SRV_ERR("failed to load model, '%s'\n", params_base.model.path.c_str());
            return false;
        }

        vocab = llama_model_get_vocab(model);

        n_ctx = llama_n_ctx(ctx);

        add_bos_token = llama_vocab_get_add_bos(vocab);
        has_eos_token = llama_vocab_eos(vocab) != LLAMA_TOKEN_NULL;

        if (!params_base.speculative.model.path.empty() || !params_base.speculative.model.hf_repo.empty()) {
            SRV_INF("loading draft model '%s'\n", params_base.speculative.model.path.c_str());

            auto params_dft = params_base;

            params_dft.devices      = params_base.speculative.devices;
            params_dft.model        = params_base.speculative.model;
            params_dft.n_ctx        = params_base.speculative.n_ctx == 0 ? params_base.n_ctx / params_base.n_parallel : params_base.speculative.n_ctx;
            params_dft.n_gpu_layers = params_base.speculative.n_gpu_layers;
            params_dft.n_parallel   = 1;

            // force F16 KV cache for the draft model for extra performance
            params_dft.cache_type_k = GGML_TYPE_F16;
            params_dft.cache_type_v = GGML_TYPE_F16;

            llama_init_dft = common_init_from_params(params_dft);

            model_dft = llama_init_dft.model.get();

            if (model_dft == nullptr) {
                SRV_ERR("failed to load draft model, '%s'\n", params_base.speculative.model.path.c_str());
                return false;
            }

            if (!common_speculative_are_compatible(ctx, llama_init_dft.context.get())) {
                SRV_ERR("the draft model '%s' is not compatible with the target model '%s'\n", params_base.speculative.model.path.c_str(), params_base.model.path.c_str());

                return false;
            }

            const int n_ctx_dft = llama_n_ctx(llama_init_dft.context.get());

            cparams_dft = common_context_params_to_llama(params_dft);
            cparams_dft.n_batch = n_ctx_dft;

            // the context is not needed - we will create one for each slot
            llama_init_dft.context.reset();
        }

        chat_templates = common_chat_templates_init(model, params_base.chat_template);
        try {
            common_chat_format_example(chat_templates.get(), params.use_jinja);
        } catch (const std::exception & e) {
            SRV_WRN("%s: Chat template parsing error: %s\n", __func__, e.what());
            SRV_WRN("%s: The chat template that comes with this model is not yet supported, falling back to chatml. This may cause the model to output suboptimal responses\n", __func__);
            chat_templates = common_chat_templates_init(model, "chatml");
        }

        std::string & mmproj_path = params_base.mmproj.path;
        if (!mmproj_path.empty()) {
            mtmd_context_params mparams = mtmd_context_params_default();
            mparams.use_gpu       = params_base.mmproj_use_gpu;
            mparams.print_timings = false;
            mparams.n_threads     = params_base.cpuparams.n_threads;
            mparams.verbosity     = params_base.verbosity > 0 ? GGML_LOG_LEVEL_DEBUG : GGML_LOG_LEVEL_INFO;
            mctx = mtmd_init_from_file(mmproj_path.c_str(), model, mparams);
            if (mctx == nullptr) {
                SRV_ERR("failed to load multimodal model, '%s'\n", mmproj_path.c_str());
                return false;
            }
            SRV_INF("loaded multimodal model, '%s'\n", mmproj_path.c_str());

            if (params_base.ctx_shift) {
                params_base.ctx_shift = false;
                SRV_WRN("%s\n", "ctx_shift is not supported by multimodal, it will be disabled");
            }

            if (params_base.n_cache_reuse) {
                params_base.n_cache_reuse = 0;
                SRV_WRN("%s\n", "cache_reuse is not supported by multimodal, it will be disabled");
            }

            if (!params_base.speculative.model.path.empty()) {
                SRV_ERR("%s\n", "err: speculative decode is not supported by multimodal");
                return false;
            }
        }

        return true;
    }

    void init() {
        const int32_t n_ctx_slot = n_ctx / params_base.n_parallel;

        SRV_INF("initializing slots, n_slots = %d\n", params_base.n_parallel);

        for (int i = 0; i < params_base.n_parallel; i++) {
            server_slot slot;

            slot.id = i;
            slot.ctx = ctx;
            slot.n_ctx = n_ctx_slot;
            slot.n_predict = params_base.n_predict;
            slot.mctx = mctx;
            slot.cache_tokens.has_mtmd = mctx != nullptr;

            if (model_dft) {
                slot.batch_spec = llama_batch_init(params_base.speculative.n_max + 1, 0, 1);

                slot.ctx_dft = llama_init_from_model(model_dft, cparams_dft);
                if (slot.ctx_dft == nullptr) {
                    SRV_ERR("%s", "failed to create draft context\n");
                    return;
                }

                slot.spec = common_speculative_init(slot.ctx_dft);
                if (slot.spec == nullptr) {
                    SRV_ERR("%s", "failed to create speculator\n");
                    return;
                }
            }

            SLT_INF(slot, "new slot n_ctx_slot = %d\n", slot.n_ctx);

            slot.params.sampling = params_base.sampling;

            slot.callback_on_release = [this](int) {
                queue_tasks.pop_deferred_task();
            };

            slot.reset();

            slots.push_back(std::move(slot));
        }

        default_generation_settings_for_props = slots[0].to_json();

        // the update_slots() logic will always submit a maximum of n_batch or n_parallel tokens
        // note that n_batch can be > n_ctx (e.g. for non-causal attention models such as BERT where the KV cache is not used)
        {
            const int32_t n_batch = llama_n_batch(ctx);
            batch = llama_batch_init(std::max(n_batch, params_base.n_parallel), 0, 1);
        }

        metrics.init();
    }

    server_slot * get_slot_by_id(int id) {
        for (server_slot & slot : slots) {
            if (slot.id == id) {
                return &slot;
            }
        }

        return nullptr;
    }

    server_slot * get_available_slot(const server_task & task) {
        server_slot * ret = nullptr;

        // find the slot that has at least n% prompt similarity
        if (ret == nullptr && slot_prompt_similarity != 0.0f) {
            int lcs_len = 0;
            float similarity = 0;

            for (server_slot & slot : slots) {
                // skip the slot if it is not available
                if (slot.is_processing()) {
                    continue;
                }

                // skip the slot if it does not contains cached tokens
                if (slot.cache_tokens.empty()) {
                    continue;
                }

                // length of the Longest Common Subsequence between the current slot's prompt and the input prompt
                int cur_lcs_len = slot.cache_tokens.get_common_prefix(task.prompt_tokens);

                // fraction of the common subsequence length compared to the current slot's prompt length
                float cur_similarity = static_cast<float>(cur_lcs_len) / static_cast<int>(slot.cache_tokens.size());

                // select the current slot if the criteria match
                if (cur_lcs_len > lcs_len && cur_similarity > slot_prompt_similarity) {
                    lcs_len = cur_lcs_len;
                    similarity = cur_similarity;
                    ret = &slot;
                }
            }

            if (ret != nullptr) {
                SLT_DBG(*ret, "selected slot by lcs similarity, lcs_len = %d, similarity = %f\n", lcs_len, similarity);
            }
        }

        // find the slot that has been least recently used
        if (ret == nullptr) {
            int64_t t_last = ggml_time_us();
            for (server_slot & slot : slots) {
                // skip the slot if it is not available
                if (slot.is_processing()) {
                    continue;
                }

                // select the current slot if the criteria match
                if (slot.t_last_used < t_last) {
                    t_last = slot.t_last_used;
                    ret = &slot;
                }
            }

            if (ret != nullptr) {
                SLT_DBG(*ret, "selected slot by lru, t_last = %" PRId64 "\n", t_last);
            }
        }

        return ret;
    }

    bool launch_slot_with_task(server_slot & slot, server_task && task) {
        slot.reset();
        slot.id_task       = task.id;
        slot.index         = task.index;
        slot.task_type     = task.type;
        slot.params        = std::move(task.params);
        slot.prompt_tokens = std::move(task.prompt_tokens);

        if (!are_lora_equal(slot.params.lora, slot.lora)) {
            // if lora is changed, we cannot reuse cached tokens
            slot.cache_tokens.clear();
            slot.lora = slot.params.lora;
        }

        if (!slot.prompt_tokens.validate(ctx)) {
            send_error(task, "Prompt contains invalid tokens", ERROR_TYPE_INVALID_REQUEST);
            return false;
        }
        SLT_DBG(slot, "launching slot : %s\n", safe_json_to_str(slot.to_json()).c_str());

        if (slot.n_predict > 0 && slot.params.n_predict > slot.n_predict) {
            // Might be better to reject the request with a 400 ?
            SLT_WRN(slot, "n_predict = %d exceeds server configuration, setting to %d\n", slot.params.n_predict, slot.n_predict);
            slot.params.n_predict = slot.n_predict;
        }

        if (slot.params.ignore_eos && has_eos_token) {
            slot.params.sampling.logit_bias.push_back({llama_vocab_eos(vocab), -INFINITY});
        }

        {
            if (slot.smpl != nullptr) {
                common_sampler_free(slot.smpl);
            }

            slot.smpl = common_sampler_init(model, slot.params.sampling);
            if (slot.smpl == nullptr) {
                // for now, the only error that may happen here is invalid grammar
                send_error(task, "Failed to parse grammar", ERROR_TYPE_INVALID_REQUEST);
                return false;
            }
        }

        if (slot.ctx_dft) {
            llama_batch_free(slot.batch_spec);

            slot.batch_spec = llama_batch_init(slot.params.speculative.n_max + 1, 0, 1);
        }

        slot.state = SLOT_STATE_STARTED;

        SLT_INF(slot, "%s", "processing task\n");

        return true;
    }

    void kv_cache_clear() {
        SRV_DBG("%s", "clearing KV cache\n");

        // clear the entire KV cache
        llama_kv_self_clear(ctx);
        clean_kv_cache = false;
    }

    bool process_token(completion_token_output & result, server_slot & slot) {
        // remember which tokens were sampled - used for repetition penalties during sampling
        const std::string token_str = result.text_to_send;
        slot.sampled = result.tok;

        slot.generated_text += token_str;
        if (slot.params.return_tokens) {
            slot.generated_tokens.push_back(result.tok);
        }
        slot.has_next_token = true;

        // check if there is incomplete UTF-8 character at the end
        bool incomplete = validate_utf8(slot.generated_text) < slot.generated_text.size();

        // search stop word and delete it
        if (!incomplete) {
            size_t pos = std::min(slot.n_sent_text, slot.generated_text.size());

            const std::string str_test = slot.generated_text.substr(pos);
            bool send_text = true;

            size_t stop_pos = slot.find_stopping_strings(str_test, token_str.size(), true);
            if (stop_pos != std::string::npos) {
                slot.generated_text.erase(
                    slot.generated_text.begin() + pos + stop_pos,
                    slot.generated_text.end());
                pos = std::min(slot.n_sent_text, slot.generated_text.size());
            } else if (slot.has_next_token) {
                stop_pos = slot.find_stopping_strings(str_test, token_str.size(), false);
                send_text = stop_pos == std::string::npos;
            }

            // check if there is any token to predict
            if (send_text) {
                // no send the stop word in the response
                result.text_to_send = slot.generated_text.substr(pos, std::string::npos);
                slot.n_sent_text += result.text_to_send.size();
                // add the token to slot queue and cache
            } else {
                result.text_to_send = "";
            }

            slot.add_token(result);
            if (slot.params.stream) {
                send_partial_response(slot, result);
            }
        }

        if (incomplete) {
            slot.has_next_token = true;
        }

        // check the limits
        if (slot.n_decoded > 0 && slot.has_next_token && !slot.has_budget(params_base)) {
            slot.stop           = STOP_TYPE_LIMIT;
            slot.has_next_token = false;

            SLT_DBG(slot, "stopped by limit, n_decoded = %d, n_predict = %d\n", slot.n_decoded, slot.params.n_predict);
        }

        if (slot.has_new_line) {
            // require that each new line has a whitespace prefix (i.e. indentation) of at least slot.params.n_indent
            if (slot.params.n_indent > 0) {
                // check the current indentation
                // TODO: improve by not doing it more than once for each new line
                if (slot.last_nl_pos > 0) {
                    size_t pos = slot.last_nl_pos;

                    int n_indent = 0;
                    while (pos < slot.generated_text.size() && (slot.generated_text[pos] == ' ' || slot.generated_text[pos] == '\t')) {
                        n_indent++;
                        pos++;
                    }

                    if (pos < slot.generated_text.size() && n_indent < slot.params.n_indent) {
                        slot.stop           = STOP_TYPE_LIMIT;
                        slot.has_next_token = false;

                        // cut the last line
                        slot.generated_text.erase(pos, std::string::npos);

                        SLT_DBG(slot, "stopped by indentation limit, n_decoded = %d, n_indent = %d\n", slot.n_decoded, n_indent);
                    }
                }

                // find the next new line
                {
                    const size_t pos = slot.generated_text.find('\n', slot.last_nl_pos);

                    if (pos != std::string::npos) {
                        slot.last_nl_pos = pos + 1;
                    }
                }
            }
        }

        // check if there is a new line in the generated text
        if (result.text_to_send.find('\n') != std::string::npos) {
            slot.has_new_line = true;

            // if we have seen a new line, we stop after a certain time limit, but only upon another new line
            if (slot.params.t_max_predict_ms > 0 && (ggml_time_us() - slot.t_start_generation > 1000.0f*slot.params.t_max_predict_ms)) {
                slot.stop           = STOP_TYPE_LIMIT;
                slot.has_next_token = false;

                SLT_DBG(slot, "stopped by time limit, n_decoded = %d, t_max_predict_ms = %d ms\n", slot.n_decoded, (int) slot.params.t_max_predict_ms);
            }
        }

        // if context shift is disabled, we stop when it reaches the context limit
        if (slot.n_past >= slot.n_ctx) {
            slot.truncated      = true;
            slot.stop           = STOP_TYPE_LIMIT;
            slot.has_next_token = false;

            SLT_DBG(slot, "stopped due to running out of context capacity, n_past = %d, n_prompt_tokens = %d, n_decoded = %d, n_ctx = %d\n",
                    slot.n_decoded, slot.n_prompt_tokens, slot.n_past, slot.n_ctx);
        }

        if (llama_vocab_is_eog(vocab, result.tok)) {
            slot.stop           = STOP_TYPE_EOS;
            slot.has_next_token = false;

            SLT_DBG(slot, "%s", "stopped by EOS\n");
        }

        const auto n_ctx_train = llama_model_n_ctx_train(model);

        if (slot.params.n_predict < 1 && slot.n_predict < 1 && slot.n_prompt_tokens + slot.n_decoded >= n_ctx_train) {
            slot.truncated      = true;
            slot.stop           = STOP_TYPE_LIMIT;
            slot.has_next_token = false; // stop prediction

            SLT_WRN(slot,
                    "n_predict (%d) is set for infinite generation. "
                    "Limiting generated tokens to n_ctx_train (%d) to avoid EOS-less generation infinite loop\n",
                    slot.params.n_predict, n_ctx_train);
        }

        SLT_DBG(slot, "n_decoded = %d, n_remaining = %d, next token: %5d '%s'\n", slot.n_decoded, slot.n_remaining, result.tok, token_str.c_str());

        return slot.has_next_token; // continue
    }

    void populate_token_probs(const server_slot & slot, completion_token_output & result, bool post_sampling, bool special, int idx) {
        size_t n_probs = slot.params.sampling.n_probs;
        size_t n_vocab = llama_vocab_n_tokens(vocab);
        if (post_sampling) {
            const auto * cur_p = common_sampler_get_candidates(slot.smpl);
            const size_t max_probs = cur_p->size;

            // set probability for sampled token
            for (size_t i = 0; i < max_probs; i++) {
                if (cur_p->data[i].id == result.tok) {
                    result.prob = cur_p->data[i].p;
                    break;
                }
            }

            // set probability for top n_probs tokens
            result.probs.reserve(max_probs);
            for (size_t i = 0; i < std::min(max_probs, n_probs); i++) {
                result.probs.push_back({
                    cur_p->data[i].id,
                    common_token_to_piece(ctx, cur_p->data[i].id, special),
                    cur_p->data[i].p
                });
            }
        } else {
            // TODO: optimize this with min-p optimization
            std::vector<llama_token_data> cur = get_token_probabilities(ctx, idx);

            // set probability for sampled token
            for (size_t i = 0; i < n_vocab; i++) {
                // set probability for sampled token
                if (cur[i].id == result.tok) {
                    result.prob = cur[i].p;
                    break;
                }
            }

            // set probability for top n_probs tokens
            result.probs.reserve(n_probs);
            for (size_t i = 0; i < std::min(n_vocab, n_probs); i++) {
                result.probs.push_back({
                    cur[i].id,
                    common_token_to_piece(ctx, cur[i].id, special),
                    cur[i].p
                });
            }
        }
    }

    void send_error(const server_task & task, const std::string & error, const enum error_type type = ERROR_TYPE_SERVER) {
        send_error(task.id, error, type);
    }

    void send_error(const server_slot & slot, const std::string & error, const enum error_type type = ERROR_TYPE_SERVER) {
        send_error(slot.id_task, error, type);
    }

    void send_error(const int id_task, const std::string & error, const enum error_type type = ERROR_TYPE_SERVER) {
        SRV_ERR("task id = %d, error: %s\n", id_task, error.c_str());

        auto res = std::make_unique<server_task_result_error>();
        res->id       = id_task;
        res->err_type = type;
        res->err_msg  = error;

        queue_results.send(std::move(res));
    }

    // if multimodal is enabled, send an error and return false
    bool ensure_no_mtmd(const int id_task) {
        if (mctx) {
            send_error(id_task, "This feature is not supported by multimodal", ERROR_TYPE_NOT_SUPPORTED);
            return false;
        }
        return true;
    }

    void send_partial_response(server_slot & slot, const completion_token_output & tkn) {
        auto res = std::make_unique<server_task_result_cmpl_partial>();

        res->id      = slot.id_task;
        res->index   = slot.index;
        res->content = tkn.text_to_send;
        res->tokens  = { tkn.tok };

        res->n_decoded           = slot.n_decoded;
        res->n_prompt_tokens     = slot.n_prompt_tokens;
        res->post_sampling_probs = slot.params.post_sampling_probs;

        res->verbose           = slot.params.verbose;
        res->oaicompat         = slot.params.oaicompat;
        res->oaicompat_model   = slot.params.oaicompat_model;
        res->oaicompat_cmpl_id = slot.params.oaicompat_cmpl_id;

        // populate res.probs_output
        if (slot.params.sampling.n_probs > 0) {
            res->prob_output = tkn; // copy the token probs
        }

        // populate timings if this is final response or timings_per_token is enabled
        if (slot.stop != STOP_TYPE_NONE || slot.params.timings_per_token) {
            res->timings = slot.get_timings();
        }

        queue_results.send(std::move(res));
    }

    void send_final_response(server_slot & slot) {
        auto res = std::make_unique<server_task_result_cmpl_final>();
        res->id              = slot.id_task;
        res->id_slot         = slot.id;

        res->index           = slot.index;
        res->content         = std::move(slot.generated_text);
        res->tokens          = std::move(slot.generated_tokens);
        res->timings         = slot.get_timings();
        res->prompt          = slot.prompt_tokens.detokenize(ctx, true);
        res->response_fields = std::move(slot.params.response_fields);

        res->truncated           = slot.truncated;
        res->n_decoded           = slot.n_decoded;
        res->n_prompt_tokens     = slot.n_prompt_tokens;
        res->n_tokens_cached     = slot.n_past;
        res->has_new_line        = slot.has_new_line;
        res->stopping_word       = slot.stopping_word;
        res->stop                = slot.stop;
        res->post_sampling_probs = slot.params.post_sampling_probs;

        res->verbose               = slot.params.verbose;
        res->stream                = slot.params.stream;
        res->oaicompat             = slot.params.oaicompat;
        res->oaicompat_model       = slot.params.oaicompat_model;
        res->oaicompat_cmpl_id     = slot.params.oaicompat_cmpl_id;
        res->oaicompat_chat_format = slot.params.oaicompat_chat_format;
        // populate res.probs_output
        if (slot.params.sampling.n_probs > 0) {
            if (!slot.params.stream && slot.stop == STOP_TYPE_WORD) {
                const llama_tokens stop_word_toks = common_tokenize(ctx, slot.stopping_word, false);

                size_t safe_offset = std::min(slot.generated_token_probs.size(), stop_word_toks.size());
                res->probs_output = std::vector<completion_token_output>(
                        slot.generated_token_probs.begin(),
                        slot.generated_token_probs.end() - safe_offset);
            } else {
                res->probs_output = std::vector<completion_token_output>(
                        slot.generated_token_probs.begin(),
                        slot.generated_token_probs.end());
            }
        }

        res->generation_params = slot.params; // copy the parameters

        queue_results.send(std::move(res));
    }

    void send_embedding(const server_slot & slot, const llama_batch & batch) {
        auto res = std::make_unique<server_task_result_embd>();
        res->id        = slot.id_task;
        res->index     = slot.index;
        res->n_tokens  = slot.n_prompt_tokens;
        res->oaicompat = slot.params.oaicompat;

        const int n_embd = llama_model_n_embd(model);

        std::vector<float> embd_res(n_embd, 0.0f);

        for (int i = 0; i < batch.n_tokens; ++i) {
            if (!batch.logits[i] || batch.seq_id[i][0] != slot.id) {
                continue;
            }

            const float * embd = llama_get_embeddings_seq(ctx, batch.seq_id[i][0]);
            if (embd == NULL) {
                embd = llama_get_embeddings_ith(ctx, i);
            }

            if (embd == NULL) {
                SLT_ERR(slot, "failed to get embeddings, token = %d, seq_id = %d\n", batch.token[i], batch.seq_id[i][0]);

                res->embedding.push_back(std::vector<float>(n_embd, 0.0f));
                continue;
            }

            // normalize only when there is pooling
            // TODO: configurable
            if (llama_pooling_type(slot.ctx) != LLAMA_POOLING_TYPE_NONE) {
                common_embd_normalize(embd, embd_res.data(), n_embd, 2);
                res->embedding.push_back(embd_res);
            } else {
                res->embedding.push_back({ embd, embd + n_embd });
            }
        }

        SLT_DBG(slot, "%s", "sending embeddings\n");

        queue_results.send(std::move(res));
    }

    void send_rerank(const server_slot & slot, const llama_batch & batch) {
        auto res = std::make_unique<server_task_result_rerank>();
        res->id    = slot.id_task;
        res->index = slot.index;
        res->n_tokens = slot.n_prompt_tokens;

        for (int i = 0; i < batch.n_tokens; ++i) {
            if (!batch.logits[i] || batch.seq_id[i][0] != slot.id) {
                continue;
            }

            const float * embd = llama_get_embeddings_seq(ctx, batch.seq_id[i][0]);
            if (embd == NULL) {
                embd = llama_get_embeddings_ith(ctx, i);
            }

            if (embd == NULL) {
                SLT_ERR(slot, "failed to get embeddings, token = %d, seq_id = %d\n", batch.token[i], batch.seq_id[i][0]);

                res->score = -1e6;
                continue;
            }

            res->score = embd[0];
        }

        SLT_DBG(slot, "sending rerank result, res.score = %f\n", res->score);

        queue_results.send(std::move(res));
    }

    //
    // Functions to create new task(s) and receive result(s)
    //

    void cancel_tasks(const std::unordered_set<int> & id_tasks) {
        std::vector<server_task> cancel_tasks;
        cancel_tasks.reserve(id_tasks.size());
        for (const auto & id_task : id_tasks) {
            SRV_WRN("cancel task, id_task = %d\n", id_task);

            server_task task(SERVER_TASK_TYPE_CANCEL);
            task.id_target = id_task;
            queue_results.remove_waiting_task_id(id_task);
            cancel_tasks.push_back(std::move(task));
        }
        // push to beginning of the queue, so it has highest priority
        queue_tasks.post(std::move(cancel_tasks), true);
    }

    // receive the results from task(s)
    void receive_multi_results(
            const std::unordered_set<int> & id_tasks,
            const std::function<void(std::vector<server_task_result_ptr>&)> & result_handler,
            const std::function<void(json)> & error_handler,
            const std::function<bool()> & is_connection_closed) {
        std::vector<server_task_result_ptr> results(id_tasks.size());
        for (int i = 0; i < (int)id_tasks.size(); i++) {
            server_task_result_ptr result = queue_results.recv_with_timeout(id_tasks, HTTP_POLLING_SECONDS);

            if (is_connection_closed()) {
                cancel_tasks(id_tasks);
                return;
            }

            if (result == nullptr) {
                i--; // retry
                continue;
            }

            if (result->is_error()) {
                error_handler(result->to_json());
                cancel_tasks(id_tasks);
                return;
            }

            GGML_ASSERT(
                dynamic_cast<server_task_result_cmpl_final*>(result.get()) != nullptr
                || dynamic_cast<server_task_result_embd*>(result.get()) != nullptr
                || dynamic_cast<server_task_result_rerank*>(result.get()) != nullptr
            );
            const size_t idx = result->get_index();
            GGML_ASSERT(idx < results.size() && "index out of range");
            results[idx] = std::move(result);
        }
        result_handler(results);
    }

    // receive the results from task(s)
    void receive_streamed_results(
            const std::unordered_set<int> & id_tasks,
            const std::function<void(server_task_result_ptr&)> & result_handler,
            const std::function<void(json)> & error_handler,
            const std::function<bool()> & is_connection_closed) {
        for (int i = 0; i < (int)id_tasks.size(); i++) {
            server_task_result_ptr result = queue_results.recv_with_timeout(id_tasks, HTTP_POLLING_SECONDS);

            if (is_connection_closed()) {
                cancel_tasks(id_tasks);
                return;
            }

            if (result == nullptr) {
                i--; // retry
                continue;
            }

            if (result->is_error()) {
                error_handler(result->to_json());
                cancel_tasks(id_tasks);
                return;
            }

            GGML_ASSERT(
                dynamic_cast<server_task_result_cmpl_final*>(result.get()) != nullptr
                || dynamic_cast<server_task_result_embd*>(result.get()) != nullptr
                || dynamic_cast<server_task_result_rerank*>(result.get()) != nullptr
            );
            const size_t idx = result->get_index();
            result_handler(result);
        }
    }

    // receive the results from task(s), in stream mode
    void receive_cmpl_results_stream(
            const std::unordered_set<int> & id_tasks,
            const std::function<bool(server_task_result_ptr&)> & result_handler,
            const std::function<void(json)> & error_handler,
            const std::function<bool()> & is_connection_closed) {
        size_t n_finished = 0;
        while (true) {
            server_task_result_ptr result = queue_results.recv_with_timeout(id_tasks, HTTP_POLLING_SECONDS);

            if (is_connection_closed()) {
                cancel_tasks(id_tasks);
                return;
            }

            if (result == nullptr) {
                continue; // retry
            }

            if (result->is_error()) {
                error_handler(result->to_json());
                cancel_tasks(id_tasks);
                return;
            }

            GGML_ASSERT(
                dynamic_cast<server_task_result_cmpl_partial*>(result.get()) != nullptr
                || dynamic_cast<server_task_result_cmpl_final*>(result.get()) != nullptr
            );
            if (!result_handler(result)) {
                cancel_tasks(id_tasks);
                break;
            }

            if (result->is_stop()) {
                if (++n_finished == id_tasks.size()) {
                    break;
                }
            }
        }
    }

    //
    // Functions to process the task
    //

    void process_single_task(server_task && task) {
        switch (task.type) {
            case SERVER_TASK_TYPE_COMPLETION:
            case SERVER_TASK_TYPE_INFILL:
            case SERVER_TASK_TYPE_EMBEDDING:
            case SERVER_TASK_TYPE_RERANK:
                {
                    const int id_slot = task.id_selected_slot;

                    server_slot * slot = id_slot != -1 ? get_slot_by_id(id_slot) : get_available_slot(task);

                    if (slot == nullptr) {
                        // if no slot is available, we defer this task for processing later
                        SRV_DBG("no slot is available, defer task, id_task = %d\n", task.id);
                        queue_tasks.defer(std::move(task));
                        break;
                    }
                    if (slot->is_processing()) {
                        // if requested slot is unavailable, we defer this task for processing later
                        SRV_DBG("requested slot is unavailable, defer task, id_task = %d\n", task.id);
                        queue_tasks.defer(std::move(task));
                        break;
                    }

                    if (!launch_slot_with_task(*slot, std::move(task))) {
                        SRV_ERR("failed to launch slot with task, id_task = %d\n", task.id);
                        break;
                    }
                } break;
            case SERVER_TASK_TYPE_CANCEL:
                {
                    // release slot linked with the task id
                    for (auto & slot : slots) {
                        if (slot.id_task == task.id_target) {
                            slot.release();
                            break;
                        }
                    }
                } break;
            case SERVER_TASK_TYPE_NEXT_RESPONSE:
                {
                    // do nothing
                } break;
            case SERVER_TASK_TYPE_METRICS:
                {
                    json slots_data = json::array();

                    int n_idle_slots       = 0;
                    int n_processing_slots = 0;

                    for (server_slot & slot : slots) {
                        json slot_data = slot.to_json();

                        if (slot.is_processing()) {
                            n_processing_slots++;
                        } else {
                            n_idle_slots++;
                        }

                        slots_data.push_back(slot_data);
                    }
                    SRV_DBG("n_idle_slots = %d, n_processing_slots = %d\n", n_idle_slots, n_processing_slots);

                    auto res = std::make_unique<server_task_result_metrics>();
                    res->id                  = task.id;
                    res->slots_data          = std::move(slots_data);
                    res->n_idle_slots        = n_idle_slots;
                    res->n_processing_slots  = n_processing_slots;
                    res->n_tasks_deferred    = queue_tasks.queue_tasks_deferred.size();
                    res->t_start             = metrics.t_start;

                    res->kv_cache_tokens_count = llama_kv_self_n_tokens(ctx);
                    res->kv_cache_used_cells   = llama_kv_self_used_cells(ctx);

                    res->n_prompt_tokens_processed_total = metrics.n_prompt_tokens_processed_total;
                    res->t_prompt_processing_total       = metrics.t_prompt_processing_total;
                    res->n_tokens_predicted_total        = metrics.n_tokens_predicted_total;
                    res->t_tokens_generation_total       = metrics.t_tokens_generation_total;

                    res->n_prompt_tokens_processed = metrics.n_prompt_tokens_processed;
                    res->t_prompt_processing       = metrics.t_prompt_processing;
                    res->n_tokens_predicted        = metrics.n_tokens_predicted;
                    res->t_tokens_generation       = metrics.t_tokens_generation;

                    res->n_decode_total          = metrics.n_decode_total;
                    res->n_busy_slots_total      = metrics.n_busy_slots_total;

                    if (task.metrics_reset_bucket) {
                        metrics.reset_bucket();
                    }
                    queue_results.send(std::move(res));
                } break;
            case SERVER_TASK_TYPE_SLOT_SAVE:
                {
                    if (!ensure_no_mtmd(task.id)) {
                        break;
                    }

                    int id_slot = task.slot_action.slot_id;
                    server_slot * slot = get_slot_by_id(id_slot);
                    if (slot == nullptr) {
                        send_error(task, "Invalid slot ID", ERROR_TYPE_INVALID_REQUEST);
                        break;
                    }
                    if (slot->is_processing()) {
                        // if requested slot is unavailable, we defer this task for processing later
                        SRV_DBG("requested slot is unavailable, defer task, id_task = %d\n", task.id);
                        queue_tasks.defer(std::move(task));
                        break;
                    }

                    const size_t token_count = slot->cache_tokens.size();
                    const int64_t t_start = ggml_time_us();

                    std::string filename = task.slot_action.filename;
                    std::string filepath = task.slot_action.filepath;

                    const llama_tokens & tokens = slot->cache_tokens.get_text_tokens();
                    const size_t nwrite = llama_state_seq_save_file(ctx, filepath.c_str(), slot->id, tokens.data(), token_count);

                    const int64_t t_end = ggml_time_us();
                    const double t_save_ms = (t_end - t_start) / 1000.0;

                    auto res = std::make_unique<server_task_result_slot_save_load>();
                    res->id       = task.id;
                    res->id_slot  = id_slot;
                    res->filename = filename;
                    res->is_save  = true;
                    res->n_tokens = token_count;
                    res->n_bytes  = nwrite;
                    res->t_ms     = t_save_ms;
                    queue_results.send(std::move(res));
                } break;
            case SERVER_TASK_TYPE_SLOT_RESTORE:
                {
                    if (!ensure_no_mtmd(task.id)) break;
                    int id_slot = task.slot_action.slot_id;
                    server_slot * slot = get_slot_by_id(id_slot);
                    if (slot == nullptr) {
                        send_error(task, "Invalid slot ID", ERROR_TYPE_INVALID_REQUEST);
                        break;
                    }
                    if (slot->is_processing()) {
                        // if requested slot is unavailable, we defer this task for processing later
                        SRV_DBG("requested slot is unavailable, defer task, id_task = %d\n", task.id);
                        queue_tasks.defer(std::move(task));
                        break;
                    }

                    const int64_t t_start = ggml_time_us();

                    std::string filename = task.slot_action.filename;
                    std::string filepath = task.slot_action.filepath;

                    llama_tokens tokens;
                    tokens.resize(slot->n_ctx);
                    size_t token_count = 0;
                    size_t nread = llama_state_seq_load_file(ctx, filepath.c_str(), slot->id, tokens.data(), tokens.size(), &token_count);
                    if (nread == 0) {
                        slot->cache_tokens.clear(); // KV may already been invalidated?
                        send_error(task, "Unable to restore slot, no available space in KV cache or invalid slot save file", ERROR_TYPE_INVALID_REQUEST);
                        break;
                    }
                    tokens.resize(token_count);
                    slot->cache_tokens.clear();
                    slot->cache_tokens.insert(tokens);

                    const int64_t t_end = ggml_time_us();
                    const double t_restore_ms = (t_end - t_start) / 1000.0;

                    auto res = std::make_unique<server_task_result_slot_save_load>();
                    res->id       = task.id;
                    res->id_slot  = id_slot;
                    res->filename = filename;
                    res->is_save  = false;
                    res->n_tokens = token_count;
                    res->n_bytes  = nread;
                    res->t_ms     = t_restore_ms;
                    queue_results.send(std::move(res));
                } break;
            case SERVER_TASK_TYPE_SLOT_ERASE:
                {
                    if (!ensure_no_mtmd(task.id)) break;
                    int id_slot = task.slot_action.slot_id;
                    server_slot * slot = get_slot_by_id(id_slot);
                    if (slot == nullptr) {
                        send_error(task, "Invalid slot ID", ERROR_TYPE_INVALID_REQUEST);
                        break;
                    }
                    if (slot->is_processing()) {
                        // if requested slot is unavailable, we defer this task for processing later
                        SRV_DBG("requested slot is unavailable, defer task, id_task = %d\n", task.id);
                        queue_tasks.defer(std::move(task));
                        break;
                    }

                    // Erase token cache
                    const size_t n_erased = slot->cache_tokens.size();
                    llama_kv_self_seq_rm(ctx, slot->id, -1, -1);
                    slot->cache_tokens.clear();

                    auto res = std::make_unique<server_task_result_slot_erase>();
                    res->id       = task.id;
                    res->id_slot  = id_slot;
                    res->n_erased = n_erased;
                    queue_results.send(std::move(res));
                } break;
            case SERVER_TASK_TYPE_SET_LORA:
                {
                    params_base.lora_adapters = std::move(task.set_lora);
                    auto res = std::make_unique<server_task_result_apply_lora>();
                    res->id = task.id;
                    queue_results.send(std::move(res));
                } break;

        }
    }

    void update_slots() {
        // check if all slots are idle
        {
            bool all_idle = true;

            for (auto & slot : slots) {
                if (slot.is_processing()) {
                    all_idle = false;
                    break;
                }
            }

            if (all_idle) {
                SRV_INF("%s", "all slots are idle\n");
                if (clean_kv_cache) {
                    kv_cache_clear();
                }

                return;
            }
        }

        {
            SRV_DBG("%s", "posting NEXT_RESPONSE\n");

            server_task task(SERVER_TASK_TYPE_NEXT_RESPONSE);
            task.id = queue_tasks.get_new_id();
            queue_tasks.post(std::move(task));
        }

        // apply context-shift if needed
        // TODO: simplify and improve
        for (server_slot & slot : slots) {
            if (slot.is_processing() && slot.n_past + 1 >= slot.n_ctx) {
                if (!params_base.ctx_shift) {
                    // this check is redundant (for good)
                    // we should never get here, because generation should already stopped in process_token()
                    slot.release();
                    send_error(slot, "context shift is disabled", ERROR_TYPE_SERVER);
                    continue;
                }

                if (mctx) {
                    // we should never reach this because params_base.ctx_shift is automatically disabled if mmproj is loaded
                    // we don't support ctx_shift because an image chunk may contains multiple tokens
                    GGML_ABORT("not supported by multimodal");
                }

                // Shift context
                const int n_keep    = slot.params.n_keep + add_bos_token;
                const int n_left    = slot.n_past - n_keep;
                const int n_discard = slot.params.n_discard ? slot.params.n_discard : (n_left / 2);

                SLT_WRN(slot, "slot context shift, n_keep = %d, n_left = %d, n_discard = %d\n", n_keep, n_left, n_discard);

                llama_kv_self_seq_rm (ctx, slot.id, n_keep            , n_keep + n_discard);
                llama_kv_self_seq_add(ctx, slot.id, n_keep + n_discard, slot.n_past,        -n_discard);

                // add generated tokens to cache
                {
                    llama_tokens new_tokens = slot.cache_tokens.get_text_tokens(); // copy
                    for (size_t i = n_keep + n_discard; i < new_tokens.size(); i++) {
                        new_tokens[i - n_discard] = new_tokens[i];
                    }

                    new_tokens.resize(slot.cache_tokens.size() - n_discard);
                    slot.cache_tokens.clear();
                    slot.cache_tokens.insert(new_tokens);
                }

                slot.n_past -= n_discard;

                slot.truncated = true;
            }
        }

        // start populating the batch for this iteration
        common_batch_clear(batch);

        // track if given slot can be batched with slots already in the batch
        server_slot * slot_batched = nullptr;

        auto accept_special_token = [&](server_slot & slot, llama_token token) {
            return params_base.special || slot.params.sampling.preserved_tokens.find(token) != slot.params.sampling.preserved_tokens.end();
        };

        // frist, add sampled tokens from any ongoing sequences
        for (auto & slot : slots) {
            if (slot.state != SLOT_STATE_GENERATING) {
                continue;
            }

            // check if we can batch this slot with the previous one
            if (!slot_batched) {
                slot_batched = &slot;
            } else if (!slot_batched->can_batch_with(slot)) {
                continue;
            }

            slot.i_batch = batch.n_tokens;

            common_batch_add(batch, slot.sampled, slot.n_past, { slot.id }, true);

            slot.n_past += 1;
            slot.cache_tokens.push_back(slot.sampled);

            SLT_DBG(slot, "slot decode token, n_ctx = %d, n_past = %d, n_cache_tokens = %d, truncated = %d\n",
                    slot.n_ctx, slot.n_past, (int) slot.cache_tokens.size(), slot.truncated);
        }

        // process in chunks of params.n_batch
        int32_t n_batch  = llama_n_batch(ctx);
        int32_t n_ubatch = llama_n_ubatch(ctx);

        // next, batch any pending prompts without exceeding n_batch
        if (params_base.cont_batching || batch.n_tokens == 0) {
            for (auto & slot : slots) {
                // check if we can batch this slot with the previous one
                if (slot.is_processing()) {
                    if (!slot_batched) {
                        slot_batched = &slot;
                    } else if (!slot_batched->can_batch_with(slot)) {
                        continue;
                    }
                }

                // this slot still has a prompt to be processed
                if (slot.state == SLOT_STATE_PROCESSING_PROMPT || slot.state == SLOT_STATE_STARTED) {
                    auto & prompt_tokens = slot.prompt_tokens;

                    // TODO: maybe move branch to outside of this loop in the future
                    if (slot.state == SLOT_STATE_STARTED) {
                        slot.t_start_process_prompt = ggml_time_us();
                        slot.t_start_generation = 0;

                        slot.n_past = 0;
                        slot.n_prompt_tokens = prompt_tokens.size();
                        slot.state = SLOT_STATE_PROCESSING_PROMPT;

                        SLT_INF(slot, "new prompt, n_ctx_slot = %d, n_keep = %d, n_prompt_tokens = %d\n", slot.n_ctx, slot.params.n_keep, slot.n_prompt_tokens);

                        // print prompt tokens (for debugging)
                        /*if (1) {
                            // first 16 tokens (avoid flooding logs)
                            for (int i = 0; i < std::min<int>(16, prompt_tokens.size()); i++) {
                                SLT_DBG(slot, "prompt token %3d: %6d '%s'\n", i, prompt_tokens[i], common_token_to_piece(ctx, prompt_tokens[i]).c_str());
                            }
                        } else {
                            // all
                            for (int i = 0; i < (int) prompt_tokens.size(); i++) {
                                SLT_DBG(slot, "prompt token %3d: %6d '%s'\n", i, prompt_tokens[i], common_token_to_piece(ctx, prompt_tokens[i]).c_str());
                            }
                        }*/

                        // empty prompt passed -> release the slot and send empty response
                        if (prompt_tokens.empty()) {
                            SLT_WRN(slot, "%s", "empty prompt - releasing slot\n");

                            slot.release();
                            slot.print_timings();
                            send_final_response(slot);
                            continue;
                        }

                        if (slot.is_non_causal()) {
                            if (slot.n_prompt_tokens > n_ubatch) {
                                slot.release();
                                std::vector<char> buff(500);
                                const char* format = "input is too large (%d tokens, normally limited to %d tokens) to process. increase the physical batch size";
                                buff.resize(snprintf(buff.data(),buff.size(),format,(int)slot.n_prompt_tokens,(int)n_ubatch)+1);
                                send_error(slot,std::string(buff.data()), ERROR_TYPE_SERVER);
                                continue;
                            }

                            if (slot.n_prompt_tokens > slot.n_ctx) {
                                slot.release();
                                send_error(slot, "input is larger than the max context size. skipping", ERROR_TYPE_SERVER);
                                continue;
                            }
                        } else {
                            if (!params_base.ctx_shift) {
                                // if context shift is disabled, we make sure prompt size is smaller than KV size
                                // TODO: there should be a separate parameter that control prompt truncation
                                //       context shift should be applied only during the generation phase
                                if (slot.n_prompt_tokens >= slot.n_ctx) {
                                    slot.release();
                                    send_error(slot, "the request exceeds the available context size. try increasing the context size or enable context shift", ERROR_TYPE_INVALID_REQUEST);
                                    continue;
                                }
                            }
                            if (slot.params.n_keep < 0) {
                                slot.params.n_keep = slot.n_prompt_tokens;
                            }
                            slot.params.n_keep = std::min(slot.n_ctx - 4, slot.params.n_keep);

                            // if input prompt is too big, truncate it
                            if (slot.n_prompt_tokens >= slot.n_ctx) {
                                if (mctx) {
                                    // we should never reach this
                                    GGML_ABORT("not supported by multimodal");
                                }
                                const int n_left = slot.n_ctx - slot.params.n_keep;

                                const int n_block_size = n_left / 2;
                                const int erased_blocks = (slot.n_prompt_tokens - slot.params.n_keep - n_block_size) / n_block_size;

                                const llama_tokens & curr_tokens = slot.prompt_tokens.get_text_tokens();
                                llama_tokens new_tokens(
                                        curr_tokens.begin(),
                                        curr_tokens.begin() + slot.params.n_keep);

                                new_tokens.insert(
                                        new_tokens.end(),
                                        curr_tokens.begin() + slot.params.n_keep + erased_blocks * n_block_size,
                                        curr_tokens.end());

                                prompt_tokens.clear();
                                prompt_tokens.insert(new_tokens);

                                slot.truncated = true;
                                slot.n_prompt_tokens = prompt_tokens.size();

                                SLT_WRN(slot, "input truncated, n_ctx = %d, n_keep = %d, n_left = %d, n_prompt_tokens = %d\n", slot.n_ctx, slot.params.n_keep, n_left, slot.n_prompt_tokens);

                                GGML_ASSERT(slot.n_prompt_tokens < slot.n_ctx);
                            }

                            if (slot.params.cache_prompt) {
                                // reuse any previously computed tokens that are common with the new prompt
                                slot.n_past = slot.cache_tokens.get_common_prefix(prompt_tokens);

                                // reuse chunks from the cached prompt by shifting their KV cache in the new position
                                if (params_base.n_cache_reuse > 0) {
                                    size_t head_c = slot.n_past; // cache
                                    size_t head_p = slot.n_past; // current prompt

                                    if (mctx) {
                                        // we should never reach this
                                        GGML_ABORT("not supported by multimodal");
                                    }

                                    SLT_DBG(slot, "trying to reuse chunks with size > %d, slot.n_past = %d\n", params_base.n_cache_reuse, slot.n_past);

                                    while (head_c < slot.cache_tokens.size() &&
                                           head_p < prompt_tokens.size()) {

                                        size_t n_match = 0;
                                        while (head_c + n_match < slot.cache_tokens.size() &&
                                               head_p + n_match < prompt_tokens.size()     &&
                                               slot.cache_tokens[head_c + n_match] == prompt_tokens[head_p + n_match]) {

                                            n_match++;
                                        }

                                        if (n_match >= (size_t) params_base.n_cache_reuse) {
                                            SLT_INF(slot, "reusing chunk with size %zu, shifting KV cache [%zu, %zu) -> [%zu, %zu)\n", n_match, head_c, head_c + n_match, head_p, head_p + n_match);
                                            //for (size_t i = head_p; i < head_p + n_match; i++) {
                                            //    SLT_DBG(slot, "cache token %3zu: %6d '%s'\n", i, prompt_tokens[i], common_token_to_piece(ctx, prompt_tokens[i]).c_str());
                                            //}

                                            const int64_t kv_shift = (int64_t) head_p - (int64_t) head_c;

                                            llama_kv_self_seq_rm (ctx, slot.id, head_p, head_c);
                                            llama_kv_self_seq_add(ctx, slot.id, head_c, head_c + n_match, kv_shift);

                                            for (size_t i = 0; i < n_match; i++) {
                                                slot.cache_tokens.set_token(head_p + i, slot.cache_tokens[head_c + i]);
                                                slot.n_past++;
                                            }

                                            head_c += n_match;
                                            head_p += n_match;
                                        } else {
                                            head_c += 1;
                                        }
                                    }

                                    SLT_DBG(slot, "after context reuse, new slot.n_past = %d\n", slot.n_past);
                                }
                            } else {
                                // if we don't cache the prompt, we have to remove the entire KV cache
                                llama_kv_self_seq_rm(ctx, slot.id, 0, -1);
                                slot.n_past = 0;
                                slot.cache_tokens.clear();
                            }
                        }

                        if (slot.n_past == slot.n_prompt_tokens && slot.n_past > 0) {
                            // we have to evaluate at least 1 token to generate logits.
                            SLT_WRN(slot, "need to evaluate at least 1 token to generate logits, n_past = %d, n_prompt_tokens = %d\n", slot.n_past, slot.n_prompt_tokens);

                            slot.n_past--;
                        }

                        slot.n_prompt_tokens_processed = 0;
                    }

                    // non-causal tasks require to fit the entire prompt in the physical batch
                    if (slot.is_non_causal()) {
                        // cannot fit the prompt in the current batch - will try next iter
                        if (batch.n_tokens + slot.n_prompt_tokens > n_batch) {
                            continue;
                        }
                    }

                    // keep only the common part
                    if (!llama_kv_self_seq_rm(ctx, slot.id, slot.n_past, -1)) {
                        // could not partially delete (likely using a non-Transformer model)
                        llama_kv_self_seq_rm(ctx, slot.id, -1, -1);

                        // there is no common part left
                        slot.n_past = 0;
                    }

                    SLT_INF(slot, "kv cache rm [%d, end)\n", slot.n_past);

                    // remove the non-common part from the cache
                    slot.cache_tokens.keep_first(slot.n_past);

                    // check if we should process the image
                    if (slot.n_past < slot.n_prompt_tokens
                            && slot.prompt_tokens[slot.n_past] == LLAMA_TOKEN_NULL) {
                        // process the image
                        int32_t new_n_past;
                        int32_t res = slot.prompt_tokens.process_chunk(ctx, mctx, slot.n_past, slot.id, new_n_past);
                        int32_t n_pos = new_n_past - slot.n_past;

                        if (res != 0) {
                            SLT_ERR(slot, "failed to process image, res = %d\n", res);
                            slot.release();
                            send_error(slot, "failed to process image", ERROR_TYPE_SERVER);
                            continue;
                        }

                        // add the image chunk to cache
                        {
                            const auto & chunk = slot.prompt_tokens.find_chunk(slot.n_past);
                            slot.cache_tokens.push_back(chunk.get()); // copy
                        }

                        slot.n_past                    += n_pos;
                        slot.n_prompt_tokens_processed += n_pos;
                    }

                    // add prompt tokens for processing in the current batch
                    while (slot.n_past < slot.n_prompt_tokens && batch.n_tokens < n_batch) {
                        // get next token to process
                        llama_token cur_tok = slot.prompt_tokens[slot.n_past];
                        if (cur_tok == LLAMA_TOKEN_NULL) {
                            break; // end of text chunk
                        }

                        // without pooling, we want to output the embeddings for all the tokens in the batch
                        const bool need_embd = slot.task_type == SERVER_TASK_TYPE_EMBEDDING && llama_pooling_type(slot.ctx) == LLAMA_POOLING_TYPE_NONE;

                        common_batch_add(batch, cur_tok, slot.n_past, { slot.id }, need_embd);
                        slot.cache_tokens.push_back(cur_tok);

                        slot.n_prompt_tokens_processed++;
                        slot.n_past++;
                    }

                    // SLT_INF(slot, "new cache_tokens: %s\n", slot.cache_tokens.str().c_str());

                    SLT_INF(slot, "prompt processing progress, n_past = %d, n_tokens = %d, progress = %f\n", slot.n_past, batch.n_tokens, (float) slot.n_prompt_tokens_processed / slot.n_prompt_tokens);

                    // entire prompt has been processed
                    if (slot.n_past == slot.n_prompt_tokens) {
                        slot.state = SLOT_STATE_DONE_PROMPT;

                        GGML_ASSERT(batch.n_tokens > 0);
                        GGML_ASSERT((size_t) slot.n_prompt_tokens == slot.prompt_tokens.size());

                        common_sampler_reset(slot.smpl);

                        // Process all prompt tokens through sampler system
                        for (int i = 0; i < slot.n_prompt_tokens; ++i) {
                            llama_token id = slot.prompt_tokens[i];
                            if (id != LLAMA_TOKEN_NULL) {
                                common_sampler_accept(slot.smpl, id, false);
                            }
                        }

                        // extract the logits only for the last token
                        batch.logits[batch.n_tokens - 1] = true;

                        slot.n_decoded = 0;
                        slot.i_batch   = batch.n_tokens - 1;

                        SLT_INF(slot, "prompt done, n_past = %d, n_tokens = %d\n", slot.n_past, batch.n_tokens);
                    }
                }

                if (batch.n_tokens >= n_batch) {
                    break;
                }
            }
        }

        if (batch.n_tokens == 0) {
            SRV_WRN("%s", "no tokens to decode\n");
            return;
        }

        SRV_DBG("decoding batch, n_tokens = %d\n", batch.n_tokens);

        if (slot_batched) {
            // make sure we're in the right embedding mode
            llama_set_embeddings(ctx, slot_batched->is_non_causal());
            // apply lora, only need to do it once per batch
            common_set_adapter_lora(ctx, slot_batched->lora);
        }

        // process the created batch of tokens
        for (int32_t i = 0; i < batch.n_tokens; i += n_batch) {
            const int32_t n_tokens = std::min(n_batch, batch.n_tokens - i);

            llama_batch batch_view = {
                n_tokens,
                batch.token    + i,
                nullptr,
                batch.pos      + i,
                batch.n_seq_id + i,
                batch.seq_id   + i,
                batch.logits   + i,
            };

            int ret = 0;

            if (params_base.embedding || params_base.reranking) {
                ret = llama_encode(ctx, batch_view);
            } else {
                ret = llama_decode(ctx, batch_view);
            }

            metrics.on_decoded(slots);

            if (ret != 0) {
                if (n_batch == 1 || ret < 0) {
                    // if you get here, it means the KV cache is full - try increasing it via the context size
                    SRV_ERR("failed to decode the batch: KV cache is full - try increasing it via the context size, i = %d, n_batch = %d, ret = %d\n", i, n_batch, ret);
                    for (auto & slot : slots) {
                        slot.release();
                        send_error(slot, "Input prompt is too big compared to KV size. Please try increasing KV size.");
                    }
                    break; // break loop of n_batch
                }

                // retry with half the batch size to try to find a free slot in the KV cache
                n_batch /= 2;
                i -= n_batch;

                SRV_WRN("failed to find free space in the KV cache, retrying with smaller batch size - try increasing it via the context size or enable defragmentation, i = %d, n_batch = %d, ret = %d\n", i, n_batch, ret);

                continue; // continue loop of n_batch
            }

            for (auto & slot : slots) {
                if (slot.i_batch < (int) i || slot.i_batch >= (int) (i + n_tokens)) {
                    continue; // continue loop of slots
                }

                if (slot.state == SLOT_STATE_DONE_PROMPT) {
                    if (slot.task_type == SERVER_TASK_TYPE_EMBEDDING) {
                        // prompt evaluated for embedding
                        send_embedding(slot, batch_view);
                        slot.release();
                        slot.i_batch = -1;
                        continue; // continue loop of slots
                    }

                    if (slot.task_type == SERVER_TASK_TYPE_RERANK) {
                        send_rerank(slot, batch_view);
                        slot.release();
                        slot.i_batch = -1;
                        continue; // continue loop of slots
                    }

                    // prompt evaluated for next-token prediction
                    slot.state = SLOT_STATE_GENERATING;
                } else if (slot.state != SLOT_STATE_GENERATING) {
                    continue; // continue loop of slots
                }

                const int tok_idx = slot.i_batch - i;

                llama_token id = common_sampler_sample(slot.smpl, ctx, tok_idx);

                slot.i_batch = -1;

                common_sampler_accept(slot.smpl, id, true);

                slot.n_decoded += 1;

                const int64_t t_current = ggml_time_us();

                if (slot.n_decoded == 1) {
                    slot.t_start_generation = t_current;
                    slot.t_prompt_processing = (slot.t_start_generation - slot.t_start_process_prompt) / 1e3;
                    metrics.on_prompt_eval(slot);
                }

                slot.t_token_generation = (t_current - slot.t_start_generation) / 1e3;

                completion_token_output result;
                result.tok          = id;
                result.text_to_send = common_token_to_piece(ctx, result.tok, accept_special_token(slot, result.tok));
                result.prob         = 1.0f; // TODO: set it here instead of doing inside populate_token_probs

                if (slot.params.sampling.n_probs > 0) {
                    populate_token_probs(slot, result, slot.params.post_sampling_probs, params_base.special, tok_idx);
                }

                if (!process_token(result, slot)) {
                    // release slot because of stop condition
                    slot.release();
                    slot.print_timings();
                    send_final_response(slot);
                    metrics.on_prediction(slot);
                    continue;
                }
            }

            // do speculative decoding
            for (auto & slot : slots) {
                if (!slot.is_processing() || !slot.can_speculate()) {
                    continue;
                }

                if (slot.state != SLOT_STATE_GENERATING) {
                    continue;
                }

                if (mctx) {
                    // we should never reach this, as speculative is automatically disabled if mmproj is loaded
                    GGML_ABORT("not supported by multimodal");
                }

                // determine the max draft that fits the current slot state
                int n_draft_max = slot.params.speculative.n_max;

                // note: n_past is not yet increased for the `id` token sampled above
                //       also, need to leave space for 1 extra token to allow context shifts
                n_draft_max = std::min(n_draft_max, slot.n_ctx - slot.n_past - 2);

                if (slot.n_remaining > 0) {
                    n_draft_max = std::min(n_draft_max, slot.n_remaining - 1);
                }

                SLT_DBG(slot, "max possible draft: %d\n", n_draft_max);

                if (n_draft_max < slot.params.speculative.n_min) {
                    SLT_DBG(slot, "the max possible draft is too small: %d < %d - skipping speculative decoding\n", n_draft_max, slot.params.speculative.n_min);

                    continue;
                }

                llama_token id = slot.sampled;

                struct common_speculative_params params_spec;
                params_spec.n_draft   = n_draft_max;
                params_spec.n_reuse   = llama_n_ctx(slot.ctx_dft) - slot.params.speculative.n_max;
                params_spec.p_min     = slot.params.speculative.p_min;

                const llama_tokens & cached_text_tokens = slot.cache_tokens.get_text_tokens();
                llama_tokens draft = common_speculative_gen_draft(slot.spec, params_spec, cached_text_tokens, id);

                // keep track of total number of tokens generated in the draft
                slot.n_draft_total += draft.size();

                // ignore small drafts
                if (slot.params.speculative.n_min > (int) draft.size()) {
                    SLT_DBG(slot, "ignoring small draft: %d < %d\n", (int) draft.size(), slot.params.speculative.n_min);

                    continue;
                }

                // construct the speculation batch
                common_batch_clear(slot.batch_spec);
                common_batch_add  (slot.batch_spec, id, slot.n_past, { slot.id }, true);

                for (size_t i = 0; i < draft.size(); ++i) {
                    common_batch_add(slot.batch_spec, draft[i], slot.n_past + 1 + i, { slot.id }, true);
                }

                SLT_DBG(slot, "decoding speculative batch, size = %d\n", slot.batch_spec.n_tokens);

                llama_decode(ctx, slot.batch_spec);

                // the accepted tokens from the speculation
                const auto ids = common_sampler_sample_and_accept_n(slot.smpl, ctx, draft);

                slot.n_past    += ids.size();
                slot.n_decoded += ids.size();

                // update how many tokens out of draft was accepted
                slot.n_draft_accepted += ids.size() - 1;

                slot.cache_tokens.push_back(id);
                slot.cache_tokens.insert({ids.begin(), ids.end() - 1});

                llama_kv_self_seq_rm(ctx, slot.id, slot.n_past, -1);

                for (size_t i = 0; i < ids.size(); ++i) {
                    completion_token_output result;

                    result.tok          = ids[i];
                    result.text_to_send = common_token_to_piece(ctx, result.tok, accept_special_token(slot, result.tok));
                    result.prob         = 1.0f; // set later

                    // TODO: set result.probs

                    if (!process_token(result, slot)) {
                        // release slot because of stop condition
                        slot.release();
                        slot.print_timings();
                        send_final_response(slot);
                        metrics.on_prediction(slot);
                        break;
                    }
                }

                SLT_DBG(slot, "accepted %d/%d draft tokens, new n_past = %d\n", (int) ids.size() - 1, (int) draft.size(), slot.n_past);
            }
        }

        SRV_DBG("%s", "run slots completed\n");
    }

    json model_meta() const {
        return json {
            {"vocab_type",  llama_vocab_type       (vocab)},
            {"n_vocab",     llama_vocab_n_tokens   (vocab)},
            {"n_ctx_train", llama_model_n_ctx_train(model)},
            {"n_embd",      llama_model_n_embd     (model)},
            {"n_params",    llama_model_n_params   (model)},
            {"size",        llama_model_size       (model)},
        };
    }
};

static void log_server_request(const httplib::Request & req, const httplib::Response & res) {
    // skip GH copilot requests when using default port
    if (req.path == "/v1/health" || req.path == "/v1/completions") {
        return;
    }

    // reminder: this function is not covered by httplib's exception handler; if someone does more complicated stuff, think about wrapping it in try-catch

    SRV_INF("request: %s %s %s %d\n", req.method.c_str(), req.path.c_str(), req.remote_addr.c_str(), res.status);

    SRV_DBG("request:  %s\n", req.body.c_str());
    SRV_DBG("response: %s\n", res.body.c_str());
}

std::function<void(int)> shutdown_handler;
std::atomic_flag is_terminating = ATOMIC_FLAG_INIT;

inline void signal_handler(int signal) {
    if (is_terminating.test_and_set()) {
        // in case it hangs, we can force terminate the server by hitting Ctrl+C twice
        // this is for better developer experience, we can remove when the server is stable enough
        fprintf(stderr, "Received second interrupt, terminating immediately.\n");
        exit(1);
    }

    shutdown_handler(signal);
}

int main(int argc, char ** argv) {
    // own arguments required by this example
    common_params params;

    if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_SERVER)) {
        return 1;
    }

    common_init();

    // struct that contains llama context and inference
    server_context ctx_server;

    llama_backend_init();
    llama_numa_init(params.numa);

    LOG_INF("server PID = %d, system info: n_threads = %d, n_threads_batch = %d, total_threads = %d\n", getpid(), params.cpuparams.n_threads, params.cpuparams_batch.n_threads, std::thread::hardware_concurrency());
    LOG_INF("\n");
    LOG_INF("%s\n", common_params_get_system_info(params).c_str());
    LOG_INF("\n");

    std::unique_ptr<httplib::Server> svr;
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (params.ssl_file_key != "" && params.ssl_file_cert != "") {
        if(params.ssl_self_cert_common != "")
        {
            if(!self_signed::createKeyAndSelfSignedCertificate(params.ssl_file_key,"",params.ssl_file_cert,params.ssl_self_cert_common)) {
                    LOG_INF("Self-certification failed\n");
                } else {
                    LOG_INF("Self-certification successful\n");
                }

        }
        LOG_INF("Running with SSL: key = %s, cert = %s\n", params.ssl_file_key.c_str(), params.ssl_file_cert.c_str());
        svr.reset(
            new httplib::SSLServer(params.ssl_file_cert.c_str(), params.ssl_file_key.c_str())
        );
    } else {
        LOG_INF("Running without SSL\n");
        svr.reset(new httplib::Server());
    }
#else
    if (params.ssl_file_key != "" && params.ssl_file_cert != "") {
        LOG_ERR("Server is built without SSL support\n");
        return 1;
    }
    svr.reset(new httplib::Server());
#endif

    std::atomic<server_state> state{SERVER_STATE_LOADING_MODEL};

    svr->set_default_headers({{"Server", "llama.cpp"}});
    svr->set_logger(log_server_request);

    auto res_error = [](httplib::Response & res, const json & error_data) {
        json final_response {{"error", error_data}};
        res.set_content(safe_json_to_str(final_response), MIMETYPE_JSON);
        res.status = json_value(error_data, "code", 500);
    };

    auto res_ok = [](httplib::Response & res, const json & data) {
        res.set_content(safe_json_to_str(data), MIMETYPE_JSON);
        res.status = 200;
    };

    svr->set_exception_handler([&res_error](const httplib::Request &, httplib::Response & res, const std::exception_ptr & ep) {
        std::string message;
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception & e) {
            message = e.what();
        } catch (...) {
            message = "Unknown Exception";
        }

        try {
            json formatted_error = format_error_response(message, ERROR_TYPE_SERVER);
            LOG_WRN("got exception: %s\n", formatted_error.dump().c_str());
            res_error(res, formatted_error);
        } catch (const std::exception & e) {
            LOG_ERR("got another exception: %s | while hanlding exception: %s\n", e.what(), message.c_str());
        }
    });

    svr->set_error_handler([&res_error](const httplib::Request &, httplib::Response & res) {
        if (res.status == 404) {
            res_error(res, format_error_response("File Not Found", ERROR_TYPE_NOT_FOUND));
        }
        // for other error codes, we skip processing here because it's already done by res_error()
    });

    // set timeouts and change hostname and port
    svr->set_read_timeout (params.timeout_read);
    svr->set_write_timeout(params.timeout_write);

    std::unordered_map<std::string, std::string> log_data;

    log_data["hostname"] = params.hostname;
    log_data["port"]     = std::to_string(params.port);

    if (params.api_keys.size() == 1) {
        auto key = params.api_keys[0];
        log_data["api_key"] = "api_key: ****" + key.substr(std::max((int)(key.length() - 4), 0));
    } else if (params.api_keys.size() > 1) {
        log_data["api_key"] = "api_key: " + std::to_string(params.api_keys.size()) + " keys loaded";
    }

    // Necessary similarity of prompt for slot selection
    ctx_server.slot_prompt_similarity = params.slot_prompt_similarity;

    //
    // Middlewares
    //

    auto middleware_validate_api_key = [&params, &res_error](const httplib::Request & req, httplib::Response & res) {
        static const std::unordered_set<std::string> public_endpoints = {
            "/health",
            "/models",
            "/v1/models",
        };

        // If API key is not set, skip validation
        if (params.api_keys.empty()) {
            return true;
        }

        // If path is public or is static file, skip validation
        if (public_endpoints.find(req.path) != public_endpoints.end() || req.path == "/") {
            return true;
        }

        // Check for API key in the header
        auto auth_header = req.get_header_value("Authorization");

        std::string prefix = "Bearer ";
        if (auth_header.substr(0, prefix.size()) == prefix) {
            std::string received_api_key = auth_header.substr(prefix.size());
            if (std::find(params.api_keys.begin(), params.api_keys.end(), received_api_key) != params.api_keys.end()) {
                return true; // API key is valid
            }
        }

        // API key is invalid or not provided
        res_error(res, format_error_response("Invalid API Key", ERROR_TYPE_AUTHENTICATION));

        LOG_WRN("Unauthorized: Invalid API Key\n");

        return false;
    };

    auto middleware_server_state = [&res_error, &state](const httplib::Request & req, httplib::Response & res) {
        server_state current_state = state.load();
        if (current_state == SERVER_STATE_LOADING_MODEL) {
            auto tmp = string_split<std::string>(req.path, '.');
            if (req.path == "/" || tmp.back() == "html") {
                res.set_content(reinterpret_cast<const char*>(loading_html), loading_html_len, "text/html; charset=utf-8");
                res.status = 503;
            } else if (req.path == "/models" || req.path == "/v1/models") {
                // allow the models endpoint to be accessed during loading
                return true;
            } else {
                res_error(res, format_error_response("Loading model", ERROR_TYPE_UNAVAILABLE));
            }
            return false;
        }
        return true;
    };

    // register server middlewares
    svr->set_pre_routing_handler([&middleware_validate_api_key, &middleware_server_state](const httplib::Request & req, httplib::Response & res) {
        res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
        // If this is OPTIONS request, skip validation because browsers don't include Authorization header
        if (req.method == "OPTIONS") {
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Methods",     "GET, POST");
            res.set_header("Access-Control-Allow-Headers",     "*");
            res.set_content("", "text/html"); // blank response, no data
            return httplib::Server::HandlerResponse::Handled; // skip further processing
        }
        if (!middleware_server_state(req, res)) {
            return httplib::Server::HandlerResponse::Handled;
        }
        if (!middleware_validate_api_key(req, res)) {
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    //
    // Route handlers (or controllers)
    //

    const auto handle_health = [&](const httplib::Request &, httplib::Response & res) {
        // error and loading states are handled by middleware
        json health = {{"status", "ok"}};
        res_ok(res, health);
    };

    const auto handle_slots = [&](const httplib::Request & req, httplib::Response & res) {
        if (!params.endpoint_slots) {
            res_error(res, format_error_response("This server does not support slots endpoint. Start it with `--slots`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        // request slots data using task queue
        int task_id = ctx_server.queue_tasks.get_new_id();
        {
            server_task task(SERVER_TASK_TYPE_METRICS);
            task.id = task_id;
            ctx_server.queue_results.add_waiting_task_id(task_id);
            ctx_server.queue_tasks.post(std::move(task), true); // high-priority task
        }

        // get the result
        server_task_result_ptr result = ctx_server.queue_results.recv(task_id);
        ctx_server.queue_results.remove_waiting_task_id(task_id);

        if (result->is_error()) {
            res_error(res, result->to_json());
            return;
        }

        // TODO: get rid of this dynamic_cast
        auto res_metrics = dynamic_cast<server_task_result_metrics*>(result.get());
        GGML_ASSERT(res_metrics != nullptr);

        // optionally return "fail_on_no_slot" error
        if (req.has_param("fail_on_no_slot")) {
            if (res_metrics->n_idle_slots == 0) {
                res_error(res, format_error_response("no slot available", ERROR_TYPE_UNAVAILABLE));
                return;
            }
        }

        res_ok(res, res_metrics->slots_data);
    };

    const auto handle_metrics = [&](const httplib::Request &, httplib::Response & res) {
        if (!params.endpoint_metrics) {
            res_error(res, format_error_response("This server does not support metrics endpoint. Start it with `--metrics`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        // request slots data using task queue
        int task_id = ctx_server.queue_tasks.get_new_id();
        {
            server_task task(SERVER_TASK_TYPE_METRICS);
            task.id = task_id;
            ctx_server.queue_results.add_waiting_task_id(task_id);
            ctx_server.queue_tasks.post(std::move(task), true); // high-priority task
        }

        // get the result
        server_task_result_ptr result = ctx_server.queue_results.recv(task_id);
        ctx_server.queue_results.remove_waiting_task_id(task_id);

        if (result->is_error()) {
            res_error(res, result->to_json());
            return;
        }

        // TODO: get rid of this dynamic_cast
        auto res_metrics = dynamic_cast<server_task_result_metrics*>(result.get());
        GGML_ASSERT(res_metrics != nullptr);

        // metrics definition: https://prometheus.io/docs/practices/naming/#metric-names
        json all_metrics_def = json {
            {"counter", {{
                    {"name",  "prompt_tokens_total"},
                    {"help",  "Number of prompt tokens processed."},
                    {"value",  (uint64_t) res_metrics->n_prompt_tokens_processed_total}
            }, {
                    {"name",  "prompt_seconds_total"},
                    {"help",  "Prompt process time"},
                    {"value",  (uint64_t) res_metrics->t_prompt_processing_total / 1.e3}
            }, {
                    {"name",  "tokens_predicted_total"},
                    {"help",  "Number of generation tokens processed."},
                    {"value",  (uint64_t) res_metrics->n_tokens_predicted_total}
            }, {
                    {"name",  "tokens_predicted_seconds_total"},
                    {"help",  "Predict process time"},
                    {"value",  (uint64_t) res_metrics->t_tokens_generation_total / 1.e3}
            }, {
                    {"name",  "n_decode_total"},
                    {"help",  "Total number of llama_decode() calls"},
                    {"value",  res_metrics->n_decode_total}
            }, {
                    {"name",  "n_busy_slots_per_decode"},
                    {"help",  "Average number of busy slots per llama_decode() call"},
                    {"value",  (float) res_metrics->n_busy_slots_total / std::max((float) res_metrics->n_decode_total, 1.f)}
            }}},
            {"gauge", {{
                    {"name",  "prompt_tokens_seconds"},
                    {"help",  "Average prompt throughput in tokens/s."},
                    {"value",  res_metrics->n_prompt_tokens_processed ? 1.e3 / res_metrics->t_prompt_processing * res_metrics->n_prompt_tokens_processed : 0.}
            },{
                    {"name",  "predicted_tokens_seconds"},
                    {"help",  "Average generation throughput in tokens/s."},
                    {"value",  res_metrics->n_tokens_predicted ? 1.e3 / res_metrics->t_tokens_generation * res_metrics->n_tokens_predicted : 0.}
            },{
                    {"name",  "kv_cache_usage_ratio"},
                    {"help",  "KV-cache usage. 1 means 100 percent usage."},
                    {"value",  1. * res_metrics->kv_cache_used_cells / params.n_ctx}
            },{
                    {"name",  "kv_cache_tokens"},
                    {"help",  "KV-cache tokens."},
                    {"value",  (uint64_t) res_metrics->kv_cache_tokens_count}
            },{
                    {"name",  "requests_processing"},
                    {"help",  "Number of requests processing."},
                    {"value",  (uint64_t) res_metrics->n_processing_slots}
            },{
                    {"name",  "requests_deferred"},
                    {"help",  "Number of requests deferred."},
                    {"value",  (uint64_t) res_metrics->n_tasks_deferred}
            }}}
        };

        std::stringstream prometheus;

        for (const auto & el : all_metrics_def.items()) {
            const auto & type        = el.key();
            const auto & metrics_def = el.value();

            for (const auto & metric_def : metrics_def) {
                const std::string name = metric_def.at("name");
                const std::string help = metric_def.at("help");

                auto value = json_value(metric_def, "value", 0.);
                prometheus << "# HELP llamacpp:" << name << " " << help  << "\n"
                            << "# TYPE llamacpp:" << name << " " << type  << "\n"
                            << "llamacpp:"        << name << " " << value << "\n";
            }
        }

        res.set_header("Process-Start-Time-Unix", std::to_string(res_metrics->t_start));

        res.set_content(prometheus.str(), "text/plain; version=0.0.4");
        res.status = 200; // HTTP OK
    };

    const auto handle_slots_save = [&ctx_server, &res_error, &res_ok, &params](const httplib::Request & req, httplib::Response & res, int id_slot) {
        json request_data = json::parse(req.body);
        std::string filename = request_data.at("filename");
        if (!fs_validate_filename(filename)) {
            res_error(res, format_error_response("Invalid filename", ERROR_TYPE_INVALID_REQUEST));
            return;
        }
        std::string filepath = params.slot_save_path + filename;

        int task_id = ctx_server.queue_tasks.get_new_id();
        {
            server_task task(SERVER_TASK_TYPE_SLOT_SAVE);
            task.id = task_id;
            task.slot_action.slot_id  = id_slot;
            task.slot_action.filename = filename;
            task.slot_action.filepath = filepath;

            ctx_server.queue_results.add_waiting_task_id(task_id);
            ctx_server.queue_tasks.post(std::move(task));
        }

        server_task_result_ptr result = ctx_server.queue_results.recv(task_id);
        ctx_server.queue_results.remove_waiting_task_id(task_id);

        if (result->is_error()) {
            res_error(res, result->to_json());
            return;
        }

        res_ok(res, result->to_json());
    };

    const auto handle_slots_restore = [&ctx_server, &res_error, &res_ok, &params](const httplib::Request & req, httplib::Response & res, int id_slot) {
        json request_data = json::parse(req.body);
        std::string filename = request_data.at("filename");
        if (!fs_validate_filename(filename)) {
            res_error(res, format_error_response("Invalid filename", ERROR_TYPE_INVALID_REQUEST));
            return;
        }
        std::string filepath = params.slot_save_path + filename;

        int task_id = ctx_server.queue_tasks.get_new_id();
        {
            server_task task(SERVER_TASK_TYPE_SLOT_RESTORE);
            task.id = task_id;
            task.slot_action.slot_id  = id_slot;
            task.slot_action.filename = filename;
            task.slot_action.filepath = filepath;

            ctx_server.queue_results.add_waiting_task_id(task_id);
            ctx_server.queue_tasks.post(std::move(task));
        }

        server_task_result_ptr result = ctx_server.queue_results.recv(task_id);
        ctx_server.queue_results.remove_waiting_task_id(task_id);

        if (result->is_error()) {
            res_error(res, result->to_json());
            return;
        }

        GGML_ASSERT(dynamic_cast<server_task_result_slot_save_load*>(result.get()) != nullptr);
        res_ok(res, result->to_json());
    };

    const auto handle_slots_erase = [&ctx_server, &res_error, &res_ok](const httplib::Request & /* req */, httplib::Response & res, int id_slot) {
        int task_id = ctx_server.queue_tasks.get_new_id();
        {
            server_task task(SERVER_TASK_TYPE_SLOT_ERASE);
            task.id = task_id;
            task.slot_action.slot_id = id_slot;

            ctx_server.queue_results.add_waiting_task_id(task_id);
            ctx_server.queue_tasks.post(std::move(task));
        }

        server_task_result_ptr result = ctx_server.queue_results.recv(task_id);
        ctx_server.queue_results.remove_waiting_task_id(task_id);

        if (result->is_error()) {
            res_error(res, result->to_json());
            return;
        }

        GGML_ASSERT(dynamic_cast<server_task_result_slot_erase*>(result.get()) != nullptr);
        res_ok(res, result->to_json());
    };

    const auto handle_slots_action = [&params, &res_error, &handle_slots_save, &handle_slots_restore, &handle_slots_erase](const httplib::Request & req, httplib::Response & res) {
        if (params.slot_save_path.empty()) {
            res_error(res, format_error_response("This server does not support slots action. Start it with `--slot-save-path`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        std::string id_slot_str = req.path_params.at("id_slot");
        int id_slot;

        try {
            id_slot = std::stoi(id_slot_str);
        } catch (const std::exception &) {
            res_error(res, format_error_response("Invalid slot ID", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        std::string action = req.get_param_value("action");

        if (action == "save") {
            handle_slots_save(req, res, id_slot);
        } else if (action == "restore") {
            handle_slots_restore(req, res, id_slot);
        } else if (action == "erase") {
            handle_slots_erase(req, res, id_slot);
        } else {
            res_error(res, format_error_response("Invalid action", ERROR_TYPE_INVALID_REQUEST));
        }
    };

    const auto handle_props = [&ctx_server, &res_ok](const httplib::Request &, httplib::Response & res) {
        // this endpoint is publicly available, please only return what is safe to be exposed
        json data = {
            { "default_generation_settings", ctx_server.default_generation_settings_for_props },
            { "total_slots",                 ctx_server.params_base.n_parallel },
            { "model_path",                  ctx_server.params_base.model.path },
            { "modalities",                  json{{"vision", ctx_server.mctx != nullptr}} }, // TODO: add more in the future
            { "chat_template",               common_chat_templates_source(ctx_server.chat_templates.get()) },
            { "bos_token",                   common_token_to_piece(ctx_server.ctx, llama_vocab_bos(ctx_server.vocab), /* special= */ true)},
            { "eos_token",                   common_token_to_piece(ctx_server.ctx, llama_vocab_eos(ctx_server.vocab), /* special= */ true)},
            { "build_info",                  build_info },
        };
        if (ctx_server.params_base.use_jinja) {
            if (auto tool_use_src = common_chat_templates_source(ctx_server.chat_templates.get(), "tool_use")) {
                data["chat_template_tool_use"] = tool_use_src;
            }
        }

        res_ok(res, data);
    };

    const auto handle_props_change = [&ctx_server, &res_error, &res_ok](const httplib::Request & req, httplib::Response & res) {
        if (!ctx_server.params_base.endpoint_props) {
            res_error(res, format_error_response("This server does not support changing global properties. Start it with `--props`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        json data = json::parse(req.body);

        // update any props here

        res_ok(res, {{ "success", true }});
    };

    const auto handle_api_show = [&ctx_server, &res_ok](const httplib::Request &, httplib::Response & res) {
        json data = {
            {
                "template", common_chat_templates_source(ctx_server.chat_templates.get()),
            },
            {
                "model_info", {
                    { "llama.context_length", ctx_server.slots.back().n_ctx, },
                }
            },
        };

        res_ok(res, data);
    };

    // handle completion-like requests (completion, chat, infill)
    // we can optionally provide a custom format for partial results and final results
    const auto handle_completions_impl = [&ctx_server, &res_error, &res_ok](
            server_task_type type,
            json & data,
            const std::vector<raw_buffer> & files,
            const std::function<bool()> & is_connection_closed,
            httplib::Response & res,
            oaicompat_type oaicompat) -> void {
        GGML_ASSERT(type == SERVER_TASK_TYPE_COMPLETION || type == SERVER_TASK_TYPE_INFILL);

        if (ctx_server.params_base.embedding) {
            res_error(res, format_error_response("This server does not support completions. Start it without `--embeddings`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        auto completion_id = gen_chatcmplid();
        std::unordered_set<int> task_ids;
        try {
            std::vector<server_task> tasks;

            const auto & prompt = data.at("prompt");
            // TODO: this log can become very long, put it behind a flag or think about a more compact format
            //SRV_DBG("Prompt: %s\n", prompt.is_string() ? prompt.get<std::string>().c_str() : prompt.dump(2).c_str());

            // process files
            mtmd::bitmaps bitmaps;
            const bool has_mtmd = ctx_server.mctx != nullptr;
            {
                if (!has_mtmd && !files.empty()) {
                    throw std::runtime_error("This server does not support multimodal");
                }
                for (auto & file : files) {
                    mtmd::bitmap bmp(mtmd_helper_bitmap_init_from_buf(file.data(), file.size()));
                    if (!bmp.ptr) {
                        throw std::runtime_error("Failed to load image");
                    }
                    // calculate bitmap hash (for KV caching)
                    std::string hash = fnv_hash(bmp.data(), bmp.nx()*bmp.ny()*3);
                    bmp.set_id(hash.c_str());
                    bitmaps.entries.push_back(std::move(bmp));
                }
            }

            // process prompt
            std::vector<server_tokens> inputs;
            if (oaicompat && !prompt.is_string()) {
                throw std::runtime_error("prompt must be a string");
            }

            if (oaicompat && has_mtmd) {
                // multimodal
                std::string prompt_str = prompt.get<std::string>();
                mtmd_input_text inp_txt = {
                    prompt_str.c_str(),
                    /* add_special */   true,
                    /* parse_special */ true,
                };
                mtmd::input_chunks chunks(mtmd_input_chunks_init());
                auto bitmaps_c_ptr = bitmaps.c_ptr();
                int32_t tokenized = mtmd_tokenize(ctx_server.mctx,
                                                    chunks.ptr.get(),
                                                    &inp_txt,
                                                    bitmaps_c_ptr.data(),
                                                    bitmaps_c_ptr.size());
                if (tokenized != 0) {
                    throw std::runtime_error("Failed to tokenize prompt");
                }

                server_tokens tmp(chunks, true);
                inputs.push_back(std::move(tmp));
            } else {
                // non-multimodal version
                auto tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, prompt, true, true);
                for (auto & p : tokenized_prompts) {
                    auto tmp = server_tokens(p, ctx_server.mctx != nullptr);
                    inputs.push_back(std::move(tmp));
                }
            }

            tasks.reserve(inputs.size());
            for (size_t i = 0; i < inputs.size(); i++) {
                server_task task = server_task(type);

                task.id    = ctx_server.queue_tasks.get_new_id();
                task.index = i;

                task.prompt_tokens    = std::move(inputs[i]);
                task.params           = server_task::params_from_json_cmpl(
                        ctx_server.ctx,
                        ctx_server.params_base,
                        data);
                task.id_selected_slot = json_value(data, "id_slot", -1);

                // OAI-compat
                task.params.oaicompat                 = oaicompat;
                task.params.oaicompat_cmpl_id         = completion_id;
                // oaicompat_model is already populated by params_from_json_cmpl

                tasks.push_back(std::move(task));
            }

            task_ids = server_task::get_list_id(tasks);
            ctx_server.queue_results.add_waiting_tasks(tasks);
            ctx_server.queue_tasks.post(std::move(tasks));
        } catch (const std::exception & e) {
            res_error(res, format_error_response(e.what(), ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        bool stream = json_value(data, "stream", false);

        if (!stream) {
            ctx_server.receive_multi_results(task_ids, [&](std::vector<server_task_result_ptr> & results) {
                if (results.size() == 1) {
                    // single result
                    res_ok(res, results[0]->to_json());
                } else {
                    // multiple results (multitask)
                    json arr = json::array();
                    for (auto & res : results) {
                        arr.push_back(res->to_json());
                    }
                    res_ok(res, arr);
                }
            }, [&](const json & error_data) {
                res_error(res, error_data);
            }, is_connection_closed);

            ctx_server.queue_results.remove_waiting_task_ids(task_ids);
        } else {
            const auto chunked_content_provider = [task_ids, &ctx_server, oaicompat](size_t, httplib::DataSink & sink) {
                ctx_server.receive_cmpl_results_stream(task_ids, [&](server_task_result_ptr & result) -> bool {
                    json res_json = result->to_json();
                    if (res_json.is_array()) {
                        for (const auto & res : res_json) {
                            if (!server_sent_event(sink, "data", res)) {
                                // sending failed (HTTP connection closed), cancel the generation
                                return false;
                            }
                        }
                        return true;
                    } else {
                        return server_sent_event(sink, "data", res_json);
                    }
                }, [&](const json & error_data) {
                    server_sent_event(sink, "error", error_data);
                }, [&sink]() {
                    // note: do not use req.is_connection_closed here because req is already destroyed
                    return !sink.is_writable();
                });
                if (oaicompat != OAICOMPAT_TYPE_NONE) {
                    static const std::string ev_done = "data: [DONE]\n\n";
                    sink.write(ev_done.data(), ev_done.size());
                }
                sink.done();
                return false;
            };

            auto on_complete = [task_ids, &ctx_server] (bool) {
                ctx_server.queue_results.remove_waiting_task_ids(task_ids);
            };

            res.set_chunked_content_provider("text/event-stream", chunked_content_provider, on_complete);
        }
    };

    //OWL BEGIN
    // handle completion-like requests (completion, chat, infill)
    // we can optionally provide a custom format for partial results and final results
    const auto handle_completions_impl_with_rag = [&ctx_server, &res_error, &res_ok](
            server_task_type type,
            json & data,
            const std::vector<raw_buffer> & files,
            const std::function<bool()> & is_connection_closed,
            httplib::Response & res,
            oaicompat_type oaicompat) -> void {
        GGML_ASSERT(type == SERVER_TASK_TYPE_COMPLETION || type == SERVER_TASK_TYPE_INFILL);

        if (ctx_server.params_base.embedding) {
            res_error(res, format_error_response("This server does not support completions. Start it without `--embeddings`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        auto completion_id = gen_chatcmplid();
        std::unordered_set<int> task_ids;
        try {
            std::vector<server_task> tasks;

            const auto & prompt = data.at("prompt");
            // TODO: this log can become very long, put it behind a flag or think about a more compact format
            //SRV_DBG("Prompt: %s\n", prompt.is_string() ? prompt.get<std::string>().c_str() : prompt.dump(2).c_str());

            // --- STAGE 1: Absorb Prompt, Get Embedding, RAG Query ---
            auto tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, prompt, true, true);
            for (const auto & tokens : tokenized_prompts) {
                // this check is necessary for models that do not add BOS token to the input
                if (tokens.empty()) {
                    res_error(res, format_error_response("Input content cannot be empty", ERROR_TYPE_INVALID_REQUEST));
                    return;
                }
            }
            std::cerr<<"prompt tokenization done"<<std::endl;

            // 1. Create a temporary task for embedding extraction from the initial prompt
            // create and queue the embedding tasks
            bool error = false;
            std::unordered_set<int> embedding_task_ids;
            {
                std::vector<server_task> tasks;
                for (size_t i = 0; i < tokenized_prompts.size(); i++) {
                    server_task task = server_task(SERVER_TASK_TYPE_EMBEDDING);

                    task.id            = ctx_server.queue_tasks.get_new_id();
                    task.index         = i;
                    task.prompt_tokens = server_tokens(tokenized_prompts[i], ctx_server.mctx != nullptr);

                    // OAI-compat
                    task.params.oaicompat = oaicompat;

                    tasks.push_back(std::move(task));
                }

                embedding_task_ids = server_task::get_list_id(tasks);
                ctx_server.queue_results.add_waiting_tasks(tasks);
                ctx_server.queue_tasks.post(std::move(tasks));
            }
            std::cerr<<"there are "<< embedding_task_ids.size() << "embedding tasks" << std::endl;

            std::vector<float> last_prompt_embedding;
            // get the result
            ctx_server.receive_multi_results(embedding_task_ids, [&](std::vector<server_task_result_ptr> & results) {
                for (auto & res : results) {
                    GGML_ASSERT(dynamic_cast<server_task_result_embd*>(res.get()) != nullptr);
                    last_prompt_embedding =dynamic_cast<server_task_result_embd*>(res.get())->embedding.back();
                }
            }, [&](const json & error_data) {
                res_error(res, error_data);
                error = true;
            }, is_connection_closed);

            ctx_server.queue_results.remove_waiting_task_ids(embedding_task_ids);

            if (error) {
                return;
            }


            if (last_prompt_embedding.empty()) {
                res_error(res, format_error_response("Failed to process prompt for RAG embedding.", ERROR_TYPE_INVALID_REQUEST));
                return;
            }
            else
                std::cerr << "last prompt token embedding has been found" << std::endl;

            // 2. Query RAG Database
            // You'll need to define 'num_chunks_to_retrieve' (e.g., from client data or server config)
            int num_chunks_to_retrieve = json_value(data, "n_rag_chunks", 7);
            int num_max_augmentations = json_value(data, "n_max_augmentations", 3);
            bool use_reranking = json_value(data, "useRERANKING", false);

            std::string db_user = "postgres";
            std::string db_password = "admin";
            std::string db_host = "localhost";
            int db_port = 5432;
            std::string db_name = "klave_rag";

            if (data.count("rag_connection")) {
                const auto & rag_connection = data.at("rag_connection");
                db_user = json_value(rag_connection, "user", std::string("postgres"));
                db_password = json_value(rag_connection, "password", std::string("admin"));
                db_host = json_value(rag_connection, "host", std::string("localhost"));
                db_port = json_value(rag_connection, "port", (int)5432);
                db_name = json_value(rag_connection, "name", std::string("klave_rag"));
                std::cerr << "rag_connection provided: " << rag_connection.dump(-1) << std::endl;
            }
            else
                std::cerr << " rag_connection provided - using default" << std::endl;

            
            if(use_reranking && (llama_vocab_sep(ctx_server.vocab) == LLAMA_TOKEN_NULL))
            {
                use_reranking = false;
                std::cerr << "Reranking cannot work because model does not include a separator token - deactivated" << std::endl;
            }

            auto rag_db = create_rag_database(db_host, db_port, db_name);
            try {
                rag_db->connect(db_user, db_password);
            } catch (const std::exception& e) {
                res_error(res, format_error_response(std::string("Database connection error: ") + e.what(), ERROR_TYPE_SERVER));
                error = true;
                return; // Exit the lambda if connection fails
            }
            auto nearest_chunks =  rag_db->searchNearest(last_prompt_embedding, num_chunks_to_retrieve);
            //std::vector<std::string> retrieved_chunks ;//= query_rag_database(last_token_embedding, num_chunks_to_retrieve);
            std::cerr<<"found " << nearest_chunks.size() << " potential chunks to use for augmnentation" << std::endl; //TODO: remove

            std::string hardcoded_sk = "0123456789012345678901234567890123456789012345678901234567890123";//TODO: keep these keys secret in the database
            ecc256_private_key recipient_sk = postgres_client::hex_to_byte_array<32>(hardcoded_sk);;
            auto recipient_pk = CryptoUtils::computePublicKey(recipient_sk);

            // 3. Reranking
            // create and queue the reranking task
            std::vector<std::string> documents;
            if (!nearest_chunks.empty()) {
                int pg_rank = 1;
                for (const auto& chunk : nearest_chunks) {
                    std::vector<uint8_t> retrieved_encrypted_content_default = std::get<12>(chunk);
                    aes_gcm_tag retrieved_tag_default = std::get<13>(chunk);
                    aes_gcm_nonce retrieved_nonce_default = std::get<14>(chunk);
                    ecc256_public_key retrieved_ephemeral_pk_default = std::get<15>(chunk);
                    ecc256_public_key retrieved_recipient_pk_default = std::get<6>(chunk);
                    auto distance = std::get<16>(chunk);
                    if(retrieved_recipient_pk_default != recipient_pk)
                    {
                        std::cerr << "mismatching recipient pk" << std::endl;
                        continue;
                    }
                    std::vector<uint8_t> decrypted_content_default;
                    if(retrieved_ephemeral_pk_default == ecc256_public_key())
                        decrypted_content_default = retrieved_encrypted_content_default;
                    else
                        decrypted_content_default = EciesUtils::decrypt_ecies(
                        retrieved_encrypted_content_default, retrieved_tag_default, retrieved_nonce_default,
                        retrieved_ephemeral_pk_default, recipient_sk);
                    if(decrypted_content_default.empty())
                    {
                        std::cerr << "rag entry is corrupted and does not decrypt" << std::endl;
                        continue;
                    }
                    std::string decrypted_chunk(std::begin(decrypted_content_default),std::end(decrypted_content_default));
                    std::cerr << "chunk[" << pg_rank << "] at distance[" << distance << "] = \n"<< decrypted_chunk.c_str() << std::endl;
                    documents.push_back(decrypted_chunk);
                    pg_rank++;
                }
                for(int i=0;i<10;i++)
                    std::cerr << "=======================================================================================" << std::endl;
            }

            std::cerr<<"within those " << nearest_chunks.size() << " potential chunks to use for augmnentation, "<< documents.size() <<" did decrypt" << std::endl; //TODO: remove
            if(!use_reranking)
                {
                    std::cerr<<"will NOT rerank " << std::endl; //TODO: remove
                    if(num_max_augmentations < num_chunks_to_retrieve)
                        documents.resize(num_max_augmentations);
                }
            else
            {
                std::cerr<<" using reranking"<<std::endl;
                std::vector<std::tuple<int,float,std::string>> ranked_documents;
                ranked_documents.reserve(documents.size());
                for (int i = 0; i < documents.size(); ++i) {
                    ranked_documents.emplace_back(i, 0.0f, documents[i]);
                    }
                if(!ranked_documents.empty()){
                    std::unordered_set<int> reranking_task_ids;
                    std::vector<server_task> reranking_tasks;
                    auto tokenized_docs = tokenize_input_prompts(ctx_server.vocab, documents, /* add_special */ false, true);
                    std::cerr<<" all of those documents correctly tokenised"<<std::endl;
                    reranking_tasks.reserve(tokenized_docs.size());
                    for (size_t i = 0; i < tokenized_docs.size(); i++) {
                        llama_tokens tmp = format_rerank(ctx_server.vocab, tokenized_prompts[0], tokenized_docs[i]);
                        server_task task   = server_task(SERVER_TASK_TYPE_RERANK);
                        task.id            = ctx_server.queue_tasks.get_new_id();
                        task.index         = i;
                        task.prompt_tokens = server_tokens(tmp, ctx_server.mctx != nullptr);
                        reranking_tasks.push_back(std::move(task));
                        }
                    reranking_task_ids = server_task::get_list_id(reranking_tasks);
                    ctx_server.queue_results.add_waiting_tasks(reranking_tasks);
                    ctx_server.queue_tasks.post(std::move(reranking_tasks));

                    std::cerr<<" reranking queries sent" <<std::endl;
                    ctx_server.receive_multi_results(reranking_task_ids, [&](std::vector<server_task_result_ptr> & results) {
                        for (auto & res : results) {
                            auto p_rerank = dynamic_cast<server_task_result_rerank*>(res.get());
                            GGML_ASSERT(p_rerank != nullptr);
                            std::get<1>(ranked_documents[p_rerank->get_index()]) =  p_rerank->score;
                        }
                    }, [&](const json & error_data) {
                        res_error(res, error_data);
                        error = true;
                    }, is_connection_closed);

                    if (error) {
                        return;
                    }
                    std::cerr<<" reranking queries completed" <<std::endl;

                    // document ranking
                    std::sort(ranked_documents.begin(), ranked_documents.end(),
                    [](const std::tuple<int,float,std::string>& a, const std::tuple<int,float,std::string>& b) {
                        return std::get<1>(a) > std::get<1>(b); // 'a.second > b.second' for descending order
                        });
                    for(int n = 0; n<ranked_documents.size(); n++)
                        std::cerr<< "rank:" << n << ", score:"<< std::get<1>(ranked_documents[n]) << ", content:\n" << std::get<2>(ranked_documents[n]) << std::endl;
                }
                documents.resize(0);
                int n_augmentations = 0;
                for (const auto& ranked_document : ranked_documents) {
                    if(n_augmentations >= num_max_augmentations)
                        break;
                    documents.push_back(std::get<2>(ranked_document));
                }
            }


            // 3. Construct Augmented Prompt
            std::string augmented_prompt_str; // Append original prompt question
            std::string delimiter = "<|im_start|>user";
            size_t beginPos = prompt.template get<std::string>().rfind(delimiter); // rfind finds the last occurrence
            if (beginPos != std::string::npos)
                augmented_prompt_str = prompt.template get<std::string>().substr(beginPos,delimiter.size()+1);

            if (!documents.empty()) {
                augmented_prompt_str += "Here is some relevant context to answer:\n";
                for (const auto& document : documents) {
                     augmented_prompt_str += "- " + document + "\n";
                }
            }
            if (beginPos != std::string::npos)
                augmented_prompt_str += "\n" + prompt.template get<std::string>().substr(beginPos+delimiter.size()+1, prompt.template get<std::string>().size()-(beginPos+delimiter.size()+1));
            else
                augmented_prompt_str += prompt.template get<std::string>();
            std::cerr<<"augmented prompt is now : "<< std::endl << augmented_prompt_str.c_str() << std::endl;
            rag_db->disconnect();
            // You might need to add specific chat formatting for the final generation
            // e.g., if it's a chat model:
            // augmented_prompt_str = "<|im_start|>user\n" + augmented_prompt_str + "<|im_end|>\n<|im_start|>assistant";

            // process files
            mtmd::bitmaps bitmaps;
            const bool has_mtmd = ctx_server.mctx != nullptr;
            {
                if (!has_mtmd && !files.empty()) {
                    throw std::runtime_error("This server does not support multimodal");
                }
                for (auto & file : files) {
                    mtmd::bitmap bmp(mtmd_helper_bitmap_init_from_buf(file.data(), file.size()));
                    if (!bmp.ptr) {
                        throw std::runtime_error("Failed to load image");
                    }
                    // calculate bitmap hash (for KV caching)
                    std::string hash = fnv_hash(bmp.data(), bmp.nx()*bmp.ny()*3);
                    bmp.set_id(hash.c_str());
                    bitmaps.entries.push_back(std::move(bmp));
                }
            }

            // process augmented prompt
            nlohmann::json augmented_prompt = augmented_prompt_str;
            std::vector<server_tokens> inputs;
            if (oaicompat && !augmented_prompt.is_string()) {
                throw std::runtime_error("augmented_prompt must be a string");
            }

            if (oaicompat && has_mtmd) {
                // multimodal
                std::string prompt_str = augmented_prompt.get<std::string>();
                mtmd_input_text inp_txt = {
                    prompt_str.c_str(),
                    /* add_special */   true,
                    /* parse_special */ true,
                };
                mtmd::input_chunks chunks(mtmd_input_chunks_init());
                auto bitmaps_c_ptr = bitmaps.c_ptr();
                int32_t tokenized = mtmd_tokenize(ctx_server.mctx,
                                                    chunks.ptr.get(),
                                                    &inp_txt,
                                                    bitmaps_c_ptr.data(),
                                                    bitmaps_c_ptr.size());
                if (tokenized != 0) {
                    throw std::runtime_error("Failed to tokenize prompt");
                }

                server_tokens tmp(chunks, true);
                inputs.push_back(std::move(tmp));
            } else {
                // non-multimodal version
                
                auto tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, augmented_prompt, true, true);
                for (auto & p : tokenized_prompts) {
                    auto tmp = server_tokens(p, ctx_server.mctx != nullptr);
                    inputs.push_back(std::move(tmp));
                }
                std::cerr<< "now using rag for completions" << std::endl;
            }

            tasks.reserve(inputs.size());
            for (size_t i = 0; i < inputs.size(); i++) {
                server_task task = server_task(type);

                task.id    = ctx_server.queue_tasks.get_new_id();
                task.index = i;

                task.prompt_tokens    = std::move(inputs[i]);
                task.params           = server_task::params_from_json_cmpl(
                        ctx_server.ctx,
                        ctx_server.params_base,
                        data);
                task.id_selected_slot = json_value(data, "id_slot", -1);

                // OAI-compat
                task.params.oaicompat                 = oaicompat;
                task.params.oaicompat_cmpl_id         = completion_id;
                // oaicompat_model is already populated by params_from_json_cmpl

                tasks.push_back(std::move(task));
            }

            task_ids = server_task::get_list_id(tasks);
            ctx_server.queue_results.add_waiting_tasks(tasks);
            ctx_server.queue_tasks.post(std::move(tasks));
        } catch (const std::exception & e) {
            res_error(res, format_error_response(e.what(), ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        bool stream = json_value(data, "stream", false);

        if (!stream) {
            std::cerr<< "using stream mode" << std::endl; //OWL WAS HERE
            ctx_server.receive_multi_results(task_ids, [&](std::vector<server_task_result_ptr> & results) {
                if (results.size() == 1) {
                    // single result
                    res_ok(res, results[0]->to_json());
                } else {
                    // multiple results (multitask)
                    json arr = json::array();
                    for (auto & res : results) {
                        arr.push_back(res->to_json());
                    }
                    res_ok(res, arr);
                }
            }, [&](const json & error_data) {
                res_error(res, error_data);
            }, is_connection_closed);

            ctx_server.queue_results.remove_waiting_task_ids(task_ids);
        } else {
            std::cerr<< "using non-stream mode (chunked)" << std::endl; //OWL WAS HERE
            const auto chunked_content_provider = [task_ids, &ctx_server, oaicompat](size_t, httplib::DataSink & sink) {
                ctx_server.receive_cmpl_results_stream(task_ids, [&](server_task_result_ptr & result) -> bool {
                    json res_json = result->to_json();
                    if (res_json.is_array()) {
                        for (const auto & res : res_json) {
                            if (!server_sent_event(sink, "data", res)) {
                                // sending failed (HTTP connection closed), cancel the generation
                                return false;
                            }
                        }
                        return true;
                    } else {
                        return server_sent_event(sink, "data", res_json);
                    }
                }, [&](const json & error_data) {
                    server_sent_event(sink, "error", error_data);
                }, [&sink]() {
                    // note: do not use req.is_connection_closed here because req is already destroyed
                    return !sink.is_writable();
                });
                if (oaicompat != OAICOMPAT_TYPE_NONE) {
                    static const std::string ev_done = "data: [DONE]\n\n";
                    sink.write(ev_done.data(), ev_done.size());
                }
                sink.done();
                return false;
            };

            auto on_complete = [task_ids, &ctx_server] (bool) {
                ctx_server.queue_results.remove_waiting_task_ids(task_ids);
            };

            res.set_chunked_content_provider("text/event-stream", chunked_content_provider, on_complete);
        }
    };


    //OWL END

    const auto handle_completions = [&handle_completions_impl](const httplib::Request & req, httplib::Response & res) {
        json data = json::parse(req.body);
        std::vector<raw_buffer> files; // dummy
        handle_completions_impl(
            SERVER_TASK_TYPE_COMPLETION,
            data,
            files,
            req.is_connection_closed,
            res,
            OAICOMPAT_TYPE_NONE);
    };

    const auto handle_completions_oai = [&handle_completions_impl](const httplib::Request & req, httplib::Response & res) {
        json data = oaicompat_completion_params_parse(json::parse(req.body));
        std::vector<raw_buffer> files; // dummy
        handle_completions_impl(
            SERVER_TASK_TYPE_COMPLETION,
            data,
            files,
            req.is_connection_closed,
            res,
            OAICOMPAT_TYPE_COMPLETION);
    };

    const auto handle_infill = [&ctx_server, &res_error, &handle_completions_impl](const httplib::Request & req, httplib::Response & res) {
        // check model compatibility
        std::string err;
        if (llama_vocab_fim_pre(ctx_server.vocab) == LLAMA_TOKEN_NULL) {
            err += "prefix token is missing. ";
        }
        if (llama_vocab_fim_suf(ctx_server.vocab) == LLAMA_TOKEN_NULL) {
            err += "suffix token is missing. ";
        }
        if (llama_vocab_fim_mid(ctx_server.vocab) == LLAMA_TOKEN_NULL) {
            err += "middle token is missing. ";
        }
        if (!err.empty()) {
            res_error(res, format_error_response(string_format("Infill is not supported by this model: %s", err.c_str()), ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        json data = json::parse(req.body);

        // validate input
        if (data.contains("prompt") && !data.at("prompt").is_string()) {
            // prompt is optional
            res_error(res, format_error_response("\"prompt\" must be a string", ERROR_TYPE_INVALID_REQUEST));
        }

        if (!data.contains("input_prefix")) {
            res_error(res, format_error_response("\"input_prefix\" is required", ERROR_TYPE_INVALID_REQUEST));
        }

        if (!data.contains("input_suffix")) {
            res_error(res, format_error_response("\"input_suffix\" is required", ERROR_TYPE_INVALID_REQUEST));
        }

        if (data.contains("input_extra") && !data.at("input_extra").is_array()) {
            // input_extra is optional
            res_error(res, format_error_response("\"input_extra\" must be an array of {\"filename\": string, \"text\": string}", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        json input_extra = json_value(data, "input_extra", json::array());
        for (const auto & chunk : input_extra) {
            // { "text": string, "filename": string }
            if (!chunk.contains("text") || !chunk.at("text").is_string()) {
                res_error(res, format_error_response("extra_context chunk must contain a \"text\" field with a string value", ERROR_TYPE_INVALID_REQUEST));
                return;
            }
            // filename is optional
            if (chunk.contains("filename") && !chunk.at("filename").is_string()) {
                res_error(res, format_error_response("extra_context chunk's \"filename\" field must be a string", ERROR_TYPE_INVALID_REQUEST));
                return;
            }
        }
        data["input_extra"] = input_extra; // default to empty array if it's not exist

        std::string prompt = json_value(data, "prompt", std::string());
        std::vector<llama_tokens> tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, prompt, false, true);
        SRV_DBG("creating infill tasks, n_prompts = %d\n", (int) tokenized_prompts.size());
        data["prompt"] = format_infill(
            ctx_server.vocab,
            data.at("input_prefix"),
            data.at("input_suffix"),
            data.at("input_extra"),
            ctx_server.params_base.n_batch,
            ctx_server.params_base.n_predict,
            ctx_server.slots[0].n_ctx, // TODO: there should be a better way
            ctx_server.params_base.spm_infill,
            tokenized_prompts[0]
        );

        std::vector<raw_buffer> files; // dummy
        handle_completions_impl(
            SERVER_TASK_TYPE_INFILL,
            data,
            files,
            req.is_connection_closed,
            res,
            OAICOMPAT_TYPE_NONE); // infill is not OAI compatible
    };

    const auto handle_chat_completions = [&ctx_server, &params, &res_error, &handle_completions_impl](const httplib::Request & req, httplib::Response & res) {
        LOG_DBG("request: %s\n", req.body.c_str());
        if (ctx_server.params_base.embedding) {
            res_error(res, format_error_response("This server does not support completions. Start it without `--embeddings`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        auto body = json::parse(req.body);
        std::vector<raw_buffer> files;
        json data = oaicompat_completion_params_parse(
            body,
            params.use_jinja,
            params.reasoning_format,
            ctx_server.chat_templates.get(),
            ctx_server.mctx,
            files);

        handle_completions_impl(
            SERVER_TASK_TYPE_COMPLETION,
            data,
            files,
            req.is_connection_closed,
            res,
            OAICOMPAT_TYPE_CHAT);
    };

    // same with handle_chat_completions, but without inference part
    const auto handle_apply_template = [&ctx_server, &params, &res_ok](const httplib::Request & req, httplib::Response & res) {
        auto body = json::parse(req.body);
        std::vector<raw_buffer> files; // dummy, unused
        json data = oaicompat_completion_params_parse(
            body,
            params.use_jinja,
            params.reasoning_format,
            ctx_server.chat_templates.get(),
            ctx_server.mctx,
            files);
        res_ok(res, {{ "prompt", std::move(data.at("prompt")) }});
    };

    const auto handle_models = [&params, &ctx_server, &state, &res_ok](const httplib::Request &, httplib::Response & res) {
        server_state current_state = state.load();
        json model_meta = nullptr;
        if (current_state == SERVER_STATE_READY) {
            model_meta = ctx_server.model_meta();
        }

        json models = {
            {"object", "list"},
            {"data", {
                {
                    {"id",       params.model_alias.empty() ? params.model.path : params.model_alias},
                    {"object",   "model"},
                    {"created",  std::time(0)},
                    {"owned_by", "llamacpp"},
                    {"meta",     model_meta},
                },
             }}
        };

        res_ok(res, models);
    };

    const auto handle_tokenize = [&ctx_server, &res_ok](const httplib::Request & req, httplib::Response & res) {
        const json body = json::parse(req.body);

        json tokens_response = json::array();
        if (body.count("content") != 0) {
            const bool add_special = json_value(body, "add_special", false);
            const bool with_pieces = json_value(body, "with_pieces", false);

            llama_tokens tokens = tokenize_mixed(ctx_server.vocab, body.at("content"), add_special, true);

            if (with_pieces) {
                for (const auto& token : tokens) {
                    std::string piece = common_token_to_piece(ctx_server.ctx, token);
                    json piece_json;

                    // Check if the piece is valid UTF-8
                    if (is_valid_utf8(piece)) {
                        piece_json = piece;
                    } else {
                        // If not valid UTF-8, store as array of byte values
                        piece_json = json::array();
                        for (unsigned char c : piece) {
                            piece_json.push_back(static_cast<int>(c));
                        }
                    }

                    tokens_response.push_back({
                        {"id", token},
                        {"piece", piece_json}
                    });
                }
            } else {
                tokens_response = tokens;
            }
        }

        const json data = format_tokenizer_response(tokens_response);
        res_ok(res, data);
    };

    const auto handle_detokenize = [&ctx_server, &res_ok](const httplib::Request & req, httplib::Response & res) {
        const json body = json::parse(req.body);

        std::string content;
        if (body.count("tokens") != 0) {
            const llama_tokens tokens = body.at("tokens");
            content = tokens_to_str(ctx_server.ctx, tokens.cbegin(), tokens.cend());
        }

        const json data = format_detokenized_response(content);
        res_ok(res, data);
    };

    const auto handle_embeddings_impl = [&ctx_server, &res_error, &res_ok](const httplib::Request & req, httplib::Response & res, oaicompat_type oaicompat) {
        const json body = json::parse(req.body);

        if (oaicompat != OAICOMPAT_TYPE_NONE && llama_pooling_type(ctx_server.ctx) == LLAMA_POOLING_TYPE_NONE) {
            res_error(res, format_error_response("Pooling type 'none' is not OAI compatible. Please use a different pooling type", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        // for the shape of input/content, see tokenize_input_prompts()
        json prompt;
        if (body.count("input") != 0) {
            prompt = body.at("input");
        } else if (body.contains("content")) {
            oaicompat = OAICOMPAT_TYPE_NONE; // "content" field is not OAI compatible
            prompt = body.at("content");
        } else {
            res_error(res, format_error_response("\"input\" or \"content\" must be provided", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        bool use_base64 = false;
        if (body.count("encoding_format") != 0) {
            const std::string& format = body.at("encoding_format");
            if (format == "base64") {
                use_base64 = true;
            } else if (format != "float") {
                res_error(res, format_error_response("The format to return the embeddings in. Can be either float or base64", ERROR_TYPE_INVALID_REQUEST));
                return;
            }
        }

        auto tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, prompt, true, true);
        for (const auto & tokens : tokenized_prompts) {
            // this check is necessary for models that do not add BOS token to the input
            if (tokens.empty()) {
                res_error(res, format_error_response("Input content cannot be empty", ERROR_TYPE_INVALID_REQUEST));
                return;
            }
        }

        // create and queue the task
        json responses = json::array();
        bool error = false;
        std::unordered_set<int> task_ids;
        {
            std::vector<server_task> tasks;
            for (size_t i = 0; i < tokenized_prompts.size(); i++) {
                server_task task = server_task(SERVER_TASK_TYPE_EMBEDDING);

                task.id            = ctx_server.queue_tasks.get_new_id();
                task.index         = i;
                task.prompt_tokens = server_tokens(tokenized_prompts[i], ctx_server.mctx != nullptr);

                // OAI-compat
                task.params.oaicompat = oaicompat;

                tasks.push_back(std::move(task));
            }

            task_ids = server_task::get_list_id(tasks);
            ctx_server.queue_results.add_waiting_tasks(tasks);
            ctx_server.queue_tasks.post(std::move(tasks));
        }

        // get the result
        ctx_server.receive_multi_results(task_ids, [&](std::vector<server_task_result_ptr> & results) {
            for (auto & res : results) {
                GGML_ASSERT(dynamic_cast<server_task_result_embd*>(res.get()) != nullptr);
                responses.push_back(res->to_json());
            }
        }, [&](const json & error_data) {
            res_error(res, error_data);
            error = true;
        }, req.is_connection_closed);

        ctx_server.queue_results.remove_waiting_task_ids(task_ids);

        if (error) {
            return;
        }

        // write JSON response
        json root = oaicompat == OAICOMPAT_TYPE_EMBEDDING
            ? format_embeddings_response_oaicompat(body, responses, use_base64)
            : json(responses);
        res_ok(res, root);
    };

    const auto handle_embeddings = [&handle_embeddings_impl](const httplib::Request & req, httplib::Response & res) {
        handle_embeddings_impl(req, res, OAICOMPAT_TYPE_NONE);
    };

    const auto handle_embeddings_oai = [&handle_embeddings_impl](const httplib::Request & req, httplib::Response & res) {
        handle_embeddings_impl(req, res, OAICOMPAT_TYPE_EMBEDDING);
    };

    //OWL BEGIN
    const auto handle_chunk_vector = [&ctx_server, &res_error, &res_ok](const httplib::Request & req, httplib::Response & res) {
        const json body = json::parse(req.body);

        // for the shape of input/content, see tokenize_input_prompts()
        std::string aggregation_rule = "average";
        if (body.count("aggregation_rule") != 0) {
            aggregation_rule = body.at("aggregation_rule");
        };
        int aggregation_sample = 300;
        if (body.count("aggregation_sample") != 0) {
            aggregation_sample = body.at("aggregation_sample");
        };

        // create and queue the task
        int n_embd = llama_model_n_embd(ctx_server.model);
        if(aggregation_rule != "average" & aggregation_rule != "last") {
            res_error(res, format_error_response("unsupported aggregation rule: "+aggregation_rule, ERROR_TYPE_INVALID_REQUEST));
            return;
        }
        std::vector<float> embeddings;
        if(aggregation_rule == "last") {
            auto embeddings_ptr = llama_get_embeddings_ith(ctx_server.ctx,-1);
            if (!embeddings_ptr) {
                res_error(res, format_error_response("could not get last token embedding: ", ERROR_TYPE_UNAVAILABLE));
                return;
            }
            embeddings = std::vector<float>(embeddings_ptr, embeddings_ptr + n_embd);
        } else if(aggregation_rule == "average") {
            std::vector<float> average(n_embd);
            for(int i = -1; i >= -aggregation_sample; i--) {
                auto embeddings_ptr = llama_get_embeddings_ith(ctx_server.ctx,i);
                if (!embeddings_ptr) {
                    res_error(res, format_error_response("could not get one of the token embedding in window: ", ERROR_TYPE_UNAVAILABLE));
                    return;
                    }
                for(int j=0;j<n_embd;j++)
                    average[j]+=embeddings_ptr[j];
            }
            for(int j=0;j<n_embd;j++)
                average[j] /= aggregation_sample;
            embeddings = std::move(average);
        }
        //write json response
        json root({{"chunk_embedding", embeddings}});
        res_ok(res, root);

    };

    const auto handle_ingest = [&ctx_server, &res_error, &res_ok](const httplib::Request & req, httplib::Response & res/*, oaicompat_type oaicompat*/) {
        json body = json::parse(req.body);
        if (body.contains("messages")) {
            body = body.at("messages")[body.at("messages").size()-1];
        };

        // for the shape of input/content, see tokenize_input_prompts()
        std::vector<llama_tokens> tokenized_prompts;
        if (body.count("input") != 0) {
            auto prompt = body.at("input");
            tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, prompt, true, true);
        } else if (body.contains("content")) {
            auto prompt = body.at("content");
            tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, prompt, true, true);
        } else if (body.contains("tokens")) {
            llama_tokens tokens = body.at("tokens");
            tokenized_prompts.push_back(tokens);
        } else {
            res_error(res, format_error_response("\"input\" or \"content\" must be provided", ERROR_TYPE_INVALID_REQUEST));
            return;
        }
        
        for (const auto & tokens : tokenized_prompts) {
            // this check is necessary for models that do not add BOS token to the input
            if (tokens.empty()) {
                res_error(res, format_error_response("Input content cannot be empty", ERROR_TYPE_INVALID_REQUEST));
                return;
            }
        }

        // create and queue the task
        bool error = false;
        std::unordered_set<int> task_ids;
        {
            std::vector<server_task> tasks;
            for (size_t i = 0; i < tokenized_prompts.size(); i++) {
                server_task task = server_task(SERVER_TASK_TYPE_EMBEDDING);

                task.id            = ctx_server.queue_tasks.get_new_id();
                task.index         = i;
                task.prompt_tokens = server_tokens(tokenized_prompts[i], ctx_server.mctx != nullptr);

                // OAI-compat
                task.params.oaicompat = OAICOMPAT_TYPE_NONE;

                tasks.push_back(std::move(task));
            }

            task_ids = server_task::get_list_id(tasks);
            ctx_server.queue_results.add_waiting_tasks(tasks);
            ctx_server.queue_tasks.post(std::move(tasks));
        }

        // get the result
        ctx_server.receive_multi_results(task_ids, [&](std::vector<server_task_result_ptr> & results) {
            for (auto & res : results) {
                GGML_ASSERT(dynamic_cast<server_task_result_embd*>(res.get()) != nullptr);
            }
        }, [&](const json & error_data) {
            res_error(res, error_data);
            error = true;
        }, req.is_connection_closed);

        ctx_server.queue_results.remove_waiting_task_ids(task_ids);

        if (error) {
            return;
        }

        // write JSON response
        json root({{"ingestion", "ok"}});
        res_ok(res, root);
    };


    const auto handle_chat_completions_rag = [&ctx_server, &params, &res_error, &handle_completions_impl_with_rag](const httplib::Request & req, httplib::Response & res) {
        LOG_DBG("request: %s\n", req.body.c_str());
        if (ctx_server.params_base.embedding) {
            res_error(res, format_error_response("This server does not support completions. Start it without `--embeddings`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        auto body = json::parse(req.body);
        std::vector<raw_buffer> files;
        json data = oaicompat_completion_params_parse(
            body,
            params.use_jinja,
            params.reasoning_format,
            ctx_server.chat_templates.get(),
            ctx_server.mctx,
            files);

        handle_completions_impl_with_rag(
            SERVER_TASK_TYPE_COMPLETION,
            data,
            files,
            req.is_connection_closed,
            res,
            OAICOMPAT_TYPE_CHAT);
    };

    // same with handle_chat_completions, but without inference part
    //OWL END
    //OWL BEGIN
    const auto handle_chunking = [&ctx_server, &res_error, &res_ok](const httplib::Request & req, httplib::Response & res) {
        const json body = json::parse(req.body);

        // for the shape of input/content, see tokenize_input_prompts()
        json prompt;
        if (body.count("input") != 0) {
            prompt = body.at("input");
        } else if (body.contains("content")) {
            prompt = body.at("content");
        } else {
            std::string err_message = format_error_response("\"input\" or \"content\" must be provided", ERROR_TYPE_INVALID_REQUEST);
            res_error(res, err_message);
            return;
        }

        // Parameters for RAG insertion, potentially parsed from the request body
        bool error = false;
        bool perform_rag_insertion = false;
        std::string documentId;
        ecc256_public_key controller_public_key_to_insert;
        ecc256_private_key recipient_private_key_to_insert;
        std::shared_ptr<rag_database> rag_db;

        if (body.count("rag_connection")) {
            const auto & rag_connection = body.at("rag_connection");
            const std::string db_user = json_value(rag_connection, "user", std::string("postgres"));
            const std::string db_password = json_value(rag_connection, "password", std::string("admin"));
            const std::string db_host = json_value(rag_connection, "host", std::string("localhost"));
            int db_port = json_value(rag_connection, "rag_connection", (int)5432);
            const std::string db_name = json_value(rag_connection, "name", std::string("klave_rag"));
            std::cerr<<"rag_connection provided:" << rag_connection.dump(-1) << std::endl;

            rag_db = create_rag_database(db_host, db_port, db_name);
            try {
                rag_db->connect(db_user, db_password);
            } catch (const std::exception& e) {
                res_error(res, format_error_response(std::string("Database connection error: ") + e.what(), ERROR_TYPE_SERVER));
                error = true;
                return; // Exit the lambda if connection fails
            }
        }
            


        if (body.count("rag_insertion_params")) {
            perform_rag_insertion = true; // Flag to enable RAG insertion
            // 1. Handle document_id or document content
            const auto& rag_params = body.at("rag_insertion_params");
            if (rag_params.count("document_id") != 0) {
                documentId = rag_params.at("document_id").template get<std::string>();
            } else if (rag_params.count("document") != 0) {
                // Document content provided, generate document_id from it
                const auto& document = rag_params.at("document");
                std::string date_str = (document.count("date")==0)?"":document.at("date").template get<std::string>();
                std::string version_str = (document.count("version")==0)?"":document.at("version").template get<std::string>();
                std::string content_type_str = (document.count("content_type")==0)?"":document.at("content_type").template get<std::string>();
                std::string url_str = (document.count("url")==0)?"":document.at("url").template get<std::string>();
                int length = (document.count("length")==0)?0:document.at("length").template get<int>();

                auto document_entry = rag_db->createOrRetrieveDocument(date_str,version_str,content_type_str,url_str,length);
                documentId = document_entry.document_id;
            } else {
                std::string err_message = format_error_response("If \"rag_insertion_params\" is provided, either \"document_id\" or \"document\" must be provided within it.", ERROR_TYPE_INVALID_REQUEST);
                error = true;
                res_error(res, err_message);
                return;
            }

            // 2. Get controller_public_key
            if (rag_params.count("controller_public_key") != 0) {
                try {
                    controller_public_key_to_insert = postgres_client::hex_to_byte_array<33>(rag_params.at("controller_public_key").template get<std::string>());
                } catch (const std::exception& e) {
                    std::string err_message = format_error_response(std::string("Invalid controller_public_key format: ") + e.what(), ERROR_TYPE_INVALID_REQUEST);
                    res_error(res, err_message);
                    return;
                }
            } else {
                res_error(res, format_error_response("\"controller_public_key\" must be provided within \"rag_insertion_params\"", ERROR_TYPE_INVALID_REQUEST));
                return;
            }

            // 3. Get recipient_private_key
            if (rag_params.count("recipient_private_key") != 0) {
                try {
                    auto skey = rag_params.at("recipient_private_key");
                    auto recipient_private_key_to_insert_str = skey.template get<std::string>();
                    recipient_private_key_to_insert = postgres_client::hex_to_byte_array<32>(recipient_private_key_to_insert_str);
                } catch (const std::exception& e) {
                    std::string err_message = format_error_response(std::string("Invalid recipient_private_key format: ") + e.what(), ERROR_TYPE_INVALID_REQUEST);
                    res_error(res, err_message);
                    return;
                }
            } else {
                std::string err_message = format_error_response("\"recipient_private_key\" must be provided within \"rag_insertion_params\"", ERROR_TYPE_INVALID_REQUEST);
                res_error(res, err_message);
                return;
            }
        }

        // Store the original tokenized prompts. We will not modify this vector.
        auto original_tokenized_prompts = tokenize_input_prompts(ctx_server.vocab, prompt, true, true);
        
        for (const auto & tokens : original_tokenized_prompts) {
            if (tokens.empty()) {
                std::string err_message = format_error_response("Input content cannot be empty", ERROR_TYPE_INVALID_REQUEST);
                res_error(res, err_message);
                return;
            }
        }

        // --- NEW CHUNKING AND TASK CREATION LOGIC ---
        // This will hold the actual token chunks that we send to the embedding model
        std::vector<llama_tokens> embedding_chunks;
        std::vector<size_t> original_prompt_indices; // To map embedding_chunks back to original_tokenized_prompts
                                                     // This is if `get_brutal_chunking` still needs the full prompt.
                                                     // If each `embedding_chunk` is now the actual content you want to embed,
                                                     // then you might just need the original string representation of `embedding_chunk`.

        // We will store the original text content for each chunk if needed later for RAG insertion
        // since `get_brutal_chunking` needs `chunk->contents`.
        std::vector<std::string> original_chunk_texts;

        // Define the chunking parameters that will be passed to the server
        const size_t ideal_stream_size = 2048; // Max tokens for one embedding pass (max size of input to llama_decode within the custom task)
        const size_t internal_rag_chunk_size = 300;   // Desired size of final RAG chunks (from get_brutal_chunking)
        const size_t overlap_for_brutal_chunking = internal_rag_chunk_size / 3; // Overlap for get_brutal_chunking
        const size_t step_size_for_brutal_chunking = internal_rag_chunk_size - overlap_for_brutal_chunking; // This is the 'n' in your formula


        for (size_t i = 0; i < original_tokenized_prompts.size(); ++i) {
            const llama_tokens& current_prompt_tokens = original_tokenized_prompts[i];
            if(current_prompt_tokens.size() <= ideal_stream_size)
            {
                embedding_chunks.push_back(current_prompt_tokens);
                continue;
            }

            std::vector<llama_tokens> current_embedding_chunks;
            size_t last_chunk_pos = current_prompt_tokens.size();
            const size_t last_chunk_size = (last_chunk_pos -internal_rag_chunk_size)%overlap_for_brutal_chunking;
            if(last_chunk_size)
            {
                current_embedding_chunks.push_back(llama_tokens(std::begin(current_prompt_tokens)+(last_chunk_pos-last_chunk_size),std::end(current_prompt_tokens)));
                last_chunk_pos -= last_chunk_size;
            }
            while(last_chunk_pos > ideal_stream_size)
            {
                current_embedding_chunks.push_back(llama_tokens(std::begin(current_prompt_tokens)+(last_chunk_pos-step_size_for_brutal_chunking),std::begin(current_prompt_tokens)+last_chunk_pos));
                last_chunk_pos -= step_size_for_brutal_chunking;
            }
            if(last_chunk_pos)
            {
                current_embedding_chunks.push_back(llama_tokens(std::begin(current_prompt_tokens),std::begin(current_prompt_tokens)+last_chunk_pos));
                last_chunk_pos = 0;
            }
            std::reverse(current_embedding_chunks.begin(), current_embedding_chunks.end());
            for(auto & chunk : current_embedding_chunks)
                embedding_chunks.push_back(std::move(chunk));
        }
        // Now, queue tasks based on the `embedding_chunks`
        std::unordered_set<int> task_ids_to_wait_for; // Use a new set for unique task IDs
        std::vector<server_task> tasks;
        

        for (size_t i = 0; i < embedding_chunks.size(); ++i) {
            server_task task = server_task(SERVER_TASK_TYPE_EMBEDDING);
            task.id            = ctx_server.queue_tasks.get_new_id(); // Each task gets a UNIQUE ID
            task.index         = i; // This index refers to the embedding_chunks vector
            task.prompt_tokens = server_tokens(embedding_chunks[i], ctx_server.mctx != nullptr);
            task.params.oaicompat = OAICOMPAT_TYPE_NONE;

            task_ids_to_wait_for.insert(task.id); // Add the new unique ID
            tasks.push_back(std::move(task));
        }

        ctx_server.queue_results.add_waiting_tasks(tasks);
        ctx_server.queue_tasks.post(std::move(tasks));
        // --- END NEW CHUNKING AND TASK CREATION LOGIC ---


        // get the result
        json responses = json::array();
        // current_prompt_index is no longer directly used for indexing `tokenized_prompts`
        // as `results` will correspond to `embedding_chunks`.
        // We'll use the mapping from `task_id_to_chunk_info`



        std::cerr<< "about to receive tasks dispatched" << std::endl;

        // --- MODIFIED RECEIVE_MULTI_RESULTS LAMBDA ---
        int task_n = 0;
        auto all_prompt = original_tokenized_prompts[0];
        std::vector<std::vector<float>> all_embedding;
        int position = 0;
        bool is_last_block = false;
        ctx_server.receive_streamed_results(task_ids_to_wait_for, [&](server_task_result_ptr & result) {
            std::cerr<< "receiving task#" << task_n << " results" << std::endl;
            if (error) {
                std::cerr<<"previous error, returning" <<std::endl;
                return false; // Exit the lambda if a pre-existing error occurred
            }

            // The results vector is not necessarily ordered by task.index or original prompt order.
            // You should iterate through results and use their task IDs to retrieve original chunk info.
            if (error)
                return false;

            auto p_embeddings = dynamic_cast<server_task_result_embd*>(result.get());
            GGML_ASSERT(p_embeddings != nullptr);
            all_embedding.insert(all_embedding.end(),std::begin(p_embeddings->embedding),std::end(p_embeddings->embedding));
            if(all_embedding.size() > all_prompt.size())
                {
                    //std::cerr<<"all_embedding(" << all_embedding.size() << ")/all_prompts(" << all_prompt.size() << ") size mismatch!" <<std::endl;
                    return false;
                }
            auto embeddinz_sz = all_embedding[0].size();
            while(true)
            {
                int chunk_sz = 0;
                if (all_embedding.size() >= internal_rag_chunk_size)
                    chunk_sz = internal_rag_chunk_size;
                else if ((task_n+1) == embedding_chunks.size())
                    {
                        chunk_sz = all_embedding.size();
                        is_last_block = true;
                    }
                else
                    return false;
                std::vector<float> chunk_vector(embeddinz_sz);
                for (int32_t j = 0; j < chunk_sz; ++j)
                    for(uint32_t dim = 0; dim < embeddinz_sz; ++dim)
                        chunk_vector[dim] += all_embedding[j][dim];
                for(uint32_t dim = 0; dim < embeddinz_sz; ++dim)
                    chunk_vector[dim] /= (float)chunk_sz;
                auto contents = common_detokenize(ctx_server.ctx,llama_tokens{std::begin(all_prompt),std::begin(all_prompt)+chunk_sz},false);

                std::vector<uint8_t> chunk_contents_bytes(std::begin(contents),std::end(contents));
                try {
                    LOG_INF("Attempting insertion of chunk with vector.sz = %d for documentId: [%i->%i]@%s\n", (int)chunk_vector.size(), position, position+chunk_sz,documentId.c_str());
                    rag_db->insertRagEntry(
                        documentId,
                        chunk_vector,
                        chunk_contents_bytes,
                        controller_public_key_to_insert,
                        recipient_private_key_to_insert
                    );
                } catch (const std::exception& e) {
                    std::cerr << "Database insertion failed for documentId " << documentId << ": " << e.what() << std::endl;
                    res_error(res, format_error_response(std::string("Database insertion error: ") + e.what(), ERROR_TYPE_SERVER));
                    error = true;
                    return false;
                }
                responses.push_back(json {{"index", position},{"tokens_evaluated", chunk_sz}});
                if(is_last_block)
                    return  true;
                position += step_size_for_brutal_chunking;
                all_embedding.erase(std::begin(all_embedding),std::begin(all_embedding)+step_size_for_brutal_chunking);
                all_prompt.erase(std::begin(all_prompt),std::begin(all_prompt)+step_size_for_brutal_chunking);
            }
        if (rag_db) {
                try {
                    rag_db->disconnect();
                } catch (const std::exception& e) {
                    std::cerr << "Database disconnection failed: " << e.what() << std::endl;
                }
            }
        }, [&](const json & error_data) {
            res_error(res, error_data);
            error = true;
        }, req.is_connection_closed);

        ctx_server.queue_results.remove_waiting_task_ids(task_ids_to_wait_for); // Use the new set of task IDs

        if (error) {
            return;
        }

        json root = json::object();
        root["chunks"] = responses;
        res_ok(res, root);
    };
    // OWL END

#define ERROR_TYPE_INTERNAL_SERVER_ERROR ERROR_TYPE_INVALID_REQUEST
const auto handle_rag_db_admin = [&ctx_server, &res_error, &res_ok](const httplib::Request& req, httplib::Response& res) {
    try {
        // Request Body Structure:
        // {
        //     "action": "create" | "drop" | "exists" | "create_document" | "delete_document", // Required.
        //     "rag_connection": {
        //          "host": "your_rag_db_host",                                             // Optional defaults to "localhost".
        //          "port": your_rag_db_port,                                                // Optional defaults to 5432.
        //          "name": "your_rag_db_name",                                             // Optional defaults to "klave_rag".
        //          "user": "your_db_user",                                                 // Optional defaults to "postgres".
        //          "password": "your_db_password",                                         // Optional defaults to "admin".
        //          }
        //     // For "create_document" action:
        //     "date": "YYYY-MM-DD",
        //     "version": "v1.0",
        //     "content_type": "text/plain",
        //     "url": "http://example.com/doc1",
        //     "length": 1234,
        //
        //     // For "delete_document" action:
        //     "document_id": 123
        // }
        const json body = json::parse(req.body);

        // 1. Validate the request body:
        if (!body.contains("action") || !body.contains("rag_connection") ) {
            res_error(res, format_error_response("Missing required parameters: action, rag_connection", ERROR_TYPE_INVALID_REQUEST));
            return;
        }
        const std::string action = body["action"].get<std::string>();
        const auto & rag_connection = body["rag_connection"];

        const std::string db_user = json_value(rag_connection, "user", std::string("postgres"));
        const std::string db_password = json_value(rag_connection, "password", std::string("admin"));
        const std::string db_host = json_value(rag_connection, "host", std::string("localhost"));
        int db_port = json_value(rag_connection, "port", (int)5432);
        const std::string db_name = json_value(rag_connection, "name", std::string("klave_rag"));

        // Create and connect the database instance
        auto rag_db = create_rag_database(db_host, db_port, db_name);
        rag_db->connect(db_user, db_password); // Connect using the set credentials

        // 2. Perform the requested action:
        if (action == "create") {
            if (ctx_server.model == nullptr) {
                
                res_error(res, format_error_response("Could not get embedding size from model. Model is not loaded yet.", ERROR_TYPE_INTERNAL_SERVER_ERROR));
                 return;
            }
            int embeddingSize = llama_model_n_embd(ctx_server.model);
            if (embeddingSize == 0) {
                 res_error(res, format_error_response("Could not get embedding size from model. Model does not support embeddings.", ERROR_TYPE_INTERNAL_SERVER_ERROR));
                 return;
            }
            rag_db->createSchema(embeddingSize);
            res_ok(res, json({{"message", "Database schema created successfully"}}));
        } else if (action == "drop") {
            rag_db->destroySchema();
            res_ok(res, json({{"message", "Database schema dropped successfully"}}));
        } else if (action == "exists") {
            bool exists = rag_db->hasSchema();
            res_ok(res, json({{"exists", exists}}));
        } else if (action == "create_document") {
            // Validate required parameters for creating a document
            if (!body.contains("date") || !body.contains("version") ||
                !body.contains("content_type") || !body.contains("url") || !body.contains("length")) {
                res_error(res, format_error_response("Missing required parameters for create_document: date, version, content_type, url, length", ERROR_TYPE_INVALID_REQUEST));
                return;
            }

            const std::string docDate = body["date"].get<std::string>();
            const std::string docVersion = body["version"].get<std::string>();
            const std::string docContentType = body["content_type"].get<std::string>();
            const std::string docUrl = body["url"].get<std::string>();
            const int docLength = body["length"].get<int>();

            auto newDoc = rag_db->createOrRetrieveDocument(docDate, docVersion, docContentType, docUrl, docLength);
            res_ok(res, json({
                {"message", "Document processed successfully"},
                {"document_id", newDoc.document_id},
                {"url", newDoc.url}
            }));
        } else if (action == "delete_document") {
            // Validate required parameters for deleting a document
            if (!body.contains("document_id")) {
                res_error(res, format_error_response("Missing required parameter for delete_document: document_id", ERROR_TYPE_INVALID_REQUEST));
                return;
            }

            //const std::string documentId = dehex_string(body["document_id"].get<std::string>());
            const std::string documentId = body["document_id"].get<std::string>();
            rag_db->deleteDocument(documentId);
            res_ok(res, json({{"message", "Document deletion attempted"}}));
        }
        else {
            res_error(res, format_error_response("Invalid action. Must be 'create', 'drop', 'exists', 'create_document', or 'delete_document'.", ERROR_TYPE_INVALID_REQUEST));
        }
    } catch (const std::exception& e) {
        // Handle database errors using res_error
        res_error(res, format_error_response(std::string("Database error: ") + e.what(), ERROR_TYPE_INTERNAL_SERVER_ERROR));
    }
};

const auto provide_quote = [&ctx_server, &params, &res_error, &res_ok](const httplib::Request& req, httplib::Response& res) {
    try {
        std::cerr << "requesting attested seld-signed certificate" << std::endl;
        std::unique_ptr<EVP_PKEY, decltype(EVP_PKEY_free)*> pkey(self_signed::load_private_key(params.ssl_file_key), EVP_PKEY_free);
        if (pkey == nullptr)
        {
            std::cerr << "requesting attested seld-signed certificate: private key is null" << std::endl;
            res_error(res, format_error_response("Failed to open private file: " + params.ssl_file_key, ERROR_TYPE_INTERNAL_SERVER_ERROR));
            return;
        }
        std::string certificate;
        if(!self_signed::createSelfSignedTdxCertificateAsString(pkey.get(), certificate))
        {
            std::cerr << "requesting attested seld-signed certificate: createSelfSignedTdxCertificateAsString failed" << std::endl;
            res_error(res, format_error_response("Failed to generate attested certificate" , ERROR_TYPE_INTERNAL_SERVER_ERROR));
            return;
        }
        res_ok(res, json({
            {"message", "TDX Attested Certificate generated successfully"},
            {"certificate_pem", certificate} // Embed the PEM string in JSON
        }));

        std::cout << "Successfully served TDX Attested Certificate from private key file: " << params.ssl_file_key << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error retrieving certificate: " << e.what() << std::endl;
        // Catch any other exceptions during file reading or JSON parsing
        res_error(res, format_error_response(std::string("Error retrieving certificate: ") + e.what(), ERROR_TYPE_INTERNAL_SERVER_ERROR));
    }
};


const auto handle_model_list_models = [&ctx_server, &res_error, &res_ok, &params](const json & body, const httplib::Request& req, httplib::Response& res) {
    try {
        std::string fullyQualifiedFilePath = params.model.path;

        // 1. Get the directory of the provided file path
        fs::path filePath(fullyQualifiedFilePath);
        fs::path directoryPath = filePath.parent_path();

        // Check if the directory exists
        if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
            std::cerr << "Error: Directory '" << directoryPath << "' does not exist or is not a directory." << std::endl;
            return;
        }
        std::cout << "Searching for .gguf files in: " << directoryPath << std::endl;

        std::vector<std::string> ggufFiles;
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            // Check if it's a regular file
            if (fs::is_regular_file(entry.status())) {
                // Get the file extension
                std::string extension = entry.path().extension().string();

                // Convert extension to lowercase for case-insensitive comparison (optional but good practice)
                std::transform(extension.begin(), extension.end(), extension.begin(),
                               [](unsigned char c){ return std::tolower(c); });

                // Check if the extension is ".gguf"
                if (extension == ".gguf") {
                    ggufFiles.push_back(entry.path().filename().string());
                }
            }
        }
        json models = {
            {"models", ggufFiles}
            };
        res_ok(res, models);


    } catch (const std::exception& e) {
        // Handle database errors using res_error
        res_error(res, format_error_response(std::string("Internal error: ") + e.what(), ERROR_TYPE_INTERNAL_SERVER_ERROR));
    }
};

const auto handle_model_change_model = [&ctx_server, &res_error, &res_ok, &state, &params](const json & body, const httplib::Request& req, httplib::Response& res) {
    try {
         // 1. Validate the request body:
        if (!body.contains("model")) {
            res_error(res, format_error_response("Missing required parameters: model", ERROR_TYPE_INVALID_REQUEST));
            return;
        }
        std::string new_model_name = body.at("model");

        std::string fullyQualifiedFilePath = params.model.path;
        // 1. Get the directory of the provided file path
        fs::path filePath(fullyQualifiedFilePath);
        fs::path directoryPath = filePath.parent_path();

        // Check if the directory exists
        if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
            std::cerr << "Error: Directory '" << directoryPath << "' does not exist or is not a directory." << std::endl;
            res_error(res, format_error_response("Internal Error (File System directory for present model could not be found)", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        // 2. Construct the full path for the potential new file with .gguf extension
        //    We use operator/ to correctly concatenate path components.
        fs::path targetFilePath = directoryPath / new_model_name ;

        // 3. Check if the target file exists and is a regular file
        if (!fs::exists(targetFilePath) && fs::is_regular_file(targetFilePath)) {
            std::cerr << "Not found: File '" << targetFilePath << "' does not exist or is not a regular file." << std::endl;
            res_error(res, format_error_response("Internal Error (model could not be located)", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        params.model.path = targetFilePath;
        state.store(SERVER_STATE_LOADING_MODEL);
        LOG_INF("%s: reloading model\n", __func__);

        if (!ctx_server.load_model(params)) {
            LOG_ERR("%s: exiting due to model loading error\n", __func__);
            res_error(res, format_error_response("Internal Error (model could not be loaded)", ERROR_TYPE_INVALID_REQUEST));
            return;
        }
    ctx_server.slots.clear();
    ctx_server.init();
    state.store(SERVER_STATE_READY);

    LOG_INF("%s: model loaded\n", __func__);


    json models = {
        {"success", true},
        {"new_model_path", (std::string)targetFilePath}
        };
        res_ok(res, models);

    } catch (const std::exception& e) {
        // Handle database errors using res_error
        res_error(res, format_error_response(std::string("Internal error: ") + e.what(), ERROR_TYPE_INTERNAL_SERVER_ERROR));
    }
};


const auto handle_model_request = [&ctx_server, &res_error, &res_ok, &handle_model_list_models, &handle_model_change_model](const httplib::Request& req, httplib::Response& res) {
    try {
        const json body = json::parse(req.body);

        // 1. Validate the request body:
        if (!body.contains("action")) {
            res_error(res, format_error_response("Missing required parameters: action", ERROR_TYPE_INVALID_REQUEST));
            return;
        }
        const std::string action = body["action"].get<std::string>();
        if(action == "list-models")
            handle_model_list_models(body,req,res);
        else if(action == "change-model")
            handle_model_change_model(body,req,res);
        else {
            res_error(res, format_error_response("Invalid action" + action + " Must be 'list-models' or 'change-model'.", ERROR_TYPE_INVALID_REQUEST));
        }
    } catch (const std::exception& e) {
        // Handle database errors using res_error
        res_error(res, format_error_response(std::string("Internal error: ") + e.what(), ERROR_TYPE_INTERNAL_SERVER_ERROR));
    }
};
//OWL END



    const auto handle_rerank = [&ctx_server, &res_error, &res_ok](const httplib::Request & req, httplib::Response & res) {
        if (!ctx_server.params_base.reranking || ctx_server.params_base.embedding) {
            res_error(res, format_error_response("This server does not support reranking. Start it with `--reranking` and without `--embedding`", ERROR_TYPE_NOT_SUPPORTED));
            return;
        }

        const json body = json::parse(req.body);

        // TODO: implement
        //int top_n = 1;
        //if (body.count("top_n") != 1) {
        //    top_n = body.at("top_n");
        //} else {
        //    res_error(res, format_error_response("\"top_n\" must be provided", ERROR_TYPE_INVALID_REQUEST));
        //    return;
        //}

        // if true, use TEI API format, otherwise use Jina API format
        // Jina: https://jina.ai/reranker/
        // TEI: https://huggingface.github.io/text-embeddings-inference/#/Text%20Embeddings%20Inference/rerank
        bool is_tei_format = body.contains("texts");

        json query;
        if (body.count("query") == 1) {
            query = body.at("query");
            if (!query.is_string()) {
                res_error(res, format_error_response("\"query\" must be a string", ERROR_TYPE_INVALID_REQUEST));
                return;
            }
        } else {
            res_error(res, format_error_response("\"query\" must be provided", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        std::vector<std::string> documents = json_value(body, "documents",
                                             json_value(body, "texts", std::vector<std::string>()));
        if (documents.empty()) {
            res_error(res, format_error_response("\"documents\" must be a non-empty string array", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        llama_tokens tokenized_query = tokenize_input_prompts(ctx_server.vocab, query, /* add_special */ false, true)[0];

        // create and queue the task
        json responses = json::array();
        bool error = false;
        std::unordered_set<int> task_ids;
        {
            std::vector<server_task> tasks;
            auto tokenized_docs = tokenize_input_prompts(ctx_server.vocab, documents, /* add_special */ false, true);
            tasks.reserve(tokenized_docs.size());
            for (size_t i = 0; i < tokenized_docs.size(); i++) {
                auto tmp = format_rerank(ctx_server.vocab, tokenized_query, tokenized_docs[i]);
                server_task task   = server_task(SERVER_TASK_TYPE_RERANK);
                task.id            = ctx_server.queue_tasks.get_new_id();
                task.index         = i;
                task.prompt_tokens = server_tokens(tmp, ctx_server.mctx != nullptr);
                tasks.push_back(std::move(task));
            }

            task_ids = server_task::get_list_id(tasks);
            ctx_server.queue_results.add_waiting_tasks(tasks);
            ctx_server.queue_tasks.post(std::move(tasks));
        }

        ctx_server.receive_multi_results(task_ids, [&](std::vector<server_task_result_ptr> & results) {
            for (auto & res : results) {
                GGML_ASSERT(dynamic_cast<server_task_result_rerank*>(res.get()) != nullptr);
                responses.push_back(res->to_json());
            }
        }, [&](const json & error_data) {
            res_error(res, error_data);
            error = true;
        }, req.is_connection_closed);

        if (error) {
            return;
        }

        // write JSON response
        json root = format_response_rerank(
            body,
            responses,
            is_tei_format,
            documents);

        res_ok(res, root);
    };

    const auto handle_lora_adapters_list = [&](const httplib::Request &, httplib::Response & res) {
        json result = json::array();
        const auto & loras = ctx_server.params_base.lora_adapters;
        for (size_t i = 0; i < loras.size(); ++i) {
            auto & lora = loras[i];
            result.push_back({
                {"id", i},
                {"path", lora.path},
                {"scale", lora.scale},
            });
        }
        res_ok(res, result);
        res.status = 200; // HTTP OK
    };

    const auto handle_lora_adapters_apply = [&](const httplib::Request & req, httplib::Response & res) {
        const json body = json::parse(req.body);
        if (!body.is_array()) {
            res_error(res, format_error_response("Request body must be an array", ERROR_TYPE_INVALID_REQUEST));
            return;
        }

        int task_id = ctx_server.queue_tasks.get_new_id();
        {
            server_task task(SERVER_TASK_TYPE_SET_LORA);
            task.id = task_id;
            task.set_lora = parse_lora_request(ctx_server.params_base.lora_adapters, body);
            ctx_server.queue_results.add_waiting_task_id(task_id);
            ctx_server.queue_tasks.post(std::move(task));
        }

        // get the result
        server_task_result_ptr result = ctx_server.queue_results.recv(task_id);
        ctx_server.queue_results.remove_waiting_task_id(task_id);

        if (result->is_error()) {
            res_error(res, result->to_json());
            return;
        }

        GGML_ASSERT(dynamic_cast<server_task_result_apply_lora*>(result.get()) != nullptr);
        res_ok(res, result->to_json());
    };

    //
    // Router
    //

    if (!params.webui) {
        LOG_INF("Web UI is disabled\n");
    } else {
        // register static assets routes
        if (!params.public_path.empty()) {
            // Set the base directory for serving static files
            bool is_found = svr->set_mount_point("/", params.public_path);
            if (!is_found) {
                LOG_ERR("%s: static assets path not found: %s\n", __func__, params.public_path.c_str());
                return 1;
            }
        } else {
            // using embedded static index.html
            svr->Get("/", [](const httplib::Request & req, httplib::Response & res) {
                if (req.get_header_value("Accept-Encoding").find("gzip") == std::string::npos) {
                    res.set_content("Error: gzip is not supported by this browser", "text/plain");
                } else {
                    res.set_header("Content-Encoding", "gzip");
                    // COEP and COOP headers, required by pyodide (python interpreter)
                    res.set_header("Cross-Origin-Embedder-Policy", "require-corp");
                    res.set_header("Cross-Origin-Opener-Policy", "same-origin");
                    res.set_content(reinterpret_cast<const char*>(index_html_gz), index_html_gz_len, "text/html; charset=utf-8");
                }
                return false;
            });
        }
    }

    // register API routes
    svr->Get ("/health",              handle_health); // public endpoint (no API key check)
    svr->Get ("/metrics",             handle_metrics);
    svr->Get ("/props",               handle_props);
    svr->Post("/props",               handle_props_change);
    svr->Post("/api/show",            handle_api_show);
    svr->Get ("/models",              handle_models); // public endpoint (no API key check)
    svr->Get ("/v1/models",           handle_models); // public endpoint (no API key check)
    svr->Post("/completion",          handle_completions); // legacy
    svr->Post("/completions",         handle_completions);
    svr->Post("/v1/completions",      handle_completions_oai);
    svr->Post("/chat/completions",    handle_chat_completions);
    svr->Post("/v1/chat/completions", handle_chat_completions);
    svr->Post("/v1/chat/completions-rag", handle_chat_completions_rag); //OWL WAS HERE
    svr->Post("/infill",              handle_infill);
    svr->Post("/embedding",           handle_embeddings); // legacy
    svr->Post("/embeddings",          handle_embeddings);
    svr->Post("/v1/embeddings",       handle_embeddings_oai);
    svr->Post("/rerank",              handle_rerank);
    svr->Post("/reranking",           handle_rerank);
    svr->Post("/v1/rerank",           handle_rerank);
    svr->Post("/v1/reranking",        handle_rerank);
    svr->Post("/tokenize",            handle_tokenize);
    svr->Post("/detokenize",          handle_detokenize);
    svr->Post("/apply-template",      handle_apply_template);
    svr->Post("/ingest",              handle_ingest); // OWL WAS HERE
    svr->Post("/compute-chunk-vector",handle_chunk_vector); // OWL WAS HERE
    svr->Post("/chunking",            handle_chunking); //OWL WAS HERE
    svr->Post("/rag_db_admin",        handle_rag_db_admin); //OWL WAS HERE
    svr->Post("/provide-quote",       provide_quote); //OWL WAS HERE
    svr->Post("/model-action",        handle_model_request); //OWL WAS HERE
    
    // LoRA adapters hotswap
    svr->Get ("/lora-adapters",       handle_lora_adapters_list);
    svr->Post("/lora-adapters",       handle_lora_adapters_apply);
    // Save & load slots
    svr->Get ("/slots",               handle_slots);
    svr->Post("/slots/:id_slot",      handle_slots_action);

    //
    // Start the server
    //
    if (params.n_threads_http < 1) {
        // +2 threads for monitoring endpoints
        params.n_threads_http = std::max(params.n_parallel + 2, (int32_t) std::thread::hardware_concurrency() - 1);
    }
    log_data["n_threads_http"] =  std::to_string(params.n_threads_http);
    svr->new_task_queue = [&params] { return new httplib::ThreadPool(params.n_threads_http); };

    // clean up function, to be called before exit
    auto clean_up = [&svr, &ctx_server]() {
        SRV_INF("%s: cleaning up before exit...\n", __func__);
        svr->stop();
        ctx_server.queue_results.terminate();
        llama_backend_free();
    };

    bool was_bound = false;
    if (string_ends_with(std::string(params.hostname), ".sock")) {
        LOG_INF("%s: setting address family to AF_UNIX\n", __func__);
        svr->set_address_family(AF_UNIX);
        // bind_to_port requires a second arg, any value other than 0 should
        // simply get ignored
        was_bound = svr->bind_to_port(params.hostname, 8080);
    } else {
        LOG_INF("%s: binding port with default address family\n", __func__);
        // bind HTTP listen port
        if (params.port == 0) {
            int bound_port = svr->bind_to_any_port(params.hostname);
            if ((was_bound = (bound_port >= 0))) {
                params.port = bound_port;
            }
        } else {
            was_bound = svr->bind_to_port(params.hostname, params.port);
        }
    }

    if (!was_bound) {
        LOG_ERR("%s: couldn't bind HTTP server socket, hostname: %s, port: %d\n", __func__, params.hostname.c_str(), params.port);
        clean_up();
        return 1;
    }

    // run the HTTP server in a thread
    std::thread t([&]() { svr->listen_after_bind(); });
    svr->wait_until_ready();

    LOG_INF("%s: HTTP server is listening, hostname: %s, port: %d, http threads: %d\n", __func__, params.hostname.c_str(), params.port, params.n_threads_http);

    // load the model
    LOG_INF("%s: loading model\n", __func__);

    if (!ctx_server.load_model(params)) {
        clean_up();
        t.join();
        LOG_ERR("%s: exiting due to model loading error\n", __func__);
        return 1;
    }

    ctx_server.init();
    state.store(SERVER_STATE_READY);

    LOG_INF("%s: model loaded\n", __func__);

    // print sample chat example to make it clear which template is used
    LOG_INF("%s: chat template, chat_template: %s, example_format: '%s'\n", __func__,
        common_chat_templates_source(ctx_server.chat_templates.get()),
        common_chat_format_example(ctx_server.chat_templates.get(), ctx_server.params_base.use_jinja).c_str());

    ctx_server.queue_tasks.on_new_task([&ctx_server](server_task && task) {
        ctx_server.process_single_task(std::move(task));
    });

    ctx_server.queue_tasks.on_update_slots([&ctx_server]() {
        ctx_server.update_slots();
    });

    shutdown_handler = [&](int) {
        // this will unblock start_loop()
        ctx_server.queue_tasks.terminate();
    };

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
    struct sigaction sigint_action;
    sigint_action.sa_handler = signal_handler;
    sigemptyset (&sigint_action.sa_mask);
    sigint_action.sa_flags = 0;
    sigaction(SIGINT, &sigint_action, NULL);
    sigaction(SIGTERM, &sigint_action, NULL);
#elif defined (_WIN32)
    auto console_ctrl_handler = +[](DWORD ctrl_type) -> BOOL {
        return (ctrl_type == CTRL_C_EVENT) ? (signal_handler(SIGINT), true) : false;
    };
    SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(console_ctrl_handler), true);
#endif

    LOG_INF("%s: server is listening on http://%s:%d - starting the main loop\n", __func__, params.hostname.c_str(), params.port);

    // this call blocks the main thread until queue_tasks.terminate() is called
    ctx_server.queue_tasks.start_loop();

    clean_up();
    t.join();

    return 0;
}
