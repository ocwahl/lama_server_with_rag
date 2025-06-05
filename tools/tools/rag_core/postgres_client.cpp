#include "postgres_client.h"
#include <iostream>
#include <sstream>
#include <iomanip> // For std::hex, std::setw, std::setfill
#include <stdexcept>
#include <memory> // For std::unique_ptr
#include <algorithm> // For std::all_of
#include <iterator> // For std::back_inserter

#include <libpq-fe.h>

std::string postgres_client::getDistanceOperator(DistanceMetric metric) {
    switch (metric) {
        case DistanceMetric::COSINE: return "<=>";
        case DistanceMetric::L2:     return "<->";
        case DistanceMetric::IP:     return "<#>";
        default: return "<->"; // Default to cosine
    }
}
// Helper function to convert a float vector to a PostgreSQL array string
std::string postgres_client::vectorToString(const std::vector<float>& vec) const {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i < vec.size() - 1) {
            ss << ",";
        }
    }
    ss << "]";
    return ss.str();
}

// Helper function to parse PostgreSQL vector string back to std::vector<float>
std::vector<float> postgres_client::stringToVector(const std::string& str) const {
    std::vector<float> vec;
    if (str.empty() || str.length() < 2 || str.front() != '[' || str.back() != ']') {
        return vec; // Invalid format
    }
    std::stringstream ss(str.substr(1, str.length() - 2)); // Remove brackets
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        try {
            vec.push_back(std::stof(segment));
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid float in vector string: " << segment << std::endl;
            // Handle error, maybe clear vec or throw
        } catch (const std::out_of_range& e) {
            std::cerr << "Float out of range in vector string: " << segment << std::endl;
            // Handle error
        }
    }
    return vec;
}

// Helper to convert hex string to bytes
std::vector<uint8_t> postgres_client::hex_to_bytes(const std::string& hex_string) {
    if (hex_string.length() % 2 != 0) {
        throw std::runtime_error("Hex string must have an even length.");
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(hex_string.length() / 2);
    for (size_t i = 0; i < hex_string.length(); i += 2) {
        std::string byte_str = hex_string.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
    }
    return bytes;
}

// Helper to convert bytes to hex string
std::string postgres_client::bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : bytes) {
        ss << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}

