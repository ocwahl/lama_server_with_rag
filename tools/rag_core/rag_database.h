#ifndef RAG_DATABASE_H
#define RAG_DATABASE_H

#include <string>
#include <vector>
#include <tuple>
#include <memory>

#include "common.h" // Assuming this provides llama_tokens and nlohmann::json
#include "utils.hpp" // Assuming this provides general utilities

#include "crypto_utils.h" // Include the core crypto utilities header
#include "ecies_utils.h"  // Include the new ECIES utilities header

using json = nlohmann::ordered_json;

// Structure to hold RAG chunk data
struct rag_chunk{
    int index = 0;
    std::string document_id; // Changed from int to std::string
    llama_tokens tokens; // Will originally stay empty, to be added later
    std::string contents; // Will originally stay empty, to be added later
    std::vector<float> embedding;
    int32_t n_tokens;

    inline json to_json() {
        return json {
            {"index",          index},
            {"document_id",    document_id},
            {"tokens",         tokens},
            {"content",        contents},
            {"embedding",      embedding},
            {"tokens_evaluated", n_tokens},
        };
    }
};

// Structure to hold document metadata
struct document_entry {
    std::string document_id; // Changed from int to std::string for hash
    std::string date;
    std::string version;
    std::string content_type;
    std::string url;
    int length;

    document_entry(std::string doc_id, std::string d, std::string v,
                   std::string ct, std::string u, int l)
        : document_id(std::move(doc_id)), date(std::move(d)), version(std::move(v)),
          content_type(std::move(ct)), url(std::move(u)), length(l) {}
};

// Enum for distance metrics
enum class DistanceMetric {
    COSINE, // <-> operator
    L2,     // <#> operator (Euclidean)
    IP      // <%> operator (Inner Product - usually 1 - cosine_similarity for normalized vectors, or negative dot product)
};

using additional_filtering_clause = std::function<std::string(const std::string& r_alias, const std::string& d_alias, const std::string& ec_alias)>;

class rag_database {
public:
    virtual ~rag_database() = default;

    virtual void connect(const std::string& user, const std::string& password) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    virtual void createSchema(size_t embedding_size) = 0;
    virtual bool hasSchema() = 0;
    virtual void destroySchema() = 0;
    virtual void setUser(const std::string& user) = 0;
    virtual void setPassword(const std::string& password) = 0;

    virtual std::string get_host_name() const = 0;
    virtual int get_port() const = 0;
    virtual std::string get_name() const = 0;


    virtual document_entry createOrRetrieveDocument(
        const std::string& date,
        const std::string& version,
        const std::string& content_type,
        const std::string& url,
        int length) = 0;

    virtual void deleteDocument(const std::string& document_id) = 0;

    // Updated signature to reflect ECIES and new types
    virtual void insertRagEntry(const std::string& document_id_hash, // Document ID (hash)
                                const std::vector<float>& embedding,
                                const std::vector<uint8_t>& contents, // Raw binary content
                                const ecc256_public_key& controller_public_key, // Controller's public key
                                const ecc256_private_key& recipient_private_key) = 0; // Recipient's private key for ECIES

    // Updated return tuple to match postgres_client's search results
    // (document_id, embedding, hash, offset, length, controller_public_key, encryption_public_key (recipient's),
    //  doc_date, doc_version, doc_content_type, doc_url, doc_length,
    //  encrypted_content, aes_gcm_tag, aes_gcm_nonce)
    virtual std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
                                   ecc256_public_key, ecc256_public_key, // controller_public_key, encryption_public_key (recipient's)
                                   std::string, std::string, std::string, std::string, int, // Document metadata
                                   std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce, // encrypted_content, tag, nonce
                                   ecc256_public_key //ephemereal_pk
                                   >>
    searchNearest(const std::vector<float>& query_embedding, int k, const additional_filtering_clause& filter_clause = nullptr, DistanceMetric distance_metric = DistanceMetric::COSINE) = 0;

    // // Search methods below also need their return types updated to match searchNearest
    // virtual std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
    //                                ecc256_public_key, ecc256_public_key,
    //                                std::string, std::string, std::string, std::string, int,
    //                                std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce
    //                                >>
    // searchByControllerKey(const std::string& controller_key) = 0; // Parameter remains string (hex)

    // virtual std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
    //                                ecc256_public_key, ecc256_public_key,
    //                                std::string, std::string, std::string, std::string, int,
    //                                std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce
    //                                >>
    // searchByEncryptionKey(const std::string& encryption_key) = 0; // Parameter remains string (hex)

    // virtual std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
    //                                ecc256_public_key, ecc256_public_key,
    //                                std::string, std::string, std::string, std::string, int,
    //                                std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce
    //                                >>
    // searchByDocumentContentType(const std::string& content_type) = 0;

    // virtual std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
    //                                ecc256_public_key, ecc256_public_key,
    //                                std::string, std::string, std::string, std::string, int,
    //                                std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce
    //                                >>
    // searchByControllerKeyAndDocumentDateRange(const std::string& controller_key, const std::string& start_date, const std::string& end_date) = 0;
};

std::shared_ptr<rag_database> create_rag_database(const std::string& host_name = "localhost", int port = 5432, const std::string& db_name = "klave_rag");

#endif // RAG_DATABASE_H
