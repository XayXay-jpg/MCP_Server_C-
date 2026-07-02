#include "cluster_manager.h"
#include "utils.h"
#include "network_utils.h"
#include "crypto_utils.h"
#include "server.h"
#include "settings_manager.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <httplib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

using json = nlohmann::json;

long GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void ClusterManager::LoadNodes() {
    std::lock_guard<std::mutex> lock(mtx);
    nodes.clear();
    std::filesystem::path configPath = SettingsManager::Get().GetConfigDir() / "cluster_nodes.json";
    std::ifstream f(configPath);
    if (!f.is_open()) return;

    try {
        json j = json::parse(f);
        for (const auto& item : j) {
            ClusterNode node;
            node.id = item.value("id", "");
            node.ip_address = item.value("ip_address", "");
            std::string saved_status = item.value("status", "pending");
            if (saved_status == "connected" || saved_status == "connecting...") {
                node.status = "offline";
            } else {
                node.status = saved_status;
            }
            node.hostname = item.value("hostname", "");
            node.platform = item.value("platform", "");
            node.os_version = item.value("os_version", "");
            node.local_ip = item.value("local_ip", "");
            node.app_version = item.value("app_version", "");
            node.is_parent = item.value("is_parent", false);
            node.last_seen = item.value("last_seen", 0LL);
            node.master_token = item.value("master_token", "");
            node.encryption_key = item.value("encryption_key", "");
            nodes.push_back(node);
        }
    } catch (...) {}
}

void ClusterManager::SaveNodes() {
    json j = json::array();
    for (const auto& n : nodes) {
        j.push_back({
            {"id", n.id},
            {"ip_address", n.ip_address},
            {"status", n.status},
            {"hostname", n.hostname},
            {"platform", n.platform},
            {"os_version", n.os_version},
            {"local_ip", n.local_ip},
            {"app_version", n.app_version},
            {"is_parent", n.is_parent},
            {"last_seen", n.last_seen},
            {"master_token", n.master_token},
            {"encryption_key", n.encryption_key}
        });
    }
    std::filesystem::path configPath = SettingsManager::Get().GetConfigDir() / "cluster_nodes.json";
    std::ofstream f(configPath);
    f << j.dump(4);
    
    // Notify AI clients about topology change
    notify_tools_changed();
}

std::string GenerateRandomString(size_t length) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.resize(length);
    for (size_t i = 0; i < length; i++) result[i] = charset[rand() % (sizeof(charset) - 1)];
    return result;
}

bool ClusterManager::RegisterNodeRequest(const std::string& id, const std::string& ip_address, const std::string& hostname, const std::string& platform, std::string& out_id) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Check if exists by id, ip_address, or hostname
    for (auto& n : nodes) {
        if (n.id == id || n.ip_address == ip_address || (n.id != "parent" && n.hostname == hostname && !hostname.empty() && hostname != "UnknownNode")) {
            n.ip_address = ip_address;
            n.hostname = hostname;
            n.platform = platform;
            n.last_seen = GetCurrentTimestamp();
            out_id = n.id;
            return false; // Already exists, just updated
        }
    }
    
    ClusterNode node;
    node.id = id;
    node.ip_address = ip_address;
    node.status = "pending";
    node.hostname = hostname;
    node.platform = platform;
    node.last_seen = GetCurrentTimestamp();
    node.master_token = GenerateRandomString(25);
    node.encryption_key = GenerateRandomString(32);
    
    nodes.push_back(node);
    SaveNodes();
    out_id = id;
    return true; // New node registered
}

