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
    EVT_TIMER(wxID_ANY, CustomButton::OnAnimTimer)
wxEND_EVENT_TABLE()

CustomButton::CustomButton(wxWindow* parent, wxWindowID id, const wxString& label, const wxPoint& pos, const wxSize& size)
    : wxPanel(parent, id, pos, size, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE),
      m_label(label), m_isHovered(false), m_isPressed(false), m_isSelected(false),
      m_indent(0), m_treeLineMode(0), m_borderRadius(4), m_alignment(wxALIGN_LEFT), m_cardMode(false)
{
    m_bgColour = wxColour("#E8F1F2");
    m_fgColour = wxColour("#13293D");
    m_hoverColour = wxColour("#D0E3E5");
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    
    m_animTimer = new wxTimer(this, wxID_ANY);
    m_hoverProgress = 0.0;
    m_selProgress = 0.0;
}

void CustomButton::SetLabel(const wxString& label) {
    m_label = label;
    Refresh();
}

bool CustomButton::SetBackgroundColour(const wxColour& colour) {
    m_bgColour = colour;
    int luminance = 0.299 * colour.Red() + 0.587 * colour.Green() + 0.114 * colour.Blue();
    if (luminance < 40) {
        m_hoverColour = colour.ChangeLightness(150); // Dark bg -> lighter hover
    } else {
        m_hoverColour = colour.ChangeLightness(90); // Light bg -> darker hover
    }
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
    if (m_isSelected == selected) return;
    m_isSelected = selected;
    if (m_isSelected) {
        m_selProgress = 0.0;
    } else {
        m_selProgress = 0.0;
    }
    m_animTimer->Start(8);
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

void CustomButton::SetBorderRadius(int radius) {
    m_borderRadius = radius;
    Refresh();
}

void CustomButton::SetAlignment(int align) {
    m_alignment = align;
    Refresh();
}

void CustomButton::SetCardMode(bool cardMode) {
    m_cardMode = cardMode;
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

    // Draw background with hover fade
    wxColour drawColor = m_bgColour;
    
    if (m_cardMode) {
        wxColour borderColor = wxColour("#262626"); // hairline
        if (m_isPressed) {
            borderColor = m_hoverColour;
            drawColor = m_bgColour.ChangeLightness(110);
        } else if (m_hoverProgress > 0.0) {
            int r = borderColor.Red() + (m_hoverColour.Red() - borderColor.Red()) * m_hoverProgress;
            int g = borderColor.Green() + (m_hoverColour.Green() - borderColor.Green()) * m_hoverProgress;
            int b = borderColor.Blue() + (m_hoverColour.Blue() - borderColor.Blue()) * m_hoverProgress;
            borderColor = wxColour(r, g, b);
            // Slight lift in background
            drawColor = m_bgColour.ChangeLightness(100 + (5 * m_hoverProgress));
        }
        
        dc.SetBrush(wxBrush(drawColor));
        dc.SetPen(wxPen(borderColor, 1));
        dc.DrawRoundedRectangle(x, 0, w, size.y, m_borderRadius);
    } else {
        if (m_isPressed) {
            drawColor = m_bgColour.ChangeLightness(80);
        } else if (m_isSelected) {
            drawColor = wxColour("#1F1F24"); // Highlight color for active tab in SaaS style
        } else if (m_hoverProgress > 0.0) {
            int r = m_bgColour.Red() + (m_hoverColour.Red() - m_bgColour.Red()) * m_hoverProgress;
            int g = m_bgColour.Green() + (m_hoverColour.Green() - m_bgColour.Green()) * m_hoverProgress;
            int b = m_bgColour.Blue() + (m_hoverColour.Blue() - m_bgColour.Blue()) * m_hoverProgress;
            drawColor = wxColour(r, g, b);
        }

        // Only draw background if it's not transparent, or if it's hovered/selected
        if (m_bgColour.Alpha() > 0 || m_hoverProgress > 0.0 || m_isSelected || m_isPressed) {
            dc.SetBrush(wxBrush(drawColor));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRoundedRectangle(x, 0, w, size.y, m_borderRadius);
        }
    }
    
    // Draw left accent bar for selected item
    if (m_isSelected) {
        dc.SetBrush(wxBrush(wxColour("#3B82F6"))); // Modern blue accent
        dc.SetPen(*wxTRANSPARENT_PEN);
        double barHeight = size.y * 0.6 * m_selProgress;
        double barY = (size.y - barHeight) / 2.0;
        dc.DrawRoundedRectangle(x, barY, 4, barHeight, 2); // 4px wide, animated height
    }

    // Draw text and icon
    wxRect contentRect(x + 16, 0, w - 32, size.y); // Default padding for left-aligned
    int alignFlags = m_alignment | wxALIGN_CENTER_VERTICAL;
    
    // If text is centered, give it full width to prevent clipping
    if (m_alignment & wxALIGN_CENTER_HORIZONTAL) {
        contentRect = wxRect(x, 0, w, size.y);
    }
    
    // In compact mode, the label is empty. We want the icon perfectly centered!
    if (m_label.IsEmpty()) {
        contentRect = wxRect(x, 0, w, size.y);
        alignFlags = wxALIGN_CENTER; // Center horizontally and vertically
    }
    
    dc.SetFont(GetFont());
    
    if (m_isSelected) {
        dc.SetTextForeground(wxColour(*wxWHITE));
    } else {
        dc.SetTextForeground(m_fgColour);
    }
    
    if (m_isSelected && m_iconSelected.IsOk()) {
        dc.DrawLabel(m_label, m_iconSelected, contentRect, alignFlags);
    } else if (!m_isSelected && m_icon.IsOk()) {
        dc.DrawLabel(m_label, m_icon, contentRect, alignFlags);
    } else {
        dc.DrawLabel(m_label, contentRect, alignFlags);
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
    m_animTimer->Start(8);
    event.Skip();
}

void CustomButton::OnMouseLeave(wxMouseEvent& event) {
    m_isHovered = false;
    m_isPressed = false;
    m_animTimer->Start(8);
    event.Skip();
}

void CustomButton::OnAnimTimer(wxTimerEvent& event) {
    bool needsRefresh = false;
    
    if (m_isHovered && m_hoverProgress < 1.0) {
        m_hoverProgress += 0.04;
        if (m_hoverProgress > 1.0) m_hoverProgress = 1.0;
        needsRefresh = true;
    } else if (!m_isHovered && m_hoverProgress > 0.0) {
        m_hoverProgress -= 0.04;
        if (m_hoverProgress < 0.0) m_hoverProgress = 0.0;
        needsRefresh = true;
    }
    
    if (m_isSelected && m_selProgress < 1.0) {
        m_selProgress += 0.04; // Smooth spring-like growth
        if (m_selProgress > 1.0) m_selProgress = 1.0;
        needsRefresh = true;
    }
    
    if (needsRefresh) {
        Refresh();
        Update();
    } else {
        m_animTimer->Stop();
    }
}
