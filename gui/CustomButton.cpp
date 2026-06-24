#include "CustomButton.h"
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>

wxBEGIN_EVENT_TABLE(CustomButton, wxPanel)
    EVT_PAINT(CustomButton::OnPaint)
    EVT_LEFT_DOWN(CustomButton::OnLeftDown)
    EVT_LEFT_UP(CustomButton::OnLeftUp)
    EVT_ENTER_WINDOW(CustomButton::OnMouseEnter)
    EVT_LEAVE_WINDOW(CustomButton::OnMouseLeave)
wxEND_EVENT_TABLE()

CustomButton::CustomButton(wxWindow* parent, wxWindowID id, const wxString& label, const wxPoint& pos, const wxSize& size)
    : wxPanel(parent, id, pos, size, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE),
      m_label(label), m_isHovered(false), m_isPressed(false), m_isSelected(false),
      m_indent(0), m_treeLineMode(0)
{
    m_bgColour = wxColour("#E8F1F2");
    m_fgColour = wxColour("#13293D");
    m_hoverColour = wxColour("#D0E3E5");
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void CustomButton::SetLabel(const wxString& label) {
    m_label = label;
    Refresh();
}

bool CustomButton::SetBackgroundColour(const wxColour& colour) {
    m_bgColour = colour;
    // Auto-generate hover color slightly lighter/darker
    m_hoverColour = colour.ChangeLightness(110);
    Refresh();
    return true;
}

bool CustomButton::SetForegroundColour(const wxColour& colour) {
    m_fgColour = colour;
    Refresh();
    return true;
}

void CustomButton::SetHoverColour(const wxColour& colour) {
    m_hoverColour = colour;
}

void CustomButton::SetSelected(bool selected) {
    m_isSelected = selected;
    Refresh();
}

void CustomButton::SetIcon(const wxBitmap& icon) {
    m_icon = icon;
    if (icon.IsOk()) {
        wxImage img = icon.ConvertToImage();
        if (img.HasAlpha()) {
            for (int y = 0; y < img.GetHeight(); ++y) {
                for (int x = 0; x < img.GetWidth(); ++x) {
                    img.SetRGB(x, y, 255, 255, 255);
                }
            }
        }
        m_iconSelected = wxBitmap(img);
    } else {
        m_iconSelected = wxNullBitmap;
    }
    Refresh();
}

void CustomButton::SetIndent(int indent) {
    m_indent = indent;
    Refresh();
}

void CustomButton::SetTreeLineMode(int mode) {
    m_treeLineMode = mode;
    Refresh();
}

void CustomButton::OnPaint(wxPaintEvent& event) {
    wxPaintDC paintDC(this);
    
    // Create GCDC for anti-aliasing (smooth rounded corners)
    wxGCDC dc(paintDC);
    
    wxSize size = GetSize();
    
    int x = m_indent;
    int w = size.x - m_indent;

    // Draw tree lines
    if (m_treeLineMode > 0) {
        dc.SetPen(wxPen(wxColour("#A0B2B2"), 2));
        int lineX = 22; // Drop down exactly from the center of main icons (10 + 12)
        int midY = size.y / 2;
        if (m_treeLineMode == 1) {
            dc.DrawLine(lineX, 0, lineX, size.y);
            dc.DrawLine(lineX, midY, x + 10, midY); // Horizontal line to the text
        } else if (m_treeLineMode == 2) {
            dc.DrawLine(lineX, 0, lineX, midY);
            dc.DrawLine(lineX, midY, x + 10, midY);
        }
    }

    // Draw background
    wxColour drawColor = m_bgColour;
    if (m_isPressed) {
        drawColor = m_bgColour.ChangeLightness(90);
    } else if (m_isSelected) {
        drawColor = wxColour("#158E8A"); // Highlight color for active tab
    } else if (m_isHovered) {
        drawColor = m_hoverColour;
    }

    dc.SetBrush(wxBrush(drawColor));
    dc.SetPen(*wxTRANSPARENT_PEN);
    
    // Draw rounded rectangle
    dc.DrawRoundedRectangle(x, 0, w, size.y, 15.0); // 15px border radius

    // Draw text and icon
    wxRect contentRect(x + 10, 0, w - 10, size.y); // 10px padding from left
    dc.SetFont(GetFont());
    if (m_isSelected) {
        dc.SetTextForeground(wxColour(*wxWHITE));
        if (m_iconSelected.IsOk()) {
            dc.DrawLabel(m_label, m_iconSelected, contentRect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
        } else {
            dc.DrawLabel(m_label, contentRect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
        }
    } else {
        dc.SetTextForeground(m_fgColour);
        if (m_icon.IsOk()) {
            dc.DrawLabel(m_label, m_icon, contentRect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
        } else {
            dc.DrawLabel(m_label, contentRect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
        }
    }
}

void CustomButton::OnLeftDown(wxMouseEvent& event) {
    m_isPressed = true;
    Refresh();
    event.Skip();
}

void CustomButton::OnLeftUp(wxMouseEvent& event) {
    if (m_isPressed) {
        m_isPressed = false;
        Refresh();
        
        // Fire button click event
        wxCommandEvent evt(wxEVT_BUTTON, GetId());
        evt.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
    }
    event.Skip();
}

void CustomButton::OnMouseEnter(wxMouseEvent& event) {
    m_isHovered = true;
    Refresh();
    event.Skip();
}

void CustomButton::OnMouseLeave(wxMouseEvent& event) {
    m_isHovered = false;
    m_isPressed = false;
    Refresh();
    event.Skip();
}
