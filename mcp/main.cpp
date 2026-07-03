#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <csignal>
#include <optional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <memory>
#include <map>
#include <cstdlib>
#include <ctime>
#include "../gui/sys_stats.h"

#include <nlohmann/json.hpp>
#include <httplib.h>

#include "utils.h"
#include "tools.h"
#include "network_utils.h"
#include "settings_manager.h"
#include "cluster_manager.h"
#include "crypto_utils.h"
#include "cluster_manager.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

// Управление сессиями SSE
struct Session {
    std::queue<std::string> messages;
    std::mutex mtx;
    std::condition_variable cv;
    bool active = true;
};

std::mutex sessions_mutex;
std::map<std::string, std::shared_ptr<Session>> active_sessions;

// Отправка ответа в очередь нужной сессии (для SSE)
void send_to_session(const std::string& session_id, const json& response) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = active_sessions.find(session_id);
    if (it != active_sessions.end()) {
        auto session = it->second;
        {
            std::lock_guard<std::mutex> slock(session->mtx);
            session->messages.push(response.dump());
        }
        session->cv.notify_one();
    } else {
        mcp_log("[Warning] Attempt to send response to unknown session: " + session_id);
    }
}

void notify_tools_changed() {
    json notification = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/tools/list_changed"}
    };
    std::lock_guard<std::mutex> lock(sessions_mutex);
    for (const auto& [id, session] : active_sessions) {
        if (session->active) {
            std::lock_guard<std::mutex> slock(session->mtx);
            session->messages.push(notification.dump());
            session->cv.notify_one();
        }
    }
}

// Обработчики методов
json handle_initialize(const json& request) {
    mcp_log("[Info] Client connected (Initialization)");
    return {
        {"jsonrpc", "2.0"},
        {"id", request.value("id", json(nullptr))},
        {"result", {
            {"protocolVersion", "2024-11-05"},
            {"capabilities", {
                {"tools", {
                    {"listChanged", true}
                }}
            }},
            {"serverInfo", {
                {"name", "secure-cpp-mcp-http"},
                {"version", "1.0.0"}
            }}
        }}
    };
}

json handle_tools_list(const json& request, const TokenInfo& token) {
    return {
        {"jsonrpc", "2.0"},
        {"id", request.value("id", json(nullptr))},
        {"result", {
            {"tools", get_available_tools(token)}
        }}
    };
}


std::unique_ptr<httplib::Server> g_svr;

void stop_mcp_server() {
    if (g_svr) {
        g_svr->stop();
    }
}

int g_server_port = 3000;

