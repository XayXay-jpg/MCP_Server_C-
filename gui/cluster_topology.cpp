#include "cluster_topology.h"
#include "../mcp/settings_manager.h"
#include "../mcp/utils.h"
#include <wx/dcgraph.h>
#include <wx/dcbuffer.h>
#include <ctime>
#include <algorithm>

BEGIN_EVENT_TABLE(ClusterTopologyPanel, wxPanel)
    EVT_PAINT(ClusterTopologyPanel::OnPaint)
    EVT_RIGHT_UP(ClusterTopologyPanel::OnRightClick)
    EVT_LEFT_DCLICK(ClusterTopologyPanel::OnLeftDblClick)
    EVT_LEFT_DOWN(ClusterTopologyPanel::OnLeftDown)
    EVT_LEFT_UP(ClusterTopologyPanel::OnLeftUp)
    EVT_MOTION(ClusterTopologyPanel::OnMouseMove)
    EVT_MOUSEWHEEL(ClusterTopologyPanel::OnMouseWheel)
    EVT_MIDDLE_DOWN(ClusterTopologyPanel::OnMiddleDown)
    EVT_MIDDLE_UP(ClusterTopologyPanel::OnMiddleUp)
    EVT_TIMER(wxID_ANY, ClusterTopologyPanel::OnTimer)
END_EVENT_TABLE()

void ClusterTopologyPanel::RenameNodeInUI(const std::string& oldId, const std::string& newId) {
    if (m_customPositions.count(oldId)) {
        m_customPositions[newId] = m_customPositions[oldId];
        m_customPositions.erase(oldId);
    }
}

static wxColour StatusColor(const std::string& status) {
    if (status == "connected") return wxColour(34, 197, 94);   // green-500
    if (status == "connecting...") return wxColour(251, 191, 36); // amber-400
    if (status == "pending")    return wxColour(148, 163, 184);  // slate-400
    if (status == "error_403")  return wxColour(239, 68, 68);    // red-500
    return wxColour(100, 116, 139);  // offline
}

ClusterTopologyPanel::ClusterTopologyPanel(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id), m_animTimer(this)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(9, 9, 11));
    m_animTimer.Start(120);
}

ClusterTopologyPanel::~ClusterTopologyPanel() {
    m_animTimer.Stop();
}

void ClusterTopologyPanel::RefreshTopology() {
    Refresh();
}

wxString ClusterTopologyPanel::FormatLastSeen(long ts) const {
    if (ts == 0) return wxT("Never");
    long now = (long)std::time(nullptr);
    long diff = now - ts;
    if (diff < 60) return wxString::Format(wxT("%lds ago"), diff);
    if (diff < 3600) return wxString::Format(wxT("%ldm ago"), diff / 60);
    return wxString::Format(wxT("%ldh ago"), diff / 3600);
}

std::string ClusterTopologyPanel::HitTest(const wxPoint& pt) const {
    double x = (pt.x - m_offsetX) / m_scale;
    double y = (pt.y - m_offsetY) / m_scale;
    wxPoint adjusted(x, y);
    for (const auto& nr : m_nodeRects) {
        if (nr.rect.Contains(adjusted)) return nr.id;
    }
    return "";
}

void ClusterTopologyPanel::OnLeftDown(wxMouseEvent& event) {
    std::string hit = HitTest(event.GetPosition());
    if (!hit.empty()) {
        m_draggedNodeId = hit;
        double adjustedX = (event.GetPosition().x - m_offsetX) / m_scale;
        double adjustedY = (event.GetPosition().y - m_offsetY) / m_scale;
        for (const auto& nr : m_nodeRects) {
            if (nr.id == hit) {
                m_dragOffset = wxPoint(adjustedX - nr.rect.x, adjustedY - nr.rect.y);
                break;
            }
        }
    }
    event.Skip();
}

void ClusterTopologyPanel::OnLeftUp(wxMouseEvent& event) {
    m_draggedNodeId.clear();
    event.Skip();
}

