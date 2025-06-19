#pragma once


#include <ctime>
#include <functional>
#include <list>
#include <memory>
#include <string>
typedef struct ec_key_st EC_KEY;

namespace self_signed
{
    void handleOpenSSLError(const std::string& message);
    bool createSelfSignedCertificate(EC_KEY* ec_key, const std::string& certPath, const std::string& commonName);
    EC_KEY* generateECCKeyPair(const std::string& privateKeyPath, const std::string& publicKeyPath);
};