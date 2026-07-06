#include "cluster_handler.h"
#include "cluster_manager.h"
#include "settings_manager.h"
#include "network_utils.h"
#include "crypto_utils.h"
#include "thread_pool.h"
#include "utils.h"
#include "server.h"
#include "../gui/sys_stats.h"
#include "confirmation_manager.h"

#ifndef _WIN32
#include <unistd.h>
#endif

using json = nlohmann::json;

void register_cluster_endpoints(httplib::Server& svr) {
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
            
            ThreadPool::GetInstance().enqueue([actualId, is_new, ip, hostname]() {
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
            });
            
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
                
                extern int g_server_port;
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
                resp["app_version"] = "0.5-alpha";

                res.status = 200;
                res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
        }
    });

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

    svr.Post("/cluster/request_approval", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json data = json::parse(req.body);
            if (!data.contains("encrypted_payload")) {
                res.status = 400;
                res.set_content("Missing encrypted payload", "text/plain");
                return;
            }
            std::string enc = data["encrypted_payload"];
            
            std::string dec;
            if (SettingsManager::Get().appMode == "Child Node") {
                dec = decrypt_aes256(SettingsManager::Get().parentEncryptionKey, enc);
            } else {
                for (const auto& node : ClusterManager::GetInstance().GetNodes()) {
                    dec = decrypt_aes256(node.encryption_key, enc);
                    if (!dec.empty()) break;
                }
            }
            
            if (dec.empty()) {
                res.status = 401;
                res.set_content("Decryption failed", "text/plain");
                return;
            }
            
            json dec_json = json::parse(dec);
            std::string token_name = dec_json.value("token_name", "Unknown");
            std::string action = dec_json.value("action", "Unknown Action");
            int level_int = dec_json.value("level", 1);
            DangerLevel level = static_cast<DangerLevel>(level_int);
            
            bool approved = ConfirmationManager::GetInstance().RequestConfirmation(token_name, action, level);
            if (approved) {
                res.status = 200;
                res.set_content("{\"status\":\"approved\"}", "application/json");
            } else {
                res.status = 403;
                res.set_content("{\"status\":\"denied\"}", "application/json");
            }
        } catch (...) {
            res.status = 400;
        }
    });
}
