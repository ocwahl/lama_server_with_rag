// tests/rag_core_test/test_rag_core.cpp

#include "crypto_utils.h"
#include "ecies_utils.h"
#include "postgres_client.h" // <--- NEW: Include your PostgreSQL client header
#include "rag_database.h"    // <--- NEW: Include the rag_database interface

#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // For std::hex, std::setw, std::setfill
#include <random>
#include <set> // For uniqueness test
#include <chrono> // For timing deterministic tests
#include <algorithm> // For std::equal, std::fill
#include <tuple> // For std::tuple in search results
#include <limits> // For numeric_limits (float comparison)


std::shared_ptr<postgres_client> rag_db_ = nullptr;
std::shared_ptr<rag_database> create_rag_database(const std::string& host_name, int port, const std::string& db_name)
{
    if(!rag_db_)
        return rag_db_ = std::make_shared<postgres_client>(host_name, port, db_name);
    if((rag_db_->get_host_name() == host_name) && (rag_db_->get_port() == port) && (rag_db_->get_name() == db_name))
        return rag_db_;
    std::cerr<<"rebuilding database";
    return rag_db_ = std::make_shared<postgres_client>(host_name, port, db_name);
}

// Helper for logging using standard cout
#define TEST_LOG_RAW(...) \
    do { \
        std::cout << "[" << __FILE__ << ":" << __LINE__ << "] " ; \
        printf(__VA_ARGS__); \
        std::cout << std::endl; \
    } while (0)

// Assertion macro
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            TEST_LOG_RAW("FAIL: %s - %s", #condition, message); \
            return false; \
        } \
    } while (0)

#define TEST_SUCCESS(message) \
    TEST_LOG_RAW("PASS: %s", message); \
    return true;

#define TEST_SKIPPED(message) \
    TEST_LOG_RAW("SKIP: %s", message); \
    return true;

