#pragma once
#include <wx/wx.h>

class CustomButton : public wxPanel {
public:
    CustomButton(wxWindow* parent, wxWindowID id, const wxString& label, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);

    void SetLabel(const wxString& label) override;
    bool SetBackgroundColour(const wxColour& colour) override;
    bool SetForegroundColour(const wxColour& colour) override;
    void SetHoverColour(const wxColour& colour);
    void SetSelected(bool selected);
    void SetIcon(const wxBitmap& icon);
    void SetIndent(int indent);
    void SetTreeLineMode(int mode);
    
private:
    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);

    wxString m_label;
    wxColour m_bgColour;
    wxColour m_fgColour;
    wxColour m_hoverColour;
    wxBitmap m_icon;
    wxBitmap m_iconSelected;
    bool m_isHovered;
    bool m_isPressed;
    bool m_isSelected;
    int m_indent;
    int m_treeLineMode;

    wxDECLARE_EVENT_TABLE();
};
