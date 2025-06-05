#ifndef ECIES_UTILS_H
#define ECIES_UTILS_H

#include <vector>
#include <array>
#include <string> // For std::string, though not directly used in the functions themselves, useful for error messages.

#include "crypto_utils.h" // Includes ecc256_public_key, ecc256_private_key, aes_gcm_tag, aes_gcm_nonce, sha256_hash

// Structure to hold ECIES encryption results
struct encryption_result {
    std::vector<uint8_t> ciphertext;
    aes_gcm_tag tag;
    aes_gcm_nonce nonce;
    ecc256_public_key ephemeral_public_key; // Public key for the ephemeral key pair
};

class EciesUtils {
public:
    /**
     * @brief Encrypts data using an ECIES-like scheme (ECDH + AES-256-GCM).
     *
     * This function generates an ephemeral ECC key pair, performs ECDH with the
     * recipient's public key to derive a shared secret, and then uses that
     * secret as the key for AES-256-GCM encryption.
     *
     * @param plaintext The data to encrypt.
     * @param recipient_public_key The recipient's public key (ecc256_public_key).
     * @return An `encryption_result` struct containing ciphertext, tag, nonce, and the ephemeral public key.
     * @throws std::runtime_error if any cryptographic operation fails.
     */
    static encryption_result encrypt_ecies(const std::vector<uint8_t>& plaintext,
                                           const ecc256_public_key& recipient_public_key);

    /**
     * @brief Decrypts data using an ECIES-like scheme (ECDH + AES-256-GCM) and verifies the authentication tag.
     *
     * This function uses the recipient's private key and the ephemeral public key (from encryption)
     * to re-derive the shared secret, and then uses that secret to decrypt the ciphertext
     * and verify its integrity.
     *
     * @param ciphertext The encrypted data.
     * @param tag The authentication tag received with the ciphertext.
     * @param nonce The nonce (IV) used during encryption.
     * @param ephemeral_public_key The ephemeral public key generated during encryption.
     * @param recipient_private_key The recipient's private key (ecc256_private_key).
     * @return The decrypted plaintext data.
     * @throws std::runtime_error if decryption setup fails or authentication tag mismatch occurs.
     */
    static std::vector<uint8_t> decrypt_ecies(const std::vector<uint8_t>& ciphertext,
                                              const aes_gcm_tag& tag,
                                              const aes_gcm_nonce& nonce,
                                              const ecc256_public_key& ephemeral_public_key,
                                              const ecc256_private_key& recipient_private_key);
};

#endif // ECIES_UTILS_H
