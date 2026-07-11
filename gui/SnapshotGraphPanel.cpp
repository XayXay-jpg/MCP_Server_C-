#include "SnapshotGraphPanel.h"
#include "../mcp/SnapshotManager.h"
#include "../mcp/thread_pool.h"
#include "LanguageManager.h"
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <iostream>

wxDEFINE_EVENT(wxEVT_SNAPSHOT_LAYOUT_READY, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_SNAPSHOT_ROLLBACK_COMPLETE, wxThreadEvent);

wxBEGIN_EVENT_TABLE(SnapshotGraphPanel, wxPanel)
    EVT_PAINT(SnapshotGraphPanel::OnPaint)
    EVT_MOTION(SnapshotGraphPanel::OnMouseMove)
    EVT_LEFT_DOWN(SnapshotGraphPanel::OnLeftDown)
    EVT_LEFT_UP(SnapshotGraphPanel::OnLeftUp)
    EVT_RIGHT_DOWN(SnapshotGraphPanel::OnRightDown)
    EVT_MOUSEWHEEL(SnapshotGraphPanel::OnMouseWheel)
wxEND_EVENT_TABLE()

SnapshotGraphPanel::SnapshotGraphPanel(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE) {
    
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    
    Bind(wxEVT_SNAPSHOT_LAYOUT_READY, &SnapshotGraphPanel::OnLayoutReady, this);
    Bind(wxEVT_SNAPSHOT_ROLLBACK_COMPLETE, &SnapshotGraphPanel::OnRollbackFinished, this);

    // Default rollback callback: trigger background rollback task
    onRollbackRequested = [this](const std::string& snapshot_id) {
        ThreadPool::GetInstance().enqueue([this, snapshot_id]() {
            bool ok = SnapshotManager::GetInstance().RollbackTo(snapshot_id);
            wxThreadEvent* evt = new wxThreadEvent(wxEVT_SNAPSHOT_ROLLBACK_COMPLETE);
            evt->SetPayload(ok);
            wxQueueEvent(this, evt);
        });
    };
}

void SnapshotGraphPanel::LoadGraphData() {
    ThreadPool::GetInstance().enqueue([this]() {
        auto dag = SnapshotManager::GetInstance().GetHistoryDAG();
        auto active_id = SnapshotManager::GetInstance().GetCurrentActiveId();
        
        auto model = SnapshotGraphLayoutEngine::ComputeLayout(dag, active_id);
        
        wxThreadEvent* evt = new wxThreadEvent(wxEVT_SNAPSHOT_LAYOUT_READY);
        evt->SetPayload(model);
        wxQueueEvent(this, evt);
    });
}

void SnapshotGraphPanel::OnLayoutReady(wxThreadEvent& event) {
    m_model = event.GetPayload<SnapshotGraphModel>();
    m_currentNodeId = SnapshotManager::GetInstance().GetCurrentActiveId();
    
    // Auto-center canvas on load if offset is default
    if (m_offsetX == 0.0 && m_offsetY == 0.0 && !m_model.nodes.empty()) {
        m_offsetX = 50.0;
        m_offsetY = 50.0;
    }
    
    Refresh();
}

void SnapshotGraphPanel::OnRollbackFinished(wxThreadEvent& event) {
    bool ok = event.GetPayload<bool>();
    if (ok) {
        wxMessageBox("Successfully rolled back to the selected state.", "Rollback Complete", wxOK | wxICON_INFORMATION);
    } else {
        wxMessageBox("Failed to perform rollback to the selected state.", "Error", wxOK | wxICON_ERROR);
    }
    LoadGraphData();
}

void SnapshotGraphPanel::OnPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    // Fill dark background
    dc.SetBackground(wxBrush(wxColour("#0B0F19"))); // Technical slate dark background
    dc.Clear();

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) return;

    // Apply zoom & pan translation
    gc->Translate(m_offsetX, m_offsetY);
    gc->Scale(m_scale, m_scale);

    // 1. Draw Links (S-curves)
    for (const auto& link : m_model.links) {
        auto from_it = std::find_if(m_model.nodes.begin(), m_model.nodes.end(), 
            [&link](const VisualGraphNode& n) { return n.id == link.from_id; });
        auto to_it = std::find_if(m_model.nodes.begin(), m_model.nodes.end(), 
            [&link](const VisualGraphNode& n) { return n.id == link.to_id; });

        if (from_it != m_model.nodes.end() && to_it != m_model.nodes.end()) {
            DrawConnectorLine(gc, *from_it, *to_it, link.is_active_path);
        }
    }

    // 2. Draw Nodes
    for (const auto& node : m_model.nodes) {
        DrawGraphNode(gc, node);
    }

    delete gc;
}