bool ClusterManager::ApproveNode(const std::string& id) {
    std::string ip, mt, ek;
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& n : nodes) {
            if (n.id == id) {
                n.status = "connecting...";
                SaveNodes();
                ip = n.ip_address;
                mt = n.master_token;
                ek = n.encryption_key;
                break;
            }
        }
    } // <-- lock released BEFORE spawning thread to avoid deadlock
    
    if (ip.empty()) {
        mcp_log("[Error] ApproveNode: node '" + id + "' not found or has empty ip_address.");
        return false;
    }
    
    // Strip protocol prefix if stored with it (legacy compatibility)
    if (ip.find("http://") == 0) ip = ip.substr(7);
    if (ip.find("https://") == 0) ip = ip.substr(8);
    while (!ip.empty() && ip.back() == '/') ip.pop_back();
    
    if (id == "parent") {
        mcp_log("[Info] ApproveNode: Connecting to parent at http://" + ip);
    } else {
        mcp_log("[Info] ApproveNode: Connecting to child '" + id + "' at http://" + ip);
    }
    
    std::thread([id, ip, mt, ek]() {
        std::string host = ip;
        int port = 3000; // default MCP port
        size_t colon_pos = host.find_last_of(':');
        if (colon_pos != std::string::npos) {
            try {
                port = std::stoi(host.substr(colon_pos + 1));
                host = host.substr(0, colon_pos);
            } catch (...) {}
        }
        
        if (id == "parent") {
            // We are the child. We want to reconnect to the parent.
            // Send a join request so the parent knows we are alive and configures us.
            const char* hn = std::getenv("HOSTNAME");
            if (!hn) hn = std::getenv("COMPUTERNAME");
            std::string child_hostname = hn ? hn : "";
            if (child_hostname.empty()) {
                char buf[256] = {};
                if (gethostname(buf, sizeof(buf)) == 0) child_hostname = buf;
            }
            if (child_hostname.empty()) child_hostname = "UnknownNode";
            
            std::string child_platform =
#ifdef _WIN32
            "Windows";
#elif defined(__APPLE__)
            "macOS";
#else
            "Linux";
#endif
            nlohmann::json req_data = {
                {"id", "child_" + std::to_string(rand() % 10000)},
                {"port", "3000"},
                {"hostname", child_hostname},
                {"platform", child_platform}
            };
            httplib::Client cli(host, port);
            cli.set_connection_timeout(5, 0);
            if (auto res = cli.Post("/cluster/join", req_data.dump(), "application/json")) {
                if (res->status == 200) {
                    mcp_log("[Info] Successfully sent join request to parent at " + host);
                    // Leave status as "connecting...", the parent will send /cluster/configure to finalize.
                } else {
                    mcp_log("[Error] Parent rejected join request: HTTP " + std::to_string(res->status));
                    std::lock_guard<std::mutex> lk(ClusterManager::GetInstance().mtx);
                    for (auto& node : ClusterManager::GetInstance().nodes) {
                        if (node.id == id) { node.status = "error_403"; break; }
                    }
                }
            } else {
                std::lock_guard<std::mutex> lk(ClusterManager::GetInstance().mtx);
                for (auto& node : ClusterManager::GetInstance().nodes) {
                    if (node.id == id) { node.status = "offline"; break; }
                }
            }
            if (g_refresh_cluster_callback) g_refresh_cluster_callback();
            return;
        }
        
        bool newly_generated = false;
        std::string final_mt = mt;
        std::string final_ek = ek;
        if (final_mt.empty()) {
            final_mt = "mcp_mt_" + generate_random_string(32);
            newly_generated = true;
        }
        if (final_ek.empty()) {
            final_ek = generate_random_string(64);
            newly_generated = true;
        }
        
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(60, 0); // User might take a while to click Yes
        
        // Gather parent metadata to send to child
        const char* hn_env = std::getenv("HOSTNAME");
        if (!hn_env) hn_env = std::getenv("COMPUTERNAME");
        std::string my_hostname = hn_env ? hn_env : "";
        if (my_hostname.empty()) {
            char buf[256] = {};
            if (gethostname(buf, sizeof(buf)) == 0) my_hostname = buf;
        }
        if (my_hostname.empty()) my_hostname = "ParentNode";
        
#ifdef _WIN32
        std::string my_platform = "Windows";
#elif defined(__APPLE__)
        std::string my_platform = "macOS";
#else
        std::string my_platform = "Linux";
#endif
        
        auto tokens = NetworkUtils::LoadTokens();
        json tokens_array = json::array();
        for (const auto& t : tokens) {
            tokens_array.push_back({
                {"id", t.id},
                {"name", t.name},
                {"raw_token", t.raw_token},
                {"creation_date", t.creation_date},
                {"active", t.active}
            });
        }
        std::string encrypted_tokens = encrypt_aes256(final_ek, tokens_array.dump());

        extern int g_server_port;
        json req = {
            {"master_token", final_mt},
            {"encryption_key", final_ek},
            {"parent_hostname", my_hostname},
            {"parent_platform", my_platform},
            {"parent_ip", ip},
            {"parent_port", g_server_port},
            {"parent_version", "0.1.8"},
            {"synced_tokens_enc", encrypted_tokens}
        };
        if (auto res = cli.Post("/cluster/configure", req.dump(), "application/json")) {
            if (res->status == 200) {
                try {
                    json resp = json::parse(res->body);
                    std::string hn = resp.value("hostname", "");
                    std::string pt = resp.value("platform", "Unknown");
                    
                    std::lock_guard<std::mutex> lk(ClusterManager::GetInstance().mtx);
                    for (auto& node : ClusterManager::GetInstance().nodes) {
                        if (node.id == id) {
                            if (newly_generated) {
                                node.master_token = final_mt;
                                node.encryption_key = final_ek;
                            }
                            if (node.hostname.empty() && !hn.empty()) node.hostname = hn;
                            node.platform = pt;
                            node.os_version = resp.value("os_version", "");
                            node.local_ip = resp.value("local_ip", "");
                            node.app_version = resp.value("app_version", "unknown");
                            node.status = "connected";
                            node.last_seen = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            ClusterManager::GetInstance().SaveNodes();
                            mcp_log("[Info] Connected to child: " + hn + " (" + pt + ") at " + ip);
                            break;
                        }
                    }
                } catch (...) {}
            } else {
                mcp_log("[Error] Child node rejected configuration: HTTP " + std::to_string(res->status));
                std::lock_guard<std::mutex> lk(ClusterManager::GetInstance().mtx);
                for (auto& node : ClusterManager::GetInstance().nodes) {
                    if (node.id == id) {
                        node.status = "error_403";
                        ClusterManager::GetInstance().SaveNodes();
                        break;
                    }
                }
            }
        } else {
            mcp_log("[Error] Could not connect to child node at " + ip);
            std::lock_guard<std::mutex> lk(ClusterManager::GetInstance().mtx);
            for (auto& node : ClusterManager::GetInstance().nodes) {
                if (node.id == id) {
                    node.status = "offline";
                    ClusterManager::GetInstance().SaveNodes();
                    break;
                }
            }
        }
        
        if (g_refresh_cluster_callback) {
            g_refresh_cluster_callback();
        }
    }).detach();
    
    return true;
}

