#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <openssl/ec.h>
#include <openssl/obj_mac.h> // For NID_secp256r1
#include <openssl/rand.h>
#include <openssl/sha.h>     // For SHA256
#include <openssl/evp.h>     // For AES-GCM
#include <openssl/err.h>     // For OpenSSL error handling

#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <iomanip> // For std::hex, std::setw, std::setfill
#include <memory>  // For std::unique_ptr
#include <stdexcept> // For std::runtime_error

// Custom deleters for OpenSSL objects for use with std::unique_ptr
struct EC_KEY_Deleter {
    void operator()(EC_KEY* ptr) const;
};

struct EVP_CIPHER_CTX_Deleter {
    void operator()(EVP_CIPHER_CTX* ptr) const;
};

// Using declarations for type aliases
// ECC (Elliptic Curve Cryptography) types for secp256r1
using ecc256_private_key = std::array<uint8_t, 32>; // secp256r1 private key size
using ecc256_public_key = std::array<uint8_t, 33>;  // secp256r1 compressed public key size

// Hash types
using sha256_hash = std::array<uint8_t, 32>; // SHA256 digest size

// AES-GCM types
using aes256_key = std::array<uint8_t, 32>;  // AES-256 key size
using aes_gcm_nonce = std::array<uint8_t, 12>; // AES-GCM recommended nonce size
using aes_gcm_tag = std::array<uint8_t, 16>;  // AES-GCM tag size

class CryptoUtils {
public:
    static std::vector<unsigned char> export_public_key_to_bytes(EC_KEY* ec_key, point_conversion_form_t format);
    // No longer need explicit constants, as they are implicitly defined by 'using' aliases
    // static constexpr size_t SECP256R1_PRIVATE_KEY_SIZE = 32;
    // static constexpr size_t SECP256R1_PUBLIC_KEY_COMPRESSED_SIZE = 33;
    // static constexpr size_t SHA256_DIGEST_LENGTH_BYTES = 32;
    // static constexpr size_t AES256_KEY_SIZE_BYTES = 32;
    // static constexpr size_t AES_GCM_NONCE_SIZE_BYTES = 12;
    // static constexpr size_t AES_GCM_TAG_SIZE_BYTES = 16;

    static sha256_hash computeSha256Bytes(const std::vector<uint8_t>& data);

    /**
     * @brief Generates a new secp256r1 private key.
     * @return An `ecc256_private_key` representing the private key.
     * @throws std::runtime_error if key generation fails.
     */
    static ecc256_private_key generatePrivateKey();

    /**
     * @brief Computes the public key (compressed form) from a private key.
     * @param private_key The `ecc256_private_key`.
     * @return An `ecc256_public_key` representing the compressed public key.
     * @throws std::runtime_error if public key computation fails.
     */
    static ecc256_public_key computePublicKey(const ecc256_private_key& private_key);

    /**
     * @brief Performs scalar multiplication: (private_key_scalar * public_key_point) = result_public_key.
     * This effectively computes a new public key by multiplying the given public key point
     * by a private key (scalar).
     * @param private_key_scalar The `ecc256_private_key` to use as the scalar.
     * @param public_key_point The `ecc256_public_key` (compressed form) to multiply.
     * @return An `ecc256_public_key` representing the new compressed public key.
     * @throws std::runtime_error if scalar multiplication fails.
     */
    static ecc256_public_key scalarMultiply(
        const ecc256_private_key& private_key_scalar,
        const ecc256_public_key& public_key_point);

    /**
     * @brief Performs an ECDH (Elliptic Curve Diffie-Hellman) key exchange.
     * Computes a shared secret using the local private key and a remote public key.
     * The result is a SHA256 hash of the derived shared secret.
     * @param private_key_local The local `ecc256_private_key`.
     * @param public_key_remote The remote `ecc256_public_key` (compressed form).
     * @return A `sha256_hash` of the shared secret.
     * @throws std::runtime_error if ECDH fails.
     */
    static sha256_hash computeEcdhSharedSecretSha256(
        const ecc256_private_key& private_key_local,
        const ecc256_public_key& public_key_remote);

    /**
     * @brief Encrypts data using AES-256-GCM.
     * @param plaintext The data to encrypt.
     * @param key The `aes256_key`.
     * @param nonce A unique `aes_gcm_nonce`. GENERATE A NEW ONE FOR EACH ENCRYPTION.
     * @param additional_authenticated_data (AAD) Optional additional data to authenticate.
     * @param ciphertext Output parameter: The encrypted data.
     * @param tag Output parameter: The `aes_gcm_tag` authentication tag.
     * @throws std::runtime_error if encryption fails.
     */
    static void aes256GcmEncrypt(
        const std::vector<uint8_t>& plaintext,
        const aes256_key& key,
        const aes_gcm_nonce& nonce,
        const std::vector<uint8_t>& additional_authenticated_data,
        std::vector<uint8_t>& ciphertext,
        aes_gcm_tag& tag);

    /**
     * @brief Decrypts data using AES-256-GCM and verifies the authentication tag.
     * @param ciphertext The encrypted data.
     * @param tag The `aes_gcm_tag` authentication tag received with the ciphertext.
     * @param key The `aes256_key`.
     * @param nonce The `aes_gcm_nonce` used during encryption.
     * @param additional_authenticated_data (AAD) Optional additional data authenticated during encryption.
     * @param plaintext Output parameter: The decrypted data.
     * @return true if decryption and tag verification succeed, false otherwise.
     * @throws std::runtime_error if decryption setup fails (but returns false for tag mismatch).
     */
    static bool aes256GcmDecrypt(
        const std::vector<uint8_t>& ciphertext,
        const aes_gcm_tag& tag,
        const aes256_key& key,
        const aes_gcm_nonce& nonce,
        const std::vector<uint8_t>& additional_authenticated_data,
        std::vector<uint8_t>& plaintext);

    /**
     * @brief Generates a cryptographically secure random nonce.
     * @return An `aes_gcm_nonce` containing the random nonce.
     * @throws std::runtime_error if nonce generation fails.
     */
    static aes_gcm_nonce generateNonce();

    // Helper to print byte arrays for debugging
    static void printBytes(const std::string& label, const uint8_t* data, size_t len);
    static void printBytes(const std::string& label, const std::vector<uint8_t>& data);

    template<size_t N>
    static void printBytes(const std::string& label, const std::array<uint8_t, N>& data) {
        printBytes(label, data.data(), data.size());
    }
};

#endif // CRYPTO_UTILS_H