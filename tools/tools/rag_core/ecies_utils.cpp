#include "ecies_utils.h"
#include "crypto_utils.h" // For CryptoUtils:: functions
#include <iostream> // For std::cerr
#include <stdexcept> // For std::runtime_error
#include <algorithm> // For std::copy

encryption_result EciesUtils::encrypt_ecies(const std::vector<uint8_t>& plaintext,
                                           const ecc256_public_key& recipient_public_key) {
    encryption_result result;

    // 1. Generate ephemeral ECC key pair (for sender)
    ecc256_private_key ephemeral_private_key = CryptoUtils::generatePrivateKey();
    result.ephemeral_public_key = CryptoUtils::computePublicKey(ephemeral_private_key);

    // 2. Compute shared secret using local ephemeral private key and recipient's public key
    sha256_hash shared_secret_sha256 = CryptoUtils::computeEcdhSharedSecretSha256(
        ephemeral_private_key, recipient_public_key);

    // The shared_secret_sha256 is 32 bytes, which is exactly aes256_key size.
    aes256_key aes_key;
    std::copy(shared_secret_sha256.begin(), shared_secret_sha256.end(), aes_key.begin());

    // 3. Generate a unique nonce for AES-GCM
    result.nonce = CryptoUtils::generateNonce();

    // 4. Encrypt the plaintext using AES-256-GCM
    // No AAD for now, but could be added if needed
    std::vector<uint8_t> additional_authenticated_data;
    CryptoUtils::aes256GcmEncrypt(
        plaintext,
        aes_key,
        result.nonce,
        additional_authenticated_data,
        result.ciphertext,
        result.tag);

    return result;
}

std::vector<uint8_t> EciesUtils::decrypt_ecies(const std::vector<uint8_t>& ciphertext,
                                              const aes_gcm_tag& tag,
                                              const aes_gcm_nonce& nonce,
                                              const ecc256_public_key& ephemeral_public_key,
                                              const ecc256_private_key& recipient_private_key) {
    // 1. Compute shared secret using recipient's private key and ephemeral public key
    sha256_hash shared_secret_sha256 = CryptoUtils::computeEcdhSharedSecretSha256(
        recipient_private_key, ephemeral_public_key);

    aes256_key aes_key;
    std::copy(shared_secret_sha256.begin(), shared_secret_sha256.end(), aes_key.begin());

    // 2. Decrypt the ciphertext using AES-256-GCM
    std::vector<uint8_t> plaintext;
    std::vector<uint8_t> additional_authenticated_data; // No AAD for now

    bool success = CryptoUtils::aes256GcmDecrypt(
        ciphertext,
        tag,
        aes_key,
        nonce,
        additional_authenticated_data,
        plaintext);

    if (!success) {
        //throw std::runtime_error("ECIES decryption failed: authentication tag mismatch or decryption error.");
        return {};
    }

    return plaintext;
}
