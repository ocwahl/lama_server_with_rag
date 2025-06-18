#include "arg.h"
#include "common.h"
#include "log.h"
#include "llama.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "secret_llama.h"
#include "llm_engine.h"

static void print_usage(int, char ** argv) {
    LOG("\nexample usage:\n");
    LOG("\n    %s -m model.gguf -p \"Hello my name is\" -n 32 -np 4\n", argv[0]);
    LOG("\n");
}

int main(int argc, char ** argv) {
    common_params params;

    params.prompt = "Hello my name is";
    params.n_predict = 32;

    if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_COMMON, print_usage)) {
        return 1;
    }

    common_init();

    // number of parallel batches
    int n_parallel = params.n_parallel;

    // total length of the sequences including the prompt
    int n_predict = params.n_predict;

    // init LLM

    llama_backend_init();
    llama_numa_init(params.numa);

    // initialize the model

    llama_model_params model_params = common_model_params_to_llama(params);
    secret_llama::v1_0::llm_model model_config;

    llama_model * model = llama_model_load_from_file(params.model.path.c_str(), model_params);

    if (model == NULL) {
        LOG_ERR("%s: error: unable to load model\n" , __func__);
        return 1;
    }


    const llama_vocab * vocab = llama_model_get_vocab(model);



    // initialize the context
    llama_context_params ctx_params = common_context_params_to_llama(params);

        
    // tokenize the prompt
    std::vector<llama_token>  tokens_list_0 = common_tokenize(vocab, params.prompt, true);

    const int n_kv_req = tokens_list_0.size() + (n_predict - tokens_list_0.size())*n_parallel;

    ctx_params.n_ctx   = n_kv_req;
    ctx_params.n_batch = std::max(n_predict, n_parallel);
    ctx_params.embeddings = true; //OWl WAS HERE

    llama_context * ctx = llama_init_from_model(model, ctx_params);

    auto sparams = llama_sampler_chain_default_params();
    sparams.no_perf = false;

    llama_sampler * smpl = llama_sampler_chain_init(sparams);

    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(params.sampling.top_k));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(params.sampling.top_p, params.sampling.min_keep));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp (params.sampling.temp));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist (params.sampling.seed));

    if (ctx == NULL) {
        LOG_ERR("%s: error: failed to create the llama_context\n" , __func__);
        return 1;
    }

    auto p_secret_llama_model = std::make_shared<secret_llama::v1_0::llama_cpp_loaded_model >(model, model_config);

    secret_llama::v1_0::llama_cpp_context ctx0;
    ctx0.seed_ = 0;
    //ctx.config_ = ?;//OWL WAS HERE
    ctx0.model_ = p_secret_llama_model;
    ctx0.ctx_ = std::unique_ptr<llama_context, secret_llama::v1_0::context_deleter>(ctx, secret_llama::v1_0::context_deleter{});
    ctx0.ctx_params_ = ctx_params;
    ctx0.sampler_ = std::unique_ptr<llama_sampler, secret_llama::v1_0::sampler_deleter>(smpl, secret_llama::v1_0::sampler_deleter{});
    ctx0.msg_strs_ = std::list<secret_llama::v1_0::llama_cpp_context::chat_message>();
    ctx0.use_jinja_ = false;
    ctx0.prev_len_ = 0;
    ctx0.formatted_ = std::vector<char>();
    ctx0.batch_ = llama_batch();
    ctx0.new_token_id_ = llama_token() ;
    ctx0.chat_templates_ = common_chat_templates_ptr();
    // std::vector<std::string> stop_strs_; // To hold stop strings from configuration
 
    ctx0.current_response_ = std::string();
 
    ctx0.batch_tokens_ = tokens_list_0;
    
    ctx0.user_turn_ = true;
    ctx0.first_pass_ = true;
    ctx0.context_id_= 0;

    // print the prompt token-by-token
    LOG("\n");

    for (auto id : ctx0.batch_tokens_) {
        LOG("%s", common_token_to_piece(ctx, id).c_str());
    }

    ctx0.ingest({std::begin(tokens_list_0),std::begin(tokens_list_0)+5});
    std::vector<float> embeddings_chunk_1;
    ctx0.get_aggregate_embeddings(5,secret_llama::v1_0::aggregation_rule::AVERAGE,embeddings_chunk_1);
    ctx0.ingest({std::begin(tokens_list_0)+5,std::begin(tokens_list_0)+10});
    std::vector<float> embeddings_chunk_2;
    ctx0.get_aggregate_embeddings(5,secret_llama::v1_0::aggregation_rule::AVERAGE,embeddings_chunk_2);



    const int n_ctx = llama_n_ctx(ctx);

    LOG_INF("\n%s: n_predict = %d, n_ctx = %d, n_batch = %u, n_parallel = %d, n_kv_req = %d\n", __func__, n_predict, n_ctx, ctx_params.n_batch, n_parallel, n_kv_req);

    // make sure the KV cache is big enough to hold all the prompt and generated tokens
    if (n_kv_req > n_ctx) {
        LOG_ERR("%s: error: n_kv_req (%d) > n_ctx, the required KV cache size is not big enough\n", __func__,  n_kv_req);
        LOG_ERR("%s:        either reduce n_parallel or increase n_ctx\n", __func__);
        return 1;
    }

    // print the prompt token-by-token

    LOG("\n");

    for (auto id : ctx0.batch_tokens_) {
        LOG("%s", common_token_to_piece(ctx, id).c_str());
    }
    LOG("\n");
    llama_perf_sampler_print(smpl);
    llama_perf_context_print(ctx);

    fprintf(stderr, "\n");


    llama_backend_free();

    return 0;
}
