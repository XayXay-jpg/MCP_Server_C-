#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>
#include "cluster_manager.h"

/**
 * @brief Identifies whether a state snapshot was triggered by a human user or an autonomous AI agent.
 */
enum class SnapshotAuthor {
    Human,
    AIAgent
};

/**
 * @brief Contains the differential changes of the cluster server topology.
 * Rather than copying the entire graph for every single snapshot, we record the structural modifications.
 */
struct ClusterDelta {
    std::vector<ClusterNode> added_nodes;       ///< Nodes added in this state transition
    std::vector<std::string> removed_node_ids;  ///< IDs of nodes deleted in this state transition
    std::vector<ClusterNode> modified_nodes;    ///< Nodes with changed metadata (IP, hostname, status, etc.)
};

/**
 * @brief Contains global host environment state captured in a snapshot.
 */
struct SystemState {
    nlohmann::json services_status = nlohmann::json::object(); ///< e.g. {"postgresql": "active"}
    nlohmann::json git_config = nlohmann::json::object();      ///< e.g. {"user.name": "Marat"}
    nlohmann::json env_vars = nlohmann::json::object();        ///< e.g. {"PATH": "..."}
    nlohmann::json custom_files = nlohmann::json::object();    ///< e.g. {"tokens.json": "..."}
};

/**
 * @brief Node in the version history Merkle-DAG.
 * 
 * Each snapshot contains its unique SHA-256 hash calculated over its payload and parent hashes.
 * This guarantees cryptographic consistency and enables instant comparison between different history points.
 */
struct Snapshot {
    std::string id;                         ///< Unique SHA-256 hash identifying this state
    std::vector<std::string> parent_ids;     ///< Hashes of the parent snapshots (supports branches and merges)
    
    std::chrono::system_clock::time_point timestamp;
    SnapshotAuthor author = SnapshotAuthor::Human;
    std::string author_name;                ///< User name or AI Agent identifier (e.g., Agent UUID)
    std::string description;                ///< Human-readable explanation of what changed in this version

    // Delta storage for minimal memory overhead
    ClusterDelta cluster_diff;              ///< Difference in cluster nodes graph
    nlohmann::json knowledge_patch;         ///< RFC 6902 JSON patch representing diff in Knowledge base
    SystemState system_state;               ///< Global system environment state

    bool is_checkpoint = false;             ///< True if this node contains a full state instead of a delta

    // Full state cache structure
    struct FullStateCache {
        std::vector<ClusterNode> nodes;
        nlohmann::json knowledge_data;
    };
    
    // Shared pointer to cached state (populated for checkpoints or on-demand to speed up reads)
    std::shared_ptr<FullStateCache> full_state = nullptr;
};
