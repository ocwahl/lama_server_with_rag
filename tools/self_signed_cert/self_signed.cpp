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
#include "sgx_ttls.h"
#include "sgx_quote_3.h"

#define RSA_3072_PUBLIC_KEY_SIZE 650
#define RSA_3072_PRIVATE_KEY_SIZE 3072

// Function to handle OpenSSL errors
void self_signed::handleOpenSSLError(const std::string& message) {
    std::cerr << "OpenSSL Error: " << message << std::endl;
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

// std::unique_ptr<BIO,decltype(BIO_free_all)*> new_file(const std::string& fname, const std::string& rwmode="w")
// {
//     return std::unique_ptr<BIO,decltype(BIO_free_all)*>(BIO_new_file(fname.c_str(), rwmode.c_str()));
// }



static int get_pkey_by_ec(EVP_PKEY *pk)
{
    std::unique_ptr<EVP_PKEY_CTX, decltype(EVP_PKEY_CTX_free)*> ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL), EVP_PKEY_CTX_free);
    if (ctx == nullptr)
        return 1;
    int res = EVP_PKEY_keygen_init(ctx.get());
    if (res <= 0)
    {
        std::cout << "EC_generate_key failed " << res << std::endl;
        return 1;
    }

    res = EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_secp384r1);
    if (res <= 0)
    {
        std::cout << "EC_generate_key failed " << res << std::endl;
        return 1;
    }

    /* Generate key */
    res = EVP_PKEY_keygen(ctx.get(), &pk);
    if (res <= 0)
    {
        std::cout << "EC_generate_key failed " << res << std::endl;
        return 1;
    }
    return 0;
}


// Function to generate an ECC key pair and save it to a PEM file
// Modified to return the generated EC_KEY*
EVP_PKEY* self_signed::generateECCKeyPair(const std::string& privateKeyPath, const std::string& publicKeyPath) {
    size_t key_size = 48;

    std::unique_ptr<EVP_PKEY, decltype(EVP_PKEY_free)*> pkey(EVP_PKEY_new(), EVP_PKEY_free);
    if (!pkey) {
        handleOpenSSLError("Failed to create EC_KEY object.");
        return nullptr;
    }

    int error_code = get_pkey_by_ec(pkey.get());
    if (error_code != 0) {
        handleOpenSSLError("Failed to get EC group for secp384r1.");
        return nullptr;
    }
    //std::unique_ptr<EC_KEY, decltype(EC_KEY_free)*> ec_key(EVP_PKEY_get1_EC_KEY(pkey.get()),EC_KEY_free);


    std::cout << "Self-Certificate Generation: ECC key pair generated successfully." << std::endl;

    if(privateKeyPath != "") {
        // Write the private key to a PEM file
        std::unique_ptr<BIO, decltype(BIO_free_all)*> private_key_bio(BIO_new_file(privateKeyPath.c_str(), "w"), BIO_free_all);
        if (!private_key_bio) {
            handleOpenSSLError("Failed to create BIO for private key file.");
            return nullptr;
        }

        if (PEM_write_bio_PrivateKey(private_key_bio.get(), pkey.get(), NULL, NULL, 0, NULL, NULL) != 1) {
        //if (PEM_write_bio_ECPrivateKey(private_key_bio.get(), ec_key.get(), NULL, NULL, 0, NULL, NULL) != 1) {
            handleOpenSSLError("Failed to write private key to PEM file.");
            return nullptr;
        }
        std::cout << "Self-Certificate Generation: Private key saved to: " << privateKeyPath << std::endl;
    }
    else {
        std::cout << "Self-Certificate Generation: Private key not saved (path is empty)" << std::endl;
    }

    if(publicKeyPath != "") {

        // Write the public key to a PEM file (optional, but often useful)
        std::unique_ptr<BIO, decltype(BIO_free_all)*> public_key_bio(BIO_new_file(publicKeyPath.c_str(), "w"), BIO_free_all);
        if (!public_key_bio) {
            handleOpenSSLError("Self-Certificate Generation: Failed to create BIO for public key file.");
            return nullptr;
        }

        if (PEM_write_bio_PUBKEY(public_key_bio.get(), pkey.get()) != 1) {
        //if (PEM_write_bio_EC_PUBKEY(public_key_bio.get(), ec_key.get()) != 1) {
            handleOpenSSLError("Self-Certificate Generation: Failed to write public key to PEM file.");
            return nullptr;
        }
        std::cout << "Self-Certificate Generation: Public key saved to: " << publicKeyPath << std::endl;
    }
    else {
        std::cout << "Self-Certificate Generation: Public key not saved (path not provided) " << std::endl;
    }
    return pkey.release(); // Return the key for self-signed cert creation
}