void SnapshotGraphPanel::DrawConnectorLine(wxGraphicsContext* gc, 
                                           const VisualGraphNode& from, 
                                           const VisualGraphNode& to, 
                                           bool isActive) {
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(from.position.x, from.position.y);
    
    // Draw Bezier S-curve
    double control_offset = ROW_SPACING / 2.0;
    path.AddCurveToPoint(from.position.x, from.position.y + control_offset,
                         to.position.x, to.position.y - control_offset,
                         to.position.x, to.position.y);

    wxColour linkColor = isActive ? wxColour("#10B981") : wxColour("#3F3F46"); // Emerald green vs Gray
    double penWidth = isActive ? 3.0 : 1.5;

    gc->SetPen(wxPen(linkColor, penWidth));
    gc->StrokePath(path);
}

void SnapshotGraphPanel::DrawGraphNode(wxGraphicsContext* gc, const VisualGraphNode& node) {
    bool isCurrent = node.is_current;
    bool isSelected = (node.id == m_selectedNodeId);

    // Glow highlight for selection
    if (isSelected) {
        gc->SetBrush(wxBrush(wxColour(99, 102, 241, 60))); // Translucent indigo
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawEllipse(node.position.x - NODE_RADIUS - 6, node.position.y - NODE_RADIUS - 6,
                        (NODE_RADIUS + 6) * 2, (NODE_RADIUS + 6) * 2);
    }

    // Determine colors
    wxColour baseColor;
    if (node.id == "workspace_modified") {
        baseColor = wxColour("#F59E0B"); // Orange/Amber
    } else if (node.author == SnapshotAuthor::Human) {
        baseColor = wxColour("#3B82F6"); // Blue for human
    } else {
        baseColor = wxColour("#D97706"); // Dark orange for agent
    }

    if (isCurrent && node.id != "workspace_modified") {
        baseColor = wxColour("#10B981"); // Emerald green for HEAD
    }

    // Node fill
    if (node.id == "workspace_modified") {
        gc->SetBrush(wxBrush(wxColour(245, 158, 11, 40))); // Translucent orange/amber
    } else {
        gc->SetBrush(wxBrush(baseColor));
    }
    
    // Node border
    wxColour strokeColor = isCurrent ? wxColour(*wxWHITE) : baseColor.ChangeLightness(120);
    if (node.id == "workspace_modified") {
        strokeColor = wxColour("#F59E0B");
        gc->SetPen(wxPen(strokeColor, 2.0, wxPENSTYLE_SHORT_DASH));
    } else {
        gc->SetPen(wxPen(strokeColor, isCurrent ? 2.5 : 1.5));
    }
    gc->DrawEllipse(node.position.x - NODE_RADIUS, node.position.y - NODE_RADIUS,
                    NODE_RADIUS * 2, NODE_RADIUS * 2);

    // Draw little inner circle if it's a checkpoint
    if (isCurrent && node.id != "workspace_modified") {
        gc->SetBrush(wxBrush(wxColour("#0B0F19")));
        gc->DrawEllipse(node.position.x - 5, node.position.y - 5, 10, 10);
    }

    // Render metadata next to node
    std::string details;
    if (node.id == "workspace_modified") {
        gc->SetFont(wxFontInfo(10).Italic(), wxColour("#F59E0B"));
        details = "* [Current State] (Modified) *";
    } else {
        gc->SetFont(wxFontInfo(10), wxColour("#E4E4E7"));
        std::string desc = node.description;
        if (desc.length() > 40) desc = desc.substr(0, 37) + "...";
        details = node.short_id + " - " + desc + " [" + node.author_name + "]";
        if (isCurrent) details = "[HEAD] " + details;
    }

    gc->DrawText(details, node.position.x + NODE_RADIUS + 10, node.position.y - 8);
}

