#include "CommandParser.h"
#include "SnapshotManager.h"
#include <sstream>
#include <algorithm>
#include <iostream>

std::vector<std::string> CommandParser::Tokenize(const std::string& cmd_line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    char quote_char = 0;

    for (size_t i = 0; i < cmd_line.length(); ++i) {
        char c = cmd_line[i];
        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false;
                quote_char = 0;
            } else {
                current += c;
            }
        } else {
            if (c == '"' || c == '\'') {
                in_quotes = true;
                quote_char = c;
            } else if (std::isspace(static_cast<unsigned char>(c))) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string CommandParser::Execute(const std::string& cmd_line) {
    auto tokens = Tokenize(cmd_line);
    if (tokens.empty()) {
        return "";
    }

    size_t start_idx = 0;
    if (tokens[0] == "mcp" || tokens[0] == "git") {
        start_idx = 1;
    }

    if (start_idx >= tokens.size()) {
        return "Error: no command specified.\nUse 'mcp help' for assistance.";
    }

    std::string cmd = tokens[start_idx];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    auto& sm = SnapshotManager::GetInstance();

    if (cmd == "status") {
        return sm.GetStatusString();
    }
    else if (cmd == "add") {
        if (start_idx + 1 >= tokens.size()) {
            return "Error: nothing specified, nothing added.\nDid you mean 'mcp add .'?";
        }
        std::string target = tokens[start_idx + 1];
        if (target == "." || target == "all" || target == "*") {
            sm.StageChange("mcp_knowledge.json");
            sm.StageChange("cluster_nodes.json");
            return "Staged: mcp_knowledge.json, cluster_nodes.json";
        } else if (target == "mcp_knowledge.json" || target == "cluster_nodes.json") {
            sm.StageChange(target);
            return "Staged: " + target;
        } else {
            return "Error: untracked file or invalid target: " + target + "\nValid files: mcp_knowledge.json, cluster_nodes.json";
        }
    }
    else if (cmd == "restore") {
        if (start_idx + 1 >= tokens.size()) {
            return "Error: nothing specified, nothing restored.";
        }
        
        bool staged = false;
        std::string target;
        if (tokens[start_idx + 1] == "--staged") {
            staged = true;
            if (start_idx + 2 < tokens.size()) {
                target = tokens[start_idx + 2];
            } else {
                return "Error: file to unstage must be specified.";
            }
        } else {
            target = tokens[start_idx + 1];
        }

        if (staged) {
            if (target == "." || target == "all" || target == "*") {
                sm.UnstageChange("mcp_knowledge.json");
                sm.UnstageChange("cluster_nodes.json");
                return "Unstaged: mcp_knowledge.json, cluster_nodes.json";
            } else {
                sm.UnstageChange(target);
                return "Unstaged: " + target;
            }
        } else {
            if (target == "." || target == "all" || target == "*") {
                sm.ResetHard();
                return "Restored working tree from HEAD.";
            } else {
                // Hard reset handles both
                sm.ResetHard();
                return "Restored workspace from HEAD.";
            }
        }
    }
    else if (cmd == "commit") {
        std::string message;
        for (size_t i = start_idx + 1; i < tokens.size(); ++i) {
            if ((tokens[i] == "-m" || tokens[i] == "--message") && i + 1 < tokens.size()) {
                message = tokens[i + 1];
                break;
            }
        }
        if (message.empty()) {
            return "Error: commit message is required.\nUse: mcp commit -m \"<description>\"";
        }

        // Verify there is actually something staged
        std::string status = sm.GetStatusString();
        if (status.find("Changes to be committed:") == std::string::npos) {
            return "Error: no changes added to commit (use \"mcp add\")";
        }

        auto snap = sm.CreateSnapshot(SnapshotAuthor::Human, "Admin", message);
        if (snap) {
            std::stringstream ss;
            ss << "[branch HEAD " << snap->id.substr(0, 8) << "] " << snap->description << "\n"
               << " Author: " << snap->author_name << "\n"
               << " 2 files changed (mcp_knowledge.json, cluster_nodes.json)\n";
            return ss.str();
        }
        return "Error: failed to create snapshot.";
    }
    else if (cmd == "log") {
        auto history = sm.GetHistoryDAG();
        if (history.empty()) {
            return "No history found.";
        }
        std::stringstream ss;
        for (const auto& snap : history) {
            ss << "\033[33mcommit " << snap->id << "\033[0m";
            if (snap->id == sm.GetCurrentActiveId()) {
                ss << " \033[32m(HEAD)\033[0m";
            }
            ss << "\n";
            ss << "Author: " << snap->author_name 
               << " [" << (snap->author == SnapshotAuthor::Human ? "Human" : "AI Agent") << "]\n";
            
            // Format time
            std::time_t t = std::chrono::system_clock::to_time_t(snap->timestamp);
            char time_buf[100];
            if (std::strftime(time_buf, sizeof(time_buf), "%c", std::localtime(&t))) {
                ss << "Date:   " << time_buf << "\n";
            }
            ss << "\n    " << snap->description << "\n\n";
        }
        return ss.str();
    }
    else if (cmd == "checkout") {
        if (start_idx + 1 >= tokens.size()) {
            return "Error: commit hash is required.\nUse: mcp checkout <hash>";
        }
        std::string hash = tokens[start_idx + 1];
        auto snap = sm.GetSnapshot(hash);
        if (!snap) {
            return "Error: snapshot not found: " + hash;
        }

        if (sm.RollbackTo(snap->id)) {
            return "Checked out snapshot: " + snap->id.substr(0, 8) + "\nHEAD is now at: " + snap->description;
        }
        return "Error: rollback failed.";
    }
    else if (cmd == "reset") {
        if (start_idx + 1 < tokens.size() && tokens[start_idx + 1] == "--hard") {
            if (sm.ResetHard()) {
                return "HEAD is now at: " + sm.GetCurrentActiveId().substr(0, 8) + "\nWorking directory clean.";
            }
            return "Error: reset failed.";
        }
        return "Error: only hard reset is supported.\nUse: mcp reset --hard";
    }
    else if (cmd == "diff") {
        return sm.GetDiffString();
    }
    else if (cmd == "help" || cmd == "-h" || cmd == "--help") {
        std::stringstream ss;
        ss << "MCP Versioning CLI Help:\n\n"
           << "  mcp status                   Show workspace files status vs HEAD\n"
           << "  mcp add <file>               Stage changes for committing\n"
           << "  mcp restore <file>           Discard working directory changes\n"
           << "  mcp restore --staged <file>  Unstage changes\n"
           << "  mcp commit -m \"<msg>\"       Commit staged changes as a new snapshot\n"
           << "  mcp log                      Display complete commit history DAG\n"
           << "  mcp checkout <hash>          Checkout a specific snapshot state\n"
           << "  mcp reset --hard             Discard all changes since HEAD\n"
           << "  mcp diff                     Show unstaged knowledge patches\n";
        return ss.str();
    }

    return "Error: unknown command: " + tokens[start_idx] + "\nUse 'mcp help' for assistance.";
}