std::string postgres_client::bytes_to_hex(const uint8_t* bytes, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

// Now using CryptoUtils for SHA256
sha256_hash postgres_client::computeSha256Bytes(const std::vector<uint8_t>& data) const {
    return CryptoUtils::computeSha256Bytes(data);
}

// Now using CryptoUtils for SHA256 and local bytes_to_hex for formatting
std::string postgres_client::computeSha256HexString(const std::vector<uint8_t>& data) const {
    sha256_hash hash = CryptoUtils::computeSha256Bytes(data);
    return bytes_to_hex(hash.data(), hash.size());
}


postgres_client::postgres_client(const std::string& host,
                                int port,
                                 const std::string& dbname,
                                 const std::string& user,
                                 const std::string& rag_table,
                                 const std::string& rag_embedding_col,
                                 const std::string& doc_table,
                                 const std::string& encrypted_content_table)
    : host_(host), port_(port), dbname_(dbname), user_(user),
      rag_table_name_(rag_table), rag_embedding_column_(rag_embedding_col),
      document_table_name_(doc_table), encrypted_content_table_name_(encrypted_content_table),
      conn_(nullptr) {
}

postgres_client::~postgres_client() {
    disconnect();
}

std::string postgres_client::connection_string(const std::string & host, int port, const std::string & dbname, const std::string & user, const std::string & password) const
{
    std::stringstream conn_string;
    conn_string << "host="<<host;
    conn_string << " port="<<port;
    conn_string << " dbname=" << dbname;
    if(!user.empty()){
        conn_string << " user=" << user;
    }
    if (!password.empty()) {
        conn_string << " password=" << password;
    }
    return conn_string.str();
}
void postgres_client::connect(const std::string& user, const std::string& password) {
    if (conn_ == nullptr) {
        if(!user.empty())
            user_ = user;
        if(!password.empty())
            password_ = password;
        auto conn_str = connection_string(host_,port_,dbname_,user_,password_);
        conn_ = PQconnectdb(conn_str.c_str());
        if (auto conn_status = PQstatus(conn_);conn_status == CONNECTION_BAD) {
            std::string errorMessage = "Connection to database failed: " + std::string(PQerrorMessage(conn_));
            std::cerr << errorMessage.c_str() << std::endl;
            PQfinish(conn_);
            conn_ = nullptr;
            throw std::runtime_error(errorMessage);
        }
        else if(conn_status == CONNECTION_OK)
        {
            std::cerr<<"connection to:"<<host_<<":"<<port_<<"/"<< dbname_<<" is OK"<<std::endl;
        }
        else
        {
            std::string errorMessage = "Other connection failure to database (connection string:" + std::string(PQerrorMessage(conn_));
            PQfinish(conn_);
            conn_ = nullptr;
            std::cerr << errorMessage.c_str() << std::endl;
            throw std::runtime_error(errorMessage);
        }
    }
}

void postgres_client::setUser(const std::string& user) {
        if(!user.empty())
            user_ = user;
}
void postgres_client::setPassword(const std::string& password) {
        if(!password.empty())
            password_ = password;
}


void postgres_client::disconnect() {
    if (conn_ != nullptr) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}
std::string postgres_client::get_host_name() const{
    return host_;
}
int postgres_client::get_port() const{
    return port_;
}
std::string postgres_client::get_name() const{
    return dbname_;
}

bool postgres_client::isConnected() const {
    return conn_ != nullptr;
}

bool postgres_client::hasSchema() {
    if (conn_ == nullptr) {
        std::cerr << "Error: Not connected to the database." << std::endl;
        return false;
    }

    // Array of table names to check
    std::vector<std::string> table_names = {document_table_name_, encrypted_content_table_name_, rag_table_name_};
    for (const std::string & table_name : table_names)
    {
        // Construct the query to check if the table exists in the current schema.
        std::string query = "SELECT EXISTS (SELECT 1 FROM pg_tables WHERE tablename = '" + table_name + "');";

        PGresult* result = PQexec(conn_, query.c_str());
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::cerr << "Error checking for table " << table_name << ": " << PQerrorMessage(conn_) << std::endl;
            PQclear(result);
            return false; // Error occurred, consider it a schema mismatch
        }

        // Get the result of the query (either 't' for true or 'f' for false)
        char* value = PQgetvalue(result, 0, 0);
        bool exists = (value != nullptr && value[0] == 't');

        PQclear(result); // Clean up the result

        if (!exists) {
            return false; // Table not found
        }
    }
    return true; // All tables exist
}


