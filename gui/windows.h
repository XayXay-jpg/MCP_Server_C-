#pragma once
#include "CustomButton.h"
#include <wx/grid.h>
#include <wx/simplebook.h>
#include <thread>
#include <atomic>
#include <wx/listctrl.h>
#include <wx/clipbrd.h>

wxDECLARE_EVENT(wxEVT_SERVER_LOG, wxThreadEvent);

class Windows : public wxFrame {
public:
    Windows(const wxString& title);
    ~Windows();

private:
    wxPanel* sidebarPanel;
    wxPanel* mainPanel;

    CustomButton* btnDashboard;
    CustomButton* btnWorkspace;
    CustomButton* btnSettings;
    CustomButton* btnLanguage;
    CustomButton* btnNetwork;
    CustomButton* btnLogs;

    wxStaticText* lblManager;
    wxStaticText* lblStatus;
    wxStaticText* lblStatusValue;
    
    wxStaticText* lblWorkspaceStatus;
    wxStaticText* lblWorkspaceValue;
    
    wxSimplebook* contentBook;
    
    // Pages
    wxPanel* pageDashboard;
    wxPanel* pageWorkspace;
    wxPanel* pageLanguage;
    wxPanel* pageNetwork;
    wxPanel* pageStub;

    // Dashboard components
    CustomButton* btnStartStop;
    wxStaticText* lblLogsHeader;
    wxTextCtrl* txtLogs;
    wxStaticText* statSessions;
    wxStaticText* statErrors;
    wxStaticText* statTools;
    wxStaticText* statUptime;
    
    // Workspace components
    wxStaticText* lblWorkspaceTitle;
    wxTextCtrl* txtWorkspacePath;
    CustomButton* btnBrowseWorkspace;
    wxCheckBox* chkCreateMissing;
    CustomButton* btnSaveWorkspace;
    
    // Language components
    CustomButton* btnLangRu;
    CustomButton* btnLangEn;
    
    // Network components
    wxStaticText* lblNetworkInstructions;
    wxStaticText* lblBindInfo;
    wxStaticText* lblLocalUrl;
    wxStaticText* lblExternalUrl;
    wxStaticText* lblPublicIP;
    wxTextCtrl* txtCustomDomain;
    
    wxGrid* gridTokens;
    CustomButton* btnCreateToken;
    CustomButton* btnDeleteToken;
    CustomButton* btnToggleToken;
    
    wxTextCtrl* txtConnString;
    CustomButton* btnCopyConn;
    CustomButton* btnCheckNetwork;
    CustomButton* lblCheckResult;
    
    
    
    // Stub components
    wxStaticText* lblStubMessage;

    bool isServerRunning;
    std::thread serverThread;
    std::string currentWorkspace;
    std::atomic<int> uptimeSeconds;

    void SetupUI();
    void UpdateLanguage();
    void ApplyTealTheme();
    
    void UpdateSidebarSelection(CustomButton* selected);
    
    void OnSidebarDashboard(wxCommandEvent& event);
    void OnSidebarWorkspace(wxCommandEvent& event);
    void OnSidebarSettings(wxCommandEvent& event);
    void OnSidebarLanguage(wxCommandEvent& event);
    void OnSidebarNetwork(wxCommandEvent& event);
    void OnSidebarLogs(wxCommandEvent& event);

    void OnStartStop(wxCommandEvent& event);
    void OnServerLog(wxThreadEvent& event);
    void OnTimer(wxTimerEvent& event);
    
    void OnBrowseWorkspace(wxCommandEvent& event);
    void OnSaveWorkspace(wxCommandEvent& event);
    
    void OnSelectLangRu(wxCommandEvent& event);
    void OnSelectLangEn(wxCommandEvent& event);

    void OnCreateToken(wxCommandEvent& event);
    void OnDeleteToken(wxCommandEvent& event);
    void OnToggleToken(wxCommandEvent& event);
    void OnTokenCellChanged(wxGridEvent& event);
    void SaveTokensFromGrid();
    void OnCopyConnection(wxCommandEvent& event);
    void OnCheckNetwork(wxCommandEvent& event);
    void OnCustomDomainChanged(wxCommandEvent& event);
    
    void RefreshTokenList();
    void RefreshConnectionUrls();

    wxTimer* m_timer;

    wxDECLARE_EVENT_TABLE();
};