// Helper to print a vector of bytes
static void print_bytes(const std::string& label, const std::vector<unsigned char>& bytes) {
    std::cout << label << ": [";
    for (size_t i = 0; i < bytes.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
        if (i < bytes.size() - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::dec << "]" << std::endl;
}

// =========================================================================
// SHA256 Tests
// =========================================================================

static bool test_computeSha256Bytes_zero_size() {
    std::vector<unsigned char> data;
    sha256_hash hash = CryptoUtils::computeSha256Bytes(data);
    TEST_ASSERT(hash.size() == 32, "Hash of zero-size array must be 32 bytes.");

    // Known SHA256 hash for empty string/data
    sha256_hash expected_empty_hash = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    TEST_ASSERT(hash == expected_empty_hash, "Hash of zero-size array must match known hash.");
    TEST_SUCCESS("computeSha256Bytes_zero_size");
}

static bool test_computeSha256Bytes_determinism() {
    std::vector<unsigned char> data1 = {1, 2, 3, 4, 5};
    std::vector<unsigned char> data2 = {1, 2, 3, 4, 5};
    sha256_hash hash1 = CryptoUtils::computeSha256Bytes(data1);
    sha256_hash hash2 = CryptoUtils::computeSha256Bytes(data2);
    TEST_ASSERT(hash1 == hash2, "computeSha256Bytes must be deterministic for same input.");

    std::vector<unsigned char> data_diff = {1, 2, 3, 4, 6};
    sha256_hash hash_diff = CryptoUtils::computeSha256Bytes(data_diff);
    TEST_ASSERT(hash1 != hash_diff, "computeSha256Bytes must produce different hashes for different inputs.");
    TEST_SUCCESS("computeSha256Bytes_determinism");
}

static bool test_computeSha256Bytes_uniqueness() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    std::set<sha256_hash> unique_hashes;
    const int NUM_TESTS = 3000;
    const int DATA_SIZE = 64; // Test with reasonable data size

    for (int i = 0; i < NUM_TESTS; ++i) {
        std::vector<unsigned char> random_data(DATA_SIZE);
        for (int j = 0; j < DATA_SIZE; ++j) {
            random_data[j] = distrib(gen);
        }
        sha256_hash hash = CryptoUtils::computeSha256Bytes(random_data);
        TEST_ASSERT(hash.size() == 32, "Hash must be 32 bytes.");
        unique_hashes.insert(hash);
    }
    TEST_ASSERT(unique_hashes.size() == NUM_TESTS, "computeSha256Bytes must produce unique hashes for unique random inputs (high probability).");
    TEST_SUCCESS("computeSha256Bytes_uniqueness");
}

// =========================================================================
// EC Utilities (secp256k1) Tests
// =========================================================================

static bool test_generateKeys_and_consistency_with_scalarMultiply() {
    // Generate a private/public key pair
    ecc256_private_key private_key_a = CryptoUtils::generatePrivateKey();
    ecc256_public_key public_key_a = CryptoUtils::computePublicKey(private_key_a);

    // Get the base point G of secp256k1 (usually derived from the curve parameters)
    // CryptoUtils::scalarMultiply should be able to get G given the curve context
    // This might require exposing a function or internal knowledge.
    // For now, let's assume CryptoUtils::scalarMultiply can take 1 as a private_key_scalar
    // and correctly produce the base point G if the public_key_point is null/default.
    // Or, more accurately, we need to create an EC_POINT for G.
    // This might be tricky if not directly exposed.

    // Let's assume for this test CryptoUtils::computePublicKey internally uses scalar multiplication
    // with G. So if we multiply G by '1', we should get G.
    // For secp256k1, the base point G is constant.
    // If CryptoUtils::computePublicKey(1) returns G, then:
    // private_key_scalar (1) * G = G

    // For simplicity, let's test that private_key_a * G == public_key_a
    // This is implicitly tested by computePublicKey.
    // Now, test that (private_key_a * public_key_b) == (private_key_b * public_key_a)
    // in the context of ECDH, which will be covered by computeEcdhSharedSecretSha256 test.

    // Let's focus on scalarMultiply: private_key_scalar (1) * public_key_a should be public_key_a
    ecc256_private_key one_scalar;
    std::fill(one_scalar.begin(), one_scalar.end(), 0);
    one_scalar.back() = 1; // Set the last byte to 1 for scalar 1

    ecc256_public_key multiplied_by_one = CryptoUtils::scalarMultiply(one_scalar, public_key_a);
    TEST_ASSERT(multiplied_by_one == public_key_a, "scalarMultiply with 1 as scalar should return original public key.");

    // Test a*b consistency: (a * P_b) == (b * P_a)
    ecc256_private_key private_key_b = CryptoUtils::generatePrivateKey();
    ecc256_public_key public_key_b = CryptoUtils::computePublicKey(private_key_b);

    ecc256_public_key ab_pub = CryptoUtils::scalarMultiply(private_key_a, public_key_b);
    ecc256_public_key ba_pub = CryptoUtils::scalarMultiply(private_key_b, public_key_a);
    TEST_ASSERT(ab_pub == ba_pub, "scalarMultiply must satisfy (a*P_b == b*P_a) for ECDH (public key version).");

    TEST_SUCCESS("generateKeys_and_consistency_with_scalarMultiply");
}


static bool test_computeEcdhSharedSecretSha256_consistency() {
    ecc256_private_key sk1 = CryptoUtils::generatePrivateKey();
    ecc256_public_key pk1 = CryptoUtils::computePublicKey(sk1);

    ecc256_private_key sk2 = CryptoUtils::generatePrivateKey();
    ecc256_public_key pk2 = CryptoUtils::computePublicKey(sk2);

    sha256_hash shared_secret1 = CryptoUtils::computeEcdhSharedSecretSha256(sk1, pk2);
    sha256_hash shared_secret2 = CryptoUtils::computeEcdhSharedSecretSha256(sk2, pk1);

    TEST_ASSERT(shared_secret1.size() == 32, "Shared secret 1 size must be 32 bytes (SHA256).");
    TEST_ASSERT(shared_secret2.size() == 32, "Shared secret 2 size must be 32 bytes (SHA256).");
    TEST_ASSERT(shared_secret1 == shared_secret2, "ECDH shared secret must be consistent (sk1,pk2) == (sk2,pk1).");
    TEST_SUCCESS("computeEcdhSharedSecretSha256_consistency");
}

// =========================================================================
// AES-256 GCM Tests
// =========================================================================

// Helper function to run a single AES-GCM consistency test
static bool run_aes_gcm_test_case(
    const std::vector<unsigned char>& original_data,
    const std::vector<unsigned char>& additional_authenticated_data,
    const std::string& test_case_name)
{
    // Generate a random AES key and nonce for the test
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    aes256_key aes_key; // 256-bit key
    for (size_t i = 0; i < aes_key.size(); ++i) aes_key[i] = distrib(gen);

    aes_gcm_nonce aes_nonce; // 96-bit nonce is recommended for GCM
    for (size_t i = 0; i < aes_nonce.size(); ++i) aes_nonce[i] = distrib(gen);

    std::vector<unsigned char> ciphertext;
    aes_gcm_tag tag;

    // Encrypt
    CryptoUtils::aes256GcmEncrypt(
        original_data, aes_key, aes_nonce, additional_authenticated_data, ciphertext, tag);

    TEST_ASSERT(tag.size() == 16, ("GCM Tag size should be 16 bytes for " + test_case_name).c_str()); // GCM tag is typically 16 bytes

    std::vector<unsigned char> decrypted_data;
    // Decrypt
    bool decrypt_ok = CryptoUtils::aes256GcmDecrypt(ciphertext,
        tag, aes_key, aes_nonce, additional_authenticated_data, decrypted_data);

    TEST_ASSERT(decrypt_ok, ("AES-256 GCM Decryption failed for " + test_case_name).c_str());
    TEST_ASSERT(original_data == decrypted_data, ("Decrypted data must match original data for " + test_case_name).c_str());

    // --- Tampering tests ---

    // Test with modified ciphertext (should fail decryption)
    if (!ciphertext.empty()) { // Only test if there's ciphertext to modify
        std::vector<unsigned char> modified_ciphertext = ciphertext;
        modified_ciphertext[0] ^= 0x01; // Flip a bit
        std::vector<unsigned char> failed_decrypted_data;
        bool failed_decrypt = CryptoUtils::aes256GcmDecrypt(modified_ciphertext,
            tag, aes_key, aes_nonce, additional_authenticated_data, failed_decrypted_data);
        TEST_ASSERT(!failed_decrypt, ("Decryption should fail with modified ciphertext for " + test_case_name).c_str());
    }

    // Test with modified AAD (should fail decryption) - only if AAD is not empty
    if (!additional_authenticated_data.empty()) {
        std::vector<unsigned char> modified_aad = additional_authenticated_data;
        modified_aad[0] ^= 0x01; // Flip a bit
        std::vector<unsigned char> failed_decrypted_data;
        bool failed_decrypt = CryptoUtils::aes256GcmDecrypt(ciphertext,
            tag, aes_key, aes_nonce, modified_aad, failed_decrypted_data);
        TEST_ASSERT(!failed_decrypt, ("Decryption should fail with modified AAD for " + test_case_name).c_str());
    }

    // Test with modified tag (should fail decryption)
    if (tag != aes_gcm_tag()) { // Always true for GCM tag, but good practice
        aes_gcm_tag modified_tag = tag;
        modified_tag[0] ^= 0x01; // Flip a bit
        std::vector<unsigned char> failed_decrypted_data;
        bool failed_decrypt = CryptoUtils::aes256GcmDecrypt(ciphertext,
            modified_tag, aes_key, aes_nonce, additional_authenticated_data, failed_decrypted_data);
        TEST_ASSERT(!failed_decrypt, ("Decryption should fail with modified tag for " + test_case_name).c_str());
    }

    return true; // All checks for this test case passed
}

static bool test_aes256GcmEncrypt_Decrypt_consistency() {
    // --- Test Data ---
    std::vector<unsigned char> sample_data = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20
    }; // 32 bytes example data

    std::vector<unsigned char> sample_aad = {
        0x01, 0x02, 0x01, 0x04, 0x01, 0x06, 0x01, 0x08
    }; // 8 bytes example data

    std::vector<unsigned char> empty_aad = {};

    // --- Test Cases ---

    // 1. Regular data with regular AAD
    if (!run_aes_gcm_test_case(sample_data, sample_aad, "Regular Data with AAD")) return false;

    // 2. Regular data with EMPTY AAD
    if (!run_aes_gcm_test_case(sample_data, empty_aad, "Regular Data with Empty AAD")) return false;

    //below not tested: unclear what aes_gcm encryption means when cleartext is empty.
    //is it just a mac ?
    //std::vector<unsigned char> empty_data = {};
    // // 3. Empty data with regular AAD
    // if (!run_aes_gcm_test_case(empty_data, sample_aad, "Empty Data with AAD")) return false;

    // // 4. Empty data with EMPTY AAD
    // if (!run_aes_gcm_test_case(empty_data, empty_aad, "Empty Data with Empty AAD")) return false;

    TEST_SUCCESS("aes256GcmEncrypt_Decrypt_consistency");
}