void postgres_client::createSchema(size_t embedding_size) {
    if (!isConnected()) {
        throw std::runtime_error("Not connected to the database.");
    }

    const std::string create_vector_extension = "CREATE EXTENSION IF NOT EXISTS vector;";
    PGresult* res_ext = PQexec(conn_, create_vector_extension.c_str());
    if (PQresultStatus(res_ext) != PGRES_COMMAND_OK) {
        std::string errorMessage = "Failed to create vector extension: " + std::string(PQerrorMessage(conn_));
        PQclear(res_ext);
        throw std::runtime_error(errorMessage);
    }
    PQclear(res_ext);

    const std::string create_document_table =
        "CREATE TABLE IF NOT EXISTS " + document_table_name_ + " ("
        "    document_id CHAR(" + std::to_string(sha256_hash{}.size() * 2) + ") PRIMARY KEY," // SHA256 hex string length
        "    date DATE,"
        "    version VARCHAR(255),"
        "    content_type VARCHAR(255),"
        "    url TEXT,"
        "    length INTEGER"
        ");";
    PGresult* res_doc = PQexec(conn_, create_document_table.c_str());
    if (PQresultStatus(res_doc) != PGRES_COMMAND_OK) {
        std::string errorMessage = "Failed to create document table: " + std::string(PQerrorMessage(conn_));
        PQclear(res_doc);
        throw std::runtime_error(errorMessage);
    }
    PQclear(res_doc);

    const std::string create_encrypted_content_table =
        "CREATE TABLE IF NOT EXISTS " + encrypted_content_table_name_ + " ("
        "    hash CHAR(" + std::to_string(sha256_hash{}.size() * 2) + ") PRIMARY KEY," // SHA256 hex string length
        //"    rag_name VARCHAR(255),"
        "    encrypted_content BYTEA,"
        "    tag CHAR(" + std::to_string(aes_gcm_tag{}.size() * 2) + ")," // AES-GCM tag hex string length
        "    nonce CHAR(" + std::to_string(aes_gcm_nonce{}.size() * 2) + ")," // AES-GCM nonce hex string length
        "    ephemeral_public_key CHAR(" + std::to_string(ecc256_public_key{}.size() * 2) + ")" // ECC public key hex string length
        ");";
    PGresult* res_enc = PQexec(conn_, create_encrypted_content_table.c_str());
    if (PQresultStatus(res_enc) != PGRES_COMMAND_OK) {
        std::string errorMessage = "Failed to create encrypted content table: " + std::string(PQerrorMessage(conn_));
        PQclear(res_enc);
        throw std::runtime_error(errorMessage);
    }
    PQclear(res_enc);

    const std::string create_rag_entries_table =
        "CREATE TABLE IF NOT EXISTS " + rag_table_name_ + " ("
        "    id SERIAL PRIMARY KEY,"
        "    document_id CHAR(" + std::to_string(sha256_hash{}.size() * 2) + ") REFERENCES " + document_table_name_ + "(document_id),"
        //"    rag_name VARCHAR(255),"
        "    embedding VECTOR(" + std::to_string(embedding_size) + "),"
        "    hash CHAR(" + std::to_string(sha256_hash{}.size() * 2) + ") REFERENCES " + encrypted_content_table_name_ + "(hash),"
        "    loffset INTEGER,"
        "    length INTEGER,"
        "    controller_public_key CHAR(" + std::to_string(ecc256_public_key{}.size() * 2) + ")," // ECC public key hex string length
        "    encryption_public_key CHAR(" + std::to_string(ecc256_public_key{}.size() * 2) + ")" // This will store the *recipient's* public key in the RAG entry
        ");";
    PGresult* res_rag = PQexec(conn_, create_rag_entries_table.c_str());
    if (PQresultStatus(res_rag) != PGRES_COMMAND_OK) {
        std::string errorMessage = "Failed to create rag entries table: " + std::string(PQerrorMessage(conn_));
        PQclear(res_rag);
        throw std::runtime_error(errorMessage);
    }
    PQclear(res_rag);

}

void postgres_client::destroySchema() {
    if (!isConnected()) {
        throw std::runtime_error("Not connected to the database.");
    }

    const std::string drop_rag_entries_table = "DROP TABLE IF EXISTS " + rag_table_name_ + ";";
    PGresult* res_rag = PQexec(conn_, drop_rag_entries_table.c_str());
    if (PQresultStatus(res_rag) != PGRES_COMMAND_OK) {
        std::cerr << "Failed to drop rag entries table: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res_rag);
    } else {
        PQclear(res_rag);
    }

    const std::string drop_encrypted_content_table = "DROP TABLE IF EXISTS " + encrypted_content_table_name_ + ";";
    PGresult* res_enc = PQexec(conn_, drop_encrypted_content_table.c_str());
    if (PQresultStatus(res_enc) != PGRES_COMMAND_OK) {
        std::cerr << "Failed to drop encrypted content table: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res_enc);
    } else {
        PQclear(res_enc);
    }

    const std::string drop_document_table = "DROP TABLE IF EXISTS " + document_table_name_ + ";";
    PGresult* res_doc = PQexec(conn_, drop_document_table.c_str());
    if (PQresultStatus(res_doc) != PGRES_COMMAND_OK) {
        std::cerr << "Failed to drop document table: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res_doc);
    } else {
        PQclear(res_doc);
    }

    const std::string drop_vector_extension = "DROP EXTENSION IF EXISTS vector;";
    PGresult* res_ext = PQexec(conn_, drop_vector_extension.c_str());
    if (PQresultStatus(res_ext) != PGRES_COMMAND_OK) {
        std::cerr << "Failed to drop vector extension: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res_ext);
    } else {
        PQclear(res_ext);
    }

}

