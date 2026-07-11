#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include "Snapshot.h"

/**
 * @brief Thread-safe Singleton managing the lifecycle, storage, and recovery of state snapshots.
 * 
 * SnapshotManager maintains the version tree in memory, handles saving/loading state 
 * transitions to disk (via an Append-Only Log and checkpoints), and applies rollback actions.
 */
class SnapshotManager {
public:
    /**
     * @brief Access the Singleton instance of SnapshotManager.
     */
    static SnapshotManager& GetInstance() {
        static SnapshotManager instance;
        return instance;
    }

    // Disable copy constructors for Singleton pattern
    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;

    /**
     * @brief Initialize the manager, setting workspace directory and loading history logs.
     * @param workspace_dir Path to the active workspace folder.
     */
    void Initialize(const std::string& workspace_dir);

    /**
     * @brief Capture the current state of cluster topology and knowledge base, creating a new snapshot.
     * 
     * Computes the diff relative to the current active head, hashes the snapshot, 
     * registers it in the DAG, and appends it to the disk log.
     * 
     * @param author Who triggered the snapshot (Human or AI Agent).
     * @param author_name Name/ID of the actor.
     * @param description Brief explanation of changes.
     * @return std::shared_ptr<Snapshot> The created snapshot.
     */
    std::shared_ptr<Snapshot> CreateSnapshot(
        SnapshotAuthor author, 
        const std::string& author_name, 
        const std::string& description,
        bool only_staged = false
    );

    /**
     * @brief Asynchronously restores the system state to the target snapshot ID.
     * 
     * Reconstructs the complete state at the target snapshot by applying deltas,
     * updates the active ClusterManager and KnowledgeLayer, and updates the active ID pointer.
     * 
     * @param snapshot_id Target state SHA-256 hash.
     * @return true if rollback succeeded, false otherwise.
     */
    bool RollbackTo(const std::string& snapshot_id);

    /**
     * @brief Retrieve the list of all snapshots in the history DAG.
     * @return std::vector<std::shared_ptr<Snapshot>> Vector of snapshot pointers.
     */
    std::vector<std::shared_ptr<Snapshot>> GetHistoryDAG();

    /**
     * @brief Retrieve a specific snapshot by its unique ID.
     * @param id The SHA-256 hash.
     * @return std::shared_ptr<Snapshot> Pointer to snapshot, or nullptr if not found.
     */
    std::shared_ptr<Snapshot> GetSnapshot(const std::string& id);

    /**
     * @brief Get the ID of the current active state (HEAD).
     */
    std::string GetCurrentActiveId() const { return current_active_id; }

    /**
     * @brief Garbage Collection: Prunes linear sequences of AI-generated snapshots to release RAM.
     * 
     * Keeps human snapshots and branching/merge points. Sequential minor AI snapshots 
     * are merged (squashed) to keep history clean and compact.
     * 
     * @param max_nodes Upper boundary threshold for historical nodes count before pruning kicks in.
     */
    void CleanAndPruneHistory(size_t max_nodes = 500);

    // --- Git-like staging & status methods ---
    
    /**
     * @brief Get status summary of the workspace compared to the active HEAD snapshot.
     */
    std::string GetStatusString();

    /**
     * @brief Stage an uncommitted change (node ID or knowledge section key).
     */
    bool StageChange(const std::string& target_id);

    /**
     * @brief Unstage a change from the staging index.
     */
    bool UnstageChange(const std::string& target_id);

    /**
     * @brief Reset workspace state, discarding all uncommitted changes since HEAD.
     */
    bool ResetHard();

    /**
     * @brief Get the visual difference representation of unstaged or staged changes.
     */
    std::string GetDiffString();

    /**
     * @brief Helpers to check workspace and staging states.
     */
    bool IsKnowledgeModified();
    bool IsTopologyModified();
    bool IsKnowledgeStaged();
    bool IsTopologyStaged();

    /**
     * @brief Reconstructs the full topology and knowledge base state for a given snapshot ID.
     * 
     * Traverses backwards to the nearest checkpoint (or root), then applies deltas forward.
     */
    std::shared_ptr<Snapshot::FullStateCache> ReconstructState(const std::string& id);

    /**
     * @brief Get the active workspace root path.
     */
    std::string GetWorkspacePath() const { return workspace_path; }

private:
    std::shared_ptr<Snapshot::FullStateCache> ReconstructStateImpl(const std::string& id);
    void CleanAndPruneHistoryImpl(size_t max_nodes = 500);

    SnapshotManager() = default;
    ~SnapshotManager() = default;
    
    std::string workspace_path;
    std::string current_active_id;              ///< HEAD pointer (current active snapshot ID)
    
    std::map<std::string, std::shared_ptr<Snapshot>> snapshot_dag; ///< Cache map of all loaded snapshots
    std::vector<std::string> staged_items;      ///< List of currently staged changes
    std::mutex mtx;                             ///< Mutex for thread safety across GUI and worker threads

    // --- Helper Methods ---
    
    /**
     * @brief Calculates a unique cryptographic SHA-256 hash of a snapshot.
     */
    std::string CalculateHash(const Snapshot& snap);
    
    /**
     * @brief Appends snapshot delta metadata to the transaction journal file on disk.
     */
    void AppendToLog(const Snapshot& snap);
    
    /**
     * @brief Writes a full state checkpoint to a dedicated file on disk.
     */
    void SaveCheckpoint(const std::string& id, const Snapshot::FullStateCache& state);

    /**
     * @brief Replays the append-only log to build the DAG state during startup initialization.
     */
    void ReplayLogs();
};