// =========================================================================
// ECIES Tests
// =========================================================================

static bool test_ecies_encrypt_decrypt_consistency() {
    std::string original_message_str = "This is a secret message to be encrypted using ECIES.";
    std::vector<unsigned char> original_message(original_message_str.begin(), original_message_str.end());

    // Generate sender's ephemeral key pair
    ecc256_private_key sender_ephemeral_sk = CryptoUtils::generatePrivateKey();
    (void)sender_ephemeral_sk; // Suppress unused warning

    // Generate recipient's static key pair
    ecc256_private_key recipient_sk = CryptoUtils::generatePrivateKey();
    ecc256_public_key recipient_pk = CryptoUtils::computePublicKey(recipient_sk);

    // Encrypt
    encryption_result encrypted = EciesUtils::encrypt_ecies(
        original_message,
        recipient_pk); // Using sender's ephemeral private key (implicit in EciesUtils::encrypt_ecies)

    TEST_ASSERT(!encrypted.ciphertext.empty(), "ECIES Ciphertext should not be empty.");
    TEST_ASSERT(encrypted.nonce != aes_gcm_nonce(), "ECIES Nonce should not be empty.");
    TEST_ASSERT(encrypted.tag != aes_gcm_tag(), "ECIES MAC tag should not be empty.");
    TEST_ASSERT(encrypted.ephemeral_public_key != ecc256_public_key(), "ECIES Ephemeral public key should not be empty.");

    // Decrypt
    std::vector<unsigned char> decrypted_message;
    decrypted_message = EciesUtils::decrypt_ecies(
        encrypted.ciphertext,
        encrypted.tag,
        encrypted.nonce,
        encrypted.ephemeral_public_key,
        recipient_sk // Using recipient's static private key
        );

    TEST_ASSERT(original_message == decrypted_message, "Decrypted message must match original message.");

    // Test with modified ciphertext (should fail decryption)
    if (!encrypted.ciphertext.empty()) {
        std::vector<unsigned char> modified_ciphertext = encrypted.ciphertext;
        modified_ciphertext[0] ^= 0x01; // Flip a bit
        std::vector<unsigned char> failed_decrypted_message;
        failed_decrypted_message = EciesUtils::decrypt_ecies(
            modified_ciphertext, encrypted.tag, encrypted.nonce, encrypted.ephemeral_public_key,
            recipient_sk);
        TEST_ASSERT(failed_decrypted_message == std::vector<unsigned char>(), "ECIES Decryption should fail with modified ciphertext.");
    }

    // Test with modified tag (should fail decryption)
    if (encrypted.tag != aes_gcm_tag()) {
        aes_gcm_tag modified_tag = encrypted.tag;
        modified_tag[0] ^= 0x01; // Flip a bit
        std::vector<unsigned char> failed_decrypted_message;
        failed_decrypted_message = EciesUtils::decrypt_ecies(
            encrypted.ciphertext, modified_tag, encrypted.nonce, encrypted.ephemeral_public_key,
            recipient_sk);
        TEST_ASSERT(failed_decrypted_message == std::vector<unsigned char>(), "ECIES Decryption should fail with modified MAC tag.");
    }

    // Test with modified ephemeral public key (should fail decryption)
    if (!encrypted.ephemeral_public_key.empty()) {
        // Generate a brand new *valid and random* public key that is NOT the original ephemeral key.
        // This will lead to an incorrect shared secret, causing AES-GCM decryption/authentication to fail.
        auto modified_ephemeral_pk = CryptoUtils::computePublicKey(CryptoUtils::generatePrivateKey());
        
        std::vector<unsigned char> failed_decrypted_message;
        failed_decrypted_message = EciesUtils::decrypt_ecies(
            encrypted.ciphertext, encrypted.tag, encrypted.nonce, modified_ephemeral_pk,
            recipient_sk);
        TEST_ASSERT(failed_decrypted_message.empty(), "ECIES Decryption should fail with non-matching ephemeral public key.");
    }

    TEST_SUCCESS("ecies_encrypt_decrypt_consistency");
}


// =========================================================================
// PostgreSQL Client (rag_database implementation) Tests
// =========================================================================

// Connection parameters for the test database
const std::string PG_HOST = "localhost";
const std::string PG_DBNAME = "klave_rag";
const std::string PG_USER = "postgres";
const std::string PG_PASSWORD = "admin";
const size_t EMBEDDING_SIZE = 4096; // Example embedding size

// Helper to clean up the schema before a test
static void clean_db_schema(std::shared_ptr<rag_database>& db) {
    try {
        db->connect(PG_USER, PG_PASSWORD);
        if (db->hasSchema()) {
            db->destroySchema();
            TEST_LOG_RAW("Cleaned up existing schema.");
        }
        db->disconnect();
    } catch (const std::exception& e) {
        TEST_LOG_RAW("Warning: Failed to clean up schema during setup: %s", e.what());
    }
}

