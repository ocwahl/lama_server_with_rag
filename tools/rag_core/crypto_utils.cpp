#include "crypto_utils.h"

// Implementations for the custom deleters
void EC_KEY_Deleter::operator()(EC_KEY* ptr) const {
    EC_KEY_free(ptr);
}

void EVP_CIPHER_CTX_Deleter::operator()(EVP_CIPHER_CTX* ptr) const {
    EVP_CIPHER_CTX_free(ptr);
}

// Implementations of CryptoUtils methods

std::vector<unsigned char> CryptoUtils::export_public_key_to_bytes(EC_KEY* ec_key, point_conversion_form_t format) {
    if (!ec_key) {
        throw std::runtime_error("Invalid EC_KEY pointer provided.");
    }

    // Get the curve group
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    if (!group) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to get EC_GROUP from EC_KEY.");
    }

    // Get the public key point
    const EC_POINT* public_point = EC_KEY_get0_public_key(ec_key);
    if (!public_point) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to get public key point from EC_KEY.");
    }

    // Determine the required length for the chosen format
    size_t len = EC_POINT_point2oct(group, public_point, format, nullptr, 0, nullptr);
    if (len == 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to determine required buffer length for public key export.");
    }

    std::vector<unsigned char> public_key_bytes(len);

    // Export the public key point to the buffer
    size_t actual_len = EC_POINT_point2oct(group, public_point, format,
                                           public_key_bytes.data(), len, nullptr);

    if (actual_len == 0 || actual_len != len) { // Check for actual success and size consistency
        ERR_print_errors_fp(stderr);
        std::string error_message = "Failed to export public key. ";
        error_message += "Actual length written: " + std::to_string(actual_len);
        error_message += ", Expected length: " + std::to_string(len);
        throw std::runtime_error(error_message);
    }

    return public_key_bytes;
}

sha256_hash CryptoUtils::computeSha256Bytes(const std::vector<uint8_t>& data) {
    sha256_hash hash;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash.data(), &sha256);
    return hash;
}


ecc256_private_key CryptoUtils::generatePrivateKey() {
    std::unique_ptr<EC_KEY, EC_KEY_Deleter> ec_key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    if (!ec_key) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_KEY for secp256r1.");
    }

    if (EC_KEY_generate_key(ec_key.get()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to generate EC private key.");
    }

    const BIGNUM* priv_bn = EC_KEY_get0_private_key(ec_key.get());
    if (!priv_bn) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to get private key BIGNUM.");
    }

    ecc256_private_key private_key_bytes;
    int num_bytes = BN_bn2binpad(priv_bn, private_key_bytes.data(), private_key_bytes.size());
    if (num_bytes != private_key_bytes.size()) { // Use size() directly from the alias
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to convert private key BIGNUM to bytes with correct padding.");
    }

    return private_key_bytes;
}

