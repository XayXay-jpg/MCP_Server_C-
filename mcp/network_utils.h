#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct TokenInfo {
    std::string id;
    std::string name;
    std::string raw_token;
    std::string creation_date;
    bool active;
};

class NetworkUtils {
public:
    // Token Management
    static std::string GenerateToken();
    static std::string HashTokenSHA256(const std::string& token);
    
    static std::vector<TokenInfo> LoadTokens();
    static void SaveTokens(const std::vector<TokenInfo>& tokens);
    static void AddToken(const TokenInfo& token);
    static void UpdateToken(const std::string& old_id, const TokenInfo& updated_token);
    static void DeleteToken(const std::string& id);
    static void ToggleTokenActive(const std::string& id);

    // Network Helpers
    static std::string GetPublicIP();
    static bool CheckLocalAccess(int port);
    static bool CheckExternalAccess(const std::string& ip, int port);
};