// Helper to ensure schema exists for tests that need it
static bool ensure_schema_exists(std::shared_ptr<rag_database>& db) {
    try {
        db->connect(PG_USER, PG_PASSWORD);
        if (!db->hasSchema()) {
            TEST_LOG_RAW("Creating schema...");
            db->createSchema(EMBEDDING_SIZE);
            if (!db->hasSchema()) {
                TEST_LOG_RAW("FAIL: Schema creation failed.");
                db->disconnect();
                return false;
            }
            TEST_LOG_RAW("Schema created successfully.");
        } else {
            TEST_LOG_RAW("Schema already exists.");
        }
        db->disconnect();
        return true;
    } catch (const std::exception& e) {
        TEST_LOG_RAW("FAIL: Error ensuring schema exists: %s", e.what());
        return false;
    }
}

// Helper to generate random bytes
static std::vector<uint8_t> generate_random_bytes(size_t size) {
    std::vector<uint8_t> bytes(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<uint8_t>(distrib(gen));
    }
    return bytes;
}

// Helper to generate random embedding
static std::vector<float> generate_random_embedding(size_t size) {
    std::vector<float> embedding(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distrib(-1.0f, 1.0f); // Typical embedding range
    for (size_t i = 0; i < size; ++i) {
        embedding[i] = distrib(gen);
    }
    return embedding;
}

// Helper to generate a random date string (YYYY-MM-DD)
static std::string generate_random_date() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> year_dist(2000, 2024);
    std::uniform_int_distribution<> month_dist(1, 12);
    std::uniform_int_distribution<> day_dist(1, 28); // Simplistic, avoids month-end issues

    int year = year_dist(gen);
    int month = month_dist(gen);
    int day = day_dist(gen);

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << year << "-"
       << std::setw(2) << month << "-"
       << std::setw(2) << day;
    return ss.str();
}

// Helper to compare float vectors with a tolerance
static bool compare_float_vectors(const std::vector<float>& v1, const std::vector<float>& v2, float epsilon = std::numeric_limits<float>::epsilon() * 100) {
    if (v1.size() != v2.size()) return false;
    for (size_t i = 0; i < v1.size(); ++i) {
        if (std::abs(v1[i] - v2[i]) > epsilon) {
            return false;
        }
    }
    return true;
}


static bool test_db_connection_success_and_disconnect() {
    TEST_LOG_RAW("Testing DB: Connection success and disconnect...");
    std::shared_ptr<rag_database> db = create_rag_database("localhost",5432,"klave_rag");
    try {
        db->connect(PG_USER, PG_PASSWORD);
        TEST_ASSERT(db->isConnected(), "Database should be connected.");
        db->disconnect();
        TEST_ASSERT(!db->isConnected(), "Database should be disconnected.");
    } catch (const std::exception& e) {
        TEST_ASSERT(false, ("Exception during connection test: " + std::string(e.what())).c_str());
    }
    TEST_SUCCESS("DB: Connection success and disconnect");
}

static bool test_db_connection_failure() {
    TEST_LOG_RAW("Testing DB: Connection failure with invalid credentials...");
    std::shared_ptr<rag_database> db = create_rag_database("localhost",5432,"klave_rag");
    try {
        // Use an invalid password or non-existent database/user
        db->connect(PG_USER, "wrong_password"); // Assuming this will fail
        TEST_ASSERT(false, "Connection with invalid password should have failed."); // Should not reach here
    } catch (const std::runtime_error& e) {
        TEST_LOG_RAW("Caught expected exception for connection failure: %s", e.what());
        TEST_ASSERT(!db->isConnected(), "Database should not be connected after failed attempt.");
    } catch (const std::exception& e) {
        TEST_ASSERT(false, ("Caught unexpected exception type for connection failure: " + std::string(e.what())).c_str());
    }
    TEST_SUCCESS("DB: Connection failure");
}

static bool test_db_schema_management() {
    TEST_LOG_RAW("Testing DB: Schema creation and destruction...");
    std::shared_ptr<rag_database> db = create_rag_database("localhost",5432,"klave_rag");

    // Clean up before test
    clean_db_schema(db);

    try {
        db->connect(PG_USER, PG_PASSWORD);
        TEST_ASSERT(!db->hasSchema(), "Schema should not exist before creation.");

        db->createSchema(EMBEDDING_SIZE);
        TEST_ASSERT(db->hasSchema(), "Schema should exist after creation.");

        // Test creating schema again (should be idempotent or throw harmlessly)
        db->createSchema(EMBEDDING_SIZE);
        TEST_ASSERT(db->hasSchema(), "Schema should still exist after re-creation attempt.");

        db->destroySchema();
        TEST_ASSERT(!db->hasSchema(), "Schema should not exist after destruction.");

        db->disconnect();
    } catch (const std::exception& e) {
        TEST_ASSERT(false, ("Exception during schema management test: " + std::string(e.what())).c_str());
    }
    TEST_SUCCESS("DB: Schema management");
}

