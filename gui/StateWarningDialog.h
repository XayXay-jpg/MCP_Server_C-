#pragma once
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <vector>
#include <string>

enum StateWarningChoice {
    SW_ACTION_PRIMARY,   // Stage & Commit OR Save Changes
    SW_ACTION_SECONDARY, // Commit Staged Only OR Rollback Anyway
    SW_CANCEL            // Cancel
};

class StateWarningDialog : public wxDialog {
public:
    StateWarningDialog(wxWindow* parent,
                       const wxString& title,
                       const wxString& message,
                       const std::vector<std::string>& files,
                       const wxString& primaryLabel,
                       const wxString& secondaryLabel,
                       bool isRollbackWarning = false);

    StateWarningChoice GetChoice() const { return m_choice; }

private:
    StateWarningChoice m_choice = SW_CANCEL;

    void OnPrimary(wxCommandEvent& event);
    void OnSecondary(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};
