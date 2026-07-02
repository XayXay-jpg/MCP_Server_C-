#pragma once

#include <string>

std::string generate_hmac_sha256(const std::string& key, const std::string& data);
std::string encrypt_aes256(const std::string& key, const std::string& data);
std::string decrypt_aes256(const std::string& key, const std::string& encrypted_b64);
