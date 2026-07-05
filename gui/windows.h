#pragma once
#include "CustomButton.h"
#include <wx/grid.h>
#include <wx/simplebook.h>
#include "SlideBook.h"
#include "cluster_topology.h"
#include <thread>
#include <atomic>
#include <wx/taskbar.h>
#include <wx/timer.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/clipbrd.h>

wxDECLARE_EVENT(wxEVT_SERVER_LOG, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_SERVER_NOTIFY, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_MCP_CONFIRM_REQUEST, wxThreadEvent);

class AnimatedLogo : public wxPanel {
public:
    AnimatedLogo(wxWindow* parent, const wxBitmap& bmp);
    void SetProgress(double progress);
    
private:
    void OnPaint(wxPaintEvent& event);
    wxImage m_img;
    double m_progress;
    wxColour m_bgColour;
    
    wxDECLARE_EVENT_TABLE();
};

class AnimatedText : public wxPanel {
public:
    AnimatedText(wxWindow* parent, const wxString& text);
    void SetProgress(double progress);
    
private:
    void OnPaint(wxPaintEvent& event);
    wxString m_text;
    double m_progress;
    wxColour m_bgColour;
    wxFont m_font;
    
    wxDECLARE_EVENT_TABLE();
};

class Windows : public wxFrame {
public:
    Windows(const wxString& title);
    ~Windows();

private:
    wxPanel* sidebarPanel = nullptr;
    wxPanel* mainPanel = nullptr;

    AnimatedText* logoText = nullptr;
    wxBitmapButton* btnToggleCompact = nullptr;
    wxBitmapButton* btnToggleTheme = nullptr;

    CustomButton* btnServerLocal = nullptr;
    CustomButton* btnCluster = nullptr;
    CustomButton* btnTools = nullptr;
    CustomButton* btnKnowledge = nullptr;
    CustomButton* btnGlobalSettings = nullptr;
    
    // Tab bar buttons (horizontal)
    CustomButton* btnTabOverview = nullptr;
    CustomButton* btnTabConnections = nullptr;
    CustomButton* btnTabLogs = nullptr;
    CustomButton* btnTabNodes = nullptr;

    wxPanel* headerPanel = nullptr;
    wxPanel* statsPanel = nullptr;
    wxPanel* statsBgPanel = nullptr;
    AnimatedLogo* logoImg = nullptr;
    wxStaticText* lblGsHeader = nullptr;
    wxStaticText* lblManager = nullptr;
    wxStaticText* lblTokenTitle = nullptr;
    wxStaticText* lblStrTitle = nullptr;
    wxStaticText* lblChkTitle = nullptr;
    wxStaticText* lblStatus = nullptr;
    wxStaticText* lblStatusValue = nullptr;
    
    wxComboBox* comboServerStats = nullptr;
    
    SlideBook* rootBook; // Master book to switch between Server View and Global Settings
    wxPanel* serverContainer; // Holds header and contentBook
    wxPanel* clusterContainer; // Holds Cluster UI
    wxStaticText* lblClusterTitle = nullptr;
    wxStaticText* lblClusterSub = nullptr;
    ClusterTopologyPanel* topoPanel = nullptr; // Topology view

    
    wxPanel* toolsContainer; // Holds Tools page
    wxStaticText* lblToolsHeader = nullptr;
    wxStaticText* lblToolsSub = nullptr;
    wxScrolledWindow* scrollTools = nullptr;
    wxBoxSizer* scrollToolsSizer = nullptr;
    wxPanel* globalSettingsContainer; // Holds pageGlobalSettings
    
    wxPanel* knowledgeContainer;
    SlideBook* knowledgeSlideBook = nullptr;
    wxPanel* bookshelfPanel = nullptr;
    wxBoxSizer* bookshelfSizer = nullptr;
    wxPanel* openBookPanel = nullptr;
    
    std::string currentKnowledgeSection;
    
    wxStaticText* lblKnowledgeHeader = nullptr;
    wxStaticText* lblKnowledgeSub = nullptr;
    wxStaticText* lblKnowledgeSectionDesc = nullptr;
    wxTextCtrl* txtKnowledgeAutoData = nullptr;
    wxTextCtrl* txtKnowledgeData = nullptr;
    CustomButton* btnKnowledgeSave = nullptr;
    CustomButton* btnKnowledgeBack = nullptr;
    SlideBook* contentBook; // Holds server tabs (Overview, Connections, Logs)
    