document_entry postgres_client::createOrRetrieveDocument(
    const std::string& date,
    const std::string& version,
    const std::string& content_type,
    const std::string& url,
    int length) {
    if (!isConnected()) {
        throw std::runtime_error("Not connected to the database.");
    }

    std::string unique_string_for_hash = url + date + version + content_type + std::to_string(length);
    std::vector<uint8_t> hash_input(unique_string_for_hash.begin(), unique_string_for_hash.end());
    std::string document_id_hash_hex = computeSha256HexString(hash_input);

    // 1. Attempt to retrieve the document first based on its generated hash (document_id)
    std::string select_query =
        "SELECT document_id, date, version, content_type, url, length "
        "FROM " + document_table_name_ + " "
        "WHERE document_id = $1;";

    const char* select_param_values[1];
    select_param_values[0] = document_id_hash_hex.c_str();

    PGresult* res_select = PQexecParams(conn_,
                                        select_query.c_str(),
                                        1,
                                        nullptr,
                                        select_param_values,
                                        nullptr,
                                        nullptr,
                                        0);

    if (PQresultStatus(res_select) != PGRES_TUPLES_OK) {
        std::string errorMessage = "Failed to retrieve document: " + std::string(PQerrorMessage(conn_));
        PQclear(res_select);
        throw std::runtime_error(errorMessage);
    }

    if (PQntuples(res_select) > 0) {
        std::string retrieved_doc_id_hex = PQgetvalue(res_select, 0, 0);
        std::string retrieved_date = PQgetvalue(res_select, 0, 1);
        std::string retrieved_version = PQgetvalue(res_select, 0, 2);
        std::string retrieved_content_type = PQgetvalue(res_select, 0, 3);
        std::string retrieved_url = PQgetvalue(res_select, 0, 4);
        int retrieved_length = std::stoi(PQgetvalue(res_select, 0, 5));

        PQclear(res_select);
        return document_entry(retrieved_doc_id_hex, retrieved_date, retrieved_version, retrieved_content_type, retrieved_url, retrieved_length);
    }

    PQclear(res_select);

    // 2. If not found, insert the new document
    std::string insert_query =
        "INSERT INTO " + document_table_name_ + " (document_id, date, version, content_type, url, length) "
        "VALUES ($1, $2, $3, $4, $5, $6) RETURNING document_id;";

    const char* insert_param_values[6];
    insert_param_values[0] = document_id_hash_hex.c_str();
    insert_param_values[1] = date.c_str();
    insert_param_values[2] = version.c_str();
    insert_param_values[3] = content_type.c_str();
    insert_param_values[4] = url.c_str();
    insert_param_values[5] = std::to_string(length).c_str();

    PGresult* res_insert = PQexecParams(conn_,
                                        insert_query.c_str(),
                                        6,
                                        nullptr,
                                        insert_param_values,
                                        nullptr,
                                        nullptr,
                                        0);

    if (PQresultStatus(res_insert) != PGRES_TUPLES_OK) {
        std::string errorMessage = "Failed to insert document: " + std::string(PQerrorMessage(conn_));
        PQclear(res_insert);
        throw std::runtime_error(errorMessage);
    }

    std::string new_doc_id_hex = PQgetvalue(res_insert, 0, 0);
    PQclear(res_insert);

    return document_entry(new_doc_id_hex, date, version, content_type, url, length);
}

void postgres_client::deleteDocument(const std::string& document_id) {
    if (!isConnected()) {
        throw std::runtime_error("Not connected to the database.");
    }

    std::string delete_doc_query = "DELETE FROM " + document_table_name_ + " WHERE document_id = $1;";

    const char* paramValues[1];
    paramValues[0] = document_id.c_str();

    PGresult* res_delete = PQexecParams(conn_,
                                        delete_doc_query.c_str(),
                                        1,
                                        nullptr,
                                        paramValues,
                                        nullptr,
                                        nullptr,
                                        0);

    if (PQresultStatus(res_delete) != PGRES_COMMAND_OK) {
        std::string errorMessage = "Failed to delete document with ID " + document_id + ": " + std::string(PQerrorMessage(conn_));
        PQclear(res_delete);
        throw std::runtime_error(errorMessage);
    }

    long rowsAffected = PQcmdTuples(res_delete) ? std::stol(PQcmdTuples(res_delete)) : 0;
    PQclear(res_delete);
}