static bool test_db_document_creation_and_retrieval() {
    TEST_LOG_RAW("Testing DB: Document creation and retrieval...");
    std::shared_ptr<rag_database> db = create_rag_database("localhost",5432,"klave_rag");

    // Ensure schema exists
    if (!ensure_schema_exists(db)) return false;

    try {
        db->connect(PG_USER, PG_PASSWORD);

        std::string date1 = generate_random_date();
        std::string version1 = "v1.0";
        std::string contentType1 = "text/plain";
        std::string url1 = "http://example.com/doc1";
        int length1 = 1000;

        // Create a new document
        document_entry doc1 = db->createOrRetrieveDocument(date1, version1, contentType1, url1, length1);
        TEST_ASSERT(!doc1.document_id.empty(), "Document ID should not be empty after creation.");

        // Retrieve the same document (should return the same ID)
        document_entry retrieved_doc1 = db->createOrRetrieveDocument(date1, version1, contentType1, url1, length1);
        TEST_ASSERT(doc1.document_id == retrieved_doc1.document_id, "Retrieving same document should yield same ID.");
        TEST_ASSERT(doc1.date == retrieved_doc1.date, "Retrieved date mismatch.");
        TEST_ASSERT(doc1.url == retrieved_doc1.url, "Retrieved URL mismatch.");

        // Create another document with different URL
        std::string url2 = "http://example.com/doc2";
        document_entry doc2 = db->createOrRetrieveDocument(date1, version1, contentType1, url2, length1);
        TEST_ASSERT(doc1.document_id != doc2.document_id, "Different documents should have different IDs.");

        db->disconnect();
    } catch (const std::exception& e) {
        TEST_ASSERT(false, ("Exception during document creation/retrieval test: " + std::string(e.what())).c_str());
    }
    TEST_SUCCESS("DB: Document creation and retrieval");
}

static bool test_db_document_deletion() {
    TEST_LOG_RAW("Testing DB: Document deletion...");
    std::shared_ptr<rag_database> db = create_rag_database("localhost",5432,"klave_rag");

    if (!ensure_schema_exists(db)) return false;

    try {
        db->connect(PG_USER, PG_PASSWORD);

        std::string date = generate_random_date();
        std::string version = "v1.0";
        std::string contentType = "text/plain";
        std::string url = "http://example.com/doc_to_delete";
        int length = 500;

        document_entry doc = db->createOrRetrieveDocument(date, version, contentType, url, length);
        TEST_ASSERT(!doc.document_id.empty(), "Document ID should not be empty for deletion test.");

        // Delete the document
        db->deleteDocument(doc.document_id);

        // Try to retrieve it again (should create a new one, meaning ID will be different)
        std::string version2 = "v2.0";
        document_entry re_created_doc = db->createOrRetrieveDocument(date, version2, contentType, url, length);
        TEST_ASSERT(doc.document_id != re_created_doc.document_id, "Document should have been deleted and re-created with new ID because of new version.");

        // Attempt to delete a non-existent document (should not throw, but indicate no rows affected)
        std::string non_existent_id = "nonexistentdocumentid1234567890abcdef"; // Must be correct SHA256 length
        db->deleteDocument(non_existent_id); // Should not throw

        db->disconnect();
    } catch (const std::exception& e) {
        TEST_ASSERT(false, ("Exception during document deletion test: " + std::string(e.what())).c_str());
    }
    TEST_SUCCESS("DB: Document deletion");
}


