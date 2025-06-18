/* Inference for Llama-2 Transformer model in pure C */
 
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <memory>
#include <string.h>
#include <common.h>
#include <chat.h>
#include <atomic>//OWL WAS HERE
 
#define FILE int
 
#include <llama.h>
#include <llama-cpp.h>
 
#undef FILE
 
//#include <platform/platform.h>//OWL WAS HERE
//#include <sdk_v3/sdk.h>//OWL WAS HERE
 
//#include <secret_llama/llm_engine.h>//OWL WAS HERE
#include <llm_engine.h>//OWL WAS HERE
 
namespace secret_llama
{
inline namespace v1_0
{

    //OWL WAS HERE - BEGIN
    llama_cpp_loaded_model::llama_cpp_loaded_model(llama_model* model, const llm_model& model_config)
        : loaded_model(model_config), model_(model)
    {
    }
    llama_cpp_loaded_model::~llama_cpp_loaded_model()
    {
        if (model_)
            llama_model_free(model_);
    }

    llama_model* llama_cpp_loaded_model::get() const { return model_; }
    int32_t llama_cpp_loaded_model::get_n_embd() const
    {
        return model_ ? llama_n_embd(model_) : 0;
    }
 
    void context_deleter::operator()(llama_context* ptr) const
    {
        if (ptr)
            llama_free(ptr);
    }

    void sampler_deleter::operator()(llama_sampler* ptr) const
    {
        if (ptr)
            llama_sampler_free(ptr);
    }
    //OWL WAS HERE - END

// OWL WAS HERE
//         llama_cpp_context::llama_cpp_context(uint64_t seed, std::shared_ptr<llama_cpp_loaded_model> model, const json::v1::map& overrides,
//             uint64_t context_id)
//             : seed_(seed), model_(model), context_id_(context_id)
//         {
//             // APP_DEBUG("[CTX {}] CONSTRUCTOR: Creating new context.", std::to_string(context_id_));//OWL WAS HERE
//             // deep copy
// /* OWL WAS HERE
//             config_ = model->get_config().engine_config;
//             for (auto& item : overrides)
//             {
//                 // override the seed value if requested by the client
//                 if (item.first == "seed")
//                     seed_ = item.second.get<double>();
//                 else
//                     config_[item.first] = item.second;
//             }
// */
//         }
 
        llama_cpp_context::~llama_cpp_context()
        {
            // APP_DEBUG("[CTX {}] DESTRUCTOR: Freeing context.", std::to_string(context_id_));//OWL WAS HERE
        }
 
        error_types llama_cpp_context::init()
        {
            // APP_DEBUG("[CTX {}] Initializing context...", std::to_string(context_id_));//OWL WAS HERE
            try
            {
                // initialize the context
                ctx_params_ = llama_context_default_params();
                ctx_params_.n_ctx = ctx_params_.n_batch;
 
                // just the basics for now
                /*OWL WAS HERE
                {
                    {
                        auto it = config_.find("n_ctx");
                        if (it != config_.end())
                        {
                            ctx_params_.n_ctx = it->second.convert_to_int<uint32_t>();
                        }
                        // APP_INFO("setting n_ctx = {}", ctx_params_.n_ctx);//OWL WAS HERE
                    }
                    {
                        auto it = config_.find("n_batch");
                        if (it != config_.end())
                        {
                            ctx_params_.n_batch = it->second.convert_to_int<uint32_t>();
                        }
                        // APP_INFO("setting n_batch = {}", ctx_params_.n_batch);//OWL WAS HERE
                    }
 
                    // these thread parameters should typically not be used in production
                    {
                        auto it = config_.find("n_threads");
                        if (it != config_.end())
                        {
                            ctx_params_.n_threads = it->second.convert_to_int<int32_t>();
                        }
                        else
                        {
                            ctx_params_.n_threads = POOL_THREADS;
                        }
                        // APP_INFO("setting n_threads = {}", ctx_params_.n_threads);//OWL WAS HERE
                    }
                    {
                        auto it = config_.find("n_threads_batch");
                        if (it != config_.end())
                        {
                            ctx_params_.n_threads_batch = it->second.convert_to_int<int32_t>();
                        }
                        else
                        {
                            ctx_params_.n_threads_batch = POOL_THREADS;
                        }
                        // APP_INFO("setting n_threads_batch = {}", ctx_params_.n_threads_batch);//OWL WAS HERE
                    }
                }
                OWL WAS HERE */ 
                ctx_ = std::unique_ptr<llama_context, context_deleter>(llama_init_from_model(model_->get(), ctx_params_));
                if (!ctx_)
                {
                    // APP_ERR("[CTX {}] failed to create the llama_context", std::to_string(context_id_));//OWL WAS HERE
                    return error_types::INVALID_CONTEXT;
                }
 
                // APP_INFO("calculated_kv_cache_size {}", std::to_string(calculate_kv_cache_size(ctx_.get(), ctx_params_)));//OWL WAS HERE
 
                // initialize the sampler chain
                sampler_ = std::unique_ptr<llama_sampler, sampler_deleter>(
                    llama_sampler_chain_init(llama_sampler_chain_default_params()));
 
                llama_sampler_chain_add(sampler_.get(), llama_sampler_init_dist(seed_));
 
/*OWL WAS HERE
                {
                    auto it = config_.find("min_p");
                    if (it != config_.end())
                    {
                        auto min_p = it->second.get<float>();
                        llama_sampler_chain_add(sampler_.get(), llama_sampler_init_min_p(min_p, 1));
                        // APP_INFO("setting min_p = {}", min_p);//OWL WAS HERE
                    }
                }
 
                {
                    auto it = config_.find("topp");
                    if (it != config_.end())
                    {
                        auto top_p = it->second.get<float>();
                        llama_sampler_chain_add(sampler_.get(), llama_sampler_init_top_p(top_p, 1));
                        // APP_INFO("setting top_p = {}", top_p);//OWL WAS HERE
                    }
                }
 
                {
                    auto it = config_.find("temperature");
                    if (it != config_.end())
                    {
                        auto temperature = it->second.get<float>();
                        llama_sampler_chain_add(sampler_.get(), llama_sampler_init_temp(temperature));
                        // APP_INFO("setting min_p = {}", temperature);//OWL WAS HERE
                    }
                }
 
                // (sampler setup code omitted for brevity but would be here)
 
*/

                std::string chat_template_str;
/* OWL WAS HERE
                {
                    auto it = config_.find("template");
                    if (it != config_.end())
                    {
                        chat_template_str = it->second.get<std::string>();
                        // APP_INFO("template = {}", chat_template_str);//OWL WAS HERE
                    }
                }
*/

                chat_templates_ = common_chat_templates_init(model_->get(), chat_template_str);
                // APP_INFO("[CTX {}] Chat template: is explicit {}, value: \"{}\"", std::to_string(context_id_),
                //     common_chat_templates_was_explicit(chat_templates_.get()) ? "true" : "false",
                //     common_chat_templates_source(chat_templates_.get()));//OWL WAS HERE
 
                formatted_.resize(llama_n_ctx(ctx_.get()));
                // APP_DEBUG("[CTX {}] Initialization complete. Context size: {}", std::to_string(context_id_),
                //     llama_n_ctx(ctx_.get()));//OWL WAS HERE
                return error_types::OK;
            }
            catch (const std::exception& e)
            {
                // APP_ERR("[CTX {}] llamacpp exception thrown: {}", std::to_string(context_id_), e.what());//OWL WAS HERE
                return error_types::EXCEPTION_THROWN;
            }
        }
 
        /**
         * @brief Calculates the total memory size of the KV cache for a given llama_context.
         *
         * This function determines the size in bytes of the Key-Value cache (K-cache and V-cache)
         * by inspecting the context's configuration. It uses the recommended public `llama.h` API
         * to ensure future compatibility.
         *
         * @param ctx A constant pointer to the llama_context to be inspected.
         * @return The total size of the KV cache in bytes as a size_t. Returns 0 if the context is invalid.
         */
        size_t llama_cpp_context::calculate_kv_cache_size(const llama_context* ctx, const llama_context_params& ctx_params)
        {
            if (!ctx)
            {
                return 0;
            }
 
            // 1. Get the model instance from the context to query its parameters.
            const llama_model* model = llama_get_model(ctx);
            if (!model)
            {
                return 0;
            }
 
            // 2. Get the fundamental dimensions that determine the cache's shape.
            // Note: We use the modern `llama_model_` prefixed functions as they are forward-compatible.
            const int32_t n_ctx = llama_n_ctx(ctx);             // Max context size
            const int32_t n_layer = llama_model_n_layer(model); // Number of layers
            const int32_t n_embd = llama_model_n_embd(model);   // Embedding dimension
            const int32_t n_head = llama_model_n_head(model);   // Number of attention heads
 
            if (n_head == 0)
            {
                // Avoid division by zero in case of invalid model parameters
                return 0;
            }
 
            const int32_t n_head_kv = llama_model_n_head_kv(model);
            const int64_t n_embd_kv = (int64_t(n_embd) / n_head) * n_head_kv;
 
            // 3. Calculate the total number of elements in one cache tensor (K or V).
            const int64_t n_elements_per_tensor = (int64_t)n_ctx * n_layer * n_embd_kv;
           
            // 5. Calculate the size in bytes for each cache tensor using its element count and data type size.
            const size_t size_k_bytes = n_elements_per_tensor * ggml_type_size(ctx_params.type_k);
            const size_t size_v_bytes = n_elements_per_tensor * ggml_type_size(ctx_params.type_v);
 
            // 6. The total size is the sum of the K-cache and V-cache.
            return size_k_bytes + size_v_bytes;
        }
       
        // Add a message to `messages` and store its content in `msg_strs`
        void llama_cpp_context::add_message(const char* role, const std::string& text) { msg_strs_.push_back({role, std::move(text)}); }
 
        // Helper function to apply the chat template and handle errors
        int llama_cpp_context::apply_chat_template_with_error_handling(const common_chat_templates* tmpls, const bool append, int& output_length)
        {
            const int new_len = apply_chat_template(tmpls, append);
            if (new_len < 0)
            {
                // APP_ERR("failed to apply the chat template");//OWL WAS HERE
                return -1;
            }
            output_length = new_len;
            return 0;
        }
 
        // Function to apply the chat template and resize `formatted` if needed
        int llama_cpp_context::apply_chat_template(const struct common_chat_templates* tmpls, const bool append)
        {
            common_chat_templates_inputs inputs;
            for (const auto& msg : msg_strs_)
            {
                common_chat_msg cmsg;
                cmsg.role = msg.role;
                cmsg.content = msg.content;
                inputs.messages.push_back(cmsg);
            }
            inputs.add_generation_prompt = append;
            inputs.use_jinja = use_jinja_;
 
            auto chat_params = common_chat_templates_apply(tmpls, inputs);
            auto result = chat_params.prompt;
            formatted_.resize(result.size() + 1);
            memcpy(formatted_.data(), result.c_str(), result.size() + 1);
            return result.size();
        }
 
        // Function to tokenize the prompt
        int llama_cpp_context::tokenize_prompt(const std::string& prompt)
        {
            const llama_vocab* vocab = llama_model_get_vocab(model_->get());
            const bool is_first = llama_kv_self_used_cells(ctx_.get()) == 0;
 
            const int n_prompt_tokens = -llama_tokenize(vocab, prompt.c_str(), prompt.size(), NULL, 0, is_first, true);
            batch_tokens_.resize(n_prompt_tokens);
            if (llama_tokenize(
                    vocab, prompt.c_str(), prompt.size(), batch_tokens_.data(), batch_tokens_.size(), is_first, true)
                < 0)
            {
                // APP_ERR("failed to tokenize the prompt");//OWL WAS HERE
                return -1;
            }
            return n_prompt_tokens;
        }
 
        // Check if we have enough space in the context to evaluate this batch
        int llama_cpp_context::check_context_size()
        {
            const int n_ctx = llama_n_ctx(ctx_.get());
            const int n_ctx_used = llama_kv_self_used_cells(ctx_.get());
            if (n_ctx_used + batch_.n_tokens > n_ctx)
            {
                // APP_ERR("[CTX {}] LOG: context size exceeded. Used: {}, Batch: {}, Total Ctx: {}",
                //     std::to_string(context_id_), n_ctx_used, batch_.n_tokens, n_ctx);//OWL WAS HERE
                return 1;
            }
            return 0;
        }
 
        // convert the token to a string
        int llama_cpp_context::convert_token_to_string(const llama_token token_id, std::vector<uint8_t>& piece)
        {
            const llama_vocab* vocab = llama_model_get_vocab(model_->get());
            char buf[256];
            int n = llama_token_to_piece(vocab, token_id, buf, sizeof(buf), 0, true);
            if (n < 0)
            {
                // APP_ERR("[CTX {}] LOG ERROR: failed to convert token {} to piece", std::to_string(context_id_), token_id);//OWL WAS HERE
                return 1;
            }
 
            piece.assign(buf, buf + n);
            return 0;
        }
 
        int llama_cpp_context::add_prompt(const std::vector<uint8_t>& prompt)
        {
            // APP_DEBUG("\n[CTX {}] START", std::to_string(context_id_));//OWL WAS HERE
            try
            {
                // APP_DEBUG("[CTX {}] user_turn_={}, first_pass_={}", std::to_string(context_id_),
                //     user_turn_ ? "true" : "false", first_pass_ ? "true" : "false");//OWL WAS HERE
 
                if (!user_turn_)
                {
                    // APP_DEBUG("[CTX {}] Not user's turn.", std::to_string(context_id_));//OWL WAS HERE
                    return to_standard_return_type(error_types::LLM_STILL_PROCESSING);
                }
                if (prompt.empty())
                {
                    // APP_DEBUG("[CTX {}] Empty prompt received.", std::to_string(context_id_));//OWL WAS HERE
                    return to_standard_return_type(error_types::INVALID_PROMPT);
                }
 
                if (!first_pass_)
                {
                    // APP_DEBUG("[CTX {}] Not first pass. Adding previous assistant response to history: \"{}\"",
                    //     std::to_string(context_id_), current_response_.c_str());//OWL WAS HERE
                    add_message("assistant", current_response_);
                    current_response_.clear();
                    if (apply_chat_template_with_error_handling(chat_templates_.get(), false, prev_len_) < 0)
                    {
                        // APP_ERR("[CTX {}] Unable to apply chat template for assistant message.",
                        //     std::to_string(context_id_));//OWL WAS HERE
                        return to_standard_return_type(error_types::UNABLE_TO_APPLY_CHAT_TEMPLATE);
                    }
                }
 
                if (first_pass_)
                {
/* OWL WAS HERE
                    auto it = config_.find("system_prompt");
                    if (it != config_.end())
                    {
                        auto system_prompt = it->second.get<std::string>();
                        if (!system_prompt.empty())
                        {
                            // APP_DEBUG("[CTX {}] First pass. Adding system prompt: \"{}\"", std::to_string(context_id_),
                            //     system_prompt.c_str());//OWL WAS HERE
                            add_message("system", system_prompt);
                        }
                    }
*/
                }
 
                std::string prompt_str(prompt.begin(), prompt.end());
                // APP_DEBUG("[CTX {}] Adding user message: \"{}\"", std::to_string(context_id_), prompt_str.c_str());//OWL WAS HERE
                add_message("user", prompt_str);
 
                int new_len;
                if (apply_chat_template_with_error_handling(chat_templates_.get(), true, new_len) < 0)
                {
                    // APP_ERR("[CTX {}] Unable to apply chat template for user message.", std::to_string(context_id_));//OWl WAS HERE
                    return to_standard_return_type(error_types::UNABLE_TO_APPLY_CHAT_TEMPLATE);
                }
                // APP_DEBUG("[CTX {}] Chat template applied. prev_len_={}, new_len={}", std::to_string(context_id_),
                //     prev_len_, new_len);//OWL WAS HERE
 
                std::string prompt_to_tokenize(formatted_.begin() + prev_len_, formatted_.begin() + new_len);
                // APP_DEBUG("[CTX {}] Final prompt segment to tokenize: \"{}\"", std::to_string(context_id_),
                //     prompt_to_tokenize.c_str());//OWL WAS HERE
 
                int n_tokens = tokenize_prompt(prompt_to_tokenize);
                if (n_tokens < 0)
                {
                    // APP_ERR("[CTX {}] Failed to tokenize prompt.", std::to_string(context_id_));//OWl WAS HERE
                    return to_standard_return_type(error_types::UNABLE_TO_APPLY_CHAT_TEMPLATE);
                }
                // APP_DEBUG("[CTX {}] Tokenized into {} tokens.", std::to_string(context_id_), n_tokens);//OWL WAS HERE
 
                // prepare a batch for the prompt
                batch_ = llama_batch_get_one(batch_tokens_.data(), batch_tokens_.size());
                // APP_DEBUG("[CTX {}] Batch created. n_tokens={}", 1, batch_.n_tokens);//OWL WAS HERE
 
                // Update prev_len_ with the latest total length so the *next* call to add_prompt
                // has the correct starting point for slicing.
                prev_len_ = new_len;
 
                first_pass_ = false;
                user_turn_ = false;
                // APP_DEBUG("[CTX {}] END", std::to_string(context_id_));//OWL WAS HERE
                return to_standard_return_type(error_types::OK);
            }
            catch (const std::exception& e)
            {
                // APP_ERR("[CTX {}] llamacpp exception thrown: {}", std::to_string(context_id_), e.what());//OWl WAS HERE
                return to_standard_return_type(error_types::EXCEPTION_THROWN);
            }
        }
 
        int llama_cpp_context::get_piece(std::vector<uint8_t>& piece, bool& complete)
        {
            // APP_DEBUG("\n[CTX {}] START", std::to_string(context_id_));//OWL WAS HERE
            try
            {
                complete = false;
                // APP_DEBUG("[CTX {}] user_turn_={}", std::to_string(context_id_), user_turn_ ? "true" : "false");//OWL WAS HERE
 
                // If it's the user's turn, generation is complete for this round.
                if (user_turn_)
                {
                    complete = true;
                    // APP_DEBUG("[CTX {}] Is user's turn, completing.", std::to_string(context_id_));//OWL WAS HERE
                    // APP_DEBUG("[CTX {}] END", std::to_string(context_id_));//OWL WAS HERE
                    return to_standard_return_type(error_types::OK);
                }
 
                // Ensure the context has enough space for the tokens in the current batch.
                if (check_context_size())
                {
                    complete = true;
                    // APP_DEBUG("[CTX {}] Context size exceeded.", std::to_string(context_id_));//OWL WAS HERE
                    return to_standard_return_type(error_types::CONTEXT_SIZE_EXCEEDED);
                }
 
                // APP_DEBUG("Decoding batch... n_tokens={}, KV cache used={}", 1, batch_.n_tokens,
                //     (int)llama_kv_self_used_cells(ctx_.get()));//OWL WAS HERE
 
                // Process the batch (the full prompt on the first call, then one token at a time).
                if (llama_decode(ctx_.get(), batch_))
                {
                    // APP_ERR("[CTX {}] llama_decode failed.", std::to_string(context_id_));//OWl WAS HERE
                    return to_standard_return_type(error_types::DECODE_FAILURE);
                }
                // APP_DEBUG("[CTX {}] Decode successful. KV cache now used={}", std::to_string(context_id_),
                //     (int)llama_kv_self_used_cells(ctx_.get()));//OWL WAS HERE
 
                // Sample the next token from the logits produced by llama_decode.
                new_token_id_ = llama_sampler_sample(sampler_.get(), ctx_.get(), -1);
                // APP_DEBUG("[CTX {}] Sampled token ID: {}", std::to_string(context_id_), new_token_id_);//OWL WAS HERE
 
                const llama_vocab* vocab = llama_model_get_vocab(model_->get());
 
                // Check for End of Generation token.
                if (llama_vocab_is_eog(vocab, new_token_id_))
                {
                    // APP_DEBUG("[CTX {}] End of generation token found.", std::to_string(context_id_));//OWL WAS HERE
                    complete = true;
                    user_turn_ = true;
                    return to_standard_return_type(error_types::OK);
                }
 
                // Convert the new token to its string representation.
                if (convert_token_to_string(new_token_id_, piece))
                {
                    // APP_ERR("[CTX {}] Failed to convert token to string.", std::to_string(context_id_));//OWL WAS HERE
                    return to_standard_return_type(error_types::UNABLE_TO_GET_PIECE);
                }
 
                std::string piece_str(piece.begin(), piece.end());
                // APP_DEBUG("[CTX {}] Generated piece: \"{}\"", std::to_string(context_id_), piece_str.c_str());//OWL WAS HERE
 
                // Check for common stop tokens.
                if (piece_str == "<|im_end|>" || piece_str == "</s>")
                {
                    // APP_DEBUG("[CTX {}] Stop string '{}' found.", std::to_string(context_id_), piece_str.c_str());//OWL WAS HERE
                    complete = true;
                    user_turn_ = true;
                    piece.clear(); // Don't send the stop token itself to the client.
                    return to_standard_return_type(error_types::OK);
                }
 
                // // *** FIX: Check against all stop strings from the chat template ***
                // std::string piece_str(piece.begin(), piece.end());
                // for (const auto& stop_str : stop_strs_) {
                //     if (piece_str == stop_str) {
                //         complete = true;
                //         user_turn_ = true;
                //         piece.clear(); // Clear the piece so the stop string is not sent to the client
                //         return to_standard_return_type(error_types::OK);
                //     }
                // }
 
                current_response_ += piece_str;
 
                // --- PREPARE FOR NEXT ITERATION ---
                // Create a new batch containing only the token we just sampled.
                // This new batch will be processed in the next call to get_piece().
 
                batch_ = llama_batch_get_one(&new_token_id_, 1);
 
                // APP_DEBUG("[CTX {}] Prepared next batch for single token.", std::to_string(context_id_));//OWL WAS HERE
                // APP_DEBUG("[CTX {}] GET_PIECE END", std::to_string(context_id_));//OWL WAS HERE
                return to_standard_return_type(error_types::OK);
            }
            catch (const std::exception& e)
            {
                // APP_ERR("[CTX {}] llamacpp exception thrown: {}", std::to_string(context_id_), e.what());//OWL WAS HERE
                return to_standard_return_type(error_types::EXCEPTION_THROWN);
            }
        }
 
        int llama_cpp_context::model_n_embd(int32_t& n_embd)
        {
            if (!model_ || !model_->get())
            {
                return to_standard_return_type(error_types::MODEL_NOT_LOADED);
            }
            n_embd = llama_model_n_embd(model_->get());
            return to_standard_return_type(error_types::OK);
        }
 
        int llama_cpp_context::get_aggregate_embeddings(uint32_t window_size, aggregation_rule agg_rule, std::vector<float>& embeddings)
        {
            int n_embd = llama_model_n_embd(model_->get());
            switch (agg_rule)
            {
                case aggregation_rule::NONE:
                case aggregation_rule::LAST:
                {
                    // Get the embeddings for the last token in the sequence (pooled embedding for the whole text)
                    // The embeddings are typically for the *last* token of the sequence, which represents the context.
                    auto embeddings_ptr = llama_get_embeddings_ith(ctx_.get(),-1);//OWL WAS HERE
                    if (!embeddings_ptr) {
                        throw std::runtime_error("Failed to get embeddings. Embeddings might not be available or 'embedding' parameter not set.");
                    }
                    embeddings = std::vector<float>(embeddings_ptr, embeddings_ptr + n_embd);//OWL WAS HERE
                    break;
                }
                case aggregation_rule::AVERAGE:
                {
                    std::vector<float> average(n_embd);
                    for(int i = -1; i >= -window_size; i--)
                        {
                            auto embeddings_ptr = llama_get_embeddings_ith(ctx_.get(),i);//OWL WAS HERE
                            if (!embeddings_ptr) {
                                throw std::runtime_error("Failed to get embeddings. Embeddings might not be available or 'embedding' parameter not set.");
                            }
                            for(int j=0;j<n_embd;j++)
                                average[j]+=embeddings_ptr[j];
                        }
                    for(int j=0;j<n_embd;j++)
                         average[j] /= window_size;
                    embeddings = std::move(average);
                    break;
                }
                // Implement other aggregation rules as needed
                case aggregation_rule::MAXIMUM:
                case aggregation_rule::MINIMUM:
                case aggregation_rule::MEDIAN:
                case aggregation_rule::ANY:
                case aggregation_rule::FIRST:
                default:
                    return to_standard_return_type(error_types::INVALID_INTERACTION_TYPE);
            }
            return to_standard_return_type(error_types::OK);
        }
 
        int llama_cpp_context::encode(const std::vector<uint8_t>& prompt_input, std::vector<int32_t>& token_ids)
        {
            //This method should return the first `num_tokens` tokens from the prompt input.
            if (prompt_input.empty()) {
                return to_standard_return_type(error_types::INVALID_PROMPT);
            }
            std::string text(prompt_input.begin(), prompt_input.end());
            bool add_bos = true; // Add BOS token if needed, can be configurable
            if (text.empty()) {
                return to_standard_return_type(error_types::INVALID_PROMPT);
            }
            // Tokenize the text using llama_tokenize
            // Ensure the model is loaded and tokenizer is available
            if (!model_ || !model_->get()) {
                return to_standard_return_type(error_types::TOKENIZER_NOT_LOADED);
            }
            const llama_vocab* vocab = llama_model_get_vocab(model_->get());
            if (!vocab) {
                return to_standard_return_type(error_types::TOKENIZER_NOT_LOADED);
            }
 
            // Tokenize the text
            // Note: llama_tokenize expects a buffer size that can accommodate the text length plus any special tokens.
            // A reasonable buffer size for tokenization. Can be dynamic if needed.
            token_ids.resize(text.length() + (add_bos ? 1 : 0));
            int actual_num_tokens = llama_tokenize(vocab, text.c_str(), text.length(), token_ids.data(), token_ids.size(), add_bos, true);
 
            return to_standard_return_type(error_types::OK);
        }
 
        int llama_cpp_context::decode(const std::vector<int32_t>& token_ids, std::vector<uint8_t>& prompt_output)
        {
            // This method should decode the provided token IDs into a prompt output.
            if (token_ids.empty()) {
                return to_standard_return_type(error_types::INVALID_TOKENS);
            }
 
            // Convert token_ids to llama_token format
            std::vector<llama_token> tokens(token_ids.size());
            for (size_t i = 0; i < token_ids.size(); ++i) {
                tokens[i] = static_cast<llama_token>(token_ids[i]);
            }
 
            // Decode the tokens into a string
            const llama_vocab* vocab = llama_model_get_vocab(model_->get());
            if (!vocab) {
                return to_standard_return_type(error_types::TOKENIZER_NOT_LOADED);
            }
 
            // Prepare a buffer for the decoded text
            std::string decoded_text = "";
            for (llama_token token : tokens)
            {
                char buffer[256]; // Sufficiently large buffer
                int n = llama_detokenize(vocab, tokens.data(), tokens.size(), buffer, sizeof(buffer), 0, true);
                if (n < 0) {
                    return to_standard_return_type(error_types::UNABLE_TO_DECODE);
                }
                decoded_text += buffer;
            }
            // Resize the output to the actual size used
            prompt_output.resize(decoded_text.size());
            prompt_output = std::vector<uint8_t>(decoded_text.begin(), decoded_text.end());
            return to_standard_return_type(error_types::OK);
        }
 
        int llama_cpp_context::ingest(const std::vector<int32_t>& token_ids)
        {
            // This method should ingest the provided token IDs into the context.
            if (token_ids.empty()) {
                return to_standard_return_type(error_types::INVALID_TOKENS);
            }
 
            // Convert token_ids to llama_token format
            std::vector<llama_token> tokens(token_ids.size());
            for (size_t i = 0; i < token_ids.size(); ++i) {
                tokens[i] = static_cast<llama_token>(token_ids[i]);
            }
           
            if (!ctx_params_.embeddings) {
                throw std::runtime_error("Context not initialized with embedding=true. Set context_params.embeddings = true.");
            }
 
            // The batch now points to data inside `batch_tokens_`, which will persist.
            batch_ = llama_batch_init(tokens.size(), 0, 1);//OWl WAS HERE
            std::vector<llama_seq_id> seq_ids = {0};//OWl WAS HERE
            auto pos0 = llama_kv_self_n_tokens(ctx_.get());
            for(int pos = 0;pos<token_ids.size();pos++)//OWl WAS HERE
                common_batch_add(batch_, token_ids[pos], pos0 + pos, seq_ids, true);//OWl WAS HERE
 
            if (llama_decode(ctx_.get(), batch_) != 0) {
                llama_batch_free(batch_);
                throw std::runtime_error("Failed to decode tokens.");
            }
 
            llama_batch_free(batch_);
 
            return to_standard_return_type(error_types::OK);
        }
 
    class llama_cpp_engine : public llm_engine
    {
        std::atomic<uint64_t> count_;
 
    public:
        virtual ~llama_cpp_engine() = default;
 
        // OWL WAS HERE
        // error_types create_context(const std::shared_ptr<loaded_model>& model, const std::shared_ptr<loaded_tokenizer>&,
        //     uint64_t seed, const json::v1::map& overrides, rpc::shared_ptr<chat_context>& context) override
        // {
        //     auto llama_model = std::static_pointer_cast<llama_cpp_loaded_model>(model);
        //     auto ctx = rpc::make_shared<llama_cpp_context>(seed, llama_model, overrides, ++count_);
 
        //     auto ret = ctx->init();
        //     if (ret != error_types::OK)
        //         return ret;
        //     context = rpc::static_pointer_cast<chat_context>(ctx);
        //     return error_types::OK;
        // }
 
        error_types parse_model(
            const llm_model& modl, void* data, uint64_t size, std::shared_ptr<loaded_model>& loaded_model) override
        {
            try
            {
                std::ignore = modl;
 
                llama_model_params model_params = llama_model_default_params();
                // The goal is to prevent the large, static model weights from displacing the smaller, more frequently
                // accessed KV Cache from the L3 cache. You will treat the EPC as a "streaming source" for weights.
                model_params.n_gpu_layers = 0;
 
    
                llama_model* model = nullptr; // OWL WAS HERE :llama_model_load_from_blob((const uint8_t*)data, size, model_params);
                if (model == NULL)
                {
                    return error_types::UNABLE_TO_LOAD_RECORD;
                }
 
                loaded_model = std::make_shared<llama_cpp_loaded_model>(model, modl);
                return error_types::OK;
            }
            catch (const std::exception& e)
            {
                // APP_ERR("llamacpp exception thrown");//OWL WAS HERE
                return error_types::EXCEPTION_THROWN;
            }
        }
 
/* OWL WAS HERE
        error_types infer(const std::vector<uint8_t>& prompt, const json::v1::map& overrides, uint64_t rng_seed,
            const std::shared_ptr<loaded_tokenizer>&, const std::shared_ptr<loaded_model>& model,
            std::vector<uint8_t>& output) override
        {
            try
            {
                error_types ret = error_types::OK;
 
                // number of tokens to predict
                int n_predict = 32;
 
                auto llama_model = std::static_pointer_cast<llama_cpp_loaded_model>(model);
 
                const llama_vocab* vocab = llama_model_get_vocab(llama_model->get());
 
                const int n_prompt =
                    -llama_tokenize(vocab, (const char*)prompt.data(), prompt.size(), NULL, 0, true, true);
 
                std::vector<llama_token> prompt_tokens(n_prompt);
                if (llama_tokenize(vocab, (const char*)prompt.data(), prompt.size(), prompt_tokens.data(),
                        prompt_tokens.size(), true, true)
                    < 0)
                {
                    return error_types::UNABLE_TO_TOKENIZE;
                }
 
                llama_context_params ctx_params = llama_context_default_params();
 
                ctx_params.n_ctx = ctx_params.n_batch;
 
                // n_ctx is the context size
                ctx_params.n_ctx = n_prompt + n_predict - 1;
                // n_batch is the maximum number of tokens that can be processed in a single call to llama_decode
                ctx_params.n_batch = n_prompt;
                // enable performance counters
                ctx_params.no_perf = false;
 
                ctx_params.n_threads = POOL_THREADS;
                ctx_params.n_threads_batch = POOL_THREADS;
 
                llama_context* ctx = llama_init_from_model(llama_model->get(), ctx_params);
 
                if (ctx == NULL)
                {
                    return error_types::MODEL_FAILED;
                }
 
                auto sparams = llama_sampler_chain_default_params();
                sparams.no_perf = false;
                llama_sampler* smpl = llama_sampler_chain_init(sparams);
 
                llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
 
                // print the prompt token-by-token
                std::string out;
 
                for (auto id : prompt_tokens)
                {
                    char buf[128];
                    int n = llama_token_to_piece(vocab, id, buf, sizeof(buf), 0, true);
                    if (n < 0)
                    {
                        return error_types::UNABLE_TO_GET_PIECE;
                    }
 
                    out += std::string(buf, n);
                }
 
                // prepare a batch for the prompt
                llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
                llama_token new_token_id;
 
                for (int n_pos = 0; n_pos + batch.n_tokens < n_prompt + n_predict;)
                {
                    // evaluate the current batch with the transformer model
                    if (llama_decode(ctx, batch))
                    {
                        return error_types::UNABLE_TO_DECODE;
                    }
 
                    n_pos += batch.n_tokens;
 
                    // sample the next token
                    {
                        new_token_id = llama_sampler_sample(smpl, ctx, -1);
 
                        // is it an end of generation?
                        if (llama_vocab_is_eog(vocab, new_token_id))
                        {
                            break;
                        }
 
                        char buf[128];
                        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
                        if (n < 0)
                        {
                            return error_types::UNABLE_TO_GET_PIECE;
                        }
                        out += std::string(buf, n);
 
                        // prepare the next batch with the sampled token
                        batch = llama_batch_get_one(&new_token_id, 1);
                    }
                }
 
                output = std::vector<uint8_t>((uint8_t*)out.data(), (uint8_t*)out.data() + out.size());
 
                llama_sampler_free(smpl);
                llama_free(ctx);
                return error_types::OK;
            }
            catch (const std::exception& e)
            {
                // APP_ERR("llamacpp exception thrown");//OWL WAS HERE
                return error_types::EXCEPTION_THROWN;
            }
        }
*/
    };

 
    error_types create_llama_cpp(std::shared_ptr<secret_llama::v1_0::llm_engine>& llmengine)
    {
#ifdef _DEBUG
        llama_log_set(
            [](enum ggml_log_level level, const char* text, void* /* user_data */)
            {
#ifndef _IN_ENCLAVE
                puts(text);
#else
                switch (level)
                {
                case GGML_LOG_LEVEL_CONT: // continue previous log not sure what to put it under as logger is stateless
                case GGML_LOG_LEVEL_DEBUG:
                    // APP_DEBUG("{}", text);//OWL WAS HERE
                    break;
                case GGML_LOG_LEVEL_INFO:
                    // APP_INFO("{}", text);//OWL WAS HERE
                    break;
                case GGML_LOG_LEVEL_WARN:
                    APP_WARN("{}", text);
                    break;
                case GGML_LOG_LEVEL_ERROR:
                    // APP_ERR("{}", text);//OWL WAS HERE
                    break;
                case GGML_LOG_LEVEL_NONE:
                default:
                    break;
                }
#endif
            },
            nullptr);
#endif
 
        llama_backend_init();
        ggml_backend_cpu_init();
 
        llmengine = std::make_shared<llama_cpp_engine>();
        return error_types::OK;
    }
} // namespace v1_0
} // namespace secret_llama
 