void postgres_client::insertRagEntry(const std::string& document_id_hash,
                                    const std::vector<float>& embedding,
                                    const std::vector<uint8_t>& contents,
                                    const ecc256_public_key& controller_public_key,
                                    const ecc256_private_key& recipient_private_key) { // Renamed for clarity
    if (!isConnected()) {
        throw std::runtime_error("Not connected to the database.");
    }

    std::string content_hash_hex = computeSha256HexString(contents);

    // ECIES encryption using recipient's public key (derived from their private key)
    ecc256_public_key recipient_public_key = CryptoUtils::computePublicKey(recipient_private_key);
    encryption_result enc_result = EciesUtils::encrypt_ecies(contents, recipient_public_key); // Call EciesUtils

    // Parameters for encrypted_content_table insertion
    const char* enc_param_values[5];
    int enc_param_lengths[5];
    int enc_param_formats[5];

    // Parameter 0: hash (CHAR, text format)
    enc_param_values[0] = content_hash_hex.c_str();
    enc_param_lengths[0] = 0;
    enc_param_formats[0] = 0;

    // Parameter 1: encrypted_content (BYTEA, binary format)
    enc_param_values[1] = reinterpret_cast<const char*>(enc_result.ciphertext.data());
    enc_param_lengths[1] = enc_result.ciphertext.size();
    enc_param_formats[1] = 1;

    // Parameter 2: tag (CHAR, text format - hex encoded)
    std::string tag_hex = bytes_to_hex(enc_result.tag.data(), enc_result.tag.size());
    enc_param_values[2] = tag_hex.c_str();
    enc_param_lengths[2] = 0;
    enc_param_formats[2] = 0;

    // Parameter 3: nonce (CHAR, text format - hex encoded)
    std::string nonce_hex = bytes_to_hex(enc_result.nonce.data(), enc_result.nonce.size());
    enc_param_values[3] = nonce_hex.c_str();
    enc_param_lengths[3] = 0;
    enc_param_formats[3] = 0;

    // Parameter 4: ephemeral_public_key (CHAR, text format - hex encoded)
    std::string ephemeral_public_key_hex = bytes_to_hex(enc_result.ephemeral_public_key.data(), enc_result.ephemeral_public_key.size());
    enc_param_values[4] = ephemeral_public_key_hex.c_str();
    enc_param_lengths[4] = 0;
    enc_param_formats[4] = 0;

    // Insert into encrypted_content_table
    PGresult* resEncrypted = PQexecParams(conn_,
        ("INSERT INTO " + encrypted_content_table_name_ +
         " (hash, encrypted_content, tag, nonce, ephemeral_public_key)"
         " VALUES ($1, $2, $3, $4, $5) ON CONFLICT (hash) DO NOTHING").c_str(),
        5, nullptr, enc_param_values, enc_param_lengths, enc_param_formats, 0);

    if (PQresultStatus(resEncrypted) != PGRES_COMMAND_OK) {
        std::string errorMessage = "Failed to insert encrypted content: " + std::string(PQerrorMessage(conn_));
        PQclear(resEncrypted);
        throw std::runtime_error(errorMessage);
    }
    PQclear(resEncrypted);

    // --- Insert into rag_entries table ---
    const char* rag_param_values[7];
    int rag_param_lengths[7];
    int rag_param_formats[7];

    // Parameter 0: document_id (CHAR, text format)
    rag_param_values[0] = document_id_hash.c_str();
    rag_param_lengths[0] = 0;
    rag_param_formats[0] = 0;

    // Parameter 1: embedding (VECTOR, text format)
    std::string embedding_str = vectorToString(embedding);
    rag_param_values[1] = embedding_str.c_str();
    rag_param_lengths[1] = 0;
    rag_param_formats[1] = 0;

    // Parameter 2: hash (CHAR, text format)
    rag_param_values[2] = content_hash_hex.c_str();
    rag_param_lengths[2] = 0;
    rag_param_formats[2] = 0;

    // Parameter 3: offset (INTEGER, text format)
    std::string offset_str = std::to_string(0); // Assuming offset is 0 for now
    rag_param_values[3] = offset_str.c_str();
    rag_param_lengths[3] = 0;
    rag_param_formats[3] = 0;

    // Parameter 4: length (INTEGER, text format)
    std::string length_str = std::to_string(contents.size());
    rag_param_values[4] = length_str.c_str();
    rag_param_lengths[4] = 0;
    rag_param_formats[4] = 0;

    // Parameter 5: controller_public_key (CHAR, text format - hex encoded)
    std::string controller_public_key_hex = bytes_to_hex(controller_public_key.data(), controller_public_key.size());
    rag_param_values[5] = controller_public_key_hex.c_str();
    rag_param_lengths[5] = 0;
    rag_param_formats[5] = 0;

    // Parameter 6: encryption_public_key (CHAR, text format - hex encoded)
    // This column will store the recipient's public key that was used for encryption.
    std::string encryption_public_key_hex = bytes_to_hex(recipient_public_key.data(), recipient_public_key.size());
    rag_param_values[6] = encryption_public_key_hex.c_str();
    rag_param_lengths[6] = 0;
    rag_param_formats[6] = 0;

    PGresult* resRag = PQexecParams(conn_,
        ("INSERT INTO " + rag_table_name_ +
         " (document_id, embedding, hash, loffset, length, controller_public_key, encryption_public_key)"
         " VALUES ($1, $2, $3, $4, $5, $6, $7)").c_str(),
        7, nullptr, rag_param_values, rag_param_lengths, rag_param_formats, 0);

    if (PQresultStatus(resRag) != PGRES_COMMAND_OK) {
        std::string errorMessage = "Failed to insert rag entry: " + std::string(PQerrorMessage(conn_));
        PQclear(resRag);
        throw std::runtime_error(errorMessage);
    }
    PQclear(resRag);
}