ecc256_public_key CryptoUtils::computePublicKey(const ecc256_private_key& private_key) {
    std::unique_ptr<EC_KEY, EC_KEY_Deleter> ec_key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    if (!ec_key) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_KEY for secp256r1.");
    }

    // Set the private key
    std::unique_ptr<BIGNUM, decltype(&BN_free)> priv_bn(BN_bin2bn(private_key.data(), private_key.size(), nullptr), BN_free);
    if (!priv_bn) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to convert private key bytes to BIGNUM.");
    }
    if (EC_KEY_set_private_key(ec_key.get(), priv_bn.get()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set EC private key.");
    }

    // Compute the public key
    const EC_GROUP* group = EC_KEY_get0_group(ec_key.get());
    if (!group) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to get EC group.");
    }
    std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)> pub_point(EC_POINT_new(group), EC_POINT_free);
    if (!pub_point) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_POINT.");
    }
    if (EC_POINT_mul(group, pub_point.get(), priv_bn.get(), nullptr, nullptr, nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to compute public key point.");
    }
    if (EC_KEY_set_public_key(ec_key.get(), pub_point.get()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set EC public key.");
    }

    // Export public key in compressed form
    ecc256_public_key public_key_bytes;
    std::vector<unsigned char> raw_public_key_bytes = export_public_key_to_bytes(ec_key.get(), POINT_CONVERSION_COMPRESSED);
    if (raw_public_key_bytes.size() == 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to export public key (export_public_key_to_bytes returned a null vector).");
    }
    // Now check for size mismatch
    if (raw_public_key_bytes.size() != (long)public_key_bytes.size()) { // Cast public_key_bytes.size() to long for comparison
        ERR_print_errors_fp(stderr);
        std::string error_message = "Failed to export public key in compressed form or size mismatch. #1";
        error_message += "Actual length: " + std::to_string(raw_public_key_bytes.size());
        error_message += ", Expected length: " + std::to_string(public_key_bytes.size());
        throw std::runtime_error(error_message);
    }
    std::copy(std::begin(raw_public_key_bytes),std::end(raw_public_key_bytes),std::begin(public_key_bytes));
    return public_key_bytes;
}

ecc256_public_key CryptoUtils::scalarMultiply(
    const ecc256_private_key& private_key_scalar,
    const ecc256_public_key& public_key_point) {

    // You don't need ec_key_base if you are directly manipulating EC_POINTs.
    // However, you do need the EC_GROUP.
    // A convenient way to get the group is to create a dummy EC_KEY, or directly
    // EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1).
    std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)> group_ptr(EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1), EC_GROUP_free);
    if (!group_ptr) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_GROUP for secp256r1.");
    }
    const EC_GROUP* group = group_ptr.get();


    // Convert the input public_key_point bytes to an EC_POINT
    std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)> base_point(EC_POINT_new(group), EC_POINT_free);
    if (!base_point) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_POINT for base_point.");
    }

    // EC_POINT_oct2point returns 1 on success, 0 on failure
    if (EC_POINT_oct2point(group, base_point.get(),
                           public_key_point.data(), public_key_point.size(), nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to convert input public key bytes to EC_POINT (oct2point failed).");
    }

    // Convert the private_key_scalar bytes to a BIGNUM
    std::unique_ptr<BIGNUM, decltype(&BN_free)> scalar_bn(BN_bin2bn(private_key_scalar.data(), private_key_scalar.size(), nullptr), BN_free);
    if (!scalar_bn) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to convert private key scalar bytes to BIGNUM.");
    }

    // Perform scalar multiplication: result_point = scalar_bn * base_point
    std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)> result_point(EC_POINT_new(group), EC_POINT_free);
    if (!result_point) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_POINT for result_point.");
    }
    // EC_POINT_mul(group, r, n, Q, m, ctx) computes r = n*G + m*Q, where G is the generator
    // We want r = scalar_bn * base_point, so n=0, Q=base_point, m=scalar_bn
    if (EC_POINT_mul(group, result_point.get(), nullptr, base_point.get(), scalar_bn.get(), nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to perform scalar multiplication (EC_POINT_mul).");
    }

    // --- Start of the corrected export part ---

    // Determine the required length for the compressed public key
    // For secp256r1 compressed, it should be 33 bytes (0x02/0x03 || X)
    size_t len = EC_POINT_point2oct(group, result_point.get(), POINT_CONVERSION_COMPRESSED, nullptr, 0, nullptr);
    if (len == 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to determine required buffer length for public key export (EC_POINT_point2oct pre-call).");
    }

    // Prepare the output buffer. ecc256_public_key is usually std::array<unsigned char, 33>
    // Ensure its size matches what EC_POINT_point2oct expects for compressed format.
    if (len != public_key_point.size()) { // Compare with the expected size from ecc256_public_key type
        std::string error_message = "Unexpected compressed public key size. ";
        error_message += "Actual calculated length: " + std::to_string(len);
        error_message += ", Expected ecc256_public_key size: " + std::to_string(public_key_point.size());
        throw std::runtime_error(error_message);
    }

    ecc256_public_key public_key_bytes; // This is typically a std::array<unsigned char, 33>

    // Export the result public key into the array
    size_t actual_len_written = EC_POINT_point2oct(group, result_point.get(), POINT_CONVERSION_COMPRESSED,
                                                   public_key_bytes.data(), public_key_bytes.size(), nullptr);

    if (actual_len_written == 0 || actual_len_written != public_key_bytes.size()) {
        ERR_print_errors_fp(stderr);
        std::string error_message = "Failed to export public key. ";
        error_message += "Bytes written: " + std::to_string(actual_len_written);
        error_message += ", Expected: " + std::to_string(public_key_bytes.size());
        throw std::runtime_error(error_message);
    }

    return public_key_bytes;
}