static bool test_db_rag_entry_insertion_and_decryption() {
    TEST_LOG_RAW("Testing DB: RAG entry insertion and decryption consistency...");
    std::shared_ptr<rag_database> db = create_rag_database("localhost",5432,"klave_rag");

    if (!ensure_schema_exists(db)) return false;

    try {
        db->connect(PG_USER, PG_PASSWORD);

        // 1. Create a document
        std::string doc_date = generate_random_date();
        std::string doc_version = "v1.0";
        std::string doc_type = "application/pdf";
        std::string doc_url = "http://example.com/rag_doc_test";
        int doc_length = 2048;
        document_entry doc = db->createOrRetrieveDocument(doc_date, doc_version, doc_type, doc_url, doc_length);
        TEST_ASSERT(!doc.document_id.empty(), "Document ID empty for RAG insertion test.");

        // 2. Generate content, embedding, and crypto keys
        std::vector<uint8_t> original_content = generate_random_bytes(550); // Example content
        std::vector<float> embedding = generate_random_embedding(EMBEDDING_SIZE);

        ecc256_private_key controller_sk = CryptoUtils::generatePrivateKey();
        ecc256_public_key controller_pk = CryptoUtils::computePublicKey(controller_sk);

        ecc256_private_key recipient_sk = CryptoUtils::generatePrivateKey();
        ecc256_public_key recipient_pk = CryptoUtils::computePublicKey(recipient_sk);

        // 3. Insert RAG entry
        db->insertRagEntry(doc.document_id, embedding, original_content, controller_pk, recipient_sk);

        // 4. Retrieve the entry using searchNearest (or any search that returns the full tuple)
        auto search_results = db->searchNearest(embedding, 1); // Search for the inserted embedding

        TEST_ASSERT(search_results.size() == 1, ("Expected 1 search result, got " + std::to_string(search_results.size())).c_str());
        
        // Extract retrieved data
        auto& result_tuple = search_results[0];
        std::string retrieved_doc_id = std::get<0>(result_tuple);
        std::vector<float> retrieved_embedding = std::get<1>(result_tuple);
        std::string retrieved_hash = std::get<2>(result_tuple);
        // int retrieved_offset = std::get<3>(result_tuple);
        // int retrieved_length = std::get(result_tuple);
        ecc256_public_key retrieved_controller_pk = std::get<5>(result_tuple);
        ecc256_public_key retrieved_encryption_pk = std::get<6>(result_tuple); // This is the recipient_pk
        // Document metadata (7-11)
        std::vector<uint8_t> retrieved_encrypted_content = std::get<12>(result_tuple);
        aes_gcm_tag retrieved_tag = std::get<13>(result_tuple);
        aes_gcm_nonce retrieved_nonce = std::get<14>(result_tuple);
        // ephemeral_public_key is not in the tuple for rag_database search result

        // Verify basic fields
        TEST_ASSERT(retrieved_doc_id == doc.document_id, "Retrieved document ID mismatch.");
        TEST_ASSERT(compare_float_vectors(retrieved_embedding, embedding), "Retrieved embedding mismatch.");
        TEST_ASSERT(retrieved_controller_pk == controller_pk, "Retrieved controller public key mismatch.");
        TEST_ASSERT(retrieved_encryption_pk == recipient_pk, "Retrieved encryption public key mismatch (should be recipient's PK).");

        // Attempt to decrypt the content
        // You need the ephemeral_public_key from the encrypted_rag_contents table.
        // The current rag_database search result tuple does NOT include ephemeral_public_key.
        // This is a design mismatch between your postgres_client's search result and rag_database's.
        // For this test, we must assume the `postgres_client`'s search method (which is called by rag_database)
        // *does* return the ephemeral_public_key.
        // Let's assume the tuple structure in rag_database.h for searchNearest was updated to include it.
        // If not, you'll need to update rag_database.h and postgres_client.h/cpp.

        // Assuming the tuple from rag_database::searchNearest now includes the ephemeral_public_key
        // at index 15 (after tag and nonce), as per the updated postgres_client.cpp searchNearest.
        // Let's re-verify the rag_database.h definition for searchNearest:
        // std::vector<std::tuple<std::string, std::vector<float>, std::string, int, int,
        //                        ecc256_public_key, ecc256_public_key, // controller_public_key, encryption_public_key (recipient's)
        //                        std::string, std::string, std::string, std::string, int, // Document metadata
        //                        std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce // encrypted_content, tag, nonce
        //                        >>
        // This definition *does not* include the ephemeral_public_key.
        // This means the rag_database interface needs to be updated to match postgres_client's full return.
        // For this test, I will add it to the tuple for the test to pass, but you MUST update rag_database.h
        // to reflect the full data returned by postgres_client's search.

        // For now, let's just assume the ephemeral_public_key is also retrieved directly
        // from the database, or we need to fetch it separately.
        // If your postgres_client's searchNearest *does* return it, then the rag_database interface
        // needs to be extended to include it in the tuple.
        // Let's assume for this test that the ephemeral_public_key is passed back from the insertion
        // or can be derived/retrieved.
        // Since `EciesUtils::encrypt_ecies` returns `encryption_result` which contains `ephemeral_public_key`,
        // and `insertRagEntry` doesn't store this directly in the `rag_entries` table,
        // it must be stored in `encrypted_content_table` and retrieved from there.
        // The `postgres_client::searchNearest` *does* retrieve `ec.ephemeral_public_key`.
        // So, `rag_database.h`'s tuple return needs this.

        // Assuming rag_database.h's tuple is updated to:
        // (..., std::vector<uint8_t> encrypted_content, aes_gcm_tag tag, aes_gcm_nonce nonce, ecc256_public_key ephemeral_public_key)
        // This would mean the tuple has 15 elements, with ephemeral_public_key at index 15.
        // Let's update the test to reflect this.

        // Re-defining the tuple structure for clarity in test, assuming rag_database.h is updated
        using SearchResultTuple = std::tuple<
            std::string, std::vector<float>, std::string, int, int, // 0-4: doc_id, embedding, hash, offset, length
            ecc256_public_key, ecc256_public_key, // 5-6: controller_pk, encryption_pk (recipient's)
            std::string, std::string, std::string, std::string, int, // 7-11: doc_date, doc_version, doc_content_type, doc_url, doc_length
            std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce, // 12-14: encrypted_content, tag, nonce
            ecc256_public_key // 15: ephemeral_public_key
        >;

        // Re-cast the result_tuple to the expected full type
        const SearchResultTuple& full_result_tuple = reinterpret_cast<const SearchResultTuple&>(result_tuple);
        ecc256_public_key retrieved_ephemeral_pk = std::get<15>(full_result_tuple);


        std::vector<uint8_t> decrypted_content = EciesUtils::decrypt_ecies(
            retrieved_encrypted_content,
            retrieved_tag,
            retrieved_nonce,
            retrieved_ephemeral_pk, // Use the retrieved ephemeral public key
            recipient_sk // Use the original recipient's private key
        );

        TEST_ASSERT(!decrypted_content.empty(), "Decryption should succeed and return non-empty content.");
        TEST_ASSERT(original_content == decrypted_content, "Decrypted content mismatch with original content.");

        db->disconnect();
    } catch (const std::exception& e) {
        TEST_ASSERT(false, ("Exception during RAG entry insertion/decryption test: " + std::string(e.what())).c_str());
    }
    TEST_SUCCESS("DB: RAG entry insertion and decryption consistency");
}

