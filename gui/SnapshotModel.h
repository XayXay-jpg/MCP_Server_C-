#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <wx/gdicmn.h>
#include "../mcp/Snapshot.h"

/**
 * @brief Node representation optimized for GUI drawing.
 * Contains position coordinates, timestamps, and active state flags.
 */
struct VisualGraphNode {
    std::string id;                                 ///< Full SHA-256 hash of the snapshot
    std::string short_id;                           ///< Short hash (first 7 characters)
    std::string description;                        ///< Commit message
    std::string author_name;                        ///< Name of human user or agent ID
    std::chrono::system_clock::time_point timestamp;///< Generation timestamp
    SnapshotAuthor author;                          ///< Human or AIAgent

    wxPoint position;                               ///< Final 2D canvas layout coordinates
    int row = 0;                                    ///< Time step (vertical index)
    int column = 0;                                 ///< Branch track (horizontal index)
    bool is_current = false;                        ///< True if this is the active workspace HEAD
};

/**
 * @brief Visual connection link between nodes in the graph view.
 */
struct VisualGraphLink {
    std::string from_id;                            ///< Parent node ID
    std::string to_id;                              ///< Child node ID
    bool is_active_path = false;                    ///< True if this link lies on the active path
};

/**
 * @brief Complete visual model ready for rendering by the custom drawing panel.
 */
struct SnapshotGraphModel {
    std::vector<VisualGraphNode> nodes;
    std::vector<VisualGraphLink> links;
};

/**
 * @brief Processes raw DAG history and computes 2D coordinates for the commit tree.
 * 
 * Arranges branching commits into clean grid columns and rows, avoiding overlaps
 * and rendering clear lineage paths.
 */
class SnapshotGraphLayoutEngine {
public:
    /**
     * @brief Computes positions for nodes and paths in the version graph.
     * 
     * Runs in a background thread and builds a ready-to-draw visual model.
     * 
     * @param dag The raw history list from SnapshotManager.
     * @param active_id The current active HEAD snapshot ID.
     * @return SnapshotGraphModel The populated model with coordinates.
     */
    static SnapshotGraphModel ComputeLayout(
        const std::vector<std::shared_ptr<Snapshot>>& dag, 
        const std::string& active_id
    );
};
