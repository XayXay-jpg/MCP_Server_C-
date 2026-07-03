#include "SlideBook.h"

wxBEGIN_EVENT_TABLE(SlideBook, wxPanel)
    EVT_SIZE(SlideBook::OnSize)
    EVT_TIMER(wxID_ANY, SlideBook::OnAnimTimer)
wxEND_EVENT_TABLE()

SlideBook::SlideBook(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxCLIP_CHILDREN),
      m_selection(0), m_targetSelection(0), m_progress(1.0), m_direction(1), m_slideDir(SLIDE_HORIZONTAL) {
    m_animTimer = new wxTimer(this, wxID_ANY);
    // Dark background to match theme
    SetBackgroundColour(wxColour("#0A0A0B"));
}

void SlideBook::AddPage(wxWindow* page, const wxString& text) {
    page->Reparent(this);
    m_pages.push_back(page);
    
    if (m_pages.size() - 1 == m_selection) {
        page->SetSize(GetClientSize());
        page->Show();
    } else {
        page->Hide();
    }
}

void SlideBook::ChangeSelection(size_t index) {
    if (index >= m_pages.size() || index == m_selection || m_pages.size() < 2) {
        return;
    }
    
    // Stop any ongoing animation and snap to target
    if (m_animTimer->IsRunning()) {
        m_animTimer->Stop();
        m_pages[m_selection]->Hide();
        m_selection = m_targetSelection;
        m_pages[m_selection]->SetPosition(wxPoint(0, 0));
        m_pages[m_selection]->SetSize(GetClientSize());
        m_pages[m_selection]->Show();
        if (index == m_selection) return;
    }
    
    m_targetSelection = index;
    m_direction = (index > m_selection) ? 1 : -1;
    m_progress = 0.0;
    
    wxSize size = GetClientSize();
    
    // Position target page off-screen
    wxWindow* targetPage = m_pages[m_targetSelection];
    targetPage->SetSize(size);
    if (m_slideDir == SLIDE_HORIZONTAL) {
        targetPage->SetPosition(wxPoint(m_direction * size.GetWidth(), 0));
    } else {
        targetPage->SetPosition(wxPoint(0, m_direction * size.GetHeight()));
    }
    targetPage->Show();
    
    // Ensure old page is correctly sized and at 0,0
    wxWindow* oldPage = m_pages[m_selection];
    oldPage->SetSize(size);
    oldPage->SetPosition(wxPoint(0, 0));
    
    m_animTimer->Start(10); // 100fps for ultra-smooth layout updates
}

void SlideBook::OnSize(wxSizeEvent& event) {
    wxSize size = GetClientSize();
    if (!m_animTimer->IsRunning()) {
        if (m_selection < m_pages.size() && m_pages[m_selection]) {
            m_pages[m_selection]->SetSize(size);
            m_pages[m_selection]->SetPosition(wxPoint(0, 0));
        }
    } else {
        // Handle resizing during animation
        m_pages[m_selection]->SetSize(size);
        m_pages[m_targetSelection]->SetSize(size);
    }
    event.Skip();
}

void SlideBook::OnAnimTimer(wxTimerEvent& event) {
    if (m_progress >= 1.0) return;
    
    m_progress += 0.015; // completes in ~66 frames (660ms at 100fps)
    if (m_progress >= 1.0) {
        m_progress = 1.0;
        m_animTimer->Stop();
        
        // Hide old page, lock new page in place
        m_pages[m_selection]->Hide();
        m_selection = m_targetSelection;
        
        m_pages[m_selection]->SetPosition(wxPoint(0, 0));
        return;
    }
    
    // Calculate easing (Ease Out Cubic)
    double p = 1.0 - m_progress;
    double easedProgress = 1.0 - (p * p * p);
    
    int w = GetClientSize().GetWidth();
    int h = GetClientSize().GetHeight();
    
    if (m_slideDir == SLIDE_HORIZONTAL) {
        int oldX = -m_direction * (int)(w * easedProgress);
        int newX = m_direction * w - m_direction * (int)(w * easedProgress);
        
        m_pages[m_selection]->SetPosition(wxPoint(oldX, 0));
        m_pages[m_targetSelection]->SetPosition(wxPoint(newX, 0));
    } else {
        int oldY = -m_direction * (int)(h * easedProgress);
        int newY = m_direction * h - m_direction * (int)(h * easedProgress);
        
        m_pages[m_selection]->SetPosition(wxPoint(0, oldY));
        m_pages[m_targetSelection]->SetPosition(wxPoint(0, newY));
    }
}
