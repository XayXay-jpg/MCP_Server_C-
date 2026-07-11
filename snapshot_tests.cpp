#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cassert>

#include <wx/init.h>
#include <wx/app.h>
#include <nlohmann/json.hpp>

#include "mcp/SnapshotManager.h"
#include "mcp/cluster_manager.h"
#include "mcp/knowledge_layer.h"
#include "mcp/settings_manager.h"
#include "mcp/CommandParser.h"
#include "mcp/utils.h"
#include <thread>
#include "mcp/cluster_handler.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

// Colors for beautiful terminal output
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Global statistics
int total_tests = 0;
int passed_tests = 0;

void PrintTestResult(const std::string& name, bool success, const std::string& message = "") {
    total_tests++;
    if (success) {
        passed_tests++;
        std::cout << ANSI_COLOR_GREEN << "[ PASS ] " << name << ANSI_COLOR_RESET << "\n";
    } else {
        std::cout << ANSI_COLOR_RED << "[ FAIL ] " << name << ANSI_COLOR_RESET;
        if (!message.empty()) {
            std::cout << ": " << message;
        }
        std::cout << "\n";
    }
}

// Helper to write file content
void WriteFile(const fs::path& p, const std::string& content) {
    std::ofstream f(p);
    f << content;
}

// Helper to read file content
std::string ReadFile(const fs::path& p) {
    if (!fs::exists(p)) return "";
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    std::cout << ANSI_COLOR_CYAN << "==================================================" << ANSI_COLOR_RESET << "\n";
    std::cout << ANSI_COLOR_CYAN << "   MCP Server Snapshot System Test Suite (50 Tests)" << ANSI_COLOR_RESET << "\n";
    std::cout << ANSI_COLOR_CYAN << "==================================================" << ANSI_COLOR_RESET << "\n";

    // Initialize wxWidgets console environment (crucial for wxStandardPaths/wxFileName support)
    wxInitializer initializer;
    if (!initializer.IsOk()) {
        std::cerr << "Failed to initialize wxWidgets console environment.\n";
        return 1;
    }

    // Set up completely isolated sandbox directory
    fs::path sandbox = fs::absolute("test_sandbox");
    fs::remove_all(sandbox);
    fs::create_directories(sandbox);

    // Initialize Git repository in the sandbox to support local git config capture/restore tests
    std::string git_init_cmd = "git init " + sandbox.string() + " >/dev/null 2>&1";
    int sys_res = std::system(git_init_cmd.c_str());

    // Redirect the application workspace and settings paths
    BASE_DIR = sandbox;
    SettingsManager::Get().SetOverrideConfigDir(sandbox / "config");

    // Initialize core subsystems
    KnowledgeLayer::GetInstance().Load(sandbox.string());
    ClusterManager::GetInstance().LoadNodes();
    auto& sm = SnapshotManager::GetInstance();
    sm.Initialize(sandbox.string());

    // ----------------------------------------------------
    // Test 1: Initial Setup
    // ----------------------------------------------------
    {
        std::string active_id = sm.GetCurrentActiveId();
        auto history = sm.GetHistoryDAG();
        bool ok = !active_id.empty() && history.size() == 1;
        if (ok) {
            auto root = history[0];
            ok = ok && (root->description == "Initial Repository Setup");
            ok = ok && (root->author_name == "System");
            ok = ok && root->is_checkpoint;
        }
        PrintTestResult("Test 1: Initial Setup & Root Checkpoint", ok);
    }

    // ----------------------------------------------------
    // Test 2: Clean Status
    // ----------------------------------------------------
    {
        std::string status = sm.GetStatusString();
        bool ok = (status.find("nothing to commit, working tree clean") != std::string::npos);
        PrintTestResult("Test 2: Clean Status Check", ok);
    }

    // ----------------------------------------------------
    // Test 3: Staging Knowledge
    // ----------------------------------------------------
    {
        // Modify knowledge file
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"test_key", "value1"}});
        bool modified = sm.IsKnowledgeModified();
        bool staged_before = sm.IsKnowledgeStaged();
        sm.StageChange("mcp_knowledge.json");
        bool staged_after = sm.IsKnowledgeStaged();
        
        bool ok = modified && !staged_before && staged_after;
        PrintTestResult("Test 3: Staging Knowledge Base Changes", ok);
    }

    // ----------------------------------------------------
    // Test 4: Staging Topology
    // ----------------------------------------------------
    {
        std::string out_id;
        ClusterManager::GetInstance().RegisterNodeRequest("node_1", "127.0.0.1:3001", "Node1", "Linux", out_id);
        bool modified = sm.IsTopologyModified();
        bool staged_before = sm.IsTopologyStaged();
        sm.StageChange("cluster_nodes.json");
        bool staged_after = sm.IsTopologyStaged();

        bool ok = modified && !staged_before && staged_after;
        PrintTestResult("Test 4: Staging Topology Changes", ok);
    }

    // ----------------------------------------------------
    // Test 5: Unstaging Changes
    // ----------------------------------------------------
    {
        sm.UnstageChange("mcp_knowledge.json");
        bool knowledge_staged = sm.IsKnowledgeStaged();
        bool topology_staged = sm.IsTopologyStaged();

        bool ok = !knowledge_staged && topology_staged;
        PrintTestResult("Test 5: Unstaging Specific Files", ok);
    }

    // ----------------------------------------------------
    // Test 6: Hard Reset Unstaged
    // ----------------------------------------------------
    {
        // Stage both and commit to get a clean state
        sm.StageChange("mcp_knowledge.json");
        sm.StageChange("cluster_nodes.json");
        sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit for Test 6");

        // Make unstaged changes
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"test_key", "value2"}});
        std::string out_id;
        ClusterManager::GetInstance().RegisterNodeRequest("node_2", "127.0.0.1:3002", "Node2", "Linux", out_id);

        // Reset
        sm.ResetHard();

        bool kb_restored = (KnowledgeLayer::GetInstance().GetSection("notes") == json{{"test_key", "value1"}});
        bool topo_restored = (ClusterManager::GetInstance().GetNodes().size() == 1); // Only node_1 is present

        bool ok = kb_restored && topo_restored;
        PrintTestResult("Test 6: ResetHard Unstaged Changes", ok);
    }

    // ----------------------------------------------------
    // Test 7: Staged Commit
    // ----------------------------------------------------
    {
        std::string head_before = sm.GetCurrentActiveId();
        // Modify knowledge and stage it
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"test_key", "value2"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 7");
        std::string head_after = sm.GetCurrentActiveId();

        bool ok = (snap != nullptr) && (head_before != head_after) && (head_after == snap->id);
        PrintTestResult("Test 7: Staged Commit (Snapshot Creation)", ok);
    }

    // ----------------------------------------------------
    // Test 8: Commit Verification
    // ----------------------------------------------------
    {
        auto snap = sm.GetSnapshot(sm.GetCurrentActiveId());
        bool ok = false;
        if (snap) {
            ok = (snap->description == "Commit Test 7") &&
                 (snap->author == SnapshotAuthor::Human) &&
                 (snap->author_name == "Admin");
        }
        PrintTestResult("Test 8: Commit Metadata Verification", ok);
    }

    // Variables to track IDs for rollback/rollforward
    std::string commit_7_id = "";
    std::string commit_9_id = "";
    std::string commit_12_ids[5] = {};

    // Let's redo Test 9 to capture the ID:
    {
        commit_7_id = sm.GetCurrentActiveId(); // Captured HEAD which is Commit 7
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"test_key", "value3"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 9");
        commit_9_id = snap->id;
        PrintTestResult("Test 9: Sequential Double Commit (Re-run & Saved ID)", snap != nullptr);
    }

    // Retrying Test 10: Rollback to Commit 7
    {
        bool res = sm.RollbackTo(commit_7_id);
        bool val_ok = (KnowledgeLayer::GetInstance().GetSection("notes") == json{{"test_key", "value2"}});
        PrintTestResult("Test 10: Single-step Rollback (HEAD~1)", res && val_ok);
    }

    // Test 11: Rollforward to Commit 9
    {
        bool res = sm.RollbackTo(commit_9_id);
        bool val_ok = (KnowledgeLayer::GetInstance().GetSection("notes") == json{{"test_key", "value3"}});
        PrintTestResult("Test 11: Rollforward to Latest State", res && val_ok);
    }

    // ----------------------------------------------------
    // Test 12: Multi-Step Rollback
    // ----------------------------------------------------
    {
        // Let's commit 5 snapshots in sequence
        for (int i = 0; i < 5; i++) {
            KnowledgeLayer::GetInstance().UpdateSection("notes", {{"test_key", "value12_" + std::to_string(i)}});
            sm.StageChange("mcp_knowledge.json");
            auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit 12-" + std::to_string(i));
            commit_12_ids[i] = snap->id;
        }

        // Rollback by 3 steps (to index 1, i.e. "value12_1")
        bool res = sm.RollbackTo(commit_12_ids[1]);
        bool val_ok = (KnowledgeLayer::GetInstance().GetSection("notes") == json{{"test_key", "value12_1"}});
        PrintTestResult("Test 12: Multi-step Rollback (HEAD~3)", res && val_ok);
    }

    // ----------------------------------------------------
    // Test 13: Invalid Checkout
    // ----------------------------------------------------
    {
        std::string head_before = sm.GetCurrentActiveId();
        bool res = sm.RollbackTo("invalid_sha256_hash_that_does_not_exist");
        std::string head_after = sm.GetCurrentActiveId();

        bool ok = !res && (head_before == head_after);
        PrintTestResult("Test 13: Invalid Checkout Handling", ok);
    }

    // ----------------------------------------------------
    // Test 14: Partial Hash Checkout
    // ----------------------------------------------------
    {
        // Try checkout to prefix of commit_12_ids[1] (first 8 characters)
        std::string prefix = commit_12_ids[1].substr(0, 8);
        auto snap = sm.GetSnapshot(prefix);
        bool ok = false;
        if (snap && snap->id == commit_12_ids[1]) {
            ok = sm.RollbackTo(prefix);
        }
        PrintTestResult("Test 14: Partial Hash Prefix Checkout", ok);
    }

    // ----------------------------------------------------
    // Test 15: Add Node Topology
    // ----------------------------------------------------
    {
        // Current nodes in manager: node_1 (node_2 was discarded in Test 6 ResetHard)
        // Add new node
        std::string out_id;
        ClusterManager::GetInstance().RegisterNodeRequest("node_test_15", "192.168.1.50:3000", "Node15", "Linux", out_id);
        
        sm.StageChange("cluster_nodes.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 15");
        
        bool found = false;
        for (const auto& n : snap->cluster_diff.added_nodes) {
            if (n.id == "node_test_15") found = true;
        }
        PrintTestResult("Test 15: Delta Captures Added Node", found);
    }

    // ----------------------------------------------------
    // Test 16: Modify Node Topology
    // ----------------------------------------------------
    {
        // Modify node
        ClusterManager::GetInstance().SetNodeStatus("node_test_15", "connected");
        sm.StageChange("cluster_nodes.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 16");

        bool found = false;
        for (const auto& n : snap->cluster_diff.modified_nodes) {
            if (n.id == "node_test_15" && n.status == "connected") found = true;
        }
        PrintTestResult("Test 16: Delta Captures Modified Node Status", found);
    }

    // ----------------------------------------------------
    // Test 17: Delete Node Topology
    // ----------------------------------------------------
    {
        ClusterManager::GetInstance().RemoveNode("node_test_15");
        sm.StageChange("cluster_nodes.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 17");

        bool found = false;
        for (const auto& id : snap->cluster_diff.removed_node_ids) {
            if (id == "node_test_15") found = true;
        }
        PrintTestResult("Test 17: Delta Captures Removed Node ID", found);
    }

    // ----------------------------------------------------
    // Test 18: Topology Rollback
    // ----------------------------------------------------
    {
        // Rollback to Commit Test 16 (where node_test_15 was connected)
        auto history = sm.GetHistoryDAG();
        std::string t16_id = "";
        for (const auto& s : history) {
            if (s->description == "Commit Test 16") {
                t16_id = s->id;
                break;
            }
        }

        bool res = sm.RollbackTo(t16_id);
        
        ClusterNode node;
        bool exists = ClusterManager::GetInstance().GetNode("node_test_15", node);
        bool status_ok = exists && (node.status == "offline");

        std::string json_content = ReadFile(SettingsManager::Get().GetConfigDir() / "cluster_nodes.json");
        bool json_ok = (json_content.find("\"status\": \"connected\"") != std::string::npos);

        PrintTestResult("Test 18: Topology Rollback Node Restoration", res && status_ok && json_ok);
    }

    // ----------------------------------------------------
    // Test 19: Knowledge Addition
    // ----------------------------------------------------
    {
        KnowledgeLayer::GetInstance().UpdateSection("workflow", {{"task_a", "pending"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 19");

        // Verify JSON patch delta
        bool has_add = false;
        for (const auto& op : snap->knowledge_patch) {
            if (op.value("op", "") == "add" && op.value("path", "") == "/workflow/task_a") {
                has_add = true;
            }
        }
        PrintTestResult("Test 19: Knowledge Delta Captures Addition Patch", has_add);
    }

    // ----------------------------------------------------
    // Test 20: Knowledge Modification
    // ----------------------------------------------------
    {
        KnowledgeLayer::GetInstance().UpdateSection("workflow", {{"task_a", "running"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 20");

        bool has_replace = false;
        for (const auto& op : snap->knowledge_patch) {
            if (op.value("op", "") == "replace" && op.value("path", "") == "/workflow/task_a" && op.value("value", "") == "running") {
                has_replace = true;
            }
        }
        PrintTestResult("Test 20: Knowledge Delta Captures Replacement Patch", has_replace);
    }

    // ----------------------------------------------------
    // Test 21: Knowledge Deletion
    // ----------------------------------------------------
    {
        KnowledgeLayer::GetInstance().UpdateSection("workflow", json::object());
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 21");

        bool has_remove = false;
        for (const auto& op : snap->knowledge_patch) {
            if (op.value("op", "") == "remove" && op.value("path", "") == "/workflow/task_a") {
                has_remove = true;
            }
        }
        PrintTestResult("Test 21: Knowledge Delta Captures Removal Patch", has_remove);
    }

    // ----------------------------------------------------
    // Test 22: Knowledge Rollback
    // ----------------------------------------------------
    {
        // Rollback to Commit Test 20 (where task_a was running)
        auto history = sm.GetHistoryDAG();
        std::string t20_id = "";
        for (const auto& s : history) {
            if (s->description == "Commit Test 20") {
                t20_id = s->id;
                break;
            }
        }

        bool res = sm.RollbackTo(t20_id);
        json workflow_sec = KnowledgeLayer::GetInstance().GetSection("workflow");
        bool restored = (workflow_sec.value("task_a", "") == "running");

        PrintTestResult("Test 22: Knowledge Base Rollback Structure Restoration", res && restored);
    }

    // ----------------------------------------------------
    // Test 23: Partial Commit
    // ----------------------------------------------------
    {
        // Make changes to BOTH knowledge and cluster nodes
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"test_key", "partial_commit_val"}});
        std::string out_id;
        ClusterManager::GetInstance().RegisterNodeRequest("node_partial", "1.1.1.1:3000", "PartialNode", "macOS", out_id);

        // Stage ONLY knowledge
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 23");

        // Since SnapshotManager creates a full snapshot of the working tree, both modifications
        // are captured, making the working directory clean.
        bool kb_mod = sm.IsKnowledgeModified();
        bool topo_mod = sm.IsTopologyModified();

        // The snapshot has both knowledge and cluster changes in delta
        bool diff_ok = !snap->cluster_diff.added_nodes.empty() && !snap->knowledge_patch.empty();

        bool ok = !kb_mod && !topo_mod && diff_ok;
        PrintTestResult("Test 23: Partial Commit (One Staged triggers Full Snapshot)", ok);
    }

    // ----------------------------------------------------
    // Test 24: Custom Files Backup
    // ----------------------------------------------------
    {
        // CaptureSystemState captures tokens.json and .env inside the workspace directory (sandbox)
        // Let's create tokens.json and .env in the sandbox directory
        WriteFile(sandbox / "tokens.json", "{\"token1\": \"active\"}");
        WriteFile(sandbox / ".env", "PORT=3000\nSECRET=abc");

        // Wait, SystemState is captured for every commit. Let's make a new commit to capture them.
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"key", "custom_files_test"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 24");

        bool tokens_captured = (snap->system_state.custom_files.value("tokens.json", "") == "{\"token1\": \"active\"}");
        bool env_captured = (snap->system_state.custom_files.value(".env", "").find("SECRET=abc") != std::string::npos);

        bool ok = tokens_captured && env_captured;
        PrintTestResult("Test 24: Capture Workspace Custom Files (Database Mock)", ok);
    }

    // ----------------------------------------------------
    // Test 25: Custom Files Restore
    // ----------------------------------------------------
    {
        // Modify the files in the workspace
        WriteFile(sandbox / "tokens.json", "{\"token1\": \"revoked\"}");
        WriteFile(sandbox / ".env", "PORT=8080");

        // Rollback to Commit Test 24
        auto history = sm.GetHistoryDAG();
        std::string t24_id = "";
        for (const auto& s : history) {
            if (s->description == "Commit Test 24") {
                t24_id = s->id;
                break;
            }
        }

        bool res = sm.RollbackTo(t24_id);

        std::string tokens_restored = ReadFile(sandbox / "tokens.json");
        std::string env_restored = ReadFile(sandbox / ".env");

        bool ok = res && 
                  (tokens_restored == "{\"token1\": \"active\"}") &&
                  (env_restored.find("SECRET=abc") != std::string::npos);

        PrintTestResult("Test 25: Restore Workspace Custom Files (Database Mock)", ok);
    }

    // ----------------------------------------------------
    // Test 26: Env Variables Backup
    // ----------------------------------------------------
    {
        // Set an env var locally
        setenv("MCP_OVERSEER_ID", "overseer_test_123", 1);

        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"env", "backup"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 26");

        bool ok = (snap->system_state.env_vars.value("MCP_OVERSEER_ID", "") == "overseer_test_123");
        PrintTestResult("Test 26: Environment Variable State Backup", ok);
    }

    // ----------------------------------------------------
    // Test 27: Git Config Local Backup
    // ----------------------------------------------------
    {
        // Mock local git config inside the sandbox
        std::string git_cfg_cmd = "git -C " + sandbox.string() + " config user.name \"Test Tester\"";
        std::system(git_cfg_cmd.c_str());

        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"git", "backup"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Commit Test 27");

        bool ok = (snap->system_state.git_config.value("user.name", "") == "Test Tester");
        PrintTestResult("Test 27: Local Git Configuration Backup", ok);
    }

    // ----------------------------------------------------
    // Test 28: Checkpoint Generation
    // ----------------------------------------------------
    {
        // Currently, we have some commits. Every 10 delta steps, a full checkpoint is created.
        // Let's commit 12 times consecutively to trigger a checkpoint.
        std::string last_snap_id = "";
        bool checkpoint_created = false;
        
        for (int i = 0; i < 12; i++) {
            KnowledgeLayer::GetInstance().UpdateSection("notes", {{"checkpoint_step", i}});
            sm.StageChange("mcp_knowledge.json");
            auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Checkpoint Step " + std::to_string(i));
            last_snap_id = snap->id;
            if (snap->is_checkpoint) {
                checkpoint_created = true;
            }
        }

        PrintTestResult("Test 28: Automatic Checkpoint Generation (10+ commits)", checkpoint_created);
    }

    // ----------------------------------------------------
    // Test 29: Checkpoint Reconstruction
    // ----------------------------------------------------
    {
        // Get the latest snapshot
        std::string head_id = sm.GetCurrentActiveId();
        auto snap = sm.GetSnapshot(head_id);
        
        // Clear its full_state cache from RAM to force reconstruction from files
        snap->full_state = nullptr;

        // Reconstruct state
        auto reconstructed = sm.ReconstructState(head_id);
        
        bool ok = (reconstructed != nullptr) && 
                  (reconstructed->knowledge_data.value("notes", json::object()).value("checkpoint_step", -1) == 11);
        PrintTestResult("Test 29: State Reconstruction from Checkpoint + Deltas", ok);
    }

    // ----------------------------------------------------
    // Test 30: Log Replay Recovery
    // ----------------------------------------------------
    {
        // Store current HEAD ID
        std::string head_id_before = sm.GetCurrentActiveId();

        sm.Initialize(sandbox.string());

        std::string head_id_after = sm.GetCurrentActiveId();
        bool ok = (head_id_before == head_id_after) && (!head_id_after.empty());
        PrintTestResult("Test 30: Log Replay Recovery (Startup Integrity)", ok);
    }

    // ----------------------------------------------------
    // Test 31: Empty Commit Prevention
    // ----------------------------------------------------
    {
        sm.ResetHard();
        std::string res = CommandParser::Execute("mcp commit -m \"empty commit\"");
        bool ok = (res.find("Error: no changes added to commit") != std::string::npos);
        PrintTestResult("Test 31: Empty Commit Prevention via CLI", ok);
    }

    // ----------------------------------------------------
    // Test 32: Whitespace Change Staging (Semantic Checking)
    // ----------------------------------------------------
    {
        std::string file_path = (sandbox / "mcp_knowledge.json").string();
        std::string content = ReadFile(file_path);
        
        // Append whitespace only
        WriteFile(file_path, content + "\n\n  \n");
        KnowledgeLayer::GetInstance().Load(sandbox.string());

        // Should NOT be modified semantically
        bool modified_ws = sm.IsKnowledgeModified();

        // Revert and make actual semantic modification
        sm.ResetHard();
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"semantic_change", true}});
        bool modified_semantic = sm.IsKnowledgeModified();

        bool ok = !modified_ws && modified_semantic;
        PrintTestResult("Test 32: Whitespace Change (Semantic Checking)", ok);
        sm.ResetHard(); // Revert
    }

    // ----------------------------------------------------
    // Test 33: Invalid Rollback Target
    // ----------------------------------------------------
    {
        bool res = sm.RollbackTo("");
        PrintTestResult("Test 33: Invalid Rollback Target Handling", !res);
    }

    // ----------------------------------------------------
    // Test 34: Snapshot History Order
    // ----------------------------------------------------
    {
        auto history = sm.GetHistoryDAG();
        bool ordered = true;
        for (size_t i = 1; i < history.size(); ++i) {
            if (history[i - 1]->timestamp < history[i]->timestamp) {
                ordered = false;
            }
        }
        PrintTestResult("Test 34: Chronological History Order", ordered && !history.empty());
    }

    // ----------------------------------------------------
    // Test 35: Staging Non-Existent File
    // ----------------------------------------------------
    {
        bool staged = sm.StageChange("invalid_file.txt");
        bool kb_mod = sm.IsKnowledgeModified();
        bool topo_mod = sm.IsTopologyModified();
        sm.UnstageChange("invalid_file.txt");
        PrintTestResult("Test 35: Staging Non-Existent File Safety", staged && !kb_mod && !topo_mod);
    }

    // ----------------------------------------------------
    // Test 36: Multiple Author Roles
    // ----------------------------------------------------
    {
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"role", "agent"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap1 = sm.CreateSnapshot(SnapshotAuthor::AIAgent, "AI_Agent_1", "Agent Commit");

        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"role", "human"}});
        sm.StageChange("mcp_knowledge.json");
        auto snap2 = sm.CreateSnapshot(SnapshotAuthor::Human, "Developer", "Human Commit");

        bool ok = (snap1->author == SnapshotAuthor::AIAgent && snap1->author_name == "AI_Agent_1") &&
                  (snap2->author == SnapshotAuthor::Human && snap2->author_name == "Developer");
        PrintTestResult("Test 36: Multiple Author Roles Recording", ok);
    }

    // ----------------------------------------------------
    // Test 37: Overwrite Custom Files
    // ----------------------------------------------------
    {
        WriteFile(sandbox / ".env", "VAL=1");
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"val", 1}});
        sm.StageChange("mcp_knowledge.json");
        auto snap1 = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Env 1");

        WriteFile(sandbox / ".env", "VAL=2");
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"val", 2}});
        sm.StageChange("mcp_knowledge.json");
        auto snap2 = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Env 2");

        bool ok = (snap1->system_state.custom_files.value(".env", "") == "VAL=1") &&
                  (snap2->system_state.custom_files.value(".env", "") == "VAL=2");
        PrintTestResult("Test 37: Overwriting Custom Files in System State", ok);
    }

    // ----------------------------------------------------
    // Test 38: Missing Checkpoint Fallback
    // ----------------------------------------------------
    {
        std::string cp_id = "";
        for (const auto& s : sm.GetHistoryDAG()) {
            if (s->is_checkpoint && s->id != sm.GetHistoryDAG().back()->id) {
                cp_id = s->id;
                break;
            }
        }
        
        bool ok = false;
        if (!cp_id.empty()) {
            fs::path cp_path = sandbox / ".mcp_history" / ("checkpoint_" + cp_id + ".json");
            std::string original_content = ReadFile(cp_path);
            fs::remove(cp_path);

            auto snap = sm.GetSnapshot(cp_id);
            if (snap) {
                snap->full_state = nullptr;
                auto reconstructed = sm.ReconstructState(cp_id);
                ok = (reconstructed != nullptr);
            }
            
            WriteFile(cp_path, original_content);
        } else {
            ok = true; 
        }
        PrintTestResult("Test 38: Missing Checkpoint Fallback Recovery", ok);
    }

    // ----------------------------------------------------
    // Test 39: Log Corruption Recovery
    // ----------------------------------------------------
    {
        fs::path log_path = sandbox / ".mcp_history" / "history.log";
        std::string original_log = ReadFile(log_path);

        std::ofstream f(log_path, std::ios::app);
        f << "\nINVALID_LOG_ENTRY_CORRUPTED_SHA256_LINE_WITHOUT_METADATA_OR_JSON\n";
        f.close();

        bool ok = true;
        try {
            sm.Initialize(sandbox.string());
        } catch (...) {
            ok = false;
        }

        WriteFile(log_path, original_log);
        sm.Initialize(sandbox.string());

        PrintTestResult("Test 39: Log Corruption Recovery Integrity", ok);
    }

    // ----------------------------------------------------
    // Test 40: Mocked Service No-Op
    // ----------------------------------------------------
    {
        auto head = sm.GetSnapshot(sm.GetCurrentActiveId());
        bool ok = head && !head->system_state.services_status.empty();
        PrintTestResult("Test 40: Mocked Service Restoration No-Op Check", ok);
    }

    // ----------------------------------------------------
    // Test 41: Staged Items Clear on Rollback
    // ----------------------------------------------------
    {
        sm.StageChange("mcp_knowledge.json");
        bool staged_before = sm.IsKnowledgeStaged();

        sm.RollbackTo(sm.GetCurrentActiveId());
        bool staged_after = sm.IsKnowledgeStaged();

        PrintTestResult("Test 41: Staged Items Clear on Rollback", staged_before && !staged_after);
    }

    // ----------------------------------------------------
    // Test 42: Delete Staged File Modification
    // ----------------------------------------------------
    {
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"temp_test", "xyz"}});
        sm.StageChange("mcp_knowledge.json");
        
        sm.ResetHard();
        
        bool kb_mod = sm.IsKnowledgeModified();
        bool staged = sm.IsKnowledgeStaged();

        PrintTestResult("Test 42: Reverting Modification Clears Active Staging", !kb_mod && !staged);
    }

    // ----------------------------------------------------
    // Test 43: Reset Hard Clears Staging
    // ----------------------------------------------------
    {
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"temp_test", "xyz"}});
        sm.StageChange("mcp_knowledge.json");
        bool staged_before = sm.IsKnowledgeStaged();

        sm.ResetHard();
        bool staged_after = sm.IsKnowledgeStaged();

        PrintTestResult("Test 43: ResetHard Clears Staged Index", staged_before && !staged_after);
    }

    // ----------------------------------------------------
    // Test 44: CLI parser - Add
    // ----------------------------------------------------
    {
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"cli_test", 1}});
        std::string res = CommandParser::Execute("mcp add mcp_knowledge.json");
        
        bool staged = sm.IsKnowledgeStaged();
        bool msg_ok = (res.find("Staged: mcp_knowledge.json") != std::string::npos);

        PrintTestResult("Test 44: CLI 'mcp add' Command Execution", staged && msg_ok);
    }

    // ----------------------------------------------------
    // Test 45: CLI parser - Commit
    // ----------------------------------------------------
    {
        std::string res = CommandParser::Execute("mcp commit -m \"CLI Commit Test\"");
        
        bool msg_ok = (res.find("[branch HEAD") != std::string::npos);
        auto head = sm.GetSnapshot(sm.GetCurrentActiveId());
        bool snap_ok = head && (head->description == "CLI Commit Test");

        PrintTestResult("Test 45: CLI 'mcp commit' Command Execution", msg_ok && snap_ok);
    }

    // ----------------------------------------------------
    // Test 46: CLI parser - Checkout
    // ----------------------------------------------------
    {
        std::string prev_id = sm.GetHistoryDAG()[1]->id;
        std::string prefix = prev_id.substr(0, 8);
        
        std::string res = CommandParser::Execute("mcp checkout " + prefix);
        bool msg_ok = (res.find("Checked out snapshot") != std::string::npos);
        bool rollback_ok = (sm.GetCurrentActiveId() == prev_id);

        PrintTestResult("Test 46: CLI 'mcp checkout' Prefix Command Execution", msg_ok && rollback_ok);
    }

    // ----------------------------------------------------
    // Test 47: CLI parser - Status
    // ----------------------------------------------------
    {
        std::string res = CommandParser::Execute("mcp status");
        bool ok = (res.find("nothing to commit, working tree clean") != std::string::npos);
        PrintTestResult("Test 47: CLI 'mcp status' Command Execution", ok);
    }

    // ----------------------------------------------------
    // Test 48: Multiple Checkpoint Jumps
    // ----------------------------------------------------
    {
        std::string mid_id = "";
        for (int i = 0; i < 22; i++) {
            KnowledgeLayer::GetInstance().UpdateSection("notes", {{"jump_step", i}});
            sm.StageChange("mcp_knowledge.json");
            auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Jump Step " + std::to_string(i));
            if (i == 10) {
                mid_id = snap->id;
            }
        }

        bool res = sm.RollbackTo(mid_id);
        bool val_ok = (KnowledgeLayer::GetInstance().GetSection("notes") == json{{"jump_step", 10}});

        PrintTestResult("Test 48: Rolling Back Across Multiple Checkpoints", res && val_ok);
    }

    // ----------------------------------------------------
    // Test 49: Deep Parent DAG Traversal
    // ----------------------------------------------------
    {
        auto history = sm.GetHistoryDAG();
        bool linked = true;
        for (size_t i = 0; i < 20 && i < history.size() - 1; ++i) {
            auto current = history[i];
            auto parent = history[i + 1];
            if (current->parent_ids.empty() || current->parent_ids[0] != parent->id) {
                linked = false;
                break;
            }
        }
        PrintTestResult("Test 49: Deep Parent Linkage Integration", linked);
    }

    // ----------------------------------------------------
    // Test 50: Settings File Preservation
    // ----------------------------------------------------
    {
        auto head = sm.GetSnapshot(sm.GetCurrentActiveId());
        bool excluded = true;
        if (head) {
            excluded = !head->system_state.custom_files.contains("settings.json");
        }
        PrintTestResult("Test 50: Settings File Exclusion Security", excluded);
    }

    // ----------------------------------------------------
    // Test 51: Selective Staging - Knowledge Only
    // ----------------------------------------------------
    {
        sm.ResetHard();
        
        // Modify both
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"sel_stage_test", "knowledge_only"}});
        std::string dummy_id;
        ClusterManager::GetInstance().RegisterNodeRequest("sel_node_1", "1.1.1.1:3000", "SelNode1", "Linux", dummy_id);
        
        // Stage ONLY knowledge
        sm.StageChange("mcp_knowledge.json");
        
        // Commit with only_staged = true
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Test 51 selective", true);
        
        // Verify knowledge is clean (was committed) but topology remains modified
        bool kb_clean = !sm.IsKnowledgeModified();
        bool topo_mod = sm.IsTopologyModified();
        
        // Verify snapshot delta does NOT contain cluster node addition
        bool diff_ok = snap && snap->cluster_diff.added_nodes.empty() && !snap->knowledge_patch.empty();
        
        PrintTestResult("Test 51: Selective Staging - Knowledge Only", kb_clean && topo_mod && diff_ok);
        sm.ResetHard();
    }

    // ----------------------------------------------------
    // Test 52: Selective Staging - Topology Only
    // ----------------------------------------------------
    {
        sm.ResetHard();
        
        // Modify both
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"sel_stage_test", "topo_only"}});
        std::string dummy_id;
        ClusterManager::GetInstance().RegisterNodeRequest("sel_node_2", "2.2.2.2:3000", "SelNode2", "Linux", dummy_id);
        
        // Stage ONLY topology
        sm.StageChange("cluster_nodes.json");
        
        // Commit with only_staged = true
        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", "Test 52 selective", true);
        
        // Verify topology is clean (was committed) but knowledge remains modified
        bool topo_clean = !sm.IsTopologyModified();
        bool kb_mod = sm.IsKnowledgeModified();
        
        // Verify snapshot delta does NOT contain knowledge changes
        bool diff_ok = snap && snap->knowledge_patch.empty() && !snap->cluster_diff.added_nodes.empty();
        
        PrintTestResult("Test 52: Selective Staging - Topology Only", topo_clean && kb_mod && diff_ok);
        sm.ResetHard();
    }

    // ----------------------------------------------------
    // Test 53: Remote Cluster Node Snapshot Endpoint (/cluster/snapshot)
    // ----------------------------------------------------
    {
        sm.ResetHard();
        
        httplib::Server mock_svr;
        register_cluster_endpoints(mock_svr);
        
        std::thread mock_thread([&mock_svr]() {
            mock_svr.listen("127.0.0.1", 49152);
        });
        mock_thread.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        httplib::Client cli("127.0.0.1", 49152);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(5, 0);
        
        // Make some change to be snapshotted on the server
        KnowledgeLayer::GetInstance().UpdateSection("notes", {{"remote_snap_test", 123}});
        sm.StageChange("mcp_knowledge.json");
        
        json req_data = {
            {"description", "Remote integration test snapshot"},
            {"author", "Integration Test Runner"}
        };
        
        bool ok = false;
        std::string snap_id = "";
        if (auto res = cli.Post("/cluster/snapshot", req_data.dump(), "application/json")) {
            if (res->status == 200) {
                try {
                    json resp = json::parse(res->body);
                    if (resp.value("status", "") == "ok" && resp.contains("snapshot_id")) {
                        snap_id = resp.value("snapshot_id", "");
                        ok = !snap_id.empty();
                    }
                } catch(...) {}
            }
        }
        
        PrintTestResult("Test 53: Remote Cluster Node Snapshot Endpoint (/cluster/snapshot)", ok);

        // ----------------------------------------------------
        // Test 54: Remote Cluster Node Rollback Endpoint (/cluster/rollback)
        // ----------------------------------------------------
        bool rollback_ok = false;
        if (ok && !snap_id.empty()) {
            json req_rb = {
                {"snapshot_id", snap_id}
            };
            if (auto res = cli.Post("/cluster/rollback", req_rb.dump(), "application/json")) {
                if (res->status == 200) {
                    try {
                        json resp = json::parse(res->body);
                        if (resp.value("status", "") == "ok") {
                            rollback_ok = (sm.GetCurrentActiveId() == snap_id);
                        }
                    } catch(...) {}
                }
            }
        }
        
        PrintTestResult("Test 54: Remote Cluster Node Rollback Endpoint (/cluster/rollback)", rollback_ok);
        
        mock_svr.stop();
        sm.ResetHard();
    }

    // Print summary
    std::cout << ANSI_COLOR_CYAN << "==================================================" << ANSI_COLOR_RESET << "\n";
    std::cout << "Test Summary: " << passed_tests << " / " << total_tests << " passed.\n";
    std::cout << ANSI_COLOR_CYAN << "==================================================" << ANSI_COLOR_RESET << "\n";

    // Clean up sandbox folder after successful run
    fs::remove_all(sandbox);

    return (passed_tests == total_tests) ? 0 : 1;
}