void ClusterTopologyPanel::OnMouseMove(wxMouseEvent& event) {
    if (m_isDraggingCanvas) {
        wxPoint delta = event.GetPosition() - m_lastMousePos;
        m_offsetX += delta.x;
        m_offsetY += delta.y;
        m_lastMousePos = event.GetPosition();
        Refresh();
    } else if (!m_draggedNodeId.empty() && event.LeftIsDown()) {
        double adjustedX = (event.GetPosition().x - m_offsetX) / m_scale;
        double adjustedY = (event.GetPosition().y - m_offsetY) / m_scale;
        m_customPositions[m_draggedNodeId] = wxPoint(adjustedX - m_dragOffset.x, adjustedY - m_dragOffset.y);
        Refresh();
    } else {
        std::string prev = m_hoveredId;
        m_hoveredId = HitTest(event.GetPosition());
        if (m_hoveredId != prev) {
            if (!m_hoveredId.empty()) SetCursor(wxCursor(wxCURSOR_HAND));
            else SetCursor(wxCursor(wxCURSOR_ARROW));
            Refresh();
        }
    }
    event.Skip();
}

void ClusterTopologyPanel::OnMouseWheel(wxMouseEvent& event) {
    double oldScale = m_scale;
    if (event.GetWheelRotation() > 0) m_scale *= 1.1;
    else m_scale /= 1.1;
    
    if (m_scale < 0.2) m_scale = 0.2;
    if (m_scale > 5.0) m_scale = 5.0;

    double px = event.GetPosition().x;
    double py = event.GetPosition().y;
    m_offsetX = px - (px - m_offsetX) * (m_scale / oldScale);
    m_offsetY = py - (py - m_offsetY) * (m_scale / oldScale);

    Refresh();
}

void ClusterTopologyPanel::OnMiddleDown(wxMouseEvent& event) {
    m_isDraggingCanvas = true;
    m_lastMousePos = event.GetPosition();
    SetCursor(wxCursor(wxCURSOR_HAND));
}

void ClusterTopologyPanel::OnMiddleUp(wxMouseEvent& event) {
    m_isDraggingCanvas = false;
    SetCursor(wxCursor(wxCURSOR_ARROW));
    m_hoveredId = HitTest(event.GetPosition());
}

void ClusterTopologyPanel::OnTimer(wxTimerEvent&) {
    m_animPhase = (m_animPhase + 1) % 24;
    // Only redraw if there are connecting nodes
    auto nodes = ClusterManager::GetInstance().GetNodes();
    for (const auto& n : nodes) {
        if (n.status == "connecting...") { Refresh(); return; }
    }
}

void ClusterTopologyPanel::DrawConnection(wxGraphicsContext* gc, int x1, int y1, int x2, int y2, const std::string& status) {
    wxColour col = StatusColor(status);
    
    // End the line slightly before the tip so the thick glow doesn't overlap the node border
    float angle = std::atan2(y2 - y1, x2 - x1);
    float asize = 11.0f;
    float lineEndX = x2 - (asize - 2) * std::cos(angle);
    float lineEndY = y2 - (asize - 2) * std::sin(angle);

    // Draw glow for connected
    if (status == "connected") {
        wxGraphicsPenInfo glowPen1(wxColour(col.Red(), col.Green(), col.Blue(), 20));
        glowPen1.Width(16).Cap(wxCAP_ROUND);
        gc->SetPen(gc->CreatePen(glowPen1));
        gc->StrokeLine(x1, y1, lineEndX, lineEndY);

        wxGraphicsPenInfo glowPen2(wxColour(col.Red(), col.Green(), col.Blue(), 50));
        glowPen2.Width(6).Cap(wxCAP_ROUND);
        gc->SetPen(gc->CreatePen(glowPen2));
        gc->StrokeLine(x1, y1, lineEndX, lineEndY);
    }
    
    // Main line
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(col).Width(2).Cap(wxCAP_ROUND)));
    wxGraphicsPath path = gc->CreatePath();
    
    if (status == "connecting...") {
        // Animated dashed line
        int dashLen = 12, gapLen = 8, cycle = dashLen + gapLen;
        int totalLen = (int)std::sqrt((lineEndX-x1)*(lineEndX-x1) + (lineEndY-y1)*(lineEndY-y1));
        float dx = (float)(lineEndX - x1) / totalLen;
        float dy = (float)(lineEndY - y1) / totalLen;
        int offset = (m_animPhase * 3) % cycle;
        float cx = x1 + dx * offset;
        float cy = y1 + dy * offset;
        bool drawing = false;
        int traveled = offset;
        while (traveled < totalLen) {
            if (!drawing) {
                path.MoveToPoint(cx, cy);
                drawing = true;
                float end = std::min((float)totalLen - traveled, (float)dashLen);
                cx += dx * end; cy += dy * end; traveled += (int)end;
            } else {
                path.AddLineToPoint(cx, cy);
                drawing = false;
                float gap = std::min((float)totalLen - traveled, (float)gapLen);
                cx += dx * gap; cy += dy * gap; traveled += (int)gap;
            }
        }
        if (drawing) path.AddLineToPoint(cx, cy);
        gc->StrokePath(path);
    } else {
        gc->StrokeLine(x1, y1, lineEndX, lineEndY);
    }
    
    // Arrow tip at y2
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(col).Width(1)));
    gc->SetBrush(gc->CreateBrush(wxBrush(col)));
    wxGraphicsPath arrow = gc->CreatePath();
    arrow.MoveToPoint(x2, y2);
    arrow.AddLineToPoint(x2 - asize * std::cos(angle - 0.40f), y2 - asize * std::sin(angle - 0.40f));
    arrow.AddLineToPoint(x2 - asize * std::cos(angle + 0.40f), y2 - asize * std::sin(angle + 0.40f));
    arrow.CloseSubpath();
    gc->FillPath(arrow);
    gc->StrokePath(arrow);
}

