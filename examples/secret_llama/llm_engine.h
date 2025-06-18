#pragma once
 
#include <vector>
#include <cstdint>
 
//#include <sdk_v3/route_reflection.h>//OWL WAS HERE
 
//#include <secret_llama/secret_llama.h>//OWL WAS HERE
#include <secret_llama.h>
#include <common/chat.h>//OWL WAS HERE
 
namespace secret_llama
{
namespace v1_0
//inline namespace v1_0 //OWL WAS HERE
{
    // is an rpc object that also adds cleanup for local cleanup
// #if defined(ENABLE_SECRET_LLAMA_TESTER_APP) && !defined(_IN_ENCLAVE)
//     class chat_context : public service::v3::route_reflection<chat_context, i_context_host>
// #else
//     class chat_context : public service::v3::route_reflection<chat_context, i_context>
// #endif    
     class chat_context : public i_context
    {
    public:
        virtual ~chat_context() = default;
 
        virtual void cleanup(){};
    };
   
    class loaded_model
    {
        protected:
        llm_model model_config_;
        public:
        loaded_model(const llm_model& model_config) : model_config_(model_config)
        {}
        virtual ~loaded_model() = default;
        const llm_model& get_config() const { return model_config_; }
        virtual int32_t get_n_embd() const = 0;
    };

    //OWL BEGIN
    class llama_cpp_loaded_model : public loaded_model
    {
        llama_model* model_ = nullptr;
    public:
        llama_cpp_loaded_model(llama_model* model, const llm_model& model_config);
        virtual ~llama_cpp_loaded_model(); 
        llama_model* get() const;
        int32_t get_n_embd() const override;
    };
    
    struct context_deleter
    {
        void operator()(llama_context* ptr) const;
    };
    
    struct sampler_deleter
    {
        void operator()(llama_sampler* ptr) const;
    };

     
    struct llama_cpp_context : public chat_context
    {
        uint64_t seed_ = 0;
        // json::v1::map config_;//OWL WAS HERE
 
        std::shared_ptr<llama_cpp_loaded_model> model_;
        std::unique_ptr<llama_context, context_deleter> ctx_;
        llama_context_params ctx_params_;
 
        std::unique_ptr<llama_sampler, sampler_deleter> sampler_;
 
        struct chat_message
        {
            const char* role;
            std::string content;
        };
        std::list<chat_message> msg_strs_;
        bool use_jinja_ = false;
        int prev_len_ = 0;
 
        std::vector<char> formatted_;
 
        llama_batch batch_;
        llama_token new_token_id_;
        common_chat_templates_ptr chat_templates_;
        // std::vector<std::string> stop_strs_; // To hold stop strings from configuration
 
        std::string current_response_;
 
        std::vector<llama_token> batch_tokens_;
 
        bool user_turn_ = true;
        bool first_pass_ = true;
        uint64_t context_id_;
 
        virtual ~llama_cpp_context(); 
        error_types init();

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
        size_t calculate_kv_cache_size(const llama_context* ctx, const llama_context_params& ctx_params);
        
        // Add a message to `messages` and store its content in `msg_strs`
        void add_message(const char* role, const std::string& text);
 
        // Helper function to apply the chat template and handle errors
        int apply_chat_template_with_error_handling(const common_chat_templates* tmpls, const bool append, int& output_length);

        // Function to apply the chat template and resize `formatted` if needed
        int apply_chat_template(const struct common_chat_templates* tmpls, const bool append);

        // Function to tokenize the prompt
        int tokenize_prompt(const std::string& prompt);

        // Check if we have enough space in the context to evaluate this batch
        int check_context_size();

        // convert the token to a string
        int convert_token_to_string(const llama_token token_id, std::vector<uint8_t>& piece);

        int add_prompt(const std::vector<uint8_t>& prompt) override;

        int get_piece(std::vector<uint8_t>& piece, bool& complete) override;

        int model_n_embd(int32_t& n_embd) override;

        int get_aggregate_embeddings(uint32_t window_size, aggregation_rule agg_rule, std::vector<float>& embeddings) override;

        int encode(const std::vector<uint8_t>& prompt_input, std::vector<int32_t>& token_ids) override;

        int decode(const std::vector<int32_t>& token_ids, std::vector<uint8_t>& prompt_output) override;

        int ingest(const std::vector<int32_t>& token_ids) override;

    };
    //OWL END
   
    class loaded_tokenizer
    {
        public:
        virtual ~loaded_tokenizer() = default;
    };
   
    class llm_engine
    {
    public:
        virtual ~llm_engine() = default;
 
        // virtual error_types create_context(const std::shared_ptr<loaded_model>& model_data, const std::shared_ptr<loaded_tokenizer>& tokenizer_data,
        //     uint64_t seed, const json::v1::map& overrides,
        //     rpc::shared_ptr<chat_context>& context) = 0;//OWL WAS HERE
 
        // virtual error_types infer(const std::vector<uint8_t>& prompt, const json::v1::map& overrides,
        //     uint64_t rng_seed, const std::shared_ptr<loaded_tokenizer>& tokenizer_bin, const std::shared_ptr<loaded_model>& model_bin,
        //     std::vector<uint8_t>& output) = 0;//OWL WAS HERE
           
        virtual error_types parse_model(const llm_model& modl, void* data, uint64_t size, std::shared_ptr<loaded_model>& loaded_model) = 0;
        virtual error_types parse_tokenizer(const tokenizer& tok, void* data, uint64_t size, std::shared_ptr<loaded_tokenizer>& loaded_tokeniser)
        {
            std::ignore = tok;
            std::ignore = data;
            std::ignore = size;
            std::ignore = loaded_tokeniser;
            return error_types::NOT_IMPLEMENTED;
        }
    };
} // namespace v1_0
} // namespace secret_llama