    // Pages
    wxPanel* pageOverview = nullptr;
    wxPanel* pageConnections = nullptr;
    wxPanel* pageLogs = nullptr;
    wxPanel* pageNodes = nullptr;
    wxScrolledWindow* pageGlobalSettings = nullptr;
    
    // Nodes page UI
    wxChoice* choiceServer = nullptr;
    wxPanel* nodeStatsPanel = nullptr;
    wxStaticText* lblNodeCpu = nullptr;
    wxStaticText* lblNodeRam = nullptr;
    wxStaticText* lblNodeDisk = nullptr;
    wxStaticText* lblNodeVram = nullptr;
    wxGauge* gaugeNodeCpu = nullptr;
    wxGauge* gaugeNodeRam = nullptr;
    wxGauge* gaugeNodeDisk = nullptr;
    wxGauge* gaugeNodeVram = nullptr;

    // Dashboard components
    CustomButton* btnStartStop = nullptr;
    
    // System Stats Panel
    wxPanel* sysStatsPanel = nullptr;
    wxStaticText* lblSysCpu = nullptr;
    wxStaticText* lblSysRam = nullptr;
    wxStaticText* lblSysDisk = nullptr;
    wxStaticText* lblSysVram = nullptr;
    wxGauge* gaugeSysCpu = nullptr;
    wxGauge* gaugeSysRam = nullptr;
    wxGauge* gaugeSysDisk = nullptr;
    wxGauge* gaugeSysVram = nullptr;
    
    wxStaticText* lblLogsHeader = nullptr;
    wxTextCtrl* txtLogs = nullptr;
    wxStaticText* statSessions = nullptr;
    wxStaticText* statErrors = nullptr;
    wxStaticText* statTools = nullptr;
    wxStaticText* statUptime = nullptr;
    
    // Workspace components
    wxStaticText* lblWorkspaceTitle = nullptr;
    wxTextCtrl* txtWorkspacePath = nullptr;
    CustomButton* btnBrowseWorkspace = nullptr;
    wxCheckBox* chkCreateMissing = nullptr;
    CustomButton* btnSaveWorkspace = nullptr;
    
    // Language components
    CustomButton* btnLangRu = nullptr;
    CustomButton* btnLangEn = nullptr;
    
    // Network components
    wxStaticText* lblNetworkInstructions = nullptr;
    wxStaticText* lblBindInfo = nullptr;
    wxStaticText* lblLocalUrl = nullptr;
    wxStaticText* lblExternalUrl = nullptr;
    wxStaticText* lblPublicIP = nullptr;
    wxTextCtrl* txtCustomDomain = nullptr;
    
    wxGrid* gridTokens = nullptr;
    CustomButton* btnCreateToken = nullptr;
    CustomButton* btnDeleteToken = nullptr;
    CustomButton* btnToggleToken = nullptr;
    CustomButton* btnEditPermissions = nullptr;
    CustomButton* btnCopyTokenUrl = nullptr;
    
    CustomButton* btnCheckNetwork = nullptr;
    CustomButton* lblCheckResult = nullptr;
    
    
    
    // --- New Global Settings Controls ---
    // New Pointers for labels to support dynamic translation
    wxStaticText* lblSecSystem = nullptr;
    wxStaticText* lblSecStorage = nullptr;
    wxStaticText* lblSecAppearance = nullptr;
    wxStaticText* lblSecSecurity = nullptr;
    wxStaticText* lblSecUpdates = nullptr;
    wxStaticText* lblSecLanguage = nullptr;
    wxStaticText* lblSecWorkspace = nullptr;
    wxStaticText* lblLogRetention = nullptr;
    wxStaticText* lblThemeLabel = nullptr;
    wxCheckBox* chkLaunchOnStartup = nullptr;
    wxCheckBox* chkAutoStartServer = nullptr;
    wxCheckBox* chkMinimizeToTray = nullptr;
    wxCheckBox* chkShowNotifications = nullptr;
    wxChoice* choiceLogRetention = nullptr;
    CustomButton* btnClearCache = nullptr;
    wxChoice* choiceTheme = nullptr;
    wxCheckBox* chkCompactMode = nullptr;
    wxCheckBox* chkAppLock = nullptr;
    wxCheckBox* chkMaskSecrets = nullptr;
    wxCheckBox* chkAutoUpdate = nullptr;
    CustomButton* btnCheckUpdates = nullptr;
    wxStaticText* lblUpdateStatus = nullptr;
    wxStaticText* lblCurrentVersion = nullptr;
    
