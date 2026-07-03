#pragma once
#include <wx/wx.h>
#include <vector>

class SlideBook : public wxPanel {
public:
    enum SlideDirection {
        SLIDE_HORIZONTAL,
        SLIDE_VERTICAL
    };

    SlideBook(wxWindow* parent, wxWindowID id = wxID_ANY);
    void SetSlideDirection(SlideDirection dir) { m_slideDir = dir; }
    void AddPage(wxWindow* page, const wxString& text);
    void ChangeSelection(size_t index);
    size_t GetSelection() const { return m_targetSelection; }
    wxWindow* GetPage(size_t index) { return index < m_pages.size() ? m_pages[index] : nullptr; }

private:
    std::vector<wxWindow*> m_pages;
    size_t m_selection;
    size_t m_targetSelection;
    wxTimer* m_animTimer;
    double m_progress; // 0.0 to 1.0
    int m_direction;   // 1 for next page, -1 for prev page
    SlideDirection m_slideDir;

    void OnSize(wxSizeEvent& event);
    void OnAnimTimer(wxTimerEvent& event);

    wxDECLARE_EVENT_TABLE();
};