void ClusterTopologyPanel::DrawNode(wxGraphicsContext* gc, const ClusterNode& node,
                                    int x, int y, int w, int h, bool isSelf)
{
    bool hovered = (m_hoveredId == node.id || (isSelf && m_hoveredId == "__self__"));
    wxColour statusCol = StatusColor(node.status);

    // Card shadow
    gc->SetBrush(gc->CreateBrush(wxBrush(wxColour(0, 0, 0, 60))));
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->DrawRoundedRectangle(x + 3, y + 3, w, h, 14);

    // Card background
    wxColour bgTop, bgBot;
    if (isSelf) {
        bgTop = wxColour(37, 99, 235);
        bgBot = wxColour(29, 78, 216);
    } else if (node.is_parent) {
        bgTop = wxColour(88, 28, 135);
        bgBot = wxColour(67, 20, 100);
    } else {
        bgTop = wxColour(24, 24, 27);
        bgBot = wxColour(17, 17, 20);
    }
    
    if (hovered) {
        bgTop = wxColour(
            std::min(255, (int)bgTop.Red() + 20),
            std::min(255, (int)bgTop.Green() + 20),
            std::min(255, (int)bgTop.Blue() + 20));
    }

    wxGraphicsPath cardPath = gc->CreatePath();
    cardPath.AddRoundedRectangle(x, y, w, h, 14);
    gc->SetBrush(gc->CreateLinearGradientBrush(x, y, x, y + h, bgTop, bgBot));
    gc->FillPath(cardPath);

    // Border
    wxColour borderCol = hovered ? wxColour(statusCol.Red(), statusCol.Green(), statusCol.Blue(), 200)
                                 : wxColour(statusCol.Red(), statusCol.Green(), statusCol.Blue(), 80);
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(borderCol).Width(hovered ? 2 : 1)));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(x, y, w, h, 14);

    // Status dot
    gc->SetBrush(gc->CreateBrush(wxBrush(statusCol)));
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->DrawEllipse(x + 12, y + 12, 10, 10);

    // Role badge
    wxString roleStr = isSelf ? wxT("THIS NODE") : (node.is_parent ? wxT("PARENT") : wxT("CHILD"));
    wxColour roleColor = isSelf ? wxColour(147, 197, 253) : (node.is_parent ? wxColour(216, 180, 254) : wxColour(167, 243, 208));
    wxFont roleFont = wxFontInfo(7).Bold().FaceName("Inter");
    gc->SetFont(roleFont, roleColor);
    gc->DrawText(roleStr, x + 28, y + 11);

    // Hostname
    wxString displayName = node.hostname.empty() ? wxString(node.id) : wxString(node.hostname);
    if (displayName.length() > 20) displayName = displayName.Left(18) + "..";
    wxFont nameFont = wxFontInfo(11).Bold().FaceName("Inter");
    gc->SetFont(nameFont, wxColour(244, 244, 245));
    gc->DrawText(displayName, x + 12, y + 30);

    // IP address
    wxString ipStr = wxString(node.ip_address.empty() ? "—" : node.ip_address);
    wxFont ipFont = wxFontInfo(8).FaceName("Inter");
    gc->SetFont(ipFont, wxColour(113, 113, 122));
    gc->DrawText(ipStr, x + 12, y + 50);

    // Platform
    if (!node.platform.empty()) {
        wxFont platFont = wxFontInfo(8).FaceName("Inter");
        gc->SetFont(platFont, wxColour(82, 82, 91));
        gc->DrawText(wxString(node.platform), x + 12, y + 66);
    }

    // Status text (right side)
    wxFont statusFont = wxFontInfo(8).Bold().FaceName("Inter");
    gc->SetFont(statusFont, statusCol);
    wxString statusStr = wxString(node.status);
    wxDouble sw, sh;
    gc->GetTextExtent(statusStr, &sw, &sh);
    gc->DrawText(statusStr, x + w - sw - 12, y + 12);
}

void ClusterTopologyPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) return;

    wxSize sz = GetClientSize();
    int W = sz.GetWidth(), H = sz.GetHeight();

    // Background
    gc->SetBrush(gc->CreateBrush(wxBrush(wxColour(9, 9, 11))));
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->DrawRectangle(0, 0, W, H);

    // Apply Transformation
    gc->Translate(m_offsetX, m_offsetY);
    gc->Scale(m_scale, m_scale);

    // Grid dots for depth (scaled and translated)
    gc->SetBrush(gc->CreateBrush(wxBrush(wxColour(39, 39, 42))));
    
    // Only draw the grid dots that are visible on the screen
    int gridStartX = (0 - m_offsetX) / m_scale;
    int gridStartY = (0 - m_offsetY) / m_scale;
    int gridEndX = (W - m_offsetX) / m_scale;
    int gridEndY = (H - m_offsetY) / m_scale;
    
    // Snap to 30 pixel boundaries
    gridStartX = (gridStartX / 30) * 30 - 30;
    gridStartY = (gridStartY / 30) * 30 - 30;
    gridEndX = (gridEndX / 30) * 30 + 30;
    gridEndY = (gridEndY / 30) * 30 + 30;
    
    for (int gx = gridStartX; gx < gridEndX; gx += 30)
        for (int gy = gridStartY; gy < gridEndY; gy += 30)
            gc->DrawEllipse(gx - 1, gy - 1, 2, 2);

    auto nodes = ClusterManager::GetInstance().GetNodes();
    auto& sm = SettingsManager::Get();

    // Separate into parents and children from our POV
    std::vector<ClusterNode> parents, children;
    for (const auto& n : nodes) {
        if (n.is_parent || n.id == "parent") parents.push_back(n);
        else children.push_back(n);
    }

    m_nodeRects.clear();

    int nw = 210, nh = 90;  // node width/height
    int selfX = (W - nw) / 2;
    int selfY = (H - nh) / 2;
    if (!parents.empty()) selfY = H / 2 - 10;
    if (!children.empty() && parents.empty()) selfY = (H - nh) / 2;

    if (m_customPositions.count("__self__")) {
        selfX = m_customPositions["__self__"].x;
        selfY = m_customPositions["__self__"].y;
    } else {
        m_customPositions["__self__"] = wxPoint(selfX, selfY);
    }

    // Draw parent nodes (above self)
    int parentCount = (int)parents.size();
    for (int i = 0; i < parentCount; i++) {
        int px = (W - nw * parentCount - 20 * (parentCount - 1)) / 2 + i * (nw + 20);
        int py = selfY - 160;
        
        if (m_customPositions.count(parents[i].id)) {
            px = m_customPositions[parents[i].id].x;
            py = m_customPositions[parents[i].id].y;
        } else {
            m_customPositions[parents[i].id] = wxPoint(px, py);
        }
        
        DrawNode(gc, parents[i], px, py, nw, nh, false);
        m_nodeRects.push_back({wxRect(px, py, nw, nh), parents[i].id, false, true});

        // Connection: parent → self
        DrawConnection(gc, px + nw/2, py + nh, selfX + nw/2, selfY, parents[i].status);
    }

    // Draw self node
    ClusterNode selfNode;
    selfNode.id = "__self__";
    const char* hn = std::getenv("HOSTNAME");
    if (!hn) hn = std::getenv("COMPUTERNAME");
    selfNode.hostname = hn ? hn : "This Node";
    selfNode.ip_address = GetLocalIP();
    selfNode.status = "connected";
#ifdef _WIN32
    selfNode.platform = "Windows";
#elif defined(__APPLE__)
    selfNode.platform = "macOS";
