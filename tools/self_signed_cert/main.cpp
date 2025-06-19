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


//using namespace self_signed;
int main() {
    // Initialize OpenSSL error handling
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms(); // Loads all algorithms, including EC and SHA256

    std::string privateKeyFile = "private_key.pem";
    std::string publicKeyFile = "public_key.pem";
    std::string selfSignedCertFile = "self_signed_certificate.pem";
    std::string commonName = "localhost"; // Or your domain, e.g., "mytestserver.com"

    EC_KEY* ec_key = self_signed::generateECCKeyPair(privateKeyFile, publicKeyFile);

    if (ec_key) {
        if (self_signed::createSelfSignedCertificate(ec_key, selfSignedCertFile, commonName)) {
            std::cout << "Self-signed certificate creation complete." << std::endl;
        } else {
            std::cerr << "Self-signed certificate creation failed." << std::endl;
            // Handle error, maybe return 1 or clean up
        }
        EC_KEY_free(ec_key); // Free the EC_KEY returned by generateECCKeyPair
    } else {
        std::cerr << "Key pair generation failed, cannot create certificate." << std::endl;
        return 1;
    }


    // Clean up OpenSSL
    EVP_cleanup();
    ERR_free_strings();

    return 0;
}