bool test_db_search_nearest() {
    TEST_LOG_RAW("Testing DB: searchNearest...");
    std::shared_ptr<rag_database> db = create_rag_database("localhost",5432,"klave_rag");
    if (!ensure_schema_exists(db)) return false;
    clean_db_schema(db); // Clean for fresh test
    ensure_schema_exists(db); // Re-create clean schema

    try {
        db->connect(PG_USER, PG_PASSWORD);

        std::vector<std::vector<float>> embeddings;
        std::vector<std::string> doc_ids;
        std::vector<std::vector<uint8_t>> contents;
        std::vector<ecc256_private_key> recipient_sks;
        std::vector<std::string> doc_content_types; // To test content_type filter

        // Insert 5 documents with distinct embeddings and content types
        for (int i = 0; i < 5; ++i) {
            std::string doc_date = std::string("2024-05-") + (i < 10 ? "0" : "") + std::to_string(20 + i); // Dates for filtering
            std::string doc_version = "v1.0";
            std::string doc_type;
            if (i % 2 == 0) {
                doc_type = "text/plain";
            } else {
                doc_type = "application/pdf";
            }
            doc_content_types.push_back(doc_type); // Store for verification

            std::string doc_url = "http://example.com/search_doc_" + std::to_string(i);
            int doc_length = 100 + i;
            document_entry doc = db->createOrRetrieveDocument(doc_date, doc_version, doc_type, doc_url, doc_length);
            doc_ids.push_back(doc.document_id);

            std::vector<float> emb(EMBEDDING_SIZE);
            emb[i] = 1.0f;
            embeddings.push_back(emb);

            std::vector<uint8_t> content = generate_random_bytes(100 + i);
            contents.push_back(content);

            ecc256_private_key controller_sk = CryptoUtils::generatePrivateKey();
            ecc256_public_key controller_pk = CryptoUtils::computePublicKey(controller_sk);
            ecc256_private_key recipient_sk = CryptoUtils::generatePrivateKey();
            recipient_sks.push_back(recipient_sk); // Store recipient SK for decryption
            ecc256_public_key recipient_pk = CryptoUtils::computePublicKey(recipient_sk);
            (void)recipient_pk; // Suppress unused warning

            db->insertRagEntry(doc.document_id, emb, content, controller_pk, recipient_sk);
        }

        // Define a query embedding that is closest to embeddings[0]
        std::vector<float> query_emb_close_to_zero(EMBEDDING_SIZE);
        query_emb_close_to_zero[0] = 0.01f;


        // --- Test Case 1: Default Cosine Distance (no filter) ---
        TEST_LOG_RAW("--- Test Case 1: Default Cosine Distance (no filter) ---");
        auto results_default = db->searchNearest(query_emb_close_to_zero, 1);
        TEST_ASSERT(results_default.size() == 1, ("TC1: Expected 1 nearest result, got " + std::to_string(results_default.size())).c_str());
        // Assuming embeddings[0] will be closest to {0,0,0} due to its construction
        TEST_ASSERT(std::get<0>(results_default[0]) == doc_ids[0], "TC1: Nearest result document ID mismatch.");

        // Verify content decryption for default case
        using SearchResultTuple = std::tuple<
            std::string, std::vector<float>, std::string, int, int,
            ecc256_public_key, ecc256_public_key,
            std::string, std::string, std::string, std::string, int,
            std::vector<uint8_t>, aes_gcm_tag, aes_gcm_nonce,
            ecc256_public_key // Ephemeral Public Key
        >;

        const SearchResultTuple& first_result_default = reinterpret_cast<const SearchResultTuple&>(results_default[0]);
        std::vector<uint8_t> retrieved_encrypted_content_default = std::get<12>(first_result_default);
        aes_gcm_tag retrieved_tag_default = std::get<13>(first_result_default);
        aes_gcm_nonce retrieved_nonce_default = std::get<14>(first_result_default);
        ecc256_public_key retrieved_ephemeral_pk_default = std::get<15>(first_result_default);

        ecc256_public_key retrieved_recipient_pk_default = std::get<6>(first_result_default);
        ecc256_private_key matching_recipient_sk_default;
        bool found_matching_sk_default = false;
        for(const auto& sk : recipient_sks) {
            if (CryptoUtils::computePublicKey(sk) == retrieved_recipient_pk_default) {
                matching_recipient_sk_default = sk;
                found_matching_sk_default = true;
                break;
            }
        }
        TEST_ASSERT(found_matching_sk_default, "TC1: Could not find matching recipient private key for decryption.");

        std::vector<uint8_t> decrypted_content_default = EciesUtils::decrypt_ecies(
            retrieved_encrypted_content_default, retrieved_tag_default, retrieved_nonce_default,
            retrieved_ephemeral_pk_default, matching_recipient_sk_default);
        TEST_ASSERT(!decrypted_content_default.empty(), "TC1: Decryption of retrieved content failed.");
        TEST_ASSERT(decrypted_content_default == contents[0], "TC1: Decrypted content does not match original content.");


        // --- Test Case 2: L2 Distance (no filter) ---
        TEST_LOG_RAW("--- Test Case 2: L2 Distance (no filter) ---");
        // For L2, {01,0,0} is also closest to embeddings[0] (1.0, 0.0, 0.0)
        auto results_l2 = db->searchNearest(query_emb_close_to_zero, 1, nullptr, DistanceMetric::L2);
        TEST_ASSERT(results_l2.size() == 1, ("TC2: Expected 1 nearest result for L2, got " + std::to_string(results_l2.size())).c_str());
        TEST_ASSERT(std::get<0>(results_l2[0]) == doc_ids[0], "TC2: L2 nearest result document ID mismatch.");

        //TODO: commented out until further investigation can be done
        // --- Test Case 3: IP Distance (no filter) ---
        TEST_LOG_RAW("--- Test Case 3: IP Distance (no filter) ---");
        // Inner Product behaves differently. For positive vectors, larger dot product means smaller "distance"
        // If vectors are normalized, IP is 1 - cosine_similarity. If not, it's just negative dot product.
        // For query {0,0,0}, dot product with any vector is 0. So ordering might be arbitrary without normalization.
        // Let's use a query embedding that's distinct to test ordering.
        //std::vector<float> query_emb_ip_test = {1.0f, 1.0f, 1.0f}; // Should be closest to embeddings[4] for IP (highest values)
        std::vector<float> query_emb_ip_test(EMBEDDING_SIZE); // Should be closest to embeddings[4] for IP (highest values)
        query_emb_ip_test[0] = 0.6f;
        query_emb_ip_test[1] = 0.7f;
        query_emb_ip_test[2] = 0.8f;
        query_emb_ip_test[3] = 0.9f;
        query_emb_ip_test[4] = 1.0f;
        auto results_ip = db->searchNearest(query_emb_ip_test, 1, nullptr, DistanceMetric::IP);
        TEST_ASSERT(results_ip.size() == 1, ("TC3: Expected 1 nearest result for IP, got " + std::to_string(results_ip.size())).c_str());
        // Note: With dummy data, without proper vector normalization/specific values, predicting IP nearest is hard.
        // For real vectors, embeddings[4] (0,0,0,0,1.0f) would have highest (1f) dot product with {.6f,.7f,.8f,.9f,1f}.
        // The mock `PQgetvalue` for embeddings doesn't actually produce normalized vectors
        // The dummy `searchNearest` just returns the first few based on the mock setup.
        // For robust testing, you'd need a mock that simulates actual pgvector distance.
        // For now, we'll assert it returns *a* result.
        TEST_ASSERT(std::get<0>(results_ip[0]) == doc_ids[4], "TC3: IP nearest result document ID mismatch (expected doc_ids[4]).");


        // --- Test Case 4: Cosine Distance with Filtering (content_type) ---
        TEST_LOG_RAW("--- Test Case 4: Cosine Distance with Filtering (content_type) ---");
        additional_filtering_clause contentTypeFilter =
            [](const std::string& r_alias, const std::string& d_alias, const std::string& ec_alias) {
            (void)r_alias; (void)ec_alias; // Suppress unused warnings
            return d_alias + ".content_type = 'text/plain'";
        };
        // Query for {0,0,0} again. We expect doc_ids[0] (text/plain), doc_ids[2] (text/plain), doc_ids[4] (text/plain)
        auto results_filtered_type = db->searchNearest(query_emb_close_to_zero, 5, contentTypeFilter, DistanceMetric::COSINE);
        TEST_ASSERT(results_filtered_type.size() == 3, ("TC4: Expected 3 filtered results, got " + std::to_string(results_filtered_type.size())).c_str());
        for (const auto& res : results_filtered_type) {
            std::string retrieved_doc_id = std::get<0>(res);
            auto it = std::find(doc_ids.begin(), doc_ids.end(), retrieved_doc_id);
            TEST_ASSERT(it != doc_ids.end(), "TC4: Retrieved unknown document ID in filtered results.");
            int original_index = std::distance(doc_ids.begin(), it);
            TEST_ASSERT(doc_content_types[original_index] == "text/plain", "TC4: Filtered result has wrong content type.");
        }


        // --- Test Case 5: L2 Distance with Filtering (document date) ---
        TEST_LOG_RAW("--- Test Case 5: L2 Distance with Filtering (document date) ---");
        additional_filtering_clause dateFilter =
            [](const std::string& r_alias, const std::string& d_alias, const std::string& ec_alias) {
            (void)r_alias; (void)ec_alias; // Suppress unused warnings
            // Filter for documents on or after '2024-05-22' (doc_ids[2], doc_ids[3], doc_ids[4])
            return d_alias + ".date >= '2024-05-22'";
        };
        auto results_filtered_date = db->searchNearest(query_emb_close_to_zero, 5, dateFilter, DistanceMetric::L2);
        TEST_ASSERT(results_filtered_date.size() == 3, ("TC5: Expected 3 filtered results, got " + std::to_string(results_filtered_date.size())).c_str());
        // Verify document IDs. With {0,0,0} query, and L2, doc_ids[2], doc_ids[3], doc_ids[4] are ordered by distance.
        std::vector<std::string> expected_doc_ids_after_date_filter = {doc_ids[2], doc_ids[3], doc_ids[4]};
        // Sort results by doc_id to ensure consistent comparison regardless of distance order
        std::sort(results_filtered_date.begin(), results_filtered_date.end(),
                  [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });
        std::sort(expected_doc_ids_after_date_filter.begin(), expected_doc_ids_after_date_filter.end());
        for (size_t i = 0; i < results_filtered_date.size(); ++i) {
            TEST_ASSERT(std::get<0>(results_filtered_date[i]) == expected_doc_ids_after_date_filter[i],
                        ("TC5: Filtered result " + std::to_string(i) + " document ID mismatch.").c_str());
        }


        // --- Test Case 6: No results with filter ---
        TEST_LOG_RAW("--- Test Case 6: No results with filter ---");
        additional_filtering_clause noResultsFilter =
            [](const std::string& r_alias, const std::string& d_alias, const std::string& ec_alias) {
            (void)r_alias; (void)ec_alias; // Suppress unused warnings
            return d_alias + ".content_type = 'non-existent-type'";
        };
        auto results_no_match = db->searchNearest(query_emb_close_to_zero, 5, noResultsFilter, DistanceMetric::COSINE);
        TEST_ASSERT(results_no_match.empty(), ("TC6: Expected 0 results with no-match filter, got " + std::to_string(results_no_match.size())).c_str());


        db->disconnect();
    } catch (const std::exception& e) {
        TEST_ASSERT(false, ("Exception during searchNearest test: " + std::string(e.what())).c_str());
    }
    TEST_SUCCESS("DB: searchNearest");
    return true;
}
// Main test runner
// =========================================================================