std::vector<uint8_t> to_vector(BIO* bio)
{
    std::vector<uint8_t> result;
    BUF_MEM* pub_mem = NULL;
    BIO_get_mem_ptr(bio, &pub_mem);
    if (!pub_mem || pub_mem->length == 0)
        return result;
    result.assign(reinterpret_cast<uint8_t*>(pub_mem->data), reinterpret_cast<uint8_t*>(pub_mem->data + pub_mem->length));
    return result;
}

// Function to create a self-signed certificate
bool self_signed::createSelfSignedCertificate(EVP_PKEY* pkey, const std::string& certPath, const std::string& commonName) {
    if (!pkey) {
        std::cerr << "Self-Certificate Generation: Error: EVP_PKEY is null, cannot create certificate." << std::endl;
        return false;
    }

    std::cout << "Self-Certificate Generation: Creating self-signed certificate..." << std::endl;

    std::unique_ptr<X509, decltype(X509_free)*> x509(X509_new(), X509_free);
    if (!x509) {
        handleOpenSSLError("Failed to create X509 object.");
        return false;
    }

    // Set certificate version (V3)
    if (X509_set_version(x509.get(), 2) != 1) { // 2 means V3 certificate
        handleOpenSSLError("Failed to set X509 version.");
        return false;
    }

    // Set serial number (a unique identifier for the certificate)
    // For self-signed, a simple incrementing number or timestamp is fine.
    // For production CAs, this must be truly unique.
    if (ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 1) != 1) {
        handleOpenSSLError("Failed to set serial number.");
        return false;
    }

    // Set validity period (e.g., 1 year)
    X509_gmtime_adj(X509_get_notBefore(x509.get()), 0); // Not valid before now
    X509_gmtime_adj(X509_get_notAfter(x509.get()), 365L * 24 * 60 * 60); // Valid for 1 year

    // Set public key from the generated EC_KEY
    if (X509_set_pubkey(x509.get(), pkey) != 1) {
        handleOpenSSLError("Failed to set public key.");
        return false;
    }



    // Set subject and issuer name (for self-signed, they are the same)
    X509_NAME* name = X509_get_subject_name(x509.get());
    if (!name) {
        handleOpenSSLError("Failed to get X509 subject name.");
        return false;
    }

    // Add subject components (Country, State, Locality, Organization, Common Name)
    if (X509_NAME_add_entry_by_txt(name, "C", MBSTRING_UTF8, (const unsigned char*)"GB", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_UTF8, (const unsigned char*)"London", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "L", MBSTRING_UTF8, (const unsigned char*)"London", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_UTF8, (const unsigned char*)"MyOrganization", -1, -1, 0) != 1 ||
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, (const unsigned char*)commonName.c_str(), -1, -1, 0) != 1) {
        handleOpenSSLError("Failed to add subject name entries.");
        return false;
    }

    // Set issuer name to be the same as subject name for self-signed
    if (X509_set_issuer_name(x509.get(), name) != 1) {
        handleOpenSSLError("Failed to set issuer name.");
        return false;
    }

    // Add X.509 V3 Extensions (optional but good practice for server certs)
    // Basic Constraints: CA:FALSE (not a CA)
    std::unique_ptr<X509_EXTENSION,decltype(X509_EXTENSION_free)*> ex (X509V3_EXT_nconf_nid(NULL, NULL, NID_basic_constraints, (const char*)"CA:FALSE"),X509_EXTENSION_free);
    if (!ex) { handleOpenSSLError("Failed to create Basic Constraints extension."); return false; }
    X509_add_ext(x509.get(), ex.get(), -1);

    // Key Usage: Digital Signature, Key Encipherment
    ex.reset(X509V3_EXT_nconf_nid(NULL, NULL, NID_key_usage, (const char*)"digitalSignature, keyEncipherment"));
    if (!ex) { handleOpenSSLError("Failed to create Key Usage extension."); return false; }
    X509_add_ext(x509.get(), ex.get(), -1);

    // Extended Key Usage: Server Authentication
    ex.reset(X509V3_EXT_nconf_nid(NULL, NULL, NID_ext_key_usage, (const char*)"serverAuth"));
    if (!ex) { handleOpenSSLError("Failed to create Extended Key Usage extension."); return false; }
    X509_add_ext(x509.get(), ex.get(), -1);

    // Subject Alternative Name (SAN) - Crucial for modern browsers
    // Example for 'localhost' and '127.0.0.1'
    std::string san_str = "DNS:localhost,IP:127.0.0.1,DNS:";
    san_str += commonName; // Add the common name as a DNS entry too

    ex.reset(X509V3_EXT_nconf_nid(NULL, NULL, NID_subject_alt_name, san_str.c_str()));
    if (!ex) { handleOpenSSLError("Failed to create Subject Alternative Name extension."); return false; }
    X509_add_ext(x509.get(), ex.get(), -1);


    // The corrected line: use the 'signing_pkey' created above
    if (X509_sign(x509.get(), pkey, EVP_sha256()) <= 0) {
        handleOpenSSLError("Failed to sign X509 certificate.");
        return false;
    }


    std::cout << "Self-signed certificate created successfully." << std::endl;

    // Write the certificate to a PEM file
    std::unique_ptr<BIO, decltype(BIO_free_all)*> cert_bio(BIO_new_file(certPath.c_str(), "w"), BIO_free_all);
    if (!cert_bio) {
        handleOpenSSLError("Failed to create BIO for certificate file.");
        return false;
    }

    if (PEM_write_bio_X509(cert_bio.get(), x509.get()) != 1) {
        handleOpenSSLError("Failed to write certificate to PEM file.");
        return false;
    }
    std::cout << "Self-Certificate Generation: Self-signed certificate saved to: " << certPath << std::endl;


    return true;
}
bool self_signed::createSelfSignedTdxCertificate(EVP_PKEY* pkey, const std::string& certPath)
{
    // Extract Public Key (DER format) to public_key_buffer
    //std::unique_ptr<EC_KEY, decltype(EC_KEY_free)*> ec_key(EVP_PKEY_get1_EC_KEY(pkey),EC_KEY_free);
    std::unique_ptr<BIO, decltype(BIO_free_all)*> pub_bio(BIO_new(BIO_s_mem()), BIO_free_all);
    if (!pub_bio)
        return false;
    if (PEM_write_bio_PUBKEY(pub_bio.get(), pkey) <= 0)
    //if (PEM_write_bio_EC_PUBKEY(pub_bio.get(), ec_key.get()) <= 0)
        return false;
    auto public_key_buffer = to_vector(pub_bio.get());

    std::unique_ptr<BIO, decltype(BIO_free_all)*> priv_bio(BIO_new(BIO_s_mem()), BIO_free_all);
    if (!priv_bio)
        return false;
    if (PEM_write_bio_PrivateKey(priv_bio.get(), pkey, NULL, NULL, 0, NULL, NULL) <= 0)
    //if (PEM_write_bio_ECPrivateKey(priv_bio.get(), ec_key.get(), NULL, NULL, 0, NULL, NULL) <= 0)
        return false;
    auto private_key_buffer = to_vector(priv_bio.get());

    public_key_buffer.resize(RSA_3072_PUBLIC_KEY_SIZE);//desperate attempt toi replicate intel buggy code
    private_key_buffer.resize(RSA_3072_PRIVATE_KEY_SIZE);//desperate attempt toi replicate intel buggy code


    // Now call the TEE function with the generated public key
    uint8_t* output_certificate = NULL;
    size_t output_certificate_size = 0;
    const unsigned char certificate_subject_name[] = "CN=Secretarium Enclave, O=Secretarium Ltd,C=GB";
    const quote3_error_t qresult = tee_get_certificate_with_evidence(
        certificate_subject_name,
        private_key_buffer.data(), // If tee_get_certificate_with_evidence uses this as an output, it needs to be sized properly beforehand or reallocated internally.
        private_key_buffer.size(), // If this is an input buffer, it will be 0, which is what led to your problem.
        public_key_buffer.data(),  // This is the generated public key
        public_key_buffer.size(),
        &output_certificate,
        &output_certificate_size);


        std::string quote_status = "OK";
        switch(qresult)
        {
            case SGX_QL_SUCCESS :  break;
            case SGX_QL_ERROR_INVALID_PARAMETER: 
                quote_status = "invalid parameters";
                break;
            case SGX_QL_ERROR_OUT_OF_MEMORY: 
                quote_status = "out of memory";
                break;
            case SGX_QL_ATT_KEY_NOT_INITIALIZED: 
                quote_status = "SGX_QL_ATT_KEY_NOT_INITIALIZED";
                break;
            case SGX_QL_ATT_KEY_CERT_DATA_INVALID: 
                quote_status = "SGX_QL_ATT_KEY_CERT_DATA_INVALID";
                break;
            case SGX_QL_OUT_OF_EPC: 
                quote_status = "SGX_QL_OUT_OF_EPC";
                break;
            case SGX_QL_ENCLAVE_LOST: 
                quote_status = "SGX_QL_ENCLAVE_LOST";
                break;
            case SGX_QL_ENCLAVE_LOAD_ERROR: 
                quote_status = "SGX_QL_ENCLAVE_LOAD_ERROR";
                break;
            case SGX_QL_ERROR_UNEXPECTED: 
                quote_status = "SGX_QL_ERROR_UNEXPECTED";
                break;
            default: 
                quote_status = "unchartered error";
                break;
        }
        if (qresult != SGX_QL_SUCCESS || output_certificate == nullptr)
            {
                std::cerr << "tee_get_certificate_with_evidence failed : " << quote_status.c_str() << std::endl;
                return false;
            }

        // temporary buffer required as if d2i_x509 call is successful
        // certificate_buffer_ptr is incremented to the byte following the parsed
        // data. sending certificate_buffer_ptr as argument will keep
        // output_certificate pointer undisturbed.
        const uint8_t* certificate_buffer_ptr = output_certificate;
        std::unique_ptr<X509, decltype(X509_free)*> x509_cert(d2i_X509(nullptr, &certificate_buffer_ptr, (long)output_certificate_size), X509_free);
        if (x509_cert == nullptr){
            tee_free_certificate(output_certificate);
            return false;
        }
        tee_free_certificate(output_certificate);
    std::cout << "Self-signed certificate created successfully." << std::endl;

    // Write the certificate to a PEM file
    std::unique_ptr<BIO, decltype(BIO_free_all)*> cert_bio(BIO_new_file(certPath.c_str(), "w"), BIO_free_all);
    if (!cert_bio) {
        handleOpenSSLError("Failed to create BIO for certificate file.");
        return false;
    }

    if (PEM_write_bio_X509(cert_bio.get(), x509_cert.get()) != 1) {
        handleOpenSSLError("Failed to write certificate to PEM file.");
        return false;
    }
    std::cout << "Self-Certificate Generation: Self-signed certificate saved to: " << certPath << std::endl;

    // Clean up
    return true;
}


