#include "SnapshotManager.h"
#include "cluster_manager.h"
#include "knowledge_layer.h"
#include "settings_manager.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <openssl/evp.h>

namespace fs = std::filesystem;

// Helper to compute SHA-256 using OpenSSL EVP
static std::string Sha256(const std::string& input) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, input.c_str(), input.length());
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    for (unsigned int i = 0; i < md_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)md_value[i];
    }
    return ss.str();
}

// Convert long time to readable string
static std::string FormatTime(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    char buf[100];
    if (std::strftime(buf, sizeof(buf), "%c", std::localtime(&t))) {
        return std::string(buf);
    }
    return "";
}

static std::string RunCommand(const std::string& cmd) {
    std::string result;
    char buffer[256];
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

static SystemState CaptureSystemState(const std::string& workspace_path) {
    SystemState state;
    
    // 1. Services
    std::vector<std::string> services = {"postgresql", "mysql", "redis", "nginx", "apache2", "docker", "mongod"};
    for (const auto& svc : services) {
        std::string status = RunCommand("systemctl is-active " + svc);
        if (status.empty()) status = "inactive";
        state.services_status[svc] = status;
    }

    // 2. Git config
    std::string git_out = RunCommand("git -C " + workspace_path + " config --list");
    std::stringstream ss(git_out);
    std::string line;
    while (std::getline(ss, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            state.git_config[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }

    // 3. Env variables
    std::vector<std::string> envs = {"PATH", "USER", "MCP_PORT", "MCP_OVERSEER_ID", "LANG"};
    for (const auto& ev : envs) {
        char* val = std::getenv(ev.c_str());
        if (val) {
            state.env_vars[ev] = std::string(val);
        }
    }

    // 4. Custom files
    std::vector<std::string> files = {"tokens.json", ".env"};
    for (const auto& filename : files) {
        fs::path fp = fs::path(workspace_path) / filename;
        if (fs::exists(fp)) {
            std::ifstream f(fp, std::ios::binary);
            if (f.is_open()) {
                std::stringstream buf;
                buf << f.rdbuf();
                state.custom_files[filename] = buf.str();
            }
        }
    }

    return state;
}

static void RestoreSystemState(const std::string& workspace_path, const SystemState& state) {
    // 1. Restore services
    for (auto it = state.services_status.begin(); it != state.services_status.end(); ++it) {
        std::string svc = it.key();
        std::string target_status = it.value().get<std::string>();
        
        std::string current_status = RunCommand("systemctl is-active " + svc);
        if (current_status != target_status) {
            if (target_status == "active") {
                RunCommand("sudo systemctl start " + svc + " 2>&1");
            } else {
                RunCommand("sudo systemctl stop " + svc + " 2>&1");
            }
        }
    }

    // 2. Restore git configs locally
    for (auto it = state.git_config.begin(); it != state.git_config.end(); ++it) {
        std::string key = it.key();
        std::string val = it.value().get<std::string>();
        RunCommand("git -C " + workspace_path + " config --local " + key + " \"" + val + "\"");
    }

    // 3. Restore env variables
    for (auto it = state.env_vars.begin(); it != state.env_vars.end(); ++it) {
        setenv(it.key().c_str(), it.value().get<std::string>().c_str(), 1);
    }

    // 4. Restore custom files
    for (auto it = state.custom_files.begin(); it != state.custom_files.end(); ++it) {
        fs::path fp = fs::path(workspace_path) / it.key();
        std::ofstream f(fp, std::ios::binary);
        if (f.is_open()) {
            f << it.value().get<std::string>();
        }
    }
}

void SnapshotManager::Initialize(const std::string& workspace_dir) {
    std::lock_guard<std::mutex> lock(mtx);
    workspace_path = workspace_dir;
    
    // Create history folder
    fs::path hist_dir = fs::path(workspace_path) / ".mcp_history";
    if (!fs::exists(hist_dir)) {
        fs::create_directories(hist_dir);
    }

    ReplayLogs();

    // If DAG is completely empty, capture the current state as the initial root commit
    if (snapshot_dag.empty()) {
        auto root = std::make_shared<Snapshot>();
        root->timestamp = std::chrono::system_clock::now();
        root->author = SnapshotAuthor::Human;
        root->author_name = "System";
        root->description = "Initial Repository Setup";
        root->is_checkpoint = true;
        
        // Capture active state
        auto cache = std::make_shared<Snapshot::FullStateCache>();
        cache->nodes = ClusterManager::GetInstance().GetNodes();
        cache->knowledge_data = KnowledgeLayer::GetInstance().GetFullKnowledge()["user_provided"];
        root->full_state = cache;
        root->system_state = CaptureSystemState(workspace_path);

        root->id = CalculateHash(*root);
        
        snapshot_dag[root->id] = root;
        current_active_id = root->id;

        AppendToLog(*root);
        SaveCheckpoint(root->id, *cache);
    }
}

std::shared_ptr<Snapshot> SnapshotManager::CreateSnapshot(
    SnapshotAuthor author, 
    const std::string& author_name, 
    const std::string& description,
    bool only_staged
) {
    std::lock_guard<std::mutex> lock(mtx);

    auto parent_state = ReconstructStateImpl(current_active_id);
    if (!parent_state) {
        // Fallback if no parent state
        parent_state = std::make_shared<Snapshot::FullStateCache>();
    }

    auto snap = std::make_shared<Snapshot>();
    if (!current_active_id.empty()) {
        snap->parent_ids.push_back(current_active_id);
    }
    snap->timestamp = std::chrono::system_clock::now();
    snap->author = author;
    snap->author_name = author_name;
    snap->description = description;

    // Get current state
    std::vector<ClusterNode> new_nodes;
    nlohmann::json new_knowledge;

    if (!only_staged || staged_items.empty()) {
        new_nodes = ClusterManager::GetInstance().GetNodes();
        new_knowledge = KnowledgeLayer::GetInstance().GetFullKnowledge()["user_provided"];
    } else {
        if (std::find(staged_items.begin(), staged_items.end(), "cluster_nodes.json") != staged_items.end()) {
            new_nodes = ClusterManager::GetInstance().GetNodes();
        } else {
            new_nodes = parent_state->nodes;
        }

        if (std::find(staged_items.begin(), staged_items.end(), "mcp_knowledge.json") != staged_items.end()) {
            new_knowledge = KnowledgeLayer::GetInstance().GetFullKnowledge()["user_provided"];
        } else {
            new_knowledge = parent_state->knowledge_data;
        }
    }

    // Compute diffs
    // 1. Knowledge JSON patch
    snap->knowledge_patch = nlohmann::json::diff(parent_state->knowledge_data, new_knowledge);

    // 2. Cluster Topology diff
    std::vector<ClusterNode> added;
    std::vector<ClusterNode> modified;
    std::vector<std::string> removed;

    for (const auto& n : new_nodes) {
        auto it = std::find_if(parent_state->nodes.begin(), parent_state->nodes.end(), 
            [&n](const ClusterNode& x) { return x.id == n.id; });
        if (it == parent_state->nodes.end()) {
            added.push_back(n);
        } else {
            // Compare metadata
            if (it->ip_address != n.ip_address || it->status != n.status || 
                it->hostname != n.hostname || it->platform != n.platform || 
                it->os_version != n.os_version || it->local_ip != n.local_ip || 
                it->app_version != n.app_version || it->is_parent != n.is_parent ||
                it->master_token != n.master_token || it->encryption_key != n.encryption_key) {
                modified.push_back(n);
            }
        }
    }

    for (const auto& old_n : parent_state->nodes) {
        auto it = std::find_if(new_nodes.begin(), new_nodes.end(), 
            [&old_n](const ClusterNode& x) { return x.id == old_n.id; });
        if (it == new_nodes.end()) {
            removed.push_back(old_n.id);
        }
    }

    snap->cluster_diff.added_nodes = added;
    snap->cluster_diff.modified_nodes = modified;
    snap->cluster_diff.removed_node_ids = removed;

    // Decide if checkpoint (every 10 commits we create a full checkpoint to optimize reconstruction)
    size_t depth = 0;
    std::string curr = current_active_id;
    while (!curr.empty() && snapshot_dag.count(curr)) {
        auto parent = snapshot_dag[curr];
        if (parent->is_checkpoint) break;
        depth++;
        if (parent->parent_ids.empty()) break;
        curr = parent->parent_ids[0];
    }
    if (depth >= 10) {
        snap->is_checkpoint = true;
    }

    snap->system_state = CaptureSystemState(workspace_path);
    snap->id = CalculateHash(*snap);

    // Save full cache
    auto cache = std::make_shared<Snapshot::FullStateCache>();
    cache->nodes = new_nodes;
    cache->knowledge_data = new_knowledge;
    snap->full_state = cache;

    snapshot_dag[snap->id] = snap;
    current_active_id = snap->id;
    staged_items.clear(); // Reset staging on successful commit

    AppendToLog(*snap);
    if (snap->is_checkpoint) {
        SaveCheckpoint(snap->id, *cache);
    }

    CleanAndPruneHistoryImpl();

    return snap;
}

bool SnapshotManager::RollbackTo(const std::string& snapshot_id) {
    if (snapshot_id.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mtx);
    
    std::string target_id = "";
    if (snapshot_dag.count(snapshot_id)) {
        target_id = snapshot_id;
    } else {
        // Resolve partial prefix match
        for (const auto& pair : snapshot_dag) {
            if (pair.first.rfind(snapshot_id, 0) == 0) {
                target_id = pair.first;
                break;
            }
        }
    }

    if (target_id.empty()) {
        for (const auto& pair : snapshot_dag) {
            if (pair.second->description == snapshot_id) {
                target_id = pair.first;
                break;
            }
        }
    }

    if (target_id.empty() || !snapshot_dag.count(target_id)) {
        return false;
    }

    auto target_state = ReconstructStateImpl(target_id);
    if (!target_state) {
        return false;
    }

    // Stop health check before modifying topology
    ClusterManager::GetInstance().StopHealthCheckTask();

    // 1. Restore nodes
    std::filesystem::path configPath = SettingsManager::Get().GetConfigDir() / "cluster_nodes.json";
    nlohmann::json nodes_array = nlohmann::json::array();
    for (const auto& n : target_state->nodes) {
        nodes_array.push_back({
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
    std::ofstream nf(configPath);
    if (nf.is_open()) {
        nf << nodes_array.dump(4);
        nf.close();
    }
    ClusterManager::GetInstance().LoadNodes();

    // 2. Restore knowledge data
    std::filesystem::path knowPath = fs::path(workspace_path) / "mcp_knowledge.json";
    std::ofstream kf(knowPath);
    if (kf.is_open()) {
        kf << target_state->knowledge_data.dump(4);
        kf.close();
    }
    KnowledgeLayer::GetInstance().Load(workspace_path);

    // 3. Restore global system state
    auto snap = snapshot_dag[target_id];
    RestoreSystemState(workspace_path, snap->system_state);

    current_active_id = target_id;
    staged_items.clear();

    // Resume health check task
    ClusterManager::GetInstance().StartHealthCheckTask();

    CleanAndPruneHistoryImpl();

    return true;
}

std::vector<std::shared_ptr<Snapshot>> SnapshotManager::GetHistoryDAG() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::shared_ptr<Snapshot>> list;
    
    // Sort DAG topologically by following parents starting from HEAD
    std::string curr = current_active_id;
    while (!curr.empty() && snapshot_dag.count(curr)) {
        auto snap = snapshot_dag[curr];
        list.push_back(snap);
        if (snap->parent_ids.empty()) break;
        curr = snap->parent_ids[0]; // Simple linear sequence path traversal
    }
    return list;
}

std::shared_ptr<Snapshot> SnapshotManager::GetSnapshot(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx);
    if (snapshot_dag.count(id)) {
        return snapshot_dag[id];
    }
    // Check partial prefix matching
    for (const auto& pair : snapshot_dag) {
        if (pair.first.rfind(id, 0) == 0) {
            return pair.second;
        }
    }
    return nullptr;
}

void SnapshotManager::CleanAndPruneHistory(size_t max_nodes) {
    std::lock_guard<std::mutex> lock(mtx);
    CleanAndPruneHistoryImpl(max_nodes);
}

void SnapshotManager::CleanAndPruneHistoryImpl(size_t max_nodes) {
    // Basic garbage collection: limit memory cache size by removing full state caches for old delta nodes
    for (auto& pair : snapshot_dag) {
        if (pair.first != current_active_id && !pair.second->is_checkpoint) {
            pair.second->full_state = nullptr; // Free memory cache
        }
    }
}

// Git-like commands

bool SnapshotManager::StageChange(const std::string& target_id) {
    std::lock_guard<std::mutex> lock(mtx);
    if (std::find(staged_items.begin(), staged_items.end(), target_id) == staged_items.end()) {
        staged_items.push_back(target_id);
        return true;
    }
    return false;
}

bool SnapshotManager::UnstageChange(const std::string& target_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = std::find(staged_items.begin(), staged_items.end(), target_id);
    if (it != staged_items.end()) {
        staged_items.erase(it);
        return true;
    }
    return false;
}

bool SnapshotManager::ResetHard() {
    return RollbackTo(current_active_id);
}

std::string SnapshotManager::GetStatusString() {
    std::lock_guard<std::mutex> lock(mtx);
    auto active_state = ReconstructStateImpl(current_active_id);
    if (!active_state) {
        active_state = std::make_shared<Snapshot::FullStateCache>();
    }

    std::vector<ClusterNode> new_nodes = ClusterManager::GetInstance().GetNodes();
    nlohmann::json new_knowledge = KnowledgeLayer::GetInstance().GetFullKnowledge()["user_provided"];

    bool knowledge_changed = (active_state->knowledge_data != new_knowledge);
    
    // Check cluster topology diff
    bool topology_changed = false;
    if (active_state->nodes.size() != new_nodes.size()) {
        topology_changed = true;
    } else {
        for (size_t i = 0; i < new_nodes.size(); i++) {
            const auto& a = active_state->nodes[i];
            const auto& b = new_nodes[i];
            if (a.id != b.id || a.ip_address != b.ip_address || a.status != b.status || 
                a.hostname != b.hostname || a.platform != b.platform) {
                topology_changed = true;
                break;
            }
        }
    }

    bool knowledge_staged = std::find(staged_items.begin(), staged_items.end(), "mcp_knowledge.json") != staged_items.end();
    bool topology_staged = std::find(staged_items.begin(), staged_items.end(), "cluster_nodes.json") != staged_items.end();

    std::stringstream ss;
    ss << "On active branch HEAD (" << (current_active_id.empty() ? "None" : current_active_id.substr(0, 8)) << ")\n\n";

    bool has_staged = (knowledge_changed && knowledge_staged) || (topology_changed && topology_staged);
    bool has_unstaged = (knowledge_changed && !knowledge_staged) || (topology_changed && !topology_staged);

    if (has_staged) {
        ss << "Changes to be committed:\n"
           << "  (use \"mcp restore --staged <file>...\" to unstage)\n";
        if (knowledge_changed && knowledge_staged) {
            ss << "\tmodified:   mcp_knowledge.json\n";
        }
        if (topology_changed && topology_staged) {
            ss << "\tmodified:   cluster_nodes.json\n";
        }
        ss << "\n";
    }

    if (has_unstaged) {
        ss << "Changes not staged for commit:\n"
           << "  (use \"mcp add <file>...\" to update what will be committed)\n"
           << "  (use \"mcp restore <file>...\" to discard changes in working directory)\n";
        if (knowledge_changed && !knowledge_staged) {
            ss << "\tmodified:   mcp_knowledge.json\n";
        }
        if (topology_changed && !topology_staged) {
            ss << "\tmodified:   cluster_nodes.json\n";
        }
        ss << "\n";
    }

    if (!has_staged && !has_unstaged) {
        ss << "nothing to commit, working tree clean\n";
    }

    return ss.str();
}

std::string SnapshotManager::GetDiffString() {
    std::lock_guard<std::mutex> lock(mtx);
    auto active_state = ReconstructStateImpl(current_active_id);
    if (!active_state) {
        active_state = std::make_shared<Snapshot::FullStateCache>();
    }

    nlohmann::json new_knowledge = KnowledgeLayer::GetInstance().GetFullKnowledge()["user_provided"];
    auto patch = nlohmann::json::diff(active_state->knowledge_data, new_knowledge);

    std::stringstream ss;
    ss << "--- HEAD:mcp_knowledge.json\n+++ Workspace:mcp_knowledge.json\n";
    if (patch.empty()) {
        ss << "No changes in knowledge base.\n";
    } else {
        ss << patch.dump(4) << "\n";
    }
    return ss.str();
}

// Helpers

std::shared_ptr<Snapshot::FullStateCache> SnapshotManager::ReconstructState(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx);
    return ReconstructStateImpl(id);
}

std::shared_ptr<Snapshot::FullStateCache> SnapshotManager::ReconstructStateImpl(const std::string& id) {
    if (id.empty() || !snapshot_dag.count(id)) {
        return nullptr;
    }

    auto snap = snapshot_dag[id];
    if (snap->full_state) {
        return snap->full_state;
    }

    // Traverse back to find nearest checkpoint
    std::vector<std::shared_ptr<Snapshot>> path;
    std::shared_ptr<Snapshot> curr = snap;
    while (curr && !curr->full_state && !curr->is_checkpoint) {
        path.push_back(curr);
        if (curr->parent_ids.empty()) break;
        curr = snapshot_dag[curr->parent_ids[0]];
    }

    auto cache = std::make_shared<Snapshot::FullStateCache>();
    if (curr) {
        if (curr->full_state) {
            cache->nodes = curr->full_state->nodes;
            cache->knowledge_data = curr->full_state->knowledge_data;
        } else if (curr->is_checkpoint) {
            // Load checkpoint from disk
            fs::path cp_path = fs::path(workspace_path) / ".mcp_history" / ("checkpoint_" + curr->id + ".json");
            std::ifstream f(cp_path);
            if (f.is_open()) {
                nlohmann::json cp_json;
                f >> cp_json;
                f.close();
                // Load nodes
                if (cp_json.contains("nodes")) {
                    for (const auto& item : cp_json["nodes"]) {
                        ClusterNode node;
                        node.id = item.value("id", "");
                        node.ip_address = item.value("ip_address", "");
                        node.status = item.value("status", "offline");
                        node.hostname = item.value("hostname", "");
                        node.platform = item.value("platform", "");
                        node.os_version = item.value("os_version", "");
                        node.local_ip = item.value("local_ip", "");
                        node.app_version = item.value("app_version", "");
                        node.is_parent = item.value("is_parent", false);
                        node.master_token = item.value("master_token", "");
                        node.encryption_key = item.value("encryption_key", "");
                        node.last_seen = item.value("last_seen", 0LL);
                        cache->nodes.push_back(node);
                    }
                }
                cache->knowledge_data = cp_json.value("knowledge_data", nlohmann::json::object());
            }
        }
    }

    // Replay forward deltas
    std::reverse(path.begin(), path.end());
    for (const auto& step : path) {
        // Apply topology delta
        for (const auto& n : step->cluster_diff.added_nodes) {
            cache->nodes.push_back(n);
        }
        for (const auto& n : step->cluster_diff.modified_nodes) {
            auto it = std::find_if(cache->nodes.begin(), cache->nodes.end(), 
                [&n](const ClusterNode& x) { return x.id == n.id; });
            if (it != cache->nodes.end()) {
                *it = n;
            }
        }
        for (const auto& r_id : step->cluster_diff.removed_node_ids) {
            auto it = std::find_if(cache->nodes.begin(), cache->nodes.end(), 
                [&r_id](const ClusterNode& x) { return x.id == r_id; });
            if (it != cache->nodes.end()) {
                cache->nodes.erase(it);
            }
        }

        // Apply JSON patch
        if (!step->knowledge_patch.empty()) {
            try {
                cache->knowledge_data = cache->knowledge_data.patch(step->knowledge_patch);
            } catch (...) {}
        }
    }

    snap->full_state = cache;
    return cache;
}

std::string SnapshotManager::CalculateHash(const Snapshot& snap) {
    std::stringstream ss;
    for (const auto& p : snap.parent_ids) {
        ss << p << ";";
    }
    ss << FormatTime(snap.timestamp) << ";";
    ss << snap.author_name << ";";
    ss << snap.description << ";";
    ss << snap.cluster_diff.added_nodes.size() << ";";
    ss << snap.cluster_diff.modified_nodes.size() << ";";
    ss << snap.cluster_diff.removed_node_ids.size() << ";";
    ss << snap.knowledge_patch.dump() << ";";
    ss << snap.system_state.services_status.dump() << ";";
    ss << snap.system_state.git_config.dump() << ";";
    ss << snap.system_state.env_vars.dump() << ";";
    ss << snap.system_state.custom_files.dump() << ";";
    return Sha256(ss.str());
}

void SnapshotManager::AppendToLog(const Snapshot& snap) {
    fs::path log_path = fs::path(workspace_path) / ".mcp_history" / "history.log";
    std::ofstream f(log_path, std::ios::app);
    if (!f.is_open()) return;

    nlohmann::json added_nodes = nlohmann::json::array();
    for (const auto& n : snap.cluster_diff.added_nodes) {
        added_nodes.push_back({{"id", n.id}, {"ip_address", n.ip_address}, {"status", n.status}, {"hostname", n.hostname}, {"platform", n.platform}});
    }
    nlohmann::json modified_nodes = nlohmann::json::array();
    for (const auto& n : snap.cluster_diff.modified_nodes) {
        modified_nodes.push_back({{"id", n.id}, {"ip_address", n.ip_address}, {"status", n.status}, {"hostname", n.hostname}, {"platform", n.platform}});
    }

    nlohmann::json j = {
        {"id", snap.id},
        {"parent_ids", snap.parent_ids},
        {"timestamp", std::chrono::system_clock::to_time_t(snap.timestamp)},
        {"author", static_cast<int>(snap.author)},
        {"author_name", snap.author_name},
        {"description", snap.description},
        {"cluster_diff", {
            {"added_nodes", added_nodes},
            {"modified_nodes", modified_nodes},
            {"removed_node_ids", snap.cluster_diff.removed_node_ids}
        }},
        {"knowledge_patch", snap.knowledge_patch},
        {"system_state", {
            {"services_status", snap.system_state.services_status},
            {"git_config", snap.system_state.git_config},
            {"env_vars", snap.system_state.env_vars},
            {"custom_files", snap.system_state.custom_files}
        }},
        {"is_checkpoint", snap.is_checkpoint}
    };

    f << j.dump() << "\n";
}

void SnapshotManager::SaveCheckpoint(const std::string& id, const Snapshot::FullStateCache& state) {
    fs::path cp_path = fs::path(workspace_path) / ".mcp_history" / ("checkpoint_" + id + ".json");
    std::ofstream f(cp_path);
    if (!f.is_open()) return;

    nlohmann::json nodes_array = nlohmann::json::array();
    for (const auto& n : state.nodes) {
        nodes_array.push_back({
            {"id", n.id},
            {"ip_address", n.ip_address},
            {"status", n.status},
            {"hostname", n.hostname},
            {"platform", n.platform},
            {"os_version", n.os_version},
            {"local_ip", n.local_ip},
            {"app_version", n.app_version},
            {"is_parent", n.is_parent},
            {"master_token", n.master_token},
            {"encryption_key", n.encryption_key},
            {"last_seen", n.last_seen}
        });
    }

    auto snap = snapshot_dag[id];
    nlohmann::json j = {
        {"nodes", nodes_array},
        {"knowledge_data", state.knowledge_data},
        {"system_state", {
            {"services_status", snap->system_state.services_status},
            {"git_config", snap->system_state.git_config},
            {"env_vars", snap->system_state.env_vars},
            {"custom_files", snap->system_state.custom_files}
        }}
    };

    f << j.dump(4);
}

void SnapshotManager::ReplayLogs() {
    fs::path log_path = fs::path(workspace_path) / ".mcp_history" / "history.log";
    std::ifstream f(log_path);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            nlohmann::json j = nlohmann::json::parse(line);
            auto snap = std::make_shared<Snapshot>();
            snap->id = j.value("id", "");
            snap->parent_ids = j.value("parent_ids", std::vector<std::string>());
            snap->timestamp = std::chrono::system_clock::from_time_t(j.value("timestamp", 0LL));
            snap->author = static_cast<SnapshotAuthor>(j.value("author", 0));
            snap->author_name = j.value("author_name", "");
            snap->description = j.value("description", "");
            snap->is_checkpoint = j.value("is_checkpoint", false);
            snap->knowledge_patch = j.value("knowledge_patch", nlohmann::json::array());

            // Reconstruct cluster diff
            if (j.contains("cluster_diff")) {
                const auto& diff = j["cluster_diff"];
                snap->cluster_diff.removed_node_ids = diff.value("removed_node_ids", std::vector<std::string>());
                
                if (diff.contains("added_nodes")) {
                    for (const auto& item : diff["added_nodes"]) {
                        ClusterNode n;
                        n.id = item.value("id", "");
                        n.ip_address = item.value("ip_address", "");
                        n.status = item.value("status", "");
                        n.hostname = item.value("hostname", "");
                        n.platform = item.value("platform", "");
                        snap->cluster_diff.added_nodes.push_back(n);
                    }
                }
                if (diff.contains("modified_nodes")) {
                    for (const auto& item : diff["modified_nodes"]) {
                        ClusterNode n;
                        n.id = item.value("id", "");
                        n.ip_address = item.value("ip_address", "");
                        n.status = item.value("status", "");
                        n.hostname = item.value("hostname", "");
                        n.platform = item.value("platform", "");
                        snap->cluster_diff.modified_nodes.push_back(n);
                    }
                }
            }

            if (j.contains("system_state")) {
                const auto& sys = j["system_state"];
                snap->system_state.services_status = sys.value("services_status", nlohmann::json::object());
                snap->system_state.git_config = sys.value("git_config", nlohmann::json::object());
                snap->system_state.env_vars = sys.value("env_vars", nlohmann::json::object());
                snap->system_state.custom_files = sys.value("custom_files", nlohmann::json::object());
            }

            snapshot_dag[snap->id] = snap;
            current_active_id = snap->id;
        } catch (...) {}
    }
}

bool SnapshotManager::IsKnowledgeModified() {
    std::lock_guard<std::mutex> lock(mtx);
    auto active_state = ReconstructStateImpl(current_active_id);
    if (!active_state) return false;
    nlohmann::json new_knowledge = KnowledgeLayer::GetInstance().GetFullKnowledge()["user_provided"];
    return active_state->knowledge_data != new_knowledge;
}

bool SnapshotManager::IsTopologyModified() {
    std::lock_guard<std::mutex> lock(mtx);
    auto active_state = ReconstructStateImpl(current_active_id);
    if (!active_state) return false;
    std::vector<ClusterNode> new_nodes = ClusterManager::GetInstance().GetNodes();
    
    if (active_state->nodes.size() != new_nodes.size()) return true;
    for (size_t i = 0; i < new_nodes.size(); i++) {
        const auto& a = active_state->nodes[i];
        const auto& b = new_nodes[i];
        if (a.id != b.id || a.ip_address != b.ip_address || a.status != b.status || 
            a.hostname != b.hostname || a.platform != b.platform) {
            return true;
        }
    }
    return false;
}

bool SnapshotManager::IsKnowledgeStaged() {
    std::lock_guard<std::mutex> lock(mtx);
    return std::find(staged_items.begin(), staged_items.end(), "mcp_knowledge.json") != staged_items.end();
}

bool SnapshotManager::IsTopologyStaged() {
    std::lock_guard<std::mutex> lock(mtx);
    return std::find(staged_items.begin(), staged_items.end(), "cluster_nodes.json") != staged_items.end();
}
