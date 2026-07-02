#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <csignal>
#include <optional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <memory>
#include <map>
#include <cstdlib>
#include <ctime>

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

// Генерация уникального ID для сессии
std::string generate_session_id() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(16);
    for (int i = 0; i < 16; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return tmp_s;
}

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


// Обработчики методов
json handle_initialize(const json& request) {
    mcp_log("[Info] Client connected (Initialization)");
    return {
        {"jsonrpc", "2.0"},
        {"id", request.value("id", json(nullptr))},
        {"result", {
            {"protocolVersion", "2024-11-05"},
            {"capabilities", {
                {"tools", json::object()}
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

int run_mcp_server(int port, const std::string& default_workspace, const std::string& auth_user, const std::string& auth_pass) {
    srand(time(NULL));

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
            
            if (req.path == "/cluster/join" || req.path == "/cluster/configure") {
                token_valid = true;
            } else if (!token_provided.empty()) {
                auto& sm = SettingsManager::Get();
                if (sm.appMode.find("Child") != std::string::npos && token_provided == sm.parentMasterToken && !sm.parentMasterToken.empty()) {
                    if (req.has_header("X-MCP-Signature")) {
                        std::string sig = req.get_header_value("X-MCP-Signature");
                        std::string expected_sig = generate_hmac_sha256(sm.parentEncryptionKey, req.body);
                        if (sig == expected_sig) {
                            token_valid = true;
                        }
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

        std::string session_id = generate_session_id();
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

        mcp_log("[Info] POST Request from Token: " + token_display);
        mcp_log("[Info] Raw Payload: " + req.body);
        if (is_direct_post && g_notify_callback) {
            g_notify_callback("New Connection (POST)", "Token: " + token_display);
        }

        try {
            json request = json::parse(req.body);
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
                    res.set_content(result.dump(), "application/json");
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
            std::string ip = req.remote_addr + ":" + data.value("port", "8080"); 
            std::string hostname = data.value("hostname", "Unknown");
            std::string platform = data.value("platform", "Unknown");
            
            if (id.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing id\"}", "application/json");
                return;
            }
            
            bool is_new = ClusterManager::GetInstance().RegisterNodeRequest(id, ip, hostname, platform);
            if (is_new && g_notify_callback) {
                g_notify_callback("Cluster Connection", "New node request from " + hostname + " (" + ip + ")");
            }
            
            res.status = 200;
            res.set_content("{\"status\":\"pending\"}", "application/json");
            
        } catch (...) {
            res.status = 400;
        }
    });

    svr.Post("/cluster/configure", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json data = json::parse(req.body);
            auto& sm = SettingsManager::Get();
            if (sm.appMode.find("Child") != std::string::npos) {
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
                if (g_notify_callback) g_notify_callback("Cluster Configuration", "Received configuration from Parent node.");
                
                ClusterManager::GetInstance().RegisterNodeRequest("parent", parent_ip, parent_hostname, "Parent Server");
                ClusterManager::GetInstance().SetNodeStatus("parent", "connected");
                if (g_refresh_cluster_callback) g_refresh_cluster_callback();
                
                const char* hn = std::getenv("HOSTNAME");
                if (!hn) hn = std::getenv("COMPUTERNAME");
                std::string hostnameStr = hn ? hn : "UnknownNode";
                std::string platformStr =
#ifdef _WIN32
                "Windows";
#elif defined(__APPLE__)
                "macOS";
#else
                "Linux";
#endif

                json resp;
                resp["status"] = "ok";
                resp["hostname"] = hostnameStr;
                resp["platform"] = platformStr;

                res.status = 200;
                res.set_content(resp.dump(), "application/json");
                return;
            }
            res.status = 400;
        } catch (...) {
            res.status = 400;
        }
    });

    svr.Post("/message", post_handler);
    svr.Post("/sse", post_handler);
    svr.Post("/mcp", post_handler);

    mcp_log("[Info] Server started.\n");
    mcp_log("[Info] Sandbox Directory: " + BASE_DIR.string());
    mcp_log("[Info] SSE Endpoint: http://" + host + ":" + std::to_string(port) + "/sse\n");

    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (SettingsManager::Get().appMode.find("Parent") != std::string::npos) {
            ClusterManager::GetInstance().ReconnectAllNodes();
        }
    }).detach();

    svr.listen(host.c_str(), port);

    return 0;
}