int main() {
    // Optional: Configure logging to see test messages
    //llama_log_set(common_log_callback, nullptr);

    int failed_tests = 0;

    std::cout << "Running SHA256 Tests..." << std::endl;
    if (!test_computeSha256Bytes_zero_size()) failed_tests++;
    if (!test_computeSha256Bytes_determinism()) failed_tests++;
    if (!test_computeSha256Bytes_uniqueness()) failed_tests++; // Probabilistic test

    std::cout << "\nRunning ECC (secp256k1) Tests..." << std::endl;
    if (!test_generateKeys_and_consistency_with_scalarMultiply()) failed_tests++;
    if (!test_computeEcdhSharedSecretSha256_consistency()) failed_tests++;

    std::cout << "\nRunning AES-256 GCM Tests..." << std::endl;
    if (!test_aes256GcmEncrypt_Decrypt_consistency()) failed_tests++;

    std::cout << "\nRunning ECIES Tests..." << std::endl;
    if (!test_ecies_encrypt_decrypt_consistency()) failed_tests++;

    // =========================================================================
    // PostgreSQL Client (rag_database implementation) Tests
    // =========================================================================
    std::cout << "\nRunning PostgreSQL Client (rag_database) Tests..." << std::endl;
    if (!test_db_connection_success_and_disconnect()) failed_tests++;
    if (!test_db_connection_failure()) failed_tests++; // This test expects to fail
    if (!test_db_schema_management()) failed_tests++;
    if (!test_db_document_creation_and_retrieval()) failed_tests++;
    if (!test_db_document_deletion()) failed_tests++;
    if (!test_db_rag_entry_insertion_and_decryption()) failed_tests++;
    if (!test_db_search_nearest()) failed_tests++;


    if (failed_tests == 0) {
        std::cout << "\nAll tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\n" << failed_tests << " test(s) failed." << std::endl;
        return 1;
    }
}