#pragma once

#include <memory>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <string>
#include <array>
/* OWL WAS HERE
#include <rpc/version.h>
#include <rpc/marshaller.h>
#include <rpc/serialiser.h>
#include <rpc/service.h>
#include <rpc/error_codes.h>
#include <rpc/types.h>
#include <rpc/casting_interface.h>
#include "sdk_v3/types.h"
#include "sdk_v3/json.h"
#include "file_system/file_system.h"
*/
//#include <common/json.hpp>//OWL WAS HERE
namespace secret_llama
{
	inline namespace v1_0
	{
		class i_context;
		class i_llm;
		class i_context_host;
		class i_llm_host;
		class i_test;
	}
}
namespace secret_llama
{
	inline namespace v1_0
	{
		enum class error_types : int
		{
			OK,
			OUT_OF_MEMORY,
			BAD_MAGIC_NUMBER,
			BAD_VERSION,
			BAD_CLASSIFIER,
			BAD_GROUP_SIZE,
			TOO_SMALL,
			INVALID_PRINTF,
			CANNOT_ENCODE_NULL_TEXT,
			UNKNOWN_ERROR,
			LLM_STILL_PROCESSING,
			LLM_NEEDS_USER_INPUT,
			LLM_STEPS_EXHAUSTED,
			UNABLE_TO_LOAD_RNG,
			RANDOM_NUMBER_GENERATOR_FAILED,
			NOT_IMPLEMENTED,
			MODEL_NOT_LOADED,
			TOKENIZER_NOT_LOADED,
			TOKENIZER_FAULT,
			INVALID_CONTEXT,
			INVALID_PROMPT,
			INVALID_TOKENS,
			INVALID_INTERACTION_TYPE,
			NOT_LOGGED_IN,
			PERMISSION_DENIED,
			UNABLE_TO_SAVE_RECORD,
			UNABLE_TO_LOAD_RECORD,
			UNABLE_TO_GET_PIECE,
			UNABLE_TO_DECODE,
			UNABLE_TO_LOAD_MODEL,
			UNABLE_TO_REMOVE_RECORD,
			UNABLE_TO_LIST_RECORDS,
			UNABLE_TO_LIST_KEYS,
			UNABLE_TO_LOAD_FILE_SYSTEM,
			UNABLE_TO_LIST_FILES,
			UNABLE_TO_REMOVE_FILE,
			UNABLE_TO_DOWNLOAD_FILE,
			UNABLE_TO_GET_DOWNLOAD_STATUS,
			UNABLE_TO_GET_SENDER,
			UNABLE_TO_MAP_FILE,
			UNABLE_TO_TOKENIZE,
			ADMIN_ACCOUNT_MUST_BE_ADMINISTRATOR,
			ACCOUNT_ALREADY_EXISTS,
			ACCOUNT_NOT_FOUND,
			MAX_USER_CONTEXTS_EXCEEDED,
			MAX_CONTEXTS_EXCEEDED,
			UNABLE_TO_CREATE_TIMEOUT,
			CONTEXT_EXPIRED,
			CONTEXT_NOT_FINISHED,
			CONTEXT_MISSING,
			CONTEXT_SIZE_EXCEEDED,
			MODEL_FAILED,
			ENGINE_NOT_LOADED,
			DECODE_FAILURE,
			UNABLE_TO_CONVERT_TOKEN_TO_PIECE,
			UNABLE_TO_APPLY_CHAT_TEMPLATE,
			EXCEPTION_THROWN,
			MAX = 2147483647,
		};

            inline static int to_standard_return_type(error_types err) {return static_cast<int>(err);}
            
        		enum class llm_engine_type
		{
			LLAMA2_C,
			LLAMA_CPP,
			BITNET,
		};
		enum class llm_interaction_type
		{
			GENERATE = 1,
			CHAT = 2,
			QUANTIZED = 4,
		};
		enum class model_format
		{
			OPENVINO,
			ONNX,
			TENSORFLOW,
			PYTORCH,
			TENSORFLOWLITE,
			GGML,
			GGUF,
			LLAMA2,
			PADDLE_PADDLE,
			CAFFE,
			MXNET,
			AUTODETECT,
		};
		enum class tensor_type
		{
			FP16,
			FP32,
			FP64,
			BF16,
			U8,
			I32,
			I64,
		};
		enum class encryption_type
		{
			NONE,
			AES_GCM,
			AES_CTR,
			AES_ECB,
		};
		enum class hash_type
		{
			NONE = 0,
			SHA1 = 1,
			SHA2_256 = 2,
			SHA2_384 = 3,
			SHA2_512 = 4,
			SHA3_256 = 5,
			SHA3_384 = 6,
			SHA3_512 = 7,
			MD5 = 8,
			CMAC_128 = 11,
		};
		enum class access
		{
			PUBLIC,
			PRIVATE,
		};
		enum class aggregation_rule
		{
			NONE,
			AVERAGE,
			MAXIMUM,
			MINIMUM,
			MEDIAN,
			ANY,
			FIRST,
			LAST,
		};
		
		/****************************************************************************/
		struct llm_model
		{
			inline static std::string table_name = "LLM_MODEL";
			std::string name{};
			std::string local_path{};
			std::string url{};
			std::string description{};
			llm_engine_type engine_type{};
			//json::v1::map engine_config{};//OWL WAS HERE
			//nlohmann::json::map engine_config{};//OWL WAS HERE
			enum encryption_type encryption_type{};
			std::vector<uint8_t> encryption_key{};
			enum hash_type hash_type{};
			std::vector<uint8_t> hash{};
			bool is_loaded{};
			enum access access{};
			uint64_t inactivitiy_timeout{};
			
			// one member-function for save/load
/* OWL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("llm_model"
				  ,("name", name)
				  ,("local_path", local_path)
				  ,("url", url)
				  ,("description", description)
				  ,("engine_type", engine_type)
				  ,("engine_config", engine_config)
				  ,("encryption_type", encryption_type)
				  ,("encryption_key", encryption_key)
				  ,("hash_type", hash_type)
				  ,("hash", hash)
				  ,("is_loaded", is_loaded)
				  ,("access", access)
				  ,("inactivitiy_timeout", inactivitiy_timeout)
				);
			}
*/
		};
		inline bool operator != (const llm_model& lhs, const llm_model& rhs)
		{
			return 
			lhs.name != rhs.name
			|| lhs.local_path != rhs.local_path
			|| lhs.url != rhs.url
			|| lhs.description != rhs.description
			|| lhs.engine_type != rhs.engine_type
			// || lhs.engine_config != rhs.engine_config //OWL WAS HERE
			|| lhs.encryption_type != rhs.encryption_type
			|| lhs.encryption_key != rhs.encryption_key
			|| lhs.hash_type != rhs.hash_type
			|| lhs.hash != rhs.hash
			|| lhs.is_loaded != rhs.is_loaded
			|| lhs.access != rhs.access
			|| lhs.inactivitiy_timeout != rhs.inactivitiy_timeout;
		}

		inline bool operator == (const llm_model& lhs, const llm_model& rhs)
		{
			return !(lhs != rhs);
		}
		
		/****************************************************************************/
		struct tokenizer
		{
			inline static std::string table_name = "LLM_TOKENISER";
			std::string name{};
			std::string local_path{};
			std::string url{};
			std::string description{};
			enum model_format model_format{};
			llm_engine_type engine_type{};
			enum tensor_type tensor_type{};
			enum encryption_type encryption_type{};
			std::vector<uint8_t> encryption_key{};
			enum hash_type hash_type{};
			std::vector<uint8_t> hash{};
			bool is_loaded{};
			enum access access{};
			
			// one member-function for save/load
/* OWL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("tokenizer"
				  ,("name", name)
				  ,("local_path", local_path)
				  ,("url", url)
				  ,("description", description)
				  ,("model_format", model_format)
				  ,("engine_type", engine_type)
				  ,("tensor_type", tensor_type)
				  ,("encryption_type", encryption_type)
				  ,("encryption_key", encryption_key)
				  ,("hash_type", hash_type)
				  ,("hash", hash)
				  ,("is_loaded", is_loaded)
				  ,("access", access)
				);
			}
*/
		};
		inline bool operator != (const tokenizer& lhs, const tokenizer& rhs)
		{
			return 
			lhs.name != rhs.name
			|| lhs.local_path != rhs.local_path
			|| lhs.url != rhs.url
			|| lhs.description != rhs.description
			|| lhs.model_format != rhs.model_format
			|| lhs.engine_type != rhs.engine_type
			|| lhs.tensor_type != rhs.tensor_type
			|| lhs.encryption_type != rhs.encryption_type
			|| lhs.encryption_key != rhs.encryption_key
			|| lhs.hash_type != rhs.hash_type
			|| lhs.hash != rhs.hash
			|| lhs.is_loaded != rhs.is_loaded
			|| lhs.access != rhs.access;
		}

		inline bool operator == (const tokenizer& lhs, const tokenizer& rhs)
		{
			return !(lhs != rhs);
		}
		
		/****************************************************************************/
		struct model_list_item
		{
			std::string name{};
			std::string local_path{};
			std::string url{};
			std::string description{};
			llm_engine_type engine_type{};
			// json::v1::map engine_config{};//OWL WAS HERE
			enum encryption_type encryption_type{};
			enum hash_type hash_type{};
			std::vector<uint8_t> hash{};
			// file_system::download_status status{};//OWL WAS HERE
			uint64_t file_size{};
			bool is_loaded{};
			enum access access{};
			uint64_t inactivitiy_timeout{};
			
			// one member-function for save/load
/* OWAL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("model_list_item"
				  ,("name", name)
				  ,("local_path", local_path)
				  ,("url", url)
				  ,("description", description)
				  ,("engine_type", engine_type)
				  ,("engine_config", engine_config)
				  ,("encryption_type", encryption_type)
				  ,("hash_type", hash_type)
				  ,("hash", hash)
				  ,("status", status)
				  ,("file_size", file_size)
				  ,("is_loaded", is_loaded)
				  ,("access", access)
				  ,("inactivitiy_timeout", inactivitiy_timeout)
				);
			}
*/
		};
		inline bool operator != (const model_list_item& lhs, const model_list_item& rhs)
		{
			return 
			lhs.name != rhs.name
			|| lhs.local_path != rhs.local_path
			|| lhs.url != rhs.url
			|| lhs.description != rhs.description
			|| lhs.engine_type != rhs.engine_type
			// || lhs.engine_config != rhs.engine_config //OWL WAS HERE
			|| lhs.encryption_type != rhs.encryption_type
			|| lhs.hash_type != rhs.hash_type
			|| lhs.hash != rhs.hash
			// || lhs.status != rhs.status //OWL WAS HERE
			|| lhs.file_size != rhs.file_size
			|| lhs.is_loaded != rhs.is_loaded
			|| lhs.access != rhs.access
			|| lhs.inactivitiy_timeout != rhs.inactivitiy_timeout;
		}

		inline bool operator == (const model_list_item& lhs, const model_list_item& rhs)
		{
			return !(lhs != rhs);
		}
		
		/****************************************************************************/
		struct tokenizer_list_item
		{
			std::string name{};
			std::string local_path{};
			std::string url{};
			std::string description{};
			enum model_format model_format{};
			llm_engine_type engine_type{};
			enum tensor_type tensor_type{};
			enum encryption_type encryption_type{};
			enum hash_type hash_type{};
			std::vector<uint8_t> hash{};
			// file_system::download_status status{};//OWL WAS HERE
			uint64_t file_size{};
			bool is_loaded{};
			enum access access{};
			
			// one member-function for save/load
/* OWL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("tokenizer_list_item"
				  ,("name", name)
				  ,("local_path", local_path)
				  ,("url", url)
				  ,("description", description)
				  ,("model_format", model_format)
				  ,("engine_type", engine_type)
				  ,("tensor_type", tensor_type)
				  ,("encryption_type", encryption_type)
				  ,("hash_type", hash_type)
				  ,("hash", hash)
				  ,("status", status)
				  ,("file_size", file_size)
				  ,("is_loaded", is_loaded)
				  ,("access", access)
				);
			}
*/
		};
		inline bool operator != (const tokenizer_list_item& lhs, const tokenizer_list_item& rhs)
		{
			return 
			lhs.name != rhs.name
			|| lhs.local_path != rhs.local_path
			|| lhs.url != rhs.url
			|| lhs.description != rhs.description
			|| lhs.model_format != rhs.model_format
			|| lhs.engine_type != rhs.engine_type
			|| lhs.tensor_type != rhs.tensor_type
			|| lhs.encryption_type != rhs.encryption_type
			|| lhs.hash_type != rhs.hash_type
			|| lhs.hash != rhs.hash
			// || lhs.status != rhs.status //OWL WAS HERE
			|| lhs.file_size != rhs.file_size
			|| lhs.is_loaded != rhs.is_loaded
			|| lhs.access != rhs.access;
		}

		inline bool operator == (const tokenizer_list_item& lhs, const tokenizer_list_item& rhs)
		{
			return !(lhs != rhs);
		}
		enum class role
		{
			GUEST,
			USER,
			ADMINISTRATOR,
		};
		
		/****************************************************************************/
		struct account
		{
			inline static std::string table_name = "ACCOUNT";
			std::string name{};
			enum role role{};
			std::vector<uint8_t> passkey{};
			
			// one member-function for save/load
/* OWL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("account"
				  ,("name", name)
				  ,("role", role)
				  ,("passkey", passkey)
				);
			}
*/
		};
		inline bool operator != (const account& lhs, const account& rhs)
		{
			return 
			lhs.name != rhs.name
			|| lhs.role != rhs.role
			|| lhs.passkey != rhs.passkey;
		}

		inline bool operator == (const account& lhs, const account& rhs)
		{
			return !(lhs != rhs);
		}
		
		/****************************************************************************/
		struct account_list_item
		{
			std::string name{};
			enum role role{};
			
			// one member-function for save/load
/* OWL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("account_list_item"
				  ,("name", name)
				  ,("role", role)
				);
			}
*/
		};
		inline bool operator != (const account_list_item& lhs, const account_list_item& rhs)
		{
			return 
			lhs.name != rhs.name
			|| lhs.role != rhs.role;
		}

		inline bool operator == (const account_list_item& lhs, const account_list_item& rhs)
		{
			return !(lhs != rhs);
		}
		enum class chat_response_state
		{
			PIECE,
			END,
		};
		
		/****************************************************************************/
		struct chat_response_piece_binary
		{
			chat_response_state state{};
			std::vector<uint8_t> piece{};
			
			// one member-function for save/load
/* OWL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("chat_response_piece_binary"
				  ,("state", state)
				  ,("piece", piece)
				);
			}
*/
		};
		inline bool operator != (const chat_response_piece_binary& lhs, const chat_response_piece_binary& rhs)
		{
			return 
			lhs.state != rhs.state
			|| lhs.piece != rhs.piece;
		}

		inline bool operator == (const chat_response_piece_binary& lhs, const chat_response_piece_binary& rhs)
		{
			return !(lhs != rhs);
		}
		
		/****************************************************************************/
		struct chat_response_piece_string
		{
			chat_response_state state{};
			std::string piece{};
			
			// one member-function for save/load
/* OWL WAS HERE
			template<typename Ar>
			void serialize(Ar &ar)
			{
				std::ignore = ar;
				ar & YAS_OBJECT_NVP("chat_response_piece_string"
				  ,("state", state)
				  ,("piece", piece)
				);
			}
*/
		};
		inline bool operator != (const chat_response_piece_string& lhs, const chat_response_piece_string& rhs)
		{
			return 
			lhs.state != rhs.state
			|| lhs.piece != rhs.piece;
		}

		inline bool operator == (const chat_response_piece_string& lhs, const chat_response_piece_string& rhs)
		{
			return !(lhs != rhs);
		}
		
		/****************************************************************************/
		class i_context_stub;
		class i_context //: public rpc::casting_interface //OWL WAS HERE
		{
			public:
// 			OWL WAS HERE
// 			static rpc::interface_ordinal get_id(uint64_t rpc_version)
// 			{
// #ifdef RPC_V2
// 				if(rpc_version == rpc::VERSION_2)
// 				{
// 					//::secret_llama::v1_0::i_context{[tag=sdk::v3::method_type::pure_function]add_prompt([const]std::vector<uint8_t>& prompt,)[tag=sdk::v3::method_type::pure_function]get_piece([out]std::vector<uint8_t>& piece,[out]bool& complete,)[tag=sdk::v3::method_type::pure_function]model_n_embd([out]int32_t& n_embd,)[tag=sdk::v3::method_type::pure_function]get_aggregate_embeddings([]uint32_t window_size,[]7410425521722818471 agg_rule,[out]std::vector<float>& embeddings,)[tag=sdk::v3::method_type::pure_function]encode([const]std::vector<uint8_t>& prompt,[out]std::vector<int32_t>& token_ids,)[tag=sdk::v3::method_type::pure_function]decode([const]std::vector<int32_t>& token_ids,[out]std::vector<uint8_t>& prompt,)[tag=sdk::v3::method_type::pure_function]ingest([const]std::vector<int32_t>& token_ids,)}
// 					return {7467689697922469476ull};
// 				}
// #endif
// 				return {0};
// 			}
			
// 			static std::vector<rpc::function_info> get_function_info();
			
			virtual ~i_context() = default;
			
			// ********************* interface methods *********************
			virtual int add_prompt(const std::vector<uint8_t>& prompt) = 0;
			virtual int get_piece(std::vector<uint8_t>& piece, bool& complete) = 0;
			virtual int model_n_embd(int32_t& n_embd) = 0;
			virtual int get_aggregate_embeddings(uint32_t window_size, aggregation_rule agg_rule, std::vector<float>& embeddings) = 0;
			virtual int encode(const std::vector<uint8_t>& prompt, std::vector<int32_t>& token_ids) = 0;
			virtual int decode(const std::vector<int32_t>& token_ids, std::vector<uint8_t>& prompt) = 0;
			virtual int ingest(const std::vector<int32_t>& token_ids) = 0;
			
			public:
			// ********************* compile time polymorphic serialisers *********************
			// template pure static class for serialising proxy request data to a stub or some other target
			template<typename __Serialiser, typename... __Args>
			struct proxy_serialiser
			{
				static int add_prompt(const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int get_piece(std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(std::vector<char>& __buffer, __Args... __args);
				static int get_aggregate_embeddings(const uint32_t& window_size, const aggregation_rule& agg_rule, std::vector<char>& __buffer, __Args... __args);
				static int encode(const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int decode(const std::vector<int32_t>& token_ids, std::vector<char>& __buffer, __Args... __args);
				static int ingest(const std::vector<int32_t>& token_ids, std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for deserialising data from a proxy or some other target into a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_deserialiser
			{
				static int add_prompt(std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_piece(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_aggregate_embeddings(uint32_t& window_size, aggregation_rule& agg_rule, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int encode(std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int decode(std::vector<int32_t>& token_ids, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int ingest(std::vector<int32_t>& token_ids, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
			// template pure static class for serialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_serialiser
			{
				static int add_prompt(std::vector<char>& __buffer, __Args... __args);
				static int get_piece(const std::vector<uint8_t>& piece, const bool& complete, std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(const int32_t& n_embd, std::vector<char>& __buffer, __Args... __args);
				static int get_aggregate_embeddings(const std::vector<float>& embeddings, std::vector<char>& __buffer, __Args... __args);
				static int encode(const std::vector<int32_t>& token_ids, std::vector<char>& __buffer, __Args... __args);
				static int decode(const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int ingest(std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for a proxy deserialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct proxy_deserialiser
			{
				static int add_prompt(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_piece(std::vector<uint8_t>& piece, bool& complete, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(int32_t& n_embd, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_aggregate_embeddings(std::vector<float>& embeddings, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int encode(std::vector<int32_t>& token_ids, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int decode(std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int ingest(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
/*  OWL WAS HERE
			// proxy class for serialising requests into a buffer for optional dispatch at a future time
			template<class Parent, typename ReturnType>
			class buffered_proxy_serialiser
			{
				public:
				using subclass = Parent;
				ReturnType add_prompt(const std::vector<uint8_t>& prompt)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::add_prompt(prompt, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_context.add_prompt", {1}, sdk::v3::method_type::pure_function, __buffer);

				}
				ReturnType ingest(const std::vector<int32_t>& token_ids)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::ingest(token_ids, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_context.ingest", {7}, sdk::v3::method_type::pure_function, __buffer);

				}
			};
*/			
			friend i_context_stub;
		};
		
		
		/****************************************************************************/
		class i_llm_stub;
		class i_llm ///: public rpc::casting_interface//OWL WAS HERE
		{
			public:
// 			OWL WAS HERE
// 			static rpc::interface_ordinal get_id(uint64_t rpc_version)
// 			{
// #ifdef RPC_V2
// 				if(rpc_version == rpc::VERSION_2)
// 				{
// 					//::secret_llama::v1_0::i_llm{[tag=sdk::v3::method_type::query]list_models([out]std::vector<15117921387636804375>& models,)[tag=sdk::v3::method_type::query]list_tokenizers([out]std::vector<5429187498066066378>& tokenizers,)[tag=sdk::v3::method_type::query]string_infer([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::string& prompt,[out]std::string& output,)[tag=sdk::v3::method_type::query]binary_infer([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::vector<uint8_t>& prompt,[out]std::vector<uint8_t>& output,)[tag=sdk::v3::method_type::query]start_string_chat([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::string& prompt,[out]std::string& context_id,)[tag=sdk::v3::method_type::query]continue_string_chat([const]std::string& context_id,[const]std::string& prompt,)[tag=sdk::v3::method_type::query]start_binary_chat([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::vector<uint8_t>& prompt,[out]std::string& context_id,)[tag=sdk::v3::method_type::query]continue_binary_chat([const]std::string& context_id,[const]std::vector<uint8_t>& prompt,)[tag=sdk::v3::method_type::query]heart_beat([const]std::string& context_id,)[tag=sdk::v3::method_type::query]create_context([const]std::string& model_name,[const]json::v1::map& overrides,[out]rpc::shared_ptr<7467689697922469476>& chat,)[tag=sdk::v3::method_type::transaction]add_account([const]std::string& sender,[]bool is_admin,[const]std::vector<uint8_t>& passkey,)[tag=sdk::v3::method_type::transaction]update_account([const]std::string& sender,[]bool is_admin,[const]std::vector<uint8_t>& passkey,)[tag=sdk::v3::method_type::transaction]remove_account([const]std::string& sender,)[tag=sdk::v3::method_type::query]list_accounts([out]std::vector<10232088079823478326>& accounts,)[tag=sdk::v3::method_type::transaction]save_model([const]5037444258246765236& model,)[tag=sdk::v3::method_type::transaction]remove_model([const]std::string& model_name,)[tag=sdk::v3::method_type::transaction]save_tokenizer([const]3998020412893845572& tokenizer,)[tag=sdk::v3::method_type::transaction]remove_tokenizer([const]std::string& tokenizer_name,)[tag=sdk::v3::method_type::transaction]load_model_into_ram([const]std::string& model_name,[]uint64_t chunk_size,)[tag=sdk::v3::method_type::transaction]unload_model_from_ram([const]std::string& model_name,)[tag=sdk::v3::method_type::queryconst]model_n_embd([const]std::string& model_name,[out]int32_t& n_embd,)[tag=sdk::v3::method_type::transaction]load_tokenizer_into_ram([const]std::string& tokenizer_name,[]uint64_t chunk_size,)[tag=sdk::v3::method_type::transaction]unload_tokenizer_from_ram([const]std::string& tokenizer_name,)[tag=sdk::v3::method_type::queryconst]list_files([const]std::string& local_path,[out]std::vector<file_system::file_description>& files,)[tag=sdk::v3::method_type::queryconst]remove_file([const]std::string& local_path,)[tag=sdk::v3::method_type::queryconst]download_file([const]std::string& url,[const]std::string& local_path,)[tag=sdk::v3::method_type::queryconst]download_status([const]std::string& local_path,[out]file_system::download_status& status,[out]size_t& downloaded_size,)}
// 					return {6128165235482876273ull};
// 				}
// #endif
// 				return {0};
// 			}
			
// 			static std::vector<rpc::function_info> get_function_info();
			
			virtual ~i_llm() = default;
			
			// ********************* interface methods *********************
			virtual int list_models(std::vector<model_list_item>& models) = 0;
			virtual int list_tokenizers(std::vector<tokenizer_list_item>& tokenizers) = 0;
			// virtual int string_infer(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::string& output) = 0;//OWL WAS HERE
			// virtual int binary_infer(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::vector<uint8_t>& output) = 0;//OWL WAS HERE
			// virtual int start_string_chat(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::string& context_id) = 0;//OWL WAS HERE
			virtual int continue_string_chat(const std::string& context_id, const std::string& prompt) = 0;
			// virtual int start_binary_chat(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::string& context_id) = 0;//OWL WAS HERE
			virtual int continue_binary_chat(const std::string& context_id, const std::vector<uint8_t>& prompt) = 0;
			virtual int heart_beat(const std::string& context_id) = 0;
			// virtual int create_context(const std::string& model_name, const json::v1::map& overrides, rpc::shared_ptr<i_context>& chat) = 0;//OWL WAS HERE
			virtual int add_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey) = 0;
			virtual int update_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey) = 0;
			virtual int remove_account(const std::string& sender) = 0;
			virtual int list_accounts(std::vector<account_list_item>& accounts) = 0;
			virtual int save_model(const llm_model& model) = 0;
			virtual int remove_model(const std::string& model_name) = 0;
			virtual int save_tokenizer(const struct tokenizer& tokenizer) = 0;
			virtual int remove_tokenizer(const std::string& tokenizer_name) = 0;
			virtual int load_model_into_ram(const std::string& model_name, uint64_t chunk_size) = 0;
			virtual int unload_model_from_ram(const std::string& model_name) = 0;
			virtual int model_n_embd(const std::string& model_name, int32_t& n_embd) const = 0;
			virtual int load_tokenizer_into_ram(const std::string& tokenizer_name, uint64_t chunk_size) = 0;
			virtual int unload_tokenizer_from_ram(const std::string& tokenizer_name) = 0;
			// virtual int list_files(const std::string& local_path, std::vector<file_system::file_description>& files) const = 0;//OWL WAS HERE
			virtual int remove_file(const std::string& local_path) const = 0;
			virtual int download_file(const std::string& url, const std::string& local_path) const = 0;
			// virtual int download_status(const std::string& local_path, file_system::download_status& status, size_t& downloaded_size) const = 0;//OWL WAS HERE
			
			public:
			// ********************* compile time polymorphic serialisers *********************
			// template pure static class for serialising proxy request data to a stub or some other target
			template<typename __Serialiser, typename... __Args>
			struct proxy_serialiser
			{
				static int list_models(std::vector<char>& __buffer, __Args... __args);
				static int list_tokenizers(std::vector<char>& __buffer, __Args... __args);
				// static int string_infer(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				// static int binary_infer(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				// static int start_string_chat(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int continue_string_chat(const std::string& context_id, const std::string& prompt, std::vector<char>& __buffer, __Args... __args);
				// static int start_binary_chat(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int continue_binary_chat(const std::string& context_id, const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int heart_beat(const std::string& context_id, std::vector<char>& __buffer, __Args... __args);
				// static int create_context(const std::string& model_name, const json::v1::map& overrides, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int add_account(const std::string& sender, const bool& is_admin, const std::vector<uint8_t>& passkey, std::vector<char>& __buffer, __Args... __args);
				static int update_account(const std::string& sender, const bool& is_admin, const std::vector<uint8_t>& passkey, std::vector<char>& __buffer, __Args... __args);
				static int remove_account(const std::string& sender, std::vector<char>& __buffer, __Args... __args);
				static int list_accounts(std::vector<char>& __buffer, __Args... __args);
				static int save_model(const llm_model& model, std::vector<char>& __buffer, __Args... __args);
				static int remove_model(const std::string& model_name, std::vector<char>& __buffer, __Args... __args);
				static int save_tokenizer(const tokenizer& tokenizer, std::vector<char>& __buffer, __Args... __args);
				static int remove_tokenizer(const std::string& tokenizer_name, std::vector<char>& __buffer, __Args... __args);
				static int load_model_into_ram(const std::string& model_name, const uint64_t& chunk_size, std::vector<char>& __buffer, __Args... __args);
				static int unload_model_from_ram(const std::string& model_name, std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(const std::string& model_name, std::vector<char>& __buffer, __Args... __args);
				static int load_tokenizer_into_ram(const std::string& tokenizer_name, const uint64_t& chunk_size, std::vector<char>& __buffer, __Args... __args);
				static int unload_tokenizer_from_ram(const std::string& tokenizer_name, std::vector<char>& __buffer, __Args... __args);
				static int list_files(const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
				static int remove_file(const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
				static int download_file(const std::string& url, const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
				static int download_status(const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for deserialising data from a proxy or some other target into a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_deserialiser
			{
				static int list_models(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_tokenizers(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int string_infer(std::string& model_name, json::v1::map& overrides, std::string& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				// static int binary_infer(std::string& model_name, json::v1::map& overrides, std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				// static int start_string_chat(std::string& model_name, json::v1::map& overrides, std::string& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int continue_string_chat(std::string& context_id, std::string& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int start_binary_chat(std::string& model_name, json::v1::map& overrides, std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int continue_binary_chat(std::string& context_id, std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int heart_beat(std::string& context_id, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int create_context(std::string& model_name, json::v1::map& overrides, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int add_account(std::string& sender, bool& is_admin, std::vector<uint8_t>& passkey, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int update_account(std::string& sender, bool& is_admin, std::vector<uint8_t>& passkey, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_account(std::string& sender, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_accounts(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_model(llm_model& model, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_model(std::string& model_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_tokenizer(tokenizer& tokenizer, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_tokenizer(std::string& tokenizer_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_model_into_ram(std::string& model_name, uint64_t& chunk_size, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_model_from_ram(std::string& model_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(std::string& model_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_tokenizer_into_ram(std::string& tokenizer_name, uint64_t& chunk_size, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_tokenizer_from_ram(std::string& tokenizer_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_files(std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_file(std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int download_file(std::string& url, std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int download_status(std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
			// template pure static class for serialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_serialiser
			{
				static int list_models(const std::vector<model_list_item>& models, std::vector<char>& __buffer, __Args... __args);
				static int list_tokenizers(const std::vector<tokenizer_list_item>& tokenizers, std::vector<char>& __buffer, __Args... __args);
				static int string_infer(const std::string& output, std::vector<char>& __buffer, __Args... __args);
				static int binary_infer(const std::vector<uint8_t>& output, std::vector<char>& __buffer, __Args... __args);
				static int start_string_chat(const std::string& context_id, std::vector<char>& __buffer, __Args... __args);
				static int continue_string_chat(std::vector<char>& __buffer, __Args... __args);
				static int start_binary_chat(const std::string& context_id, std::vector<char>& __buffer, __Args... __args);
				static int continue_binary_chat(std::vector<char>& __buffer, __Args... __args);
				static int heart_beat(std::vector<char>& __buffer, __Args... __args);
				// static int create_context(rpc::interface_descriptor& chat, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int add_account(std::vector<char>& __buffer, __Args... __args);
				static int update_account(std::vector<char>& __buffer, __Args... __args);
				static int remove_account(std::vector<char>& __buffer, __Args... __args);
				static int list_accounts(const std::vector<account_list_item>& accounts, std::vector<char>& __buffer, __Args... __args);
				static int save_model(std::vector<char>& __buffer, __Args... __args);
				static int remove_model(std::vector<char>& __buffer, __Args... __args);
				static int save_tokenizer(std::vector<char>& __buffer, __Args... __args);
				static int remove_tokenizer(std::vector<char>& __buffer, __Args... __args);
				static int load_model_into_ram(std::vector<char>& __buffer, __Args... __args);
				static int unload_model_from_ram(std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(const int32_t& n_embd, std::vector<char>& __buffer, __Args... __args);
				static int load_tokenizer_into_ram(std::vector<char>& __buffer, __Args... __args);
				static int unload_tokenizer_from_ram(std::vector<char>& __buffer, __Args... __args);
				// static int list_files(const std::vector<file_system::file_description>& files, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int remove_file(std::vector<char>& __buffer, __Args... __args);
				static int download_file(std::vector<char>& __buffer, __Args... __args);
				// static int download_status(const file_system::download_status& status, const size_t& downloaded_size, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
			};
			
			// template pure static class for a proxy deserialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct proxy_deserialiser
			{
				static int list_models(std::vector<model_list_item>& models, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_tokenizers(std::vector<tokenizer_list_item>& tokenizers, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int string_infer(std::string& output, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int binary_infer(std::vector<uint8_t>& output, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int start_string_chat(std::string& context_id, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int continue_string_chat(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int start_binary_chat(std::string& context_id, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int continue_binary_chat(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int heart_beat(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int create_context(rpc::interface_descriptor& chat, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int add_account(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int update_account(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_account(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_accounts(std::vector<account_list_item>& accounts, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_model(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_model(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_tokenizer(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_tokenizer(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_model_into_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_model_from_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(int32_t& n_embd, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_tokenizer_into_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_tokenizer_from_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int list_files(std::vector<file_system::file_description>& files, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int remove_file(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int download_file(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int download_status(file_system::download_status& status, size_t& downloaded_size, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
			};
			
			// proxy class for serialising requests into a buffer for optional dispatch at a future time
/* OWL WAS HERE
			template<class Parent, typename ReturnType>
			class buffered_proxy_serialiser
			{
				public:
				using subclass = Parent;
				ReturnType continue_string_chat(const std::string& context_id, const std::string& prompt)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::continue_string_chat(context_id, prompt, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.continue_string_chat", {6}, sdk::v3::method_type::query, __buffer);

				}
				ReturnType continue_binary_chat(const std::string& context_id, const std::vector<uint8_t>& prompt)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::continue_binary_chat(context_id, prompt, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.continue_binary_chat", {8}, sdk::v3::method_type::query, __buffer);

				}
				ReturnType heart_beat(const std::string& context_id)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::heart_beat(context_id, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.heart_beat", {9}, sdk::v3::method_type::query, __buffer);

				}
				ReturnType add_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::add_account(sender, is_admin, passkey, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.add_account", {11}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType update_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::update_account(sender, is_admin, passkey, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.update_account", {12}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType remove_account(const std::string& sender)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_account(sender, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.remove_account", {13}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType save_model(const llm_model& model)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::save_model(model, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.save_model", {15}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType remove_model(const std::string& model_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_model(model_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.remove_model", {16}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType save_tokenizer(const struct tokenizer& tokenizer)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::save_tokenizer(tokenizer, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.save_tokenizer", {17}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType remove_tokenizer(const std::string& tokenizer_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_tokenizer(tokenizer_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.remove_tokenizer", {18}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType load_model_into_ram(const std::string& model_name, uint64_t chunk_size)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::load_model_into_ram(model_name, chunk_size, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.load_model_into_ram", {19}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType unload_model_from_ram(const std::string& model_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::unload_model_from_ram(model_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.unload_model_from_ram", {20}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType load_tokenizer_into_ram(const std::string& tokenizer_name, uint64_t chunk_size)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::load_tokenizer_into_ram(tokenizer_name, chunk_size, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.load_tokenizer_into_ram", {22}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType unload_tokenizer_from_ram(const std::string& tokenizer_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::unload_tokenizer_from_ram(tokenizer_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.unload_tokenizer_from_ram", {23}, sdk::v3::method_type::transaction, __buffer);

				}
				ReturnType remove_file(const std::string& local_path) const
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_file(local_path, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.remove_file", {25}, sdk::v3::method_type::query, __buffer);

				}
				ReturnType download_file(const std::string& url, const std::string& local_path) const
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::download_file(url, local_path, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm.download_file", {26}, sdk::v3::method_type::query, __buffer);

				}
			};
*/

			friend i_llm_stub;
		};
		
		
		/****************************************************************************/
		class i_context_host_stub;
		class i_context_host// : public rpc::casting_interface//OWL WAS HERE
		{
			public:
/* OWL WAS HERE
			static rpc::interface_ordinal get_id(uint64_t rpc_version)
			{
#ifdef RPC_V2
				if(rpc_version == rpc::VERSION_2)
				{
					//::secret_llama::v1_0::i_context_host{[]add_prompt([const]std::vector<uint8_t>& prompt,)[]get_piece([out]std::vector<uint8_t>& piece,[out]bool& complete,)[]model_n_embd([out]int32_t& n_embd,)[]get_aggregate_embeddings([]uint32_t window_size,[]7410425521722818471 agg_rule,[out]std::vector<float>& embeddings,)[]encode([const]std::vector<uint8_t>& prompt,[out]std::vector<int32_t>& token_ids,)[]decode([const]std::vector<int32_t>& token_ids,[out]std::vector<uint8_t>& prompt,)[]ingest([const]std::vector<int32_t>& token_ids,)}
					return {12485719200977370610ull};
				}
#endif
				return {0};
			}
			
			static std::vector<rpc::function_info> get_function_info();
*/			
			virtual ~i_context_host() = default;
			
			// ********************* interface methods *********************
			virtual int add_prompt(const std::vector<uint8_t>& prompt) = 0;
			virtual int get_piece(std::vector<uint8_t>& piece, bool& complete) = 0;
			virtual int model_n_embd(int32_t& n_embd) = 0;
			virtual int get_aggregate_embeddings(uint32_t window_size, aggregation_rule agg_rule, std::vector<float>& embeddings) = 0;
			virtual int encode(const std::vector<uint8_t>& prompt, std::vector<int32_t>& token_ids) = 0;
			virtual int decode(const std::vector<int32_t>& token_ids, std::vector<uint8_t>& prompt) = 0;
			virtual int ingest(const std::vector<int32_t>& token_ids) = 0;
			
			public:
			// ********************* compile time polymorphic serialisers *********************
			// template pure static class for serialising proxy request data to a stub or some other target
			template<typename __Serialiser, typename... __Args>
			struct proxy_serialiser
			{
				static int add_prompt(const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int get_piece(std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(std::vector<char>& __buffer, __Args... __args);
				static int get_aggregate_embeddings(const uint32_t& window_size, const aggregation_rule& agg_rule, std::vector<char>& __buffer, __Args... __args);
				static int encode(const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int decode(const std::vector<int32_t>& token_ids, std::vector<char>& __buffer, __Args... __args);
				static int ingest(const std::vector<int32_t>& token_ids, std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for deserialising data from a proxy or some other target into a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_deserialiser
			{
				static int add_prompt(std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_piece(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_aggregate_embeddings(uint32_t& window_size, aggregation_rule& agg_rule, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int encode(std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int decode(std::vector<int32_t>& token_ids, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int ingest(std::vector<int32_t>& token_ids, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
			// template pure static class for serialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_serialiser
			{
				static int add_prompt(std::vector<char>& __buffer, __Args... __args);
				static int get_piece(const std::vector<uint8_t>& piece, const bool& complete, std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(const int32_t& n_embd, std::vector<char>& __buffer, __Args... __args);
				static int get_aggregate_embeddings(const std::vector<float>& embeddings, std::vector<char>& __buffer, __Args... __args);
				static int encode(const std::vector<int32_t>& token_ids, std::vector<char>& __buffer, __Args... __args);
				static int decode(const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int ingest(std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for a proxy deserialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct proxy_deserialiser
			{
				static int add_prompt(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_piece(std::vector<uint8_t>& piece, bool& complete, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(int32_t& n_embd, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int get_aggregate_embeddings(std::vector<float>& embeddings, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int encode(std::vector<int32_t>& token_ids, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int decode(std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int ingest(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
			// proxy class for serialising requests into a buffer for optional dispatch at a future time
/* OWL WAS HERE
			template<class Parent, typename ReturnType>
			class buffered_proxy_serialiser
			{
				public:
				using subclass = Parent;
				ReturnType add_prompt(const std::vector<uint8_t>& prompt)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::add_prompt(prompt, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_context_host.add_prompt", {1}, 0, __buffer);

				}
				ReturnType ingest(const std::vector<int32_t>& token_ids)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::ingest(token_ids, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_context_host.ingest", {7}, 0, __buffer);

				}
			};
*/

			friend i_context_host_stub;
		};
		
		
		/****************************************************************************/
		class i_llm_host_stub;
		class i_llm_host// : public rpc::casting_interface//OWL WAS HERE
		{
			public:
/*
			static rpc::interface_ordinal get_id(uint64_t rpc_version)
			{
#ifdef RPC_V2
				if(rpc_version == rpc::VERSION_2)
				{
					//::secret_llama::v1_0::i_llm_host{[]list_models([out]std::vector<15117921387636804375>& models,)[]list_tokenizers([out]std::vector<5429187498066066378>& tokenizers,)[]string_infer([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::string& prompt,[out]std::string& output,)[]binary_infer([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::vector<uint8_t>& prompt,[out]std::vector<uint8_t>& output,)[]start_string_chat([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::string& prompt,[out]std::string& context_id,)[]continue_string_chat([const]std::string& context_id,[const]std::string& prompt,)[]start_binary_chat([const]std::string& model_name,[const]json::v1::map& overrides,[const]std::vector<uint8_t>& prompt,[out]std::string& context_id,)[]continue_binary_chat([const]std::string& context_id,[const]std::vector<uint8_t>& prompt,)[]heart_beat([const]std::string& context_id,)[]create_context([const]std::string& model_name,[const]json::v1::map& overrides,[out]rpc::shared_ptr<12485719200977370610>& chat,)[]add_account([const]std::string& sender,[]bool is_admin,[const]std::vector<uint8_t>& passkey,)[]update_account([const]std::string& sender,[]bool is_admin,[const]std::vector<uint8_t>& passkey,)[]remove_account([const]std::string& sender,)[]list_accounts([out]std::vector<10232088079823478326>& accounts,)[]save_model([const]5037444258246765236& model,)[]remove_model([const]std::string& model_name,)[]save_tokenizer([const]3998020412893845572& tokenizer,)[]remove_tokenizer([const]std::string& tokenizer_name,)[]load_model_into_ram([const]std::string& model_name,[]uint64_t chunk_size,)[]unload_model_from_ram([const]std::string& model_name,)[const]model_n_embd([const]std::string& model_name,[]int32_t& n_embd,)[]load_tokenizer_into_ram([const]std::string& tokenizer_name,[]uint64_t chunk_size,)[]unload_tokenizer_from_ram([const]std::string& tokenizer_name,)[const]list_files([const]std::string& local_path,[out]std::vector<file_system::file_description>& files,)[const]remove_file([const]std::string& local_path,)[const]download_file([const]std::string& url,[const]std::string& local_path,)[const]download_status([const]std::string& local_path,[out]file_system::download_status& status,[out]size_t& downloaded_size,)}
					return {12945847096657495081ull};
				}
#endif
				return {0};
			}
			
			static std::vector<rpc::function_info> get_function_info();
*/			
			virtual ~i_llm_host() = default;
			
			// ********************* interface methods *********************
			virtual int list_models(std::vector<model_list_item>& models) = 0;
			virtual int list_tokenizers(std::vector<tokenizer_list_item>& tokenizers) = 0;
			// virtual int string_infer(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::string& output) = 0;//OWL WAS HERE
			// virtual int binary_infer(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::vector<uint8_t>& output) = 0;//OWL WAS HERE
			// virtual int start_string_chat(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::string& context_id) = 0;//OWL WAS HERE
			virtual int continue_string_chat(const std::string& context_id, const std::string& prompt) = 0;
//			virtual int start_binary_chat(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::string& context_id) = 0;//OWL WAS HERE
			virtual int continue_binary_chat(const std::string& context_id, const std::vector<uint8_t>& prompt) = 0;
			virtual int heart_beat(const std::string& context_id) = 0;
//			virtual int create_context(const std::string& model_name, const json::v1::map& overrides, rpc::shared_ptr<i_context_host>& chat) = 0;//OWL WAS HERE
			virtual int add_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey) = 0;
			virtual int update_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey) = 0;
			virtual int remove_account(const std::string& sender) = 0;
			virtual int list_accounts(std::vector<account_list_item>& accounts) = 0;
			virtual int save_model(const llm_model& model) = 0;
			virtual int remove_model(const std::string& model_name) = 0;
			virtual int save_tokenizer(const struct tokenizer& tokenizer) = 0;
			virtual int remove_tokenizer(const std::string& tokenizer_name) = 0;
			virtual int load_model_into_ram(const std::string& model_name, uint64_t chunk_size) = 0;
			virtual int unload_model_from_ram(const std::string& model_name) = 0;
			virtual int model_n_embd(const std::string& model_name, int32_t& n_embd) const = 0;
			virtual int load_tokenizer_into_ram(const std::string& tokenizer_name, uint64_t chunk_size) = 0;
			virtual int unload_tokenizer_from_ram(const std::string& tokenizer_name) = 0;
//			virtual int list_files(const std::string& local_path, std::vector<file_system::file_description>& files) const = 0;//OWL WAS HERE
			virtual int remove_file(const std::string& local_path) const = 0;
			virtual int download_file(const std::string& url, const std::string& local_path) const = 0;
//			virtual int download_status(const std::string& local_path, file_system::download_status& status, size_t& downloaded_size) const = 0;//OWL WAS HERE
			
			public:
			// ********************* compile time polymorphic serialisers *********************
			// template pure static class for serialising proxy request data to a stub or some other target
			template<typename __Serialiser, typename... __Args>
			struct proxy_serialiser
			{
				static int list_models(std::vector<char>& __buffer, __Args... __args);
				static int list_tokenizers(std::vector<char>& __buffer, __Args... __args);
				// static int string_infer(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				// static int binary_infer(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				// static int start_string_chat(const std::string& model_name, const json::v1::map& overrides, const std::string& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int continue_string_chat(const std::string& context_id, const std::string& prompt, std::vector<char>& __buffer, __Args... __args);
				// static int start_binary_chat(const std::string& model_name, const json::v1::map& overrides, const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int continue_binary_chat(const std::string& context_id, const std::vector<uint8_t>& prompt, std::vector<char>& __buffer, __Args... __args);
				static int heart_beat(const std::string& context_id, std::vector<char>& __buffer, __Args... __args);
				// static int create_context(const std::string& model_name, const json::v1::map& overrides, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int add_account(const std::string& sender, const bool& is_admin, const std::vector<uint8_t>& passkey, std::vector<char>& __buffer, __Args... __args);
				static int update_account(const std::string& sender, const bool& is_admin, const std::vector<uint8_t>& passkey, std::vector<char>& __buffer, __Args... __args);
				static int remove_account(const std::string& sender, std::vector<char>& __buffer, __Args... __args);
				static int list_accounts(std::vector<char>& __buffer, __Args... __args);
				static int save_model(const llm_model& model, std::vector<char>& __buffer, __Args... __args);
				static int remove_model(const std::string& model_name, std::vector<char>& __buffer, __Args... __args);
				static int save_tokenizer(const tokenizer& tokenizer, std::vector<char>& __buffer, __Args... __args);
				static int remove_tokenizer(const std::string& tokenizer_name, std::vector<char>& __buffer, __Args... __args);
				static int load_model_into_ram(const std::string& model_name, const uint64_t& chunk_size, std::vector<char>& __buffer, __Args... __args);
				static int unload_model_from_ram(const std::string& model_name, std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(const std::string& model_name, const int32_t& n_embd, std::vector<char>& __buffer, __Args... __args);
				static int load_tokenizer_into_ram(const std::string& tokenizer_name, const uint64_t& chunk_size, std::vector<char>& __buffer, __Args... __args);
				static int unload_tokenizer_from_ram(const std::string& tokenizer_name, std::vector<char>& __buffer, __Args... __args);
				static int list_files(const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
				static int remove_file(const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
				static int download_file(const std::string& url, const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
				static int download_status(const std::string& local_path, std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for deserialising data from a proxy or some other target into a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_deserialiser
			{
				static int list_models(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_tokenizers(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int string_infer(std::string& model_name, json::v1::map& overrides, std::string& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				// static int binary_infer(std::string& model_name, json::v1::map& overrides, std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				// static int start_string_chat(std::string& model_name, json::v1::map& overrides, std::string& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int continue_string_chat(std::string& context_id, std::string& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int start_binary_chat(std::string& model_name, json::v1::map& overrides, std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int continue_binary_chat(std::string& context_id, std::vector<uint8_t>& prompt, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int heart_beat(std::string& context_id, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int create_context(std::string& model_name, json::v1::map& overrides, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int add_account(std::string& sender, bool& is_admin, std::vector<uint8_t>& passkey, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int update_account(std::string& sender, bool& is_admin, std::vector<uint8_t>& passkey, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_account(std::string& sender, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_accounts(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_model(llm_model& model, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_model(std::string& model_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_tokenizer(tokenizer& tokenizer, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_tokenizer(std::string& tokenizer_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_model_into_ram(std::string& model_name, uint64_t& chunk_size, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_model_from_ram(std::string& model_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(std::string& model_name, int32_t& n_embd, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_tokenizer_into_ram(std::string& tokenizer_name, uint64_t& chunk_size, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_tokenizer_from_ram(std::string& tokenizer_name, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_files(std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_file(std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int download_file(std::string& url, std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int download_status(std::string& local_path, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
			// template pure static class for serialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_serialiser
			{
				static int list_models(const std::vector<model_list_item>& models, std::vector<char>& __buffer, __Args... __args);
				static int list_tokenizers(const std::vector<tokenizer_list_item>& tokenizers, std::vector<char>& __buffer, __Args... __args);
				static int string_infer(const std::string& output, std::vector<char>& __buffer, __Args... __args);
				static int binary_infer(const std::vector<uint8_t>& output, std::vector<char>& __buffer, __Args... __args);
				static int start_string_chat(const std::string& context_id, std::vector<char>& __buffer, __Args... __args);
				static int continue_string_chat(std::vector<char>& __buffer, __Args... __args);
				static int start_binary_chat(const std::string& context_id, std::vector<char>& __buffer, __Args... __args);
				static int continue_binary_chat(std::vector<char>& __buffer, __Args... __args);
				static int heart_beat(std::vector<char>& __buffer, __Args... __args);
				// static int create_context(rpc::interface_descriptor& chat, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int add_account(std::vector<char>& __buffer, __Args... __args);
				static int update_account(std::vector<char>& __buffer, __Args... __args);
				static int remove_account(std::vector<char>& __buffer, __Args... __args);
				static int list_accounts(const std::vector<account_list_item>& accounts, std::vector<char>& __buffer, __Args... __args);
				static int save_model(std::vector<char>& __buffer, __Args... __args);
				static int remove_model(std::vector<char>& __buffer, __Args... __args);
				static int save_tokenizer(std::vector<char>& __buffer, __Args... __args);
				static int remove_tokenizer(std::vector<char>& __buffer, __Args... __args);
				static int load_model_into_ram(std::vector<char>& __buffer, __Args... __args);
				static int unload_model_from_ram(std::vector<char>& __buffer, __Args... __args);
				static int model_n_embd(std::vector<char>& __buffer, __Args... __args);
				static int load_tokenizer_into_ram(std::vector<char>& __buffer, __Args... __args);
				static int unload_tokenizer_from_ram(std::vector<char>& __buffer, __Args... __args);
				// static int list_files(const std::vector<file_system::file_description>& files, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
				static int remove_file(std::vector<char>& __buffer, __Args... __args);
				static int download_file(std::vector<char>& __buffer, __Args... __args);
				// static int download_status(const file_system::download_status& status, const size_t& downloaded_size, std::vector<char>& __buffer, __Args... __args);//OWL WAS HERE
			};
			
			// template pure static class for a proxy deserialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct proxy_deserialiser
			{
				static int list_models(std::vector<model_list_item>& models, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_tokenizers(std::vector<tokenizer_list_item>& tokenizers, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int string_infer(std::string& output, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int binary_infer(std::vector<uint8_t>& output, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int start_string_chat(std::string& context_id, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int continue_string_chat(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int start_binary_chat(std::string& context_id, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int continue_binary_chat(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int heart_beat(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int create_context(rpc::interface_descriptor& chat, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int add_account(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int update_account(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_account(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int list_accounts(std::vector<account_list_item>& accounts, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_model(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_model(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int save_tokenizer(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int remove_tokenizer(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_model_into_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_model_from_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int model_n_embd(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int load_tokenizer_into_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int unload_tokenizer_from_ram(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int list_files(std::vector<file_system::file_description>& files, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
				static int remove_file(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				static int download_file(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
				// static int download_status(file_system::download_status& status, size_t& downloaded_size, const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);//OWL WAS HERE
			};
			
			// proxy class for serialising requests into a buffer for optional dispatch at a future time
/* OWL WAS HERE
			template<class Parent, typename ReturnType>
			class buffered_proxy_serialiser
			{
				public:
				using subclass = Parent;
				ReturnType continue_string_chat(const std::string& context_id, const std::string& prompt)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::continue_string_chat(context_id, prompt, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.continue_string_chat", {6}, 0, __buffer);

				}
				ReturnType continue_binary_chat(const std::string& context_id, const std::vector<uint8_t>& prompt)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::continue_binary_chat(context_id, prompt, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.continue_binary_chat", {8}, 0, __buffer);

				}
				ReturnType heart_beat(const std::string& context_id)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::heart_beat(context_id, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.heart_beat", {9}, 0, __buffer);

				}
				ReturnType add_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::add_account(sender, is_admin, passkey, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.add_account", {11}, 0, __buffer);

				}
				ReturnType update_account(const std::string& sender, bool is_admin, const std::vector<uint8_t>& passkey)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::update_account(sender, is_admin, passkey, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.update_account", {12}, 0, __buffer);

				}
				ReturnType remove_account(const std::string& sender)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_account(sender, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.remove_account", {13}, 0, __buffer);

				}
				ReturnType save_model(const llm_model& model)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::save_model(model, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.save_model", {15}, 0, __buffer);

				}
				ReturnType remove_model(const std::string& model_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_model(model_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.remove_model", {16}, 0, __buffer);

				}
				ReturnType save_tokenizer(const struct tokenizer& tokenizer)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::save_tokenizer(tokenizer, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.save_tokenizer", {17}, 0, __buffer);

				}
				ReturnType remove_tokenizer(const std::string& tokenizer_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_tokenizer(tokenizer_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.remove_tokenizer", {18}, 0, __buffer);

				}
				ReturnType load_model_into_ram(const std::string& model_name, uint64_t chunk_size)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::load_model_into_ram(model_name, chunk_size, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.load_model_into_ram", {19}, 0, __buffer);

				}
				ReturnType unload_model_from_ram(const std::string& model_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::unload_model_from_ram(model_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.unload_model_from_ram", {20}, 0, __buffer);

				}
				ReturnType model_n_embd(const std::string& model_name, int32_t& n_embd) const
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::model_n_embd(model_name, n_embd, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.model_n_embd", {21}, 0, __buffer);

				}
				ReturnType load_tokenizer_into_ram(const std::string& tokenizer_name, uint64_t chunk_size)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::load_tokenizer_into_ram(tokenizer_name, chunk_size, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.load_tokenizer_into_ram", {22}, 0, __buffer);

				}
				ReturnType unload_tokenizer_from_ram(const std::string& tokenizer_name)
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::unload_tokenizer_from_ram(tokenizer_name, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.unload_tokenizer_from_ram", {23}, 0, __buffer);

				}
				ReturnType remove_file(const std::string& local_path) const
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::remove_file(local_path, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.remove_file", {25}, 0, __buffer);

				}
				ReturnType download_file(const std::string& url, const std::string& local_path) const
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::download_file(url, local_path, __buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_llm_host.download_file", {26}, 0, __buffer);

				}
			};
*/

			friend i_llm_host_stub;
		};
		
		
		/****************************************************************************/
		class i_test_stub;
		class i_test// : public rpc::casting_interface//OWL WAS HERE
		{
			public:
/* OWL WAS HERE
			static rpc::interface_ordinal get_id(uint64_t rpc_version)
			{
#ifdef RPC_V2
				if(rpc_version == rpc::VERSION_2)
				{
					//::secret_llama::v1_0::i_test{[tag=sdk::v3::method_type::query]run()}
					return {16270105671066644041ull};
				}
#endif
				return {0};
			}
			
			static std::vector<rpc::function_info> get_function_info();
*/			
			virtual ~i_test() = default;
			
			// ********************* interface methods *********************
			virtual int run() = 0;
			
			public:
			// ********************* compile time polymorphic serialisers *********************
			// template pure static class for serialising proxy request data to a stub or some other target
			template<typename __Serialiser, typename... __Args>
			struct proxy_serialiser
			{
				static int run(std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for deserialising data from a proxy or some other target into a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_deserialiser
			{
				static int run(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
			// template pure static class for serialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct stub_serialiser
			{
				static int run(std::vector<char>& __buffer, __Args... __args);
			};
			
			// template pure static class for a proxy deserialising reply data from a stub
			template<typename __Serialiser, typename... __Args>
			struct proxy_deserialiser
			{
				static int run(const char* __rpc_buf, size_t __rpc_buf_size, __Args... __args);
			};
			
			// proxy class for serialising requests into a buffer for optional dispatch at a future time
/* OWL WAS HERE
			template<class Parent, typename ReturnType>
			class buffered_proxy_serialiser
			{
				public:
				using subclass = Parent;
				ReturnType run()
				{
					std::vector<char> __buffer;
					auto __this = static_cast<subclass*>(this);
					auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::run(__buffer, __this->get_encoding());
					return __this->register_call(__err, "secret_llama.v1_0.i_test.run", {1}, sdk::v3::method_type::query, __buffer);

				}
			};
*/

			friend i_test_stub;
		};
		
	}
}

/****************************************************************************/
namespace rpc
{
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::llm_model>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::llm_model{std::string table_name, std::string name, std::string local_path, std::string url, std::string description, 7410425521722818471 engine_type, json::v1::map engine_config, 7410425521722818471 encryption_type, std::vector<uint8_t> encryption_key, 7410425521722818471 hash_type, std::vector<uint8_t> hash, bool is_loaded, 7410425521722818471 access, uint64_t inactivitiy_timeout}
				auto id = 5037444258246765236ull;
				return id;
			}
#endif
			return 0;
		}
	};
*/
	
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::tokenizer>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::tokenizer{std::string table_name, std::string name, std::string local_path, std::string url, std::string description, 7410425521722818471 model_format, 7410425521722818471 engine_type, 7410425521722818471 tensor_type, 7410425521722818471 encryption_type, std::vector<uint8_t> encryption_key, 7410425521722818471 hash_type, std::vector<uint8_t> hash, bool is_loaded, 7410425521722818471 access}
				auto id = 3998020412893845572ull;
				return id;
			}
#endif
			return 0;
		}
	};
*/	
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::model_list_item>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::model_list_item{std::string name, std::string local_path, std::string url, std::string description, 7410425521722818471 engine_type, json::v1::map engine_config, 7410425521722818471 encryption_type, 7410425521722818471 hash_type, std::vector<uint8_t> hash, file_system::download_status status, uint64_t file_size, bool is_loaded, 7410425521722818471 access, uint64_t inactivitiy_timeout}
				auto id = 15117921387636804375ull;
				return id;
			}
#endif
			return 0;
		}
	};
*/	
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::tokenizer_list_item>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::tokenizer_list_item{std::string name, std::string local_path, std::string url, std::string description, 7410425521722818471 model_format, 7410425521722818471 engine_type, 7410425521722818471 tensor_type, 7410425521722818471 encryption_type, 7410425521722818471 hash_type, std::vector<uint8_t> hash, file_system::download_status status, uint64_t file_size, bool is_loaded, 7410425521722818471 access}
				auto id = 5429187498066066378ull;
				return id;
			}
#endif
			return 0;
		}
	};
*/
	
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::account>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::account{std::string table_name, std::string name, 7410425521722818471 role, std::vector<uint8_t> passkey}
				auto id = 3851587480228827667ull;
				return id;
			}
#endif
			return 0;
		}
	};
*/	
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::account_list_item>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::account_list_item{std::string name, 7410425521722818471 role}
				auto id = 10232088079823478326ull;
				return id;
			}
#endif
			return 0;
		}
	};
*/	
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::chat_response_piece_binary>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::chat_response_piece_binary{7410425521722818471 state, std::vector<uint8_t> piece}
				auto id = 1941073051621372831ull;
				return id;
			}
#endif
			return 0;
		}
	};
*/	
	
	/****************************************************************************/
/*
	template<>
	class id<::secret_llama::v1_0::chat_response_piece_string>
	{
		public:
		static constexpr uint64_t get(uint64_t rpc_version)
		{
#ifdef RPC_V2
			if(rpc_version == rpc::VERSION_2)
			{
				//struct::::secret_llama::v1_0::chat_response_piece_string{7410425521722818471 state, std::string piece}
				auto id = 4022467270635439436ull;
				return id;
			}
#endif
			return 0;
		}
	};
	
	template<> rpc::interface_descriptor rpc::service::proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<::secret_llama::v1_0::i_context>& iface, rpc::shared_ptr<rpc::object_stub>& stub);
	template<> rpc::interface_descriptor rpc::service::stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<::secret_llama::v1_0::i_context>& iface);
	template<> rpc::interface_descriptor rpc::service::proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<::secret_llama::v1_0::i_llm>& iface, rpc::shared_ptr<rpc::object_stub>& stub);
	template<> rpc::interface_descriptor rpc::service::stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<::secret_llama::v1_0::i_llm>& iface);
	template<> rpc::interface_descriptor rpc::service::proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<::secret_llama::v1_0::i_context_host>& iface, rpc::shared_ptr<rpc::object_stub>& stub);
	template<> rpc::interface_descriptor rpc::service::stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<::secret_llama::v1_0::i_context_host>& iface);
	template<> rpc::interface_descriptor rpc::service::proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<::secret_llama::v1_0::i_llm_host>& iface, rpc::shared_ptr<rpc::object_stub>& stub);
	template<> rpc::interface_descriptor rpc::service::stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<::secret_llama::v1_0::i_llm_host>& iface);
	template<> rpc::interface_descriptor rpc::service::proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<::secret_llama::v1_0::i_test>& iface, rpc::shared_ptr<rpc::object_stub>& stub);
	template<> rpc::interface_descriptor rpc::service::stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<::secret_llama::v1_0::i_test>& iface);
*/
}
