#include "utils.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <filesystem>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string GetLocalIP() {
    std::string localIp = "127.0.0.1";
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return localIp;
#endif

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock != -1) {
        struct sockaddr_in serv;
        memset(&serv, 0, sizeof(serv));
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = inet_addr("8.8.8.8");
        serv.sin_port = htons(53);

        if (connect(sock, (const struct sockaddr*)&serv, sizeof(serv)) == 0) {
            struct sockaddr_in name;
            socklen_t namelen = sizeof(name);
            if (getsockname(sock, (struct sockaddr*)&name, &namelen) == 0) {
                localIp = inet_ntoa(name.sin_addr);
            }
        }
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    return localIp;
}

fs::path BASE_DIR;
std::atomic<int> g_active_sessions{0};
std::atomic<int> g_tool_calls{0};

// Определение глобальных переменных
std::function<void(const std::string&)> g_log_callback = nullptr;
std::function<void(const std::string&, const std::string&)> g_notify_callback = nullptr;
std::function<bool(const std::string&, const std::string&)> g_confirm_callback = nullptr;
std::function<void()> g_refresh_cluster_callback = nullptr;

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

std::optional<fs::path> validate_path(const std::string& user_path, const fs::path& base_dir) {
    try {
        fs::path full_path = base_dir / user_path;
        fs::path resolved_path = fs::weakly_canonical(full_path);
        
        std::string base_str = base_dir.string();
        std::string res_str = resolved_path.string();
        
        if (!base_str.empty() && base_str.back() != fs::path::preferred_separator) {
            base_str += fs::path::preferred_separator;
        }
        
        if (res_str == base_dir.string() || res_str.rfind(base_str, 0) == 0) {
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
