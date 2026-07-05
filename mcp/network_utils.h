#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

struct TokenPermissions {
    // Map of server_id -> array of allowed tools
    std::map<std::string, std::vector<std::string>> allowed_servers;
    std::map<std::string, std::string> server_workspaces;
    std::vector<std::string> allowed_tasks;
    
    // Map of server_id -> array of tools that require confirmation
    std::map<std::string, std::vector<std::string>> requires_confirmation;
    
    TokenPermissions() {
        // By default, deny everything except local which might have some defaults, 
        // but user requested "Closed by default". 
        // We'll leave it empty.
    }
    
    bool has_tool_access(const std::string& server_id, const std::string& tool_name) const {
        // First check explicit server ID
        auto it = allowed_servers.find(server_id);
        if (it != allowed_servers.end()) {
            for (const auto& tool : it->second) {
                if (tool == "*" || tool == tool_name) return true;
            }
        }
        // Then check global wildcard
        it = allowed_servers.find("*");
        if (it != allowed_servers.end()) {
            for (const auto& tool : it->second) {
                if (tool == "*" || tool == tool_name) return true;
            }
        }
        return false;
    }

    bool needs_confirmation(const std::string& server_id, const std::string& tool_name) const {
        // Check explicit server ID
        auto it = requires_confirmation.find(server_id);
        if (it != requires_confirmation.end()) {
            for (const auto& tool : it->second) {
                if (tool == "*" || tool == tool_name) return true;
            }
        }
        // Check global wildcard
        it = requires_confirmation.find("*");
        if (it != requires_confirmation.end()) {
            for (const auto& tool : it->second) {
                if (tool == "*" || tool == tool_name) return true;
            }
        }
        return false;
    }
};

struct TokenInfo {
    std::string id;
    std::string name;
    std::string raw_token;
    std::string creation_date;
    bool active;
    std::string role = "Developer"; // Admin, Developer, Intern, Automation, Observer, Restricted AI
    std::string overseer_node_id;
    TokenPermissions permissions;
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
