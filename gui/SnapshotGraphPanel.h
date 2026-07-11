#pragma once

#include <wx/wx.h>
#include <wx/graphics.h>
#include "SnapshotModel.h"
#include <functional>

// Thread-safe event declarations for communicating between Worker Threads and the GUI
wxDECLARE_EVENT(wxEVT_SNAPSHOT_LAYOUT_READY, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_SNAPSHOT_ROLLBACK_COMPLETE, wxThreadEvent);

/**
 * @brief Interactive vector-drawn widget representing the Git-like version tree.
 * 
 * Implements mouse dragging (panning), scrolling (zooming), selection highlights, 
 * and notifies the controller when a node is selected or a rollback is requested.
 */
class SnapshotGraphPanel : public wxPanel {
public:
    /**
     * @brief Constructor for the SnapshotGraphPanel.
     */
    SnapshotGraphPanel(wxWindow* parent, wxWindowID id = wxID_ANY);
    virtual ~SnapshotGraphPanel() = default;

    /**
     * @brief Initiates an asynchronous task to fetch DAG history and recalculate layout.
     * When ready, triggers wxEVT_SNAPSHOT_LAYOUT_READY event.
     */
    void LoadGraphData();

    /**
     * @brief Callback invoked when a user clicks on a node. Passes the selected Snapshot ID.
     */
    std::function<void(const std::string&)> onNodeSelected;

    /**
     * @brief Callback invoked when a user confirms a rollback action. Passes the target Snapshot ID.
     */
    std::function<void(const std::string&)> onRollbackRequested;

    /**
     * @brief Callback invoked when a user requests to discard all changes from context menu on virtual state.
     */
    std::function<void()> onDiscardAllRequested;

private:
    SnapshotGraphModel m_model;             ///< Coordinates and topology data currently rendered
    std::string m_selectedNodeId;           ///< Currently clicked node hash in the UI
    std::string m_currentNodeId;            ///< Active state HEAD node hash in the workspace

    // --- Interactive Canvas Variables ---
    double m_scale = 1.0;                   ///< Current zoom scale (factor of 1.0)
    double m_offsetX = 0.0;                 ///< Panned canvas X offset
    double m_offsetY = 0.0;                 ///< Panned canvas Y offset
    bool m_isDraggingCanvas = false;        ///< Flag tracking canvas middle/left drag panning
    wxPoint m_lastMousePos;                 ///< Last recorded mouse cursor coordinate for delta computation

    // --- Drawing Parameters ---
    const int NODE_RADIUS = 14;             ///< Node radius in pixels
    const int ROW_SPACING = 70;             ///< Vertical pixel spacing between generations (rows)
    const int COL_SPACING = 50;             ///< Horizontal pixel spacing between branches (columns)

    // --- Render Methods ---
    void OnPaint(wxPaintEvent& event);
    void DrawGraphNode(wxGraphicsContext* gc, const VisualGraphNode& node);
    
    /**
     * @brief Draw a smooth anti-aliased cubic Bezier curve connecting two version nodes.
     */
    void DrawConnectorLine(wxGraphicsContext* gc, 
                           const VisualGraphNode& from, 
                           const VisualGraphNode& to, 
                           bool isActive);

    // --- Mouse Input Handlers ---
    void OnMouseMove(wxMouseEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);

    // --- Async Thread Event Receivers ---
    void OnLayoutReady(wxThreadEvent& event);
    void OnRollbackFinished(wxThreadEvent& event);

    /**
     * @brief Helper to locate which node resides under the mouse click coordinate.
     * @param pt Mouse cursor position in local coordinates.
     * @return Snapshot ID of the hovered node, or empty string if none.
     */
    std::string HitTest(const wxPoint& pt) const;

    wxDECLARE_EVENT_TABLE();
};