    // Tray Icon
    wxTaskBarIcon* trayIcon = nullptr;

    bool isServerRunning;
    std::thread serverThread;
    std::string currentWorkspace;
    std::atomic<int> uptimeSeconds;

    void SetupUI();
    void UpdateLanguage();
    void ApplyTheme();
    
    void UpdateSidebarSelection(CustomButton* selected);
    void UpdateTabSelection(CustomButton* selected);
    
    void OnSidebarServerLocal(wxCommandEvent& event);
    void OnSidebarCluster(wxCommandEvent& event);
    void OnSidebarTools(wxCommandEvent& event);
    void OnSidebarKnowledge(wxCommandEvent& event);
    void OnSidebarGlobalSettings(wxCommandEvent& event);
    
    void RefreshNodesList();
    
    void OnTabConnections(wxCommandEvent& event);
    void OnTabLogs(wxCommandEvent& event);
    void OnTabNodes(wxCommandEvent& event);
    void OnNodeSelected(wxCommandEvent& event);
    
    void CancelSidebarAnimation();
    
    void OnToolToggled(wxCommandEvent& event);

    void OnKnowledgeCardClick(wxCommandEvent& event);
    void OnKnowledgeBackClick(wxCommandEvent& event);
    void OnKnowledgeSave(wxCommandEvent& event);

    void OnStartStop(wxCommandEvent& event);
    void OnServerLog(wxThreadEvent& event);
    void OnServerNotify(wxThreadEvent& event);
    void OnMcpConfirmRequest(wxThreadEvent& event);
    void OnTimer(wxTimerEvent& event);
    
    void OnBrowseWorkspace(wxCommandEvent& event);
    void OnSaveWorkspace(wxCommandEvent& event);
    
    void OnSelectLangRu(wxCommandEvent& event);
    void OnSelectLangEn(wxCommandEvent& event);

    void OnCreateToken(wxCommandEvent& event);
    void OnDeleteToken(wxCommandEvent& event);
    void OnToggleToken(wxCommandEvent& event);
    void OnEditPermissions(wxCommandEvent& event);
    void OnTokenCellChanged(wxGridEvent& event);
    void SaveTokensFromGrid();
    void OnCopyTokenUrl(wxCommandEvent& event);
    void OnCheckNetwork(wxCommandEvent& event);
    void OnCustomDomainChanged(wxCommandEvent& event);
    void OnAddNode(wxCommandEvent& event);
    void OnAddParentNode();
    
    void OnGridSize(wxSizeEvent& event);
    
    void RefreshTokenList();
    void RefreshToolsList();
    void RefreshConnectionUrls();

    void OnCloseWindow(wxCloseEvent& event);
    void OnTrayIconActivated(wxTaskBarIconEvent& event);
    
    // Settings Handlers
    void OnSettingChanged(wxCommandEvent& event);
    void OnClearCache(wxCommandEvent& event);
    void OnSettingsPage(wxCommandEvent& event);
    
    void OnToggleCompact(wxCommandEvent& event);
    void OnToggleTheme(wxCommandEvent& event);

    void OnTabOverview(wxCommandEvent& event);
    void OnCheckUpdates(wxCommandEvent& event);
    void OnComboServerStats(wxCommandEvent& event);

    wxTimer* m_timer = nullptr;
    wxTimer* m_animTimer = nullptr;
    int targetSidebarWidth = 240;
    int currentSidebarWidth = 240;
    
    wxImage compactIconImg;
    double currentIconAngle = 0.0;
    double targetIconAngle = 0.0;
    
    void OnAnimTimer(wxTimerEvent& event);

    wxDECLARE_EVENT_TABLE();
};