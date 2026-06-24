#include "utils.h"
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

fs::path BASE_DIR;
std::atomic<int> g_active_sessions{0};
std::atomic<int> g_tool_calls{0};

std::function<void(const std::string&)> g_log_callback = nullptr;

void mcp_log(const std::string& message) {
    if (g_log_callback) {
        g_log_callback(message);
    }
    std::cerr << message << "\n"; // Always log to console as well
}

json make_error(const json& id, int code, const std::string& message) {
    json response = {
        {"jsonrpc", "2.0"},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
    if (!id.is_null()) {
        response["id"] = id;
    }
    return response;
}

std::optional<fs::path> validate_path(const std::string& user_path) {
    try {
        fs::path full_path = BASE_DIR / user_path;
        fs::path resolved_path = fs::weakly_canonical(full_path);
        
        std::string base_str = BASE_DIR.string();
        std::string res_str = resolved_path.string();
        
        if (!base_str.empty() && base_str.back() != fs::path::preferred_separator) {
            base_str += fs::path::preferred_separator;
        }
        
        if (res_str == BASE_DIR.string() || res_str.rfind(base_str, 0) == 0) {
            return resolved_path;
        } else {
            mcp_log("[Security] Path validation failed: " + res_str + " is outside sandbox.");
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        mcp_log(std::string("[Error] Path validation exception: ") + e.what());
        return std::nullopt;
    }
}