sha256_hash CryptoUtils::computeEcdhSharedSecretSha256(
    const ecc256_private_key& private_key_local,
    const ecc256_public_key& public_key_remote) {

    // 0. Get the EC_GROUP object for secp256r1
    std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)> group_ptr(EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1), EC_GROUP_free);
    if (!group_ptr) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_GROUP for secp256r1.");
    }
    const EC_GROUP* group = group_ptr.get();

    // 1. Create EC_KEY object for the local private key
    std::unique_ptr<EC_KEY, EC_KEY_Deleter> ec_key_local(EC_KEY_new()); // Create empty EC_KEY
    if (!ec_key_local) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_KEY for local private key.");
    }
    // Set the group for the EC_KEY first
    if (EC_KEY_set_group(ec_key_local.get(), group) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set EC_GROUP for local EC_KEY.");
    }

    std::unique_ptr<BIGNUM, decltype(&BN_free)> priv_bn_local(BN_bin2bn(private_key_local.data(), private_key_local.size(), nullptr), BN_free);
    if (!priv_bn_local) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to convert local private key bytes to BIGNUM.");
    }

    if (EC_KEY_set_private_key(ec_key_local.get(), priv_bn_local.get()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set local EC private key.");
    }

    // It's crucial to compute and set the public key for the local EC_KEY
    // based on the private key. ECDH_compute_key will need it.
    std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)> pub_point_local(EC_POINT_new(group), EC_POINT_free);
    if (!pub_point_local) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_POINT for local public key.");
    }
    // pub_point_local = priv_bn_local * G (generator point)
    if (EC_POINT_mul(group, pub_point_local.get(), priv_bn_local.get(), nullptr, nullptr, nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to compute local public key from private key.");
    }
    if (EC_KEY_set_public_key(ec_key_local.get(), pub_point_local.get()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set local EC public key.");
    }


    // 2. Create EC_POINT object for the remote public key from its bytes
    std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)> pub_point_remote(EC_POINT_new(group), EC_POINT_free);
    if (!pub_point_remote) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EC_POINT for remote public key.");
    }

    // Use EC_POINT_oct2point to convert public_key_remote bytes to EC_POINT
    // o2i_ECPublicKey is for populating an EC_KEY, not an EC_POINT.
    if (EC_POINT_oct2point(group, pub_point_remote.get(),
                           public_key_remote.data(), public_key_remote.size(), nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to convert remote public key bytes to EC_POINT (EC_POINT_oct2point failed). This might indicate malformed public key bytes or wrong curve format.");
    }

    // 3. Compute the shared secret
    // The shared secret length for secp256r1 is 32 bytes (size of the private key).
    std::vector<uint8_t> shared_secret_buffer(private_key_local.size()); // Allocate 32 bytes for the secret

    int secret_len = ECDH_compute_key(
        shared_secret_buffer.data(),      // buffer for shared secret
        shared_secret_buffer.size(),      // size of buffer
        pub_point_remote.get(),           // remote public key point
        ec_key_local.get(),               // local EC_KEY (contains private key)
        nullptr);                         // kdf (optional)

    if (secret_len <= 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to compute ECDH shared secret. Check input keys for validity (e.g., malformed, point at infinity).");
    }
    // No need to resize if buffer was already allocated to exact expected size.
    // shared_secret_buffer.resize(secret_len); // This line is not necessary if size is exactly 32

    // 4. Hash the shared secret (using SHA256 as a KDF for this example)
    sha256_hash shared_secret_hash;
    SHA256(shared_secret_buffer.data(), secret_len, shared_secret_hash.data()); // Use secret_len for actual data size

    return shared_secret_hash;
}