void ClusterManager::RemoveNode(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = std::remove_if(nodes.begin(), nodes.end(), [&](const ClusterNode& n) {
        return n.id == id;
    });
    if (it != nodes.end()) {
        nodes.erase(it, nodes.end());
        SaveNodes();
    }
}

bool ClusterManager::UpdateNodeId(const std::string& oldId, const std::string& newId) {
    std::lock_guard<std::mutex> lock(mtx);
    // Check if newId already exists
    for (const auto& n : nodes) {
        if (n.id == newId) return false;
    }
    
    for (auto& n : nodes) {
        if (n.id == oldId) {
            n.id = newId;
            n.hostname = newId; // Update hostname for GUI display
            SaveNodes();
            
            // Also update token permissions to prevent access loss
            auto tokens = NetworkUtils::LoadTokens();
            bool tokens_changed = false;
            for (auto& t : tokens) {
                if (t.permissions.allowed_servers.count(oldId)) {
                    t.permissions.allowed_servers[newId] = t.permissions.allowed_servers[oldId];
                    t.permissions.allowed_servers.erase(oldId);
                    tokens_changed = true;
                }
                if (t.permissions.server_workspaces.count(oldId)) {
                    t.permissions.server_workspaces[newId] = t.permissions.server_workspaces[oldId];
                    t.permissions.server_workspaces.erase(oldId);
                    tokens_changed = true;
                }
            }
            if (tokens_changed) {
                NetworkUtils::SaveTokens(tokens);
            }
            
            return true;
        }
    }
    return false;
}

bool ClusterManager::UpdateNodeMetadata(const std::string& id, const std::string& os_version, const std::string& local_ip, const std::string& app_version, bool is_parent) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& n : nodes) {
        if (n.id == id) {
            n.os_version = os_version;
            n.local_ip = local_ip;
            n.app_version = app_version;
            n.is_parent = is_parent;
            SaveNodes();
            return true;
        }
    }
    return false;
}

bool ClusterManager::SetNodeStatus(const std::string& id, const std::string& status) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& n : nodes) {
        if (n.id == id) {
            n.status = status;
            SaveNodes();
            return true;
        }
    }
    return false;
}

void ClusterManager::ReconnectAllNodes() {
    std::vector<std::string> ids_to_reconnect;
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& n : nodes) {
            if (n.id != "parent" && (n.status == "offline" || n.status == "error_403")) {
                n.status = "connecting...";
                ids_to_reconnect.push_back(n.id);
            }
        }
        if (!ids_to_reconnect.empty()) {
            SaveNodes();
        }
    }
    
    if (g_refresh_cluster_callback) g_refresh_cluster_callback();
    
    for (const auto& id : ids_to_reconnect) {
        ApproveNode(id);
    }
}

std::vector<ClusterNode> ClusterManager::GetNodes() {
    std::lock_guard<std::mutex> lock(mtx);
    return nodes;
}

bool ClusterManager::GetNode(const std::string& id, ClusterNode& out_node) {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& n : nodes) {
        if (n.id == id) {
            out_node = n;
            return true;
        }
    }
    return false;
}

void ClusterManager::PingNode(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& n : nodes) {
        if (n.id == id) {
            n.last_seen = GetCurrentTimestamp();
            return;
        }
    }
}
