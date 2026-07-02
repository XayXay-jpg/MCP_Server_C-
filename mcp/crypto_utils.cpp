#include "crypto_utils.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

std::string generate_hmac_sha256(const std::string& key, const std::string& data) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len;

    HMAC(EVP_sha256(), 
         key.c_str(), key.length(), 
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), 
         result, &result_len);

    std::stringstream ss;
    for (unsigned int i = 0; i < result_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
    }
    return ss.str();
}