void CryptoUtils::aes256GcmEncrypt(
    const std::vector<uint8_t>& plaintext,
    const aes256_key& key,
    const aes_gcm_nonce& nonce,
    const std::vector<uint8_t>& additional_authenticated_data,
    std::vector<uint8_t>& ciphertext,
    aes_gcm_tag& tag) {

    std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_Deleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX.");
    }

    // Initialize encryption operation
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to initialize AES-GCM encryption context.");
    }

    // Set IV length (nonce length)
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set AES-GCM IV length.");
    }

    // Set key and IV (nonce)
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nonce.data()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set key and IV for AES-GCM encryption.");
    }

    // Provide AAD (Additional Authenticated Data) if any
    int len;
    if (!additional_authenticated_data.empty()) {
        if (EVP_EncryptUpdate(ctx.get(), nullptr, &len, additional_authenticated_data.data(), additional_authenticated_data.size()) != 1) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("Failed to provide AAD for AES-GCM encryption.");
        }
    }

    // Provide the plaintext data
    // Max possible size for ciphertext is plaintext size + block size - 1 (for padding) + tag size
    // For GCM, no explicit padding, but some internal buffering. Safe to over-allocate slightly.
    ciphertext.resize(plaintext.size() + aes256_key{}.size()); // Max possible size (plaintext + max overhead)
    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to encrypt plaintext data.");
    }
    int ciphertext_len = len;

    // Finalize encryption (handle padding and generate tag)
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + ciphertext_len, &len) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to finalize AES-GCM encryption.");
    }
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len); // Trim to actual size

    // Get the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to get AES-GCM authentication tag.");
    }
}

bool CryptoUtils::aes256GcmDecrypt(
    const std::vector<uint8_t>& ciphertext,
    const aes_gcm_tag& tag,
    const aes256_key& key,
    const aes_gcm_nonce& nonce,
    const std::vector<uint8_t>& additional_authenticated_data,
    std::vector<uint8_t>& plaintext) {

    std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_Deleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX.");
    }

    // Initialize decryption operation
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to initialize AES-GCM decryption context.");
    }

    // Set IV length (nonce length)
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set AES-GCM IV length for decryption.");
    }

    // Set key and IV (nonce)
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nonce.data()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set key and IV for AES-GCM decryption.");
    }

    // Provide AAD (Additional Authenticated Data) if any
    int len;
    if (!additional_authenticated_data.empty()) {
        if (EVP_DecryptUpdate(ctx.get(), nullptr, &len, additional_authenticated_data.data(), additional_authenticated_data.size()) != 1) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("Failed to provide AAD for AES-GCM decryption.");
        }
    }

    // Provide the ciphertext data
    plaintext.resize(ciphertext.size()); // Plaintext size will be no more than ciphertext size
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to decrypt ciphertext data.");
    }
    int plaintext_len = len;

    // Set the expected authentication tag
    // Corrected: EVP_CIPHER_CTX_ctrl expects a void* for the data pointer
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), const_cast<void*>(static_cast<const void*>(tag.data()))) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to set AES-GCM authentication tag for verification.");
    }

    // Finalize decryption and verify tag
    // EVP_DecryptFinal_ex returns 1 on success (tag matches), 0 on failure (tag mismatch)
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + plaintext_len, &len) > 0) {
        plaintext_len += len;
        plaintext.resize(plaintext_len); // Trim to actual size
        return true; // Decryption and tag verification successful
    } else {
        // Tag mismatch or other finalization error
        ERR_print_errors_fp(stderr); // Print errors to stderr
        plaintext.clear(); // Clear plaintext as it's untrustworthy
        return false; // Tag verification failed
    }
}

aes_gcm_nonce CryptoUtils::generateNonce() {
    aes_gcm_nonce nonce;
    if (RAND_bytes(nonce.data(), nonce.size()) != 1) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to generate random nonce.");
    }
    return nonce;
}

void CryptoUtils::printBytes(const std::string& label, const uint8_t* data, size_t len) {
    std::cout << label << " (" << len << " bytes): ";
    std::cout << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        std::cout << std::setw(2) << static_cast<int>(data[i]);
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;
}

void CryptoUtils::printBytes(const std::string& label, const std::vector<uint8_t>& data) {
    printBytes(label, data.data(), data.size());
}
