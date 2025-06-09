#ifndef POSTGRES_CLIENT_H
#define POSTGRES_CLIENT_H

//#include <libpq-fe.h>
#include <vector>
#include <string>
#include <array>
#include <tuple> // For std::tuple in searchNearest
#include <vector> // For std::vector<uint8_t> parameters

#include "crypto_utils.h" // Include the refactored crypto utilities
#include "ecies_utils.h"  // Include the new ECIES utilities
#include "rag_database.h"


struct pg_conn;
typedef struct pg_conn PGconn;


class postgres_client : public rag_database {
public:
    postgres_client(const std::string& host = "localhost",
                    int port = 5432,
                    const std::string& dbname = "klave_rag",
                    const std::string& user = "postgres",
                    const std::string& rag_table = "rag_entries",
                    const std::string& rag_embedding_col = "embedding",
                    const std::string& doc_table = "documents",
                    const std::string& encrypted_content_table = "encrypted_rag_contents");
    ~postgres_client();

    void connect(const std::string& user = "", const std::string& password = "") override;
    void setUser(const std::string& user) override;
    void setPassword(const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;

    std::string get_host_name() const override;
    int get_port() const override;
    std::string get_name() const override;

    // Schema management
    bool hasSchema();
    void createSchema(size_t embedding_size);
    void destroySchema();

    // Document management
    document_entry createOrRetrieveDocument(
        const std::string& date,
        const std::string& version,
        const std::string& content_type,
        const std::string& url,
        int length);

    void deleteDocument(const std::string& document_id);

    // RAG entry management
    void insertRagEntry(const std::string& document_id_hash,
                        const std::vector<float>& embedding,
                        const std::vector<uint8_t>& contents, // Changed to vector<uint8_t> for raw bytes
                        const ecc256_public_key& controller_public_key, // Changed to ecc256_public_key
                        const ecc256_private_key& recipient_private_key); // Changed to ecc256_private_key for decryption

    // Search
    // The tuple return type is updated to reflect the new column types and order,
    // especially for the crypto-related fields.
    std::vector<nearest_result> searchNearest(const std::vector<float>& query_embedding, int n_retrievals, const additional_filtering_clause& filter_clause = nullptr, DistanceMetric distance_metric = DistanceMetric::COSINE );


    // Crypto functions - now using CryptoUtils
    // computeSha256Bytes and computeSha256HexString will now call CryptoUtils functions
    sha256_hash computeSha256Bytes(const std::vector<uint8_t>& data) const;
    std::string computeSha256HexString(const std::vector<uint8_t>& data) const;

private:
    std::string host_;
    int port_;
    std::string dbname_;
    std::string user_;
    std::string password_; // Store password for reconnects or dynamic connections
    std::string rag_table_name_;
    std::string rag_embedding_column_;
    std::string document_table_name_;
    std::string encrypted_content_table_name_;
    PGconn* conn_;

    std::string connection_string(const std::string& host, int port, const std::string& dbname,
                                  const std::string& user, const std::string& password) const;
    static std::string getDistanceOperator(DistanceMetric metric);
    std::string vectorToString(const std::vector<float>& vec) const;
    std::vector<float> stringToVector(const std::string& str) const; // Helper to parse vector string

public:
    // Helper to convert hex string to bytes (needed for DB insertion/retrieval)
    static std::vector<uint8_t> hex_to_bytes(const std::string& hex_string);
    template<size_t N>
    static std::array<uint8_t,N> hex_to_byte_array(const std::string& hex_string,bool allow_length_discrepancy = false)
    {
        std::array<uint8_t,N> res = {};
        auto v = hex_to_bytes(hex_string);
        if(!allow_length_discrepancy && v.size() != N)
            throw std::runtime_error("hex string has incorrect length");
        if(v.size() <= N)
            std::copy(std::begin(v),std::end(v),std::begin(res));
        else
            std::copy(std::begin(v),std::begin(v)+N,std::begin(res));
        return res;
    }
    // Helper to convert bytes to hex string (needed for DB insertion/retrieval and display)
    static std::string bytes_to_hex(const std::vector<uint8_t>& bytes);
    static std::string bytes_to_hex(const uint8_t* bytes, size_t len);
};

#endif // POSTGRES_CLIENT_H
