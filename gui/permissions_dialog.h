#pragma once
#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <string>
#include <vector>
#include "../mcp/network_utils.h"

class PermissionsDialog : public wxDialog {
public:
    PermissionsDialog(wxWindow* parent, const std::string& token_id);

private:
    std::string m_tokenId;
    TokenInfo m_tokenInfo;
    
    wxListCtrl* listServers;
    wxListCtrl* listTools;
    wxTextCtrl* txtWorkspace;
    
    wxButton* btnSave;
    wxButton* btnCancel;

    wxChoice* choicePresets;
    wxButton* btnApplyPreset;
    
    wxChoice* choiceOverseer;
    wxCheckBox* chkToolConfirm;
    
    std::vector<std::string> m_overseerNodeIds;
    std::vector<std::string> m_internalRoles;

    void SetupUI();
    void LoadData();
    void OnServerSelected(wxListEvent& event);
    void OnToolToggled(wxListEvent& event);
    void OnWorkspaceChanged(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnToolSelected(wxListEvent& event);
    void OnToolConfirmToggled(wxCommandEvent& event);
    void OnApplyPreset(wxCommandEvent& event);
    
    // Helper to get available tools
    std::vector<std::string> GetAllTools();

    wxDECLARE_EVENT_TABLE();
};