std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
                       ecc256_public_key, ecc256_public_key, // controller_public_key, encryption_public_key
                       std::string, std::string, std::string, std::string, int, // Document metadata
                       std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce, // encrypted_content, tag, nonce
                       ecc256_public_key //ephemereal_pk
                       >>
postgres_client::searchNearest(const std::vector<float>& query_embedding, int k, const additional_filtering_clause& filter_clause, DistanceMetric distance_metric ) {
    if (!isConnected()) {
        throw std::runtime_error("Not connected to the database.");
    }

    std::string where_clause = "";
    if (filter_clause) {
        where_clause = " WHERE " + filter_clause("r", "d", "ec");
    }
    std::string distance_operator = getDistanceOperator(distance_metric);
    std::string query_vector_str = vectorToString(query_embedding);
    std::string query =
        "SELECT r.document_id, r.embedding, r.hash, r.loffset, r.length, "
        "       r.controller_public_key, r.encryption_public_key, " // encryption_public_key is recipient's public key
        "       d.date, d.version, d.content_type, d.url, d.length AS doc_length, "
        "       ec.encrypted_content, ec.tag, ec.nonce, ec.ephemeral_public_key " // Added ephemeral_public_key
        "FROM " + rag_table_name_ + " r "
        "JOIN " + document_table_name_ + " d ON r.document_id = d.document_id "
        "JOIN " + encrypted_content_table_name_ + " ec ON r.hash = ec.hash "
        + where_clause +
        "ORDER BY r." + rag_embedding_column_ + " " + distance_operator + " '" + query_vector_str + "' "
        "LIMIT " + std::to_string(k) + ";";

    PGresult* res = PQexec(conn_, query.c_str());

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr<<"error:" << PQerrorMessage(conn_) << std::endl;
        std::string errorMessage = "Nearest neighbor search failed: " + std::string(PQerrorMessage(conn_));
        PQclear(res);
        throw std::runtime_error(errorMessage);
    }

    int num_rows = PQntuples(res);
    std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
                           ecc256_public_key, ecc256_public_key, // controller_public_key, encryption_public_key
                           std::string, std::string, std::string, std::string, int, // Document metadata
                           std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce, // encrypted_content, tag, nonce
                           ecc256_public_key //ephemereal_pk
                           >> results;

    for (int i = 0; i < num_rows; ++i) {
        std::string doc_id_retrieved = PQgetvalue(res, i, 0);
        std::vector<float> retrieved_embedding = stringToVector(PQgetvalue(res, i, 1));
        std::string hash_retrieved = PQgetvalue(res, i, 2);
        int offset = std::stoi(PQgetvalue(res, i, 3));
        int length = std::stoi(PQgetvalue(res, i, 4));

        ecc256_public_key controller_pk_bytes;
        std::vector<uint8_t> controller_pk_vec = hex_to_bytes(PQgetvalue(res, i, 5));
        if (controller_pk_vec.size() == controller_pk_bytes.size()) {
            std::copy(controller_pk_vec.begin(), controller_pk_vec.end(), controller_pk_bytes.begin());
        } else {
             std::cerr << "Warning: Mismatch in controller_public_key size for row " << i << ". Expected " << controller_pk_bytes.size() << ", got " << controller_pk_vec.size() << std::endl;
             // Handle error or set to default/empty if size is critical
        }

        ecc256_public_key encryption_pk_bytes;
        std::vector<uint8_t> encryption_pk_vec = hex_to_bytes(PQgetvalue(res, i, 6));
        if (encryption_pk_vec.size() == encryption_pk_bytes.size()) {
            std::copy(encryption_pk_vec.begin(), encryption_pk_vec.end(), encryption_pk_bytes.begin());
        } else {
             std::cerr << "Warning: Mismatch in encryption_public_key size for row " << i << ". Expected " << encryption_pk_bytes.size() << ", got " << encryption_pk_vec.size() << std::endl;
             // Handle error or set to default/empty if size is critical
        }

        // Document metadata
        std::string doc_date = PQgetvalue(res, i, 7);
        std::string doc_version = PQgetvalue(res, i, 8);
        std::string doc_content_type = PQgetvalue(res, i, 9);
        std::string doc_url = PQgetvalue(res, i, 10);
        int doc_length = std::stoi(PQgetvalue(res, i, 11));

        // Encrypted content and crypto metadata
        size_t encrypted_content_len = PQgetlength(res, i, 12);
        std::vector<uint8_t> encrypted_content(
            reinterpret_cast<const uint8_t*>(PQgetvalue(res, i, 12)),
            reinterpret_cast<const uint8_t*>(PQgetvalue(res, i, 12)) + encrypted_content_len
        );
        std::string encrypted_content_with_prefix(std::begin(encrypted_content),std::end(encrypted_content));
        // Remove the "\x" prefix if present
        std::string clean_encrypted_content_hex;
        if (encrypted_content_with_prefix.rfind("\\x", 0) == 0) { // Check if it starts with "\x"
            clean_encrypted_content_hex = encrypted_content_with_prefix.substr(2);
        } else {
            clean_encrypted_content_hex = encrypted_content_with_prefix;
        }
        encrypted_content = hex_to_bytes(clean_encrypted_content_hex);
        auto enc_hash_hex = computeSha256HexString(encrypted_content);

        aes_gcm_tag tag_bytes;
        std::string tag_hex = PQgetvalue(res, i, 13);
        std::vector<uint8_t> tag_vec = hex_to_bytes(tag_hex);
        if (tag_vec.size() == tag_bytes.size()) {
            std::copy(tag_vec.begin(), tag_vec.end(), tag_bytes.begin());
        } else {
            std::cerr << "Warning: Mismatch in tag size for row " << i << ". Expected " << tag_bytes.size() << ", got " << tag_vec.size() << std::endl;
        }

        aes_gcm_nonce nonce_bytes;
        std::string nonce_hex = PQgetvalue(res, i, 14);
        std::vector<uint8_t> nonce_vec = hex_to_bytes(nonce_hex);
        if (nonce_vec.size() == nonce_bytes.size()) {
            std::copy(nonce_vec.begin(), nonce_vec.end(), nonce_bytes.begin());
        } else {
             std::cerr << "Warning: Mismatch in nonce size for row " << i << ". Expected " << nonce_bytes.size() << ", got " << nonce_vec.size() << std::endl;
        }

        ecc256_public_key ephemeral_pk_bytes;
        std::string ephemeral_public_key_hex = PQgetvalue(res, i, 15);
        std::vector<uint8_t> ephemeral_pk_vec = hex_to_bytes(ephemeral_public_key_hex);
        if (ephemeral_pk_vec.size() == ephemeral_pk_bytes.size()) {
            std::copy(ephemeral_pk_vec.begin(), ephemeral_pk_vec.end(), ephemeral_pk_bytes.begin());
        } else {
             std::cerr << "Warning: Mismatch in ephemeral_public_key size for row " << i << ". Expected " << ephemeral_pk_bytes.size() << ", got " << ephemeral_pk_vec.size() << std::endl;
        }


        results.emplace_back(
            doc_id_retrieved,
            retrieved_embedding,
            hash_retrieved,
            offset,
            length,
            controller_pk_bytes,
            encryption_pk_bytes, // The stored recipient's public key
            doc_date, doc_version, doc_content_type, doc_url, doc_length,
            encrypted_content,
            tag_bytes,
            nonce_bytes,
            ephemeral_pk_bytes
        );
    }

    PQclear(res);
    return results;
}