#else
    selfNode.platform = "Linux";
#endif
    DrawNode(gc, selfNode, selfX, selfY, nw, nh, true);
    m_nodeRects.push_back({wxRect(selfX, selfY, nw, nh), "__self__", true, false});

    // Draw child nodes (below self)
    int childCount = (int)children.size();
    int childRowY = selfY + nh + 80;
    int totalChildW = childCount * nw + (childCount - 1) * 30;
    int childStartX = (W - totalChildW) / 2;

    for (int i = 0; i < childCount; i++) {
        int cx = childStartX + i * (nw + 30);
        int cy = childRowY;
        
        if (m_customPositions.count(children[i].id)) {
            cx = m_customPositions[children[i].id].x;
            cy = m_customPositions[children[i].id].y;
        } else {
            m_customPositions[children[i].id] = wxPoint(cx, cy);
        }
        
        DrawNode(gc, children[i], cx, cy, nw, nh, false);
        m_nodeRects.push_back({wxRect(cx, cy, nw, nh), children[i].id, false, false});

        // Connection: self → child
        DrawConnection(gc, selfX + nw/2, selfY + nh, cx + nw/2, cy, children[i].status);
    }

    // Empty state hint
    if (nodes.empty()) {
        wxFont hintFont = wxFontInfo(13).FaceName("Inter");
        gc->SetFont(hintFont, wxColour(63, 63, 70));
        wxDouble tw, th;
        wxString hint = wxT("Right-click to add a child node");
        gc->GetTextExtent(hint, &tw, &th);
        gc->DrawText(hint, (W - tw) / 2, selfY + nh + 30);
    }

    // Help hint (drawn in screen space, so we reset transform)
    gc->SetTransform(gc->CreateMatrix());
    wxFont helpFont = wxFontInfo(8).FaceName("Inter");
    gc->SetFont(helpFont, wxColour(39, 39, 42));
    gc->DrawText(wxT("Middle Click: Pan | Mouse Wheel: Zoom | Right Click: Actions"), 12, H - 18);

    delete gc;
}

void ClusterTopologyPanel::OnRightClick(wxMouseEvent& event) {
    std::string hitId = HitTest(event.GetPosition());

    wxMenu menu;

    if (hitId.empty() || hitId == "__self__") {
        menu.Append(CTA_ADD_CHILD, wxT("➕  Add Child Node..."));
        int sel = GetPopupMenuSelectionFromUser(menu, event.GetPosition());
        if (sel == CTA_ADD_CHILD && onAction) {
            onAction("__self__", CTA_ADD_CHILD);
        }
        return;
    }

    // Find node info
    ClusterNode node;
    bool found = false;
    auto nodes = ClusterManager::GetInstance().GetNodes();
    for (const auto& n : nodes) {
        if (n.id == hitId) { node = n; found = true; break; }
    }
    if (!found) return;

    menu.AppendSeparator();
    wxMenuItem* titleItem = menu.Append(wxID_ANY, wxString("  ") + wxString(node.hostname.empty() ? node.id : node.hostname));
    titleItem->Enable(false);
    menu.AppendSeparator();

    if (node.status != "connecting...") {
        if (!node.is_parent) menu.Append(CTA_RECONNECT, wxT("🔄  Reconnect"));
        else menu.Append(CTA_RECONNECT, wxT("🔄  Refresh Connection"));
    }
    if (node.status == "pending") {
        menu.Append(CTA_APPROVE, wxT("✅  Approve Connection"));
        menu.Append(CTA_REJECT,  wxT("❌  Reject"));
    }
    menu.Append(CTA_DETAILS, wxT("ℹ️  View Details"));
    if (!node.is_parent) {
        menu.Append(CTA_RENAME,  wxT("✏️  Rename"));
    }
    menu.AppendSeparator();
    menu.Append(CTA_REMOVE, wxT("🗑️  Remove"));

    int sel = GetPopupMenuSelectionFromUser(menu, event.GetPosition());
    if (sel != wxID_NONE && onAction) {
        onAction(hitId, static_cast<ClusterTopologyAction>(sel));
    }
}

void ClusterTopologyPanel::OnLeftDblClick(wxMouseEvent& event) {
    std::string hitId = HitTest(event.GetPosition());
    if (!hitId.empty() && hitId != "__self__" && onAction) {
        onAction(hitId, CTA_DETAILS);
    }
    event.Skip();
}
