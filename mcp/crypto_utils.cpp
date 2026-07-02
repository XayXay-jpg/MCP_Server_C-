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

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <httplib.h>

std::string encrypt_aes256(const std::string& key, const std::string& data) {
    if (data.empty()) return "";
    unsigned char iv[16];
    RAND_bytes(iv, 16);
    
    unsigned char key_bin[32];
    memset(key_bin, 0, 32);
    memcpy(key_bin, key.data(), std::min((size_t)32, key.length()));

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key_bin, iv);

    std::vector<unsigned char> ciphertext(data.length() + EVP_MAX_BLOCK_LENGTH);
    int len;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, (const unsigned char*)data.data(), data.length());
    int ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    std::string result((char*)iv, 16);
    result.append((char*)ciphertext.data(), ciphertext_len);
    
    std::stringstream ss;
    for (size_t i = 0; i < result.length(); i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)result[i];
    }
    return ss.str();
}

std::string decrypt_aes256(const std::string& key, const std::string& encrypted_hex) {
    if (encrypted_hex.empty() || encrypted_hex.length() % 2 != 0) return "";
    
    std::string encrypted;
    encrypted.reserve(encrypted_hex.length() / 2);
    for (size_t i = 0; i < encrypted_hex.length(); i += 2) {
        std::string byteString = encrypted_hex.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        encrypted.push_back(byte);
    }
    
    if (encrypted.length() < 16) return "";
    
    unsigned char iv[16];
    memcpy(iv, encrypted.data(), 16);
    
    unsigned char key_bin[32];
    memset(key_bin, 0, 32);
    memcpy(key_bin, key.data(), std::min((size_t)32, key.length()));

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key_bin, iv);

    std::vector<unsigned char> plaintext(encrypted.length());
    int len;
    EVP_DecryptUpdate(ctx, plaintext.data(), &len, (const unsigned char*)encrypted.data() + 16, encrypted.length() - 16);
    int plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    return std::string((char*)plaintext.data(), plaintext_len);
}
