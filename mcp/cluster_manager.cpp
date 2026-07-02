#include "cluster_manager.h"
#include <fstream>
#include <fstream>
#include <chrono>
#include <thread>
#include <httplib.h>

using json = nlohmann::json;

long GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void ClusterManager::LoadNodes() {
    std::lock_guard<std::mutex> lock(mtx);
    nodes.clear();
    std::ifstream f("cluster_nodes.json");
    if (!f.is_open()) return;

    try {
        json j = json::parse(f);
        for (const auto& item : j) {
            ClusterNode node;
            node.id = item.value("id", "");
            node.ip_address = item.value("ip_address", "");
            node.status = item.value("status", "pending");
            node.hostname = item.value("hostname", "");
            node.platform = item.value("platform", "");
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
            {"last_seen", n.last_seen},
            {"master_token", n.master_token},
            {"encryption_key", n.encryption_key}
        });
    }
    std::ofstream f("cluster_nodes.json");
    f << j.dump(4);
}

std::string GenerateRandomString(size_t length) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.resize(length);
    for (size_t i = 0; i < length; i++) result[i] = charset[rand() % (sizeof(charset) - 1)];
    return result;
}

bool ClusterManager::RegisterNodeRequest(const std::string& id, const std::string& ip_address, const std::string& hostname, const std::string& platform) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Check if exists
    for (auto& n : nodes) {
        if (n.id == id) {
            n.ip_address = ip_address;
            n.hostname = hostname;
            n.platform = platform;
            n.last_seen = GetCurrentTimestamp();
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
    return true; // New node registered
}

bool ClusterManager::ApproveNode(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& n : nodes) {
        if (n.id == id) {
            n.status = "connected";
            SaveNodes();
            
            // Send configure packet to child
            std::string ip = n.ip_address;
            if (ip.find("http://") == 0) ip = ip.substr(7);
            if (ip.find("https://") == 0) ip = ip.substr(8);
            
            std::thread([ip, mt = n.master_token, ek = n.encryption_key]() {
                httplib::Client cli("http://" + ip);
                cli.set_connection_timeout(5, 0);
                json req = {
                    {"master_token", mt},
                    {"encryption_key", ek}
                };
                cli.Post("/cluster/configure", req.dump(), "application/json");
            }).detach();
            
            return true;
        }
    }
    return false;
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
