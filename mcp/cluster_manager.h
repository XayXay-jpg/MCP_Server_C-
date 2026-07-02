#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

struct ClusterNode {
    std::string id;           // Unique ID
    std::string ip_address;   // IP:port
    std::string status;       // "pending", "connected", "offline", "error_403"
    std::string hostname;     // Machine hostname
    std::string platform;     // OS platform
    std::string os_version;   // Detailed OS version
    std::string local_ip;     // LAN IP address
    std::string app_version;  // MCP Server version
    bool is_parent = false;   // True if this node is OUR parent (not our child)
    std::string master_token;
    std::string encryption_key;
    long last_seen = 0;
};

class ClusterManager {
public:
    static ClusterManager& GetInstance() {
        static ClusterManager instance;
        return instance;
    }

    // Initialize or load nodes
    void LoadNodes();
    void SaveNodes();

    // Register a new incoming connection request
    bool RegisterNodeRequest(const std::string& id, const std::string& ip_address, const std::string& hostname, const std::string& platform);
    
    // Admin action: approve a node
    bool ApproveNode(const std::string& id);
    
    // Admin action: remove or deny a node
    void RemoveNode(const std::string& id);

    // Get list of nodes
    std::vector<ClusterNode> GetNodes();
    
    // Get a specific node
    bool GetNode(const std::string& id, ClusterNode& out_node);

    // Update last seen timestamp
    void PingNode(const std::string& id);

    bool UpdateNodeId(const std::string& oldId, const std::string& newId);
    bool SetNodeStatus(const std::string& id, const std::string& status);
    
    // Attempt to reconnect to all saved nodes
    void ReconnectAllNodes();

private:
    ClusterManager() {}
    std::vector<ClusterNode> nodes;
    std::mutex mtx;
};
