#pragma once


#include <ctime>
#include <functional>
#include <list>
#include <memory>
#include <string>
typedef struct evp_pkey_st EVP_PKEY;

namespace self_signed
{
    void handleOpenSSLError(const std::string& message);
    bool createSelfSignedCertificate(EVP_PKEY* ec_key, const std::string& certPath, const std::string& commonName);
    bool createSelfSignedTdxCertificate(EVP_PKEY* ec_key, const std::string& certPath);
    EVP_PKEY* generateECCKeyPair(const std::string& privateKeyPath, const std::string& publicKeyPath);
    bool createKeyAndSelfSignedCertificate(const std::string& privateKeyPath, const std::string& publicKeyPath, const std::string& certPath, const std::string& commonName);

};