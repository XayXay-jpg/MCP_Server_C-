#pragma once

#include <string>

std::string generate_hmac_sha256(const std::string& key, const std::string& data);
