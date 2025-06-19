#include "self_signed.h"
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509.h> // For X.509 certificates
#include <openssl/x509v3.h> // For X.509 extensions
#include <iostream>
#include <fstream>
#include <string>

// Function to handle OpenSSL errors
void self_signed::handleOpenSSLError(const std::string& message) {
    std::cerr << "OpenSSL Error: " << message << std::endl;
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

// Function to generate an ECC key pair and save it to a PEM file
// Modified to return the generated EC_KEY*
EC_KEY* self_signed::generateECCKeyPair(const std::string& privateKeyPath, const std::string& publicKeyPath) {
    std::cout << "Generating ECC key pair (secp256r1)..." << std::endl;

    EC_KEY* ec_key = EC_KEY_new();
    if (!ec_key) {
        handleOpenSSLError("Failed to create EC_KEY object.");
        return nullptr;
    }

    EC_GROUP* ec_group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec_group) {
        EC_KEY_free(ec_key);
        handleOpenSSLError("Failed to get EC group for secp256r1.");
        return nullptr;
    }

    if (EC_KEY_set_group(ec_key, ec_group) != 1) {
        EC_KEY_free(ec_key);
        EC_GROUP_free(ec_group); // Free the group here before exiting
        handleOpenSSLError("Failed to set EC group to EC_KEY.");
        return nullptr;
    }

    if (EC_KEY_generate_key(ec_key) != 1) {
        EC_KEY_free(ec_key);
        EC_GROUP_free(ec_group); // Free the group here before exiting
        handleOpenSSLError("Failed to generate EC key pair.");
        return nullptr;
    }
    std::cout << "ECC key pair generated successfully." << std::endl;

    // Write the private key to a PEM file
    BIO* private_key_bio = BIO_new_file(privateKeyPath.c_str(), "w");
    if (!private_key_bio) {
        EC_KEY_free(ec_key);
        EC_GROUP_free(ec_group); // Free the group here before exiting
        handleOpenSSLError("Failed to create BIO for private key file.");
        return nullptr;
    }

    if (PEM_write_bio_ECPrivateKey(private_key_bio, ec_key, NULL, NULL, 0, NULL, NULL) != 1) {
        BIO_free_all(private_key_bio);
        EC_KEY_free(ec_key);
        EC_GROUP_free(ec_group); // Free the group here before exiting
        handleOpenSSLError("Failed to write private key to PEM file.");
        return nullptr;
    }
    BIO_free_all(private_key_bio);
    std::cout << "Private key saved to: " << privateKeyPath << std::endl;

    // Write the public key to a PEM file (optional, but often useful)
    BIO* public_key_bio = BIO_new_file(publicKeyPath.c_str(), "w");
    if (!public_key_bio) {
        EC_KEY_free(ec_key);
        EC_GROUP_free(ec_group); // Free the group here before exiting
        handleOpenSSLError("Failed to create BIO for public key file.");
        return nullptr;
    }

    if (PEM_write_bio_EC_PUBKEY(public_key_bio, ec_key) != 1) {
        BIO_free_all(public_key_bio);
        EC_KEY_free(ec_key);
        EC_GROUP_free(ec_group); // Free the group here before exiting
        handleOpenSSLError("Failed to write public key to PEM file.");
        return nullptr;
    }
    BIO_free_all(public_key_bio);
    std::cout << "Public key saved to: " << publicKeyPath << std::endl;

    // The EC_GROUP is now owned by EC_KEY, so don't free it separately here.
    // EC_KEY_free will free the associated group.
    EC_GROUP_free(ec_group); // Still good practice to free if not directly owned by ec_key.
                            // In this context, it's safer to free if we created it explicitly.
                            // OpenSSL's documentation can be a bit tricky on ownership.

    return ec_key; // Return the key for self-signed cert creation
}


