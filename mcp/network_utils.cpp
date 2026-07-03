#include "network_utils.h"
#include <fstream>
#include <iostream>
#include <array>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <httplib.h>
#include "server.h"

using json = nlohmann::json;

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#include <process.h>
#define GETPID _getpid
#else
#define POPEN popen
#define PCLOSE pclose
#include <unistd.h>
#define GETPID getpid
#endif

// Helper to execute system command and get output
static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(POPEN(cmd, "r"), PCLOSE);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string NetworkUtils::GenerateToken() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(32);
    srand((unsigned)time(NULL) * GETPID());
    for (int i = 0; i < 32; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return "mcp_" + tmp_s;
}

std::string NetworkUtils::HashTokenSHA256(const std::string& token) {
    // Escape single quotes just in case, though alphanumeric token is safe
    std::string safe_token = token;
    std::string cmd = "echo -n '" + safe_token + "' | sha256sum | awk '{print $1}'";
    return exec(cmd.c_str());
}

#include "settings_manager.h"

std::vector<TokenInfo> NetworkUtils::LoadTokens() {
    std::vector<TokenInfo> tokens;
    std::filesystem::path configPath = SettingsManager::Get().GetConfigDir() / "tokens.json";
    std::ifstream f(configPath);
    if (!f.is_open()) return tokens;

    try {
        json j = json::parse(f);
        for (const auto& item : j) {
            TokenInfo t;
            t.id = item.value("id", "");
            t.name = item.value("name", "New Token");
            t.raw_token = item.value("raw_token", "");
            t.creation_date = item.value("creation_date", "");
            t.active = item.value("active", false);
            if (item.contains("permissions")) {
                if (item["permissions"].contains("servers") && item["permissions"]["servers"].is_object()) {
                    for (auto& [server_id, tools_val] : item["permissions"]["servers"].items()) {
                        if (tools_val.is_array()) {
                            t.permissions.allowed_servers[server_id] = tools_val.get<std::vector<std::string>>();
                        }
                    }
                }
                if (item["permissions"].contains("server_workspaces") && item["permissions"]["server_workspaces"].is_object()) {
                    for (auto& [server_id, ws_val] : item["permissions"]["server_workspaces"].items()) {
                        if (ws_val.is_string()) {
                            t.permissions.server_workspaces[server_id] = ws_val.get<std::string>();
                        }
                    }
                }
                t.permissions.allowed_tasks = item["permissions"].value("allowed_tasks", std::vector<std::string>{});
            }
            tokens.push_back(t);
        }
    } catch (...) {
        // Ignore parsing errors and return empty/partial
    }
    return tokens;
}

void NetworkUtils::SaveTokens(const std::vector<TokenInfo>& tokens) {
    json j = json::array();
    for (const auto& t : tokens) {
        j.push_back({
            {"id", t.id},
            {"name", t.name},
            {"raw_token", t.raw_token},
            {"creation_date", t.creation_date},
            {"active", t.active},
            {"permissions", {
                {"servers", t.permissions.allowed_servers},
                {"server_workspaces", t.permissions.server_workspaces},
                {"allowed_tasks", t.permissions.allowed_tasks}
            }}
        });
    }
    std::filesystem::path configPath = SettingsManager::Get().GetConfigDir() / "tokens.json";
    std::ofstream f(configPath);
    f << j.dump(4);
    
    // Notify AI clients about potential permission changes
    notify_tools_changed();
}

void NetworkUtils::AddToken(const TokenInfo& token) {
    auto tokens = LoadTokens();
    tokens.push_back(token);
    SaveTokens(tokens);
}

void NetworkUtils::UpdateToken(const std::string& old_id, const TokenInfo& updated_token) {
    auto tokens = LoadTokens();
    for (auto& t : tokens) {
        if (t.id == old_id) {
            t = updated_token;
            break;
        }
    }
    SaveTokens(tokens);
}

void NetworkUtils::DeleteToken(const std::string& id) {
    auto tokens = LoadTokens();
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [&](const TokenInfo& t) { return t.id == id; }), tokens.end());
    SaveTokens(tokens);
}

void NetworkUtils::ToggleTokenActive(const std::string& id) {
    auto tokens = LoadTokens();
    for (auto& t : tokens) {
        if (t.id == id) {
            t.active = !t.active;
            break;
        }
    }
    SaveTokens(tokens);
}

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <string.h>

bool CheckPort(const std::string& ip, int port, int timeout_sec) {
#ifdef _WIN32
    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) return false;
    
    unsigned long mode = 1;
    ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return false;
    
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        if (ip == "localhost") {
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        } else {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return false;
        }
    }
    
    int res = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
#ifdef _WIN32
    if (res == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
        closesocket(sockfd);
        return false;
    }
#else
    if (res < 0 && errno != EINPROGRESS) {
        close(sockfd);
        return false;
    }
#endif
    
    if (res == 0) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        return true;
    }
    
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    
    res = select((int)sockfd + 1, NULL, &fdset, NULL, &tv);
    if (res == 1) {
        int so_error;
#ifdef _WIN32
        int len = sizeof(so_error);
#else
        socklen_t len = sizeof(so_error);
#endif
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
        if (so_error == 0) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return true;
        }
    }
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    return false;
}

std::string NetworkUtils::GetPublicIP() {
    // Query our public IP by binding directly to the physical network interface,
    // bypassing any active VPN tunnel (e.g. singbox/tun).
    // Only the IP address is sent to api.ipify.org — no tokens or private data.
    std::string ip = exec(
        "curl -s --max-time 3 --interface enp42s0 https://api.ipify.org 2>/dev/null"
    );
    // Validate: must be non-empty, no HTML tags, no spaces, contain a dot (IPv4)
    if (!ip.empty() && ip.size() <= 45 &&
        ip.find('<') == std::string::npos &&
        ip.find(' ') == std::string::npos &&
        ip.find('.') != std::string::npos)
    {
        return ip;
    }
    return "Unknown";
}

bool NetworkUtils::CheckLocalAccess(int port) {
    return CheckPort("127.0.0.1", port, 1);
}

bool NetworkUtils::CheckExternalAccess(const std::string& ip, int port) {
    if (ip == "Unknown" || ip.empty()) return false;
    return CheckPort(ip, port, 1);
}
