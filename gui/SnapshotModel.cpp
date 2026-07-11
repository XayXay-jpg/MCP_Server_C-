#include "SnapshotModel.h"
#include "../mcp/SnapshotManager.h"
#include <algorithm>
#include <map>
#include <set>

SnapshotGraphModel SnapshotGraphLayoutEngine::ComputeLayout(
    const std::vector<std::shared_ptr<Snapshot>>& dag, 
    const std::string& active_id
) {
    SnapshotGraphModel model;
    if (dag.empty()) {
        return model;
    }

    // Create a copy of the DAG
    auto list = dag;
    
    // Check if the workspace has modifications
    bool modified = SnapshotManager::GetInstance().IsKnowledgeModified() || SnapshotManager::GetInstance().IsTopologyModified();
    if (modified) {
        auto workspace_node = std::make_shared<Snapshot>();
        workspace_node->id = "workspace_modified";
        if (!active_id.empty()) {
            workspace_node->parent_ids.push_back(active_id);
        }
        workspace_node->timestamp = std::chrono::system_clock::now();
        workspace_node->author = SnapshotAuthor::Human;
        workspace_node->author_name = "You";
        workspace_node->description = "Uncommitted Changes (Current State)";
        workspace_node->is_checkpoint = false;
        
        // Append to our layout list at the end (so it renders at the bottom)
        list.push_back(workspace_node);
    }

    // Reverse the list so the root (no parents) is at index 0 (top of the page)
    std::reverse(list.begin(), list.end());

    // Build row indexes
    std::map<std::string, int> node_rows;
    for (size_t i = 0; i < list.size(); ++i) {
        node_rows[list[i]->id] = static_cast<int>(i);
    }

    // Active path to highlight the current lineage path leading to active_id
    std::set<std::string> active_path;
    std::string curr = active_id;
    while (!curr.empty()) {
        active_path.insert(curr);
        auto it = std::find_if(dag.begin(), dag.end(), 
            [&curr](const std::shared_ptr<Snapshot>& s) { return s->id == curr; });
        if (it != dag.end() && !(*it)->parent_ids.empty()) {
            curr = (*it)->parent_ids[0];
        } else {
            break;
        }
    }
    if (modified) {
        active_path.insert("workspace_modified");
    }

    std::map<std::string, int> node_columns;

    // Run grid-routing column allocation
    for (size_t row_idx = 0; row_idx < list.size(); ++row_idx) {
        const auto& snap = list[row_idx];
        int parent_col = 0;
        if (!snap->parent_ids.empty() && node_columns.count(snap->parent_ids[0])) {
            parent_col = node_columns[snap->parent_ids[0]];
        }

        int col = parent_col;
        while (true) {
            bool blocked = false;

            // 1. Check if any node in this row already has this column
            for (size_t prev_idx = 0; prev_idx < row_idx; ++prev_idx) {
                if (node_columns[list[prev_idx]->id] == col) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) {
                col++;
                continue;
            }

            // 2. Check if any active link passes through this column in this row.
            for (const auto& other : list) {
                for (const auto& pid : other->parent_ids) {
                    if (node_rows.count(pid) && node_rows.count(other->id)) {
                        int r_parent = node_rows[pid];
                        int r_child = node_rows[other->id];
                        if (r_parent < static_cast<int>(row_idx) && r_child > static_cast<int>(row_idx)) {
                            int c_parent = node_columns.count(pid) ? node_columns[pid] : -1;
                            int c_child = node_columns.count(other->id) ? node_columns[other->id] : -1;
                            if (c_parent == col || c_child == col) {
                                blocked = true;
                                break;
                            }
                        }
                    }
                }
                if (blocked) break;
            }

            if (!blocked) {
                break;
            }
            col++;
        }

        node_columns[snap->id] = col;

        VisualGraphNode v_node;
        v_node.id = snap->id;
        v_node.short_id = (snap->id == "workspace_modified") ? "*" : snap->id.substr(0, 7);
        v_node.description = snap->description;
        v_node.author_name = snap->author_name;
        v_node.timestamp = snap->timestamp;
        v_node.author = snap->author;
        v_node.row = static_cast<int>(row_idx);
        v_node.column = col;
        v_node.is_current = (snap->id == active_id);

        v_node.position.x = 100 + col * 85;
        v_node.position.y = 60 + static_cast<int>(row_idx) * 70;

        model.nodes.push_back(v_node);
    }

    // Build visual links
    for (const auto& snap : list) {
        for (const auto& parent_id : snap->parent_ids) {
            auto parent_it = std::find_if(model.nodes.begin(), model.nodes.end(),
                [&parent_id](const VisualGraphNode& n) { return n.id == parent_id; });
            
            if (parent_it != model.nodes.end()) {
                VisualGraphLink link;
                link.from_id = parent_id;
                link.to_id = snap->id;
                link.is_active_path = (active_path.count(parent_id) && active_path.count(snap->id));
                model.links.push_back(link);
            }
        }
    }

    return model;
}
