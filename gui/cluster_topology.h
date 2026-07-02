#pragma once
#include <wx/wx.h>
#include <wx/graphics.h>
#include <wx/timer.h>
#include "../mcp/cluster_manager.h"
#include <functional>
#include <string>

// Context menu actions
enum ClusterTopologyAction {
    CTA_RECONNECT = 6000,
    CTA_REMOVE,
    CTA_RENAME,
    CTA_DETAILS,
    CTA_ADD_CHILD,
    CTA_APPROVE,
    CTA_REJECT,
};

class ClusterTopologyPanel : public wxPanel {
public:
    ClusterTopologyPanel(wxWindow* parent, wxWindowID id = wxID_ANY);
    ~ClusterTopologyPanel();

    void RefreshTopology();

    // Callback for right-click menu actions (Node ID, Action)
    std::function<void(const std::string&, ClusterTopologyAction)> onAction;

    void RenameNodeInUI(const std::string& oldId, const std::string& newId);

private:
    struct NodeRect {
        wxRect rect;
        std::string id;
        bool is_self;
        bool is_parent_of_me;
    };

    wxTimer m_animTimer;
    int m_animPhase = 0;
    std::vector<NodeRect> m_nodeRects;
    std::string m_hoveredId;

    void OnPaint(wxPaintEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnRightClick(wxMouseEvent& event);
    void OnLeftDblClick(wxMouseEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnMiddleDown(wxMouseEvent& event);
    void OnMiddleUp(wxMouseEvent& event);

    double m_scale = 1.0;
    double m_offsetX = 0.0;
    double m_offsetY = 0.0;
    bool m_isDraggingCanvas = false;
    wxPoint m_lastMousePos;

    std::string m_draggedNodeId;
    wxPoint m_dragOffset;
    std::map<std::string, wxPoint> m_customPositions;

    void DrawNode(wxGraphicsContext* gc, const ClusterNode& node,
                  int x, int y, int w, int h, bool isSelf);
    void DrawConnection(wxGraphicsContext* gc, int x1, int y1, int x2, int y2,
                        const std::string& status);

    std::string HitTest(const wxPoint& pt) const;
    wxString FormatLastSeen(long ts) const;

    DECLARE_EVENT_TABLE()
};