void SnapshotGraphPanel::OnMouseMove(wxMouseEvent& event) {
    if (event.Dragging() && m_isDraggingCanvas) {
        wxPoint pos = event.GetPosition();
        m_offsetX += (pos.x - m_lastMousePos.x);
        m_offsetY += (pos.y - m_lastMousePos.y);
        m_lastMousePos = pos;
        Refresh();
    }
}

void SnapshotGraphPanel::OnLeftDown(wxMouseEvent& event) {
    wxPoint pos = event.GetPosition();
    std::string hit = HitTest(pos);
    
    if (!hit.empty()) {
        m_selectedNodeId = hit;
        if (onNodeSelected) {
            onNodeSelected(hit);
        }
        Refresh();

        // Right click or double-click to rollback
        if (event.LeftDClick() || event.RightDown()) {
            // Trigger confirmation
        }
    } else {
        m_isDraggingCanvas = true;
        m_lastMousePos = pos;
    }
}

void SnapshotGraphPanel::OnLeftUp(wxMouseEvent& event) {
    m_isDraggingCanvas = false;
    
    // Check if double click happened
    wxPoint pos = event.GetPosition();
    std::string hit = HitTest(pos);
    if (!hit.empty() && event.LeftDClick()) {
        if (hit == "workspace_modified") return; // No rollback to workspace_modified
        auto snap = SnapshotManager::GetInstance().GetSnapshot(hit);
        if (snap) {
            wxString msg = wxString::Format("Are you sure you want to rollback to this state?\n\nCommit: %s\nAuthor: %s\nDescription: %s",
                                            snap->id.substr(0, 8), snap->author_name, snap->description);
            if (wxMessageBox(msg, "Confirm Rollback", wxYES_NO | wxICON_WARNING, this) == wxYES) {
                if (onRollbackRequested) {
                    onRollbackRequested(hit);
                }
            }
        }
    }
}

void SnapshotGraphPanel::OnMouseWheel(wxMouseEvent& event) {
    double zoom = (event.GetWheelRotation() > 0) ? 1.1 : 0.9;
    double new_scale = m_scale * zoom;
    
    if (new_scale >= 0.3 && new_scale <= 3.0) {
        // Zoom centering logic
        wxPoint mousePos = event.GetPosition();
        m_offsetX = mousePos.x - (mousePos.x - m_offsetX) * (new_scale / m_scale);
        m_offsetY = mousePos.y - (mousePos.y - m_offsetY) * (new_scale / m_scale);
        m_scale = new_scale;
        Refresh();
    }
}

void SnapshotGraphPanel::OnRightDown(wxMouseEvent& event) {
    wxPoint pos = event.GetPosition();
    std::string hit = HitTest(pos);
    if (!hit.empty()) {
        m_selectedNodeId = hit;
        if (onNodeSelected) {
            onNodeSelected(hit);
        }
        Refresh();
        
        wxMenu menu;
        if (hit == "workspace_modified") {
            menu.Append(10003, "Discard all changes");
        } else {
            menu.Append(10002, "Restore this snapshot");
        }
        
        menu.Bind(wxEVT_MENU, [this, hit](wxCommandEvent& evMenu) {
            if (evMenu.GetId() == 10002) {
                if (onRollbackRequested) {
                    onRollbackRequested(hit);
                }
            } else if (evMenu.GetId() == 10003) {
                if (onDiscardAllRequested) {
                    onDiscardAllRequested();
                }
            }
        });
        
        PopupMenu(&menu, pos);
    }
}

std::string SnapshotGraphPanel::HitTest(const wxPoint& pt) const {
    // Convert click point back to canvas coordinates
    double cx = (pt.x - m_offsetX) / m_scale;
    double cy = (pt.y - m_offsetY) / m_scale;

    for (const auto& node : m_model.nodes) {
        double dx = node.position.x - cx;
        double dy = node.position.y - cy;
        if ((dx * dx + dy * dy) <= (NODE_RADIUS * NODE_RADIUS * 1.5)) {
            return node.id;
        }
    }
    return "";
}