bool self_signed::createKeyAndSelfSignedCertificate(const std::string& privateKeyPath, const std::string& publicKeyPath, const std::string& certPath, const std::string& commonName)
{
    // Initialize OpenSSL error handling
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms(); // Loads all algorithms, including EC and SHA256
    bool success = false;
    std::unique_ptr<EVP_PKEY, decltype(EVP_PKEY_free)*> pkey(self_signed::generateECCKeyPair(privateKeyPath, publicKeyPath), EVP_PKEY_free);

    while(true)
    {
        if(!pkey) {
            std::cerr << "Self-Certificate Generation: Key pair generation failed, cannot create certificate." << std::endl;
            break;
            }
        if(commonName == "TDX"){
            if (self_signed::createSelfSignedTdxCertificate(pkey.get(), certPath)) {
                std::cout << "Self-signed TDX certificate creation complete." << std::endl;
                success = true;
                break;
                } 
            std::cerr << "Self-signed TDX certificate creation failed." << std::endl;
            break;
        }

        if (self_signed::createSelfSignedCertificate(pkey.get(), certPath, commonName)) {
                std::cout << "Self-signed certificate creation complete." << std::endl;
                success = true;
                break;
                }
        std::cerr << "Self-signed certificate creation failed." << std::endl;
        break;
    }
    // Clean up OpenSSL
    EVP_cleanup();
    ERR_free_strings();

    return success;
}