int run_mcp_server(int port, const std::string& default_workspace, const std::string& auth_user, const std::string& auth_pass) {
    srand(time(NULL));
    g_server_port = port;
    
    // Initialize cluster nodes and reconnect if necessary
    ClusterManager::GetInstance().LoadNodes();
    ClusterManager::GetInstance().ReconnectAllNodes();

    std::string host = "0.0.0.0"; // 0.0.0.0 позволяет принимать подключения из сети    
    try {
        if (!fs::exists(default_workspace)) {
            fs::create_directories(default_workspace);
        }
        BASE_DIR = fs::canonical(default_workspace);
    } catch (const std::exception& e) {
        mcp_log("[Fatal] Failed to initialize sandbox directory: " + std::string(e.what()));
        return 1;
    }

    g_svr = std::make_unique<httplib::Server>();
    auto& svr = *g_svr;

    std::string expected_auth = "";
    if (!auth_user.empty() && !auth_pass.empty()) {
        mcp_log("[Info] Server configured with static token placeholder, but using dynamic token verification.");
    }

    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.method == "OPTIONS") {
            return httplib::Server::HandlerResponse::Unhandled;
        }

        bool has_valid_session = false;
        if (req.has_param("sessionId")) {
            std::string sid = req.get_param_value("sessionId");
            std::lock_guard<std::mutex> lock(sessions_mutex);
            if (active_sessions.find(sid) != active_sessions.end()) {
                has_valid_session = true;
            }
        }

        if (!has_valid_session) {
            std::string token_provided = "";
            
            // Check Authorization header (Bearer <token>)
            if (req.has_header("Authorization")) {
                std::string auth_header = req.get_header_value("Authorization");
                if (auth_header.rfind("Bearer ", 0) == 0) {
                    token_provided = auth_header.substr(7);
                }
            }
            
            // Check query parameter (?token=)
            if (token_provided.empty() && req.has_param("token")) {
                token_provided = req.get_param_value("token");
            }

            bool token_valid = false;
            
            if (req.path == "/cluster/join" || req.path == "/cluster/configure" || req.path == "/cluster/stats") {
                token_valid = true;
            } else if (!token_provided.empty()) {
                auto& sm = SettingsManager::Get();
                if (!sm.parentMasterToken.empty() && token_provided == sm.parentMasterToken) {
                    if (req.has_header("X-MCP-Signature")) {
                        token_valid = true; // Signature will be verified in post_handler
                    } else {
                        mcp_log("[Auth] Parent token matched but no X-MCP-Signature header from " + req.remote_addr);
                    }
                }
                
                if (!token_valid) {
                    auto tokens = NetworkUtils::LoadTokens();
                    for (const auto& t : tokens) {
                        if (t.active && t.raw_token == token_provided) {
                            token_valid = true;
                            break;
                        }
                    }
                }
            }

            if (!token_valid) {
                std::string log_reason = token_provided.empty() ? "no token provided" : "invalid/revoked token";
                mcp_log("[Auth] 401 Unauthorized: " + req.method + " " + req.path + " from " + req.remote_addr + " — " + log_reason);
                res.status = 401;
                res.set_header("WWW-Authenticate", "Bearer realm=\"MCP Server\"");
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "*");
                res.set_content("Unauthorized", "text/plain");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // Глобальные заголовки CORS (очень важно для браузерных фронтендов типа Open WebUI)
    auto set_cors = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "*");
    };

    svr.Options("/sse", [set_cors](const httplib::Request&, httplib::Response& res) {
        set_cors(res);
        res.status = 204;
    });
    
    svr.Options("/message", [set_cors](const httplib::Request&, httplib::Response& res) {
        set_cors(res);
        res.status = 204;
    });

    // 1. SSE Endpoint
    svr.Get("/sse", [host, port, set_cors](const httplib::Request& req, httplib::Response& res) {
        set_cors(res);
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");

        std::string session_id = generate_random_string(16);
        auto session = std::make_shared<Session>();
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            active_sessions[session_id] = session;
            g_active_sessions = active_sessions.size();
        }

        std::string token_provided = "";
        if (req.has_header("Authorization")) {
            std::string auth_header = req.get_header_value("Authorization");
            if (auth_header.rfind("Bearer ", 0) == 0) token_provided = auth_header.substr(7);
        }
        if (token_provided.empty() && req.has_param("token")) token_provided = req.get_param_value("token");
        
        std::string token_display = "None";
        if (!token_provided.empty()) {
            auto tokens = NetworkUtils::LoadTokens();
            for (const auto& t : tokens) {
                if (t.raw_token == token_provided) {
                    token_display = t.name.empty() ? t.id : t.name;
                    std::string masked = t.raw_token.length() > 8 ? 
                        (t.raw_token.substr(0, 4) + "..." + t.raw_token.substr(t.raw_token.length() - 4)) : "***";
                    token_display += " [" + masked + "]";
                    break;
                }
            }
        }

        mcp_log("[Info] New SSE connection. SessionID: " + session_id + " | Token: " + token_display);
        if (g_notify_callback) {
            g_notify_callback("New Connection (SSE)", "Token: " + token_display);
        }

        res.set_chunked_content_provider(
            "text/event-stream",
            [session, session_id, host, port](size_t offset, httplib::DataSink &sink) {
                if (offset == 0) {
                    // Отправляем первое событие endpoint (как требует MCP)
                    std::string endpoint = "http://" + host + ":" + std::to_string(port) + "/message?sessionId=" + session_id;
                    std::string evt = "event: endpoint\ndata: " + endpoint + "\n\n";
                    sink.write(evt.data(), evt.size());
                }

                // Ждем появления сообщений в очереди с таймаутом для проверки соединения
                std::unique_lock<std::mutex> lock(session->mtx);
                bool msg_available = session->cv.wait_for(lock, std::chrono::seconds(15), [session]() {
                    return !session->messages.empty() || !session->active;
                });

                if (!msg_available) {
                    // Таймаут. Отправляем keep-alive ping, чтобы проверить жив ли сокет
                    std::string ping = ":\n\n";
                    if (!sink.write(ping.data(), ping.size())) {
                        // Клиент отключился
                        std::lock_guard<std::mutex> slock(sessions_mutex);
                        active_sessions.erase(session_id);
                        g_active_sessions = active_sessions.size();
                        return false;
                    }
                    return true;
                }

                if (!session->active && session->messages.empty()) {
                    sink.done();
                    std::lock_guard<std::mutex> slock(sessions_mutex);
                    active_sessions.erase(session_id);
                    g_active_sessions = active_sessions.size();
                    return false; // Конец потока
                }

                // Отправляем все сообщения
                while (!session->messages.empty()) {
                    std::string msg = std::move(session->messages.front());
                    session->messages.pop();
                    std::string evt = "event: message\ndata: " + msg + "\n\n";
                    if (!sink.write(evt.data(), evt.size())) {
                        std::lock_guard<std::mutex> slock(sessions_mutex);
                        active_sessions.erase(session_id);
                        g_active_sessions = active_sessions.size();
                        return false;
                    }
                }
                return true; // Продолжаем слушать
            }
        );
    });

    // 2. HTTP POST Messages Endpoint (and fallback for Open WebUI direct POST)
    auto post_handler = [set_cors](const httplib::Request& req, httplib::Response& res) {
        set_cors(res);

        auto& sm_check = SettingsManager::Get();
        if (!sm_check.parentMasterToken.empty()) {
            std::string tp = "";
            if (req.has_header("Authorization")) {
                std::string auth_header = req.get_header_value("Authorization");
                if (auth_header.rfind("Bearer ", 0) == 0) tp = auth_header.substr(7);
            }
            if (tp.empty() && req.has_param("token")) tp = req.get_param_value("token");
            
            if (tp == sm_check.parentMasterToken) {
                if (req.has_header("X-MCP-Signature")) {
                    std::string sig = req.get_header_value("X-MCP-Signature");
                    std::string expected_sig = generate_hmac_sha256(sm_check.parentEncryptionKey, req.body);
                    if (sig != expected_sig) {
                        mcp_log("[Auth] HMAC signature mismatch in post_handler!");
                        res.status = 401;
                        res.set_content("Invalid Signature", "text/plain");
                        return;
                    }
                } else {
                    res.status = 401;
                    res.set_content("Missing Signature", "text/plain");
                    return;
                }
            }
        }

        bool is_direct_post = false;
        std::string session_id;
        
        if (req.has_param("sessionId")) {
            session_id = req.get_param_value("sessionId");
        } else {
            is_direct_post = true; // Open WebUI sends POST without sessionId directly!
        }

        // Extract and log token and payload
        std::string token_provided = "";
        if (req.has_header("Authorization")) {
            std::string auth_header = req.get_header_value("Authorization");
            if (auth_header.rfind("Bearer ", 0) == 0) {
                token_provided = auth_header.substr(7);
            }
        }
        if (token_provided.empty() && req.has_param("token")) {
            token_provided = req.get_param_value("token");
        }
        
        std::string token_display = "None";
        TokenInfo current_token;
        bool has_token = false;
        
        if (!token_provided.empty()) {
            auto tokens = NetworkUtils::LoadTokens();
            for (const auto& t : tokens) {
                if (t.raw_token == token_provided) {
                    current_token = t;
                    has_token = true;
                    token_display = t.name.empty() ? t.id : t.name;
                    std::string masked = t.raw_token.length() > 8 ? 
                        (t.raw_token.substr(0, 4) + "..." + t.raw_token.substr(t.raw_token.length() - 4)) : "***";
                    token_display += " [" + masked + "]";
                    break;
                }
            }
        }

        auto& sm_mode = SettingsManager::Get();
        bool is_child_mode = !sm_mode.parentMasterToken.empty();
        
        // Decrypt full payload if present
        std::string actual_body = req.body;
        bool was_encrypted = false;
        
        if (is_child_mode) {
            try {
                json peek = json::parse(req.body);
                if (peek.contains("encrypted_payload")) {
                    std::string enc = peek.value("encrypted_payload", "");
                    std::string dec = decrypt_aes256(sm_mode.parentEncryptionKey, enc);
                    if (!dec.empty()) {
                        actual_body = dec;
                        was_encrypted = true;
                        
                        // Extract forwarded user token from decrypted payload
                        json decrypted_peek = json::parse(actual_body);
                        if (decrypted_peek.contains("_forwarded_token")) {
                            token_provided = decrypted_peek.value("_forwarded_token", "");
                            has_token = false; // Reset to re-fetch below
                        }
                    }
                }
            } catch (...) {}
        }
        
        if (!token_provided.empty() && !has_token) {
            auto tokens = NetworkUtils::LoadTokens();
            for (const auto& t : tokens) {
                if (t.raw_token == token_provided) {
                    current_token = t;
                    has_token = true;
                    token_display = t.name.empty() ? t.id : t.name;
                    std::string masked = t.raw_token.length() > 8 ? 
                        (t.raw_token.substr(0, 4) + "..." + t.raw_token.substr(t.raw_token.length() - 4)) : "***";
                    token_display += " [" + masked + "]";
                    break;
                }
            }
        }
        
        mcp_log("[Info] POST Request from Token: " + token_display);
        mcp_log("[Info] Raw Payload: " + req.body);
        
        // Child server: log incoming tool requests from parent
        if (is_child_mode && has_token == false && req.has_header("X-MCP-Signature")) {
            try {
                json peek = json::parse(req.body);
                std::string method = peek.value("method", "");
                if (method == "tools/call" && peek.contains("params")) {
                    std::string tool_name = peek["params"].value("name", "unknown");
                    mcp_log("[Child] Incoming tool request from Parent (" + req.remote_addr + "): " + tool_name);
                    if (g_notify_callback) g_notify_callback("Tool Called by Parent", "Tool: " + tool_name + " from " + req.remote_addr);
                }
            } catch (...) {}
        }
        
        if (is_direct_post && g_notify_callback) {
            g_notify_callback("New Connection (POST)", "Token: " + token_display);
        }

        try {
            json request = json::parse(actual_body);
            if (request.contains("_forwarded_token_enc")) {
                request.erase("_forwarded_token_enc");
            }
            if (request.contains("_forwarded_token")) {
                request["_is_forwarded"] = true;
                request.erase("_forwarded_token");
            }
            json result;
            
            if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
                result = make_error(request.value("id", json(nullptr)), -32600, "Invalid Request");
            } else {
                std::string method = request.value("method", "");
                if (method == "initialize") {
                    result = handle_initialize(request);
                } else if (method == "tools/list") {
                    result = handle_tools_list(request, current_token);
                } else if (method == "tools/call") {
                    result = handle_tools_call(request, current_token);
                } else if (method == "notifications/initialized" || method == "ping") {
                    if (request.contains("id")) {
                        result = {
                            {"jsonrpc", "2.0"},
                            {"id", request["id"]},
                            {"result", json::object()}
                        };
                    } else {
                        // Empty response for notifications
                        result = json(nullptr);
                    }
                } else {
                    result = make_error(request.value("id", json(nullptr)), -32601, "Method not found");
                }
            }
            
            if (is_direct_post) {
                // Если это прямой POST-запрос (Open WebUI Streamable HTTP), возвращаем JSON сразу в ответе!
                res.status = 200;
                if (!result.is_null()) {
                    std::string res_str = result.dump();
                    if (was_encrypted) {
                        res_str = json{{"encrypted_payload", encrypt_aes256(sm_mode.parentEncryptionKey, res_str)}}.dump();
                    }
                    res.set_content(res_str, "application/json");
                } else {
                    res.set_content("{}", "application/json");
                }
            } else {
                // Если это SSE, MCP требует 202 Accepted, а ответ шлем в очередь
                res.status = 202;
                res.set_content("Accepted", "text/plain");
                if (!result.is_null()) {
                    send_to_session(session_id, result);
                }
            }
            
        } catch (const json::parse_error& e) {
            mcp_log("[Error] JSON parse error: " + std::string(e.what()));
            json err = make_error(json(nullptr), -32700, "Parse error");
            if (is_direct_post) {
                res.status = 400;
                res.set_content(err.dump(), "application/json");
            } else {
                res.status = 202;
                res.set_content("Accepted", "text/plain");
                send_to_session(session_id, err);
            }
        } catch (const std::exception& e) {
            mcp_log("[Error] General error: " + std::string(e.what()));
            json err = make_error(json(nullptr), -32603, "Internal error");
            if (is_direct_post) {
                res.status = 500;
                res.set_content(err.dump(), "application/json");
            } else {
                res.status = 202;
                res.set_content("Accepted", "text/plain");
                send_to_session(session_id, err);
            }
        }
    };
    
    svr.Post("/cluster/join", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json data = json::parse(req.body);
            std::string id = data.value("id", "");
            std::string port_str = data.value("port", "8080");
            std::string ip = req.remote_addr + ":" + port_str; 
            std::string hostname = data.value("hostname", "Unknown");
            std::string platform = data.value("platform", "Unknown");
            
            mcp_log("[Cluster] /cluster/join from " + req.remote_addr + " hostname='" + hostname + "' port=" + port_str + " id=" + id);
            
            if (id.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing id\"}", "application/json");
                return;
            }
            
            std::string actualId;
            bool is_new = ClusterManager::GetInstance().RegisterNodeRequest(id, ip, hostname, platform, actualId);
            mcp_log("[Cluster] RegisterNodeRequest: is_new=" + std::string(is_new ? "true" : "false") + " actualId='" + actualId + "' stored_ip=" + ip);
            
            res.status = 200;
            res.set_content("{\"status\":\"pending\"}", "application/json");
            
            std::thread([actualId, is_new, ip, hostname]() {
                if (is_new) {
                    if (g_confirm_callback) {
                        bool approved = g_confirm_callback("Incoming Join Request", "Node '" + hostname + "' (" + ip + ") wants to join the cluster.\nApprove this connection?");
                        if (approved) {
                            mcp_log("[Cluster] User approved new node '" + actualId + "'");
                            ClusterManager::GetInstance().ApproveNode(actualId);
                        } else {
                            ClusterManager::GetInstance().RemoveNode(actualId);
                            if (g_refresh_cluster_callback) g_refresh_cluster_callback();
                        }
                    } else if (g_notify_callback) {
                        g_notify_callback("Cluster Connection", "New node request from " + hostname + " (" + ip + ")");
                    }
                } else {
                    // Node already exists, auto-approve and re-configure it
                    mcp_log("[Cluster] Known node '" + actualId + "' reconnected, calling ApproveNode...");
                    ClusterManager::GetInstance().ApproveNode(actualId);
                }
            }).detach();
            
        } catch (...) {
            res.status = 400;
        }
    });

    svr.Post("/cluster/configure", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json data = json::parse(req.body);
            auto& sm = SettingsManager::Get();
                std::string parent_hostname = data.value("parent_hostname", "Parent Node");
                std::string parent_ip = req.remote_addr;
                
                bool is_new_parent = sm.parentMasterToken.empty() || sm.parentMasterToken != data.value("master_token", "");
                
                if (is_new_parent && g_confirm_callback) {
                    std::string prompt = sm.parentMasterToken.empty() ? 
                        "Allow Parent Node '" + parent_hostname + "' (" + parent_ip + ") to control this device?" :
                        "A different Parent Node '" + parent_hostname + "' (" + parent_ip + ") is trying to connect.\nOverride existing connection?";
                    
                    bool approved = g_confirm_callback("Incoming Connection", prompt);
                    if (!approved) {
                        res.status = 403;
                        res.set_content("{\"error\":\"Rejected by user\"}", "application/json");
                        return;
                    }
                }

                sm.parentMasterToken = data.value("master_token", "");
                sm.parentEncryptionKey = data.value("encryption_key", "");
                sm.Save();
                mcp_log("[Info] Node successfully configured by Parent.");
                
                if (data.contains("synced_tokens_enc")) {
                    std::string enc = data.value("synced_tokens_enc", "");
                    std::string decrypted = decrypt_aes256(sm.parentEncryptionKey, enc);
                    if (!decrypted.empty()) {
                        try {
                            json tokens_json = json::parse(decrypted);
                            auto tokens = NetworkUtils::LoadTokens();
                            // Only update active status and properties, don't overwrite local permissions
                            for (const auto& tj : tokens_json) {
                                bool found = false;
                                for (auto& t : tokens) {
                                    if (t.raw_token == tj.value("raw_token", "")) {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    TokenInfo nt;
                                    nt.id = tj.value("id", "");
                                    nt.name = tj.value("name", "");
                                    nt.raw_token = tj.value("raw_token", "");
                                    nt.creation_date = tj.value("creation_date", "");
                                    nt.active = tj.value("active", true);
                                    tokens.push_back(nt);
                                }
                            }
                            NetworkUtils::SaveTokens(tokens);
                            mcp_log("[Info] Synchronized tokens from parent.");
                        } catch (...) {}
                    }
                }
                
                if (g_notify_callback) g_notify_callback("Cluster Configuration", "Received configuration from Parent node '" + parent_hostname + "'.");
                
                // Store parent node with all metadata — build ip:port correctly
                int parent_port = data.value("parent_port", 3000);
                std::string parent_addr = parent_ip + ":" + std::to_string(parent_port);
                std::string dummyId;
                ClusterManager::GetInstance().RegisterNodeRequest("parent", parent_addr, parent_hostname, data.value("parent_platform", "Unknown"), dummyId);
                {
                    ClusterManager::GetInstance().UpdateNodeMetadata("parent", data.value("parent_platform", ""), parent_addr, data.value("parent_version", ""), true);
                }
                ClusterManager::GetInstance().SetNodeStatus("parent", "connected");
                if (g_refresh_cluster_callback) g_refresh_cluster_callback();
                
                const char* hn = std::getenv("HOSTNAME");
                if (!hn) hn = std::getenv("COMPUTERNAME");
                std::string hostnameStr = hn ? hn : "";
                if (hostnameStr.empty()) {
                    char buf[256] = {};
                    if (gethostname(buf, sizeof(buf)) == 0) hostnameStr = buf;
                }
                if (hostnameStr.empty()) hostnameStr = "UnknownNode";
                std::string platformStr =
#ifdef _WIN32
                "Windows";
#elif defined(__APPLE__)
                "macOS";
#else
                "Linux";
#endif
                // Collect local IP
                std::string localIp = req.local_addr;
                if (localIp.empty() || localIp == "127.0.0.1" || localIp == "localhost" || localIp == "0.0.0.0") {
                    localIp = GetLocalIP();
                }

                json resp;
                resp["status"] = "ok";
                resp["hostname"] = hostnameStr;
                resp["platform"] = platformStr;
                resp["local_ip"] = localIp;
                resp["app_version"] = "0.1.8";

                res.status = 200;
                res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
        }
    });

    svr.Post("/message", post_handler);
    svr.Post("/sse", post_handler);
    svr.Post("/mcp", post_handler);

    // /cluster/stats — returns system resource usage for this node (used by parent GUI)
    svr.Get("/cluster/stats", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");

        SystemStats stats = GetSystemStats();

        json resp = {
            {"cpu_percent",  stats.cpuPercent},
            {"ram_used_gb",  stats.ramUsedGB},
            {"ram_total_gb", stats.ramTotalGB},
            {"ram_percent",  stats.ramPercent},
            {"disk_used_gb", stats.diskUsedGB},
            {"disk_total_gb",stats.diskTotalGB},
            {"disk_percent", stats.diskPercent},
            {"vram_used_gb", stats.vramUsedGB},
            {"vram_total_gb",stats.vramTotalGB},
            {"vram_percent", stats.vramPercent},
            {"has_vram",     stats.hasVRAM}
        };
        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    });

    mcp_log("[Info] Server started.\n");
    mcp_log("[Info] Sandbox Directory: " + BASE_DIR.string());
    mcp_log("[Info] SSE Endpoint: http://" + host + ":" + std::to_string(port) + "/sse\n");

    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        ClusterManager::GetInstance().ReconnectAllNodes();
    }).detach();

    svr.listen(host.c_str(), port);

    return 0;
}