// Function to create a self-signed certificate
bool self_signed::createSelfSignedCertificate(EC_KEY* ec_key, const std::string& certPath, const std::string& commonName) {
    if (!ec_key) {
        std::cerr << "Error: EC_KEY is null, cannot create certificate." << std::endl;
        return false;
    }

    std::cout << "Creating self-signed certificate..." << std::endl;

    X509* x509 = X509_new();
    if (!x509) {
        handleOpenSSLError("Failed to create X509 object.");
        return false;
    }

    // Set certificate version (V3)
    if (X509_set_version(x509, 2) != 1) { // 2 means V3 certificate
        X509_free(x509);
        handleOpenSSLError("Failed to set X509 version.");
        return false;
    }

    // Set serial number (a unique identifier for the certificate)
    // For self-signed, a simple incrementing number or timestamp is fine.
    // For production CAs, this must be truly unique.
    if (ASN1_INTEGER_set(X509_get_serialNumber(x509), 1) != 1) {
        X509_free(x509);
        handleOpenSSLError("Failed to set serial number.");
        return false;
    }

    // Set validity period (e.g., 1 year)
    X509_gmtime_adj(X509_get_notBefore(x509), 0); // Not valid before now
    X509_gmtime_adj(X509_get_notAfter(x509), 365L * 24 * 60 * 60); // Valid for 1 year

    // Set public key from the generated EC_KEY
    EVP_PKEY* pkey = EVP_PKEY_new(); // Create a new EVP_PKEY object
    if (!pkey) {
        X509_free(x509);
        handleOpenSSLError("Failed to create EVP_PKEY object.");
        return false;
    }
    // Corrected line: Use EVP_PKEY_set1_EC_KEY to associate the EC_KEY with pkey
    if (EVP_PKEY_set1_EC_KEY(pkey, ec_key) != 1) {
        EVP_PKEY_free(pkey); // Free pkey on failure
        X509_free(x509);
        handleOpenSSLError("Failed to set EC_KEY to EVP_PKEY.");
        return false;
    }
    if (X509_set_pubkey(x509, pkey) != 1) {
        EVP_PKEY_free(pkey); // Free pkey on failure
        X509_free(x509);
        handleOpenSSLError("Failed to set public key.");
        return false;
    }



    // Set subject and issuer name (for self-signed, they are the same)
    X509_NAME* name = X509_get_subject_name(x509);
    if (!name) {
        X509_free(x509);
        handleOpenSSLError("Failed to get X509 subject name.");
        return false;
    }

    // Add subject components (Country, State, Locality, Organization, Common Name)
    if (X509_NAME_add_entry_by_txt(name, "C", MBSTRING_UTF8, (const unsigned char*)"GB", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_UTF8, (const unsigned char*)"London", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "L", MBSTRING_UTF8, (const unsigned char*)"London", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_UTF8, (const unsigned char*)"MyOrganization", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, (const unsigned char*)commonName.c_str(), -1, -1, 0) != 1) {
        X509_free(x509);
        handleOpenSSLError("Failed to add subject name entries.");
        return false;
    }

    // Set issuer name to be the same as subject name for self-signed
    if (X509_set_issuer_name(x509, name) != 1) {
        X509_free(x509);
        handleOpenSSLError("Failed to set issuer name.");
        return false;
    }

    // Add X.509 V3 Extensions (optional but good practice for server certs)
    // Basic Constraints: CA:FALSE (not a CA)
    X509_EXTENSION* ex = X509V3_EXT_nconf_nid(NULL, NULL, NID_basic_constraints, (const char*)"CA:FALSE");
    if (!ex) { X509_free(x509); handleOpenSSLError("Failed to create Basic Constraints extension."); return false; }
    X509_add_ext(x509, ex, -1);
    X509_EXTENSION_free(ex);

    // Key Usage: Digital Signature, Key Encipherment
    ex = X509V3_EXT_nconf_nid(NULL, NULL, NID_key_usage, (const char*)"digitalSignature, keyEncipherment");
    if (!ex) { X509_free(x509); handleOpenSSLError("Failed to create Key Usage extension."); return false; }
    X509_add_ext(x509, ex, -1);
    X509_EXTENSION_free(ex);

    // Extended Key Usage: Server Authentication
    ex = X509V3_EXT_nconf_nid(NULL, NULL, NID_ext_key_usage, (const char*)"serverAuth");
    if (!ex) { X509_free(x509); handleOpenSSLError("Failed to create Extended Key Usage extension."); return false; }
    X509_add_ext(x509, ex, -1);
    X509_EXTENSION_free(ex);

    // Subject Alternative Name (SAN) - Crucial for modern browsers
    // Example for 'localhost' and '127.0.0.1'
    std::string san_str = "DNS:localhost,IP:127.0.0.1,DNS:";
    san_str += commonName; // Add the common name as a DNS entry too

    ex = X509V3_EXT_nconf_nid(NULL, NULL, NID_subject_alt_name, san_str.c_str());
    if (!ex) { X509_free(x509); handleOpenSSLError("Failed to create Subject Alternative Name extension."); return false; }
    X509_add_ext(x509, ex, -1);
    X509_EXTENSION_free(ex);

    // Sign the certificate with the private key
    // Create another EVP_PKEY for signing. This is for the *private* key used to sign.
    // It's crucial this is the private key associated with the public key already set in x509.
    EVP_PKEY* signing_pkey = EVP_PKEY_new();
    if (!signing_pkey) {
        // Free x509 which might have taken ownership of pkey's internal key
        // You would need careful error handling here based on OpenSSL's memory model
        X509_free(x509);
        handleOpenSSLError("Failed to create EVP_PKEY for signing.");
        return false;
    }

    // Set the EC_KEY (which contains the private key) into the new EVP_PKEY for signing
    if (EVP_PKEY_set1_EC_KEY(signing_pkey, ec_key) != 1) {
        EVP_PKEY_free(signing_pkey);
        X509_free(x509);
        handleOpenSSLError("Failed to set EC_KEY to EVP_PKEY for signing.");
        return false;
    }

    // The corrected line: use the 'signing_pkey' created above
    if (X509_sign(x509, signing_pkey, EVP_sha256()) <= 0) {
        EVP_PKEY_free(signing_pkey); // Free signing_pkey on failure
        X509_free(x509);
        handleOpenSSLError("Failed to sign X509 certificate.");
        return false;
    }

    EVP_PKEY_free(signing_pkey); // Free the EVP_PKEY used for signing after use.

    std::cout << "Self-signed certificate created successfully." << std::endl;

    // Write the certificate to a PEM file
    BIO* cert_bio = BIO_new_file(certPath.c_str(), "w");
    if (!cert_bio) {
        X509_free(x509);
        handleOpenSSLError("Failed to create BIO for certificate file.");
        return false;
    }

    if (PEM_write_bio_X509(cert_bio, x509) != 1) {
        BIO_free_all(cert_bio);
        X509_free(x509);
        handleOpenSSLError("Failed to write certificate to PEM file.");
        return false;
    }
    BIO_free_all(cert_bio);
    std::cout << "Self-signed certificate saved to: " << certPath << std::endl;

    // Clean up
    X509_free(x509);

    return true;
}

