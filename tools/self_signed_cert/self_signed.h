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
    bool createSelfSignedCertificate(EVP_PKEY* pkey, const std::string& certPath, const std::string& commonName);
    bool createSelfSignedTdxCertificate(EVP_PKEY* pkey, const std::string& certPath);
    bool createSelfSignedTdxCertificateAsString(EVP_PKEY* pkey, std::string& certificate);
    EVP_PKEY* generateECCKeyPair(const std::string& privateKeyPath, const std::string& publicKeyPath);
    bool createKeyAndSelfSignedCertificate(const std::string& privateKeyPath, const std::string& publicKeyPath, const std::string& certPath, const std::string& commonName);
    EVP_PKEY* load_private_key(const std::string& privateKeyPath);

};