#include "windows.h"
#include "LanguageManager.h"
#include "../mcp/server.h"
#include "../mcp/utils.h"
#include "../mcp/network_utils.h"
#include "CustomButton.h"

#include <wx/statline.h>
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <wx/simplebook.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <filesystem>

wxDEFINE_EVENT(wxEVT_SERVER_LOG, wxThreadEvent);

enum {
    ID_BTN_DASHBOARD = 1001,
    ID_BTN_WORKSPACE,
    ID_BTN_SETTINGS,
    ID_BTN_LANGUAGE,
    ID_BTN_NETWORK,
    ID_BTN_LOGS,
    ID_BTN_STARTSTOP,
    ID_TIMER,
    ID_BTN_BROWSE_WS,
    ID_BTN_SAVE_WS,
    ID_BTN_LANG_RU,
    ID_BTN_LANG_EN,
    ID_BTN_CREATE_TOKEN,
    ID_BTN_DELETE_TOKEN,
    ID_BTN_TOGGLE_TOKEN,
    ID_BTN_COPY_CONN,
    ID_BTN_CHECK_NETWORK,
    ID_TXT_CUSTOM_DOMAIN
};

Windows::Windows(const wxString& title) : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(1000, 750)), isServerRunning(false), uptimeSeconds(0) {
    // Determine default workspace at runtime — avoids hardcoding any username
    const char* homeDir = getenv("HOME");
    currentWorkspace = homeDir ? std::string(homeDir) + "/llm_workspace" : "/tmp/llm_workspace";
    
    // Set global logger callback to send events to GUI
    g_log_callback = [this](const std::string& msg) {
        wxThreadEvent* event = new wxThreadEvent(wxEVT_SERVER_LOG);
        event->SetString(msg);
        wxQueueEvent(this, event);
    };

    SetupUI();
    ApplyTealTheme();
    UpdateLanguage();

    m_timer = new wxTimer(this, ID_TIMER);
}

Windows::~Windows() {
    if (isServerRunning) {
        stop_mcp_server();
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }
    g_log_callback = nullptr;
}

void Windows::SetupUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    // Sidebar
    sidebarPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(210, -1));
    sidebarPanel->SetBackgroundColour(wxColour("#E8F1F2"));
    
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxFileName f(exePath);
    wxString rootPath = f.GetPath() + "/../";

    // Sidebar Logo
    wxBoxSizer* logoSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxImage logoImg(rootPath + "gui/icons/logo.png", wxBITMAP_TYPE_PNG);
    if (logoImg.IsOk()) {
        wxStaticBitmap* logoBitmap = new wxStaticBitmap(sidebarPanel, wxID_ANY, wxBitmap(logoImg.Scale(32, 32, wxIMAGE_QUALITY_HIGH)));
        logoSizer->Add(logoBitmap, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    }
    
    wxStaticText* logoText = new wxStaticText(sidebarPanel, wxID_ANY, "MCP");
    logoText->SetFont(wxFontInfo(20).Bold());
    logoText->SetForegroundColour(wxColour("#13293D"));
    logoSizer->Add(logoText, 0, wxALIGN_CENTER_VERTICAL);
    
    sidebarSizer->Add(logoSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 20);

    btnDashboard = new CustomButton(sidebarPanel, ID_BTN_DASHBOARD, "", wxDefaultPosition, wxSize(190, 50));
    btnWorkspace = new CustomButton(sidebarPanel, ID_BTN_WORKSPACE, "", wxDefaultPosition, wxSize(190, 50));
    btnSettings = new CustomButton(sidebarPanel, ID_BTN_SETTINGS, "", wxDefaultPosition, wxSize(190, 50));
    btnLanguage = new CustomButton(sidebarPanel, ID_BTN_LANGUAGE, "", wxDefaultPosition, wxSize(190, 40));
    btnNetwork = new CustomButton(sidebarPanel, ID_BTN_NETWORK, "", wxDefaultPosition, wxSize(190, 40));
    btnLogs = new CustomButton(sidebarPanel, ID_BTN_LOGS, "", wxDefaultPosition, wxSize(190, 50));

    btnLanguage->SetIndent(30);
    btnLanguage->SetTreeLineMode(1);
    
    btnNetwork->SetIndent(30);
    btnNetwork->SetTreeLineMode(2);

    auto ScaleIcon = [](const wxImage& img, int targetSize) -> wxBitmap {
        int w = img.GetWidth();
        int h = img.GetHeight();
        if (w > h) {
            h = std::max(1, (h * targetSize) / w);
            w = targetSize;
        } else {
            w = std::max(1, (w * targetSize) / h);
            h = targetSize;
        }
        return wxBitmap(img.Scale(w, h, wxIMAGE_QUALITY_HIGH));
    };

    wxImage dashImg(rootPath + "gui/icons/dashboard.png", wxBITMAP_TYPE_PNG);
    if (dashImg.IsOk()) btnDashboard->SetIcon(ScaleIcon(dashImg, 26)); // Slightly wider to balance

    wxImage wsImg(rootPath + "gui/icons/workspace.png", wxBITMAP_TYPE_PNG);
    if (wsImg.IsOk()) btnWorkspace->SetIcon(ScaleIcon(wsImg, 24));
    
    wxImage settingsImg(rootPath + "gui/icons/settings.png", wxBITMAP_TYPE_PNG);
    if (settingsImg.IsOk()) btnSettings->SetIcon(ScaleIcon(settingsImg, 24));
    
    wxImage logsImg(rootPath + "gui/icons/logs.png", wxBITMAP_TYPE_PNG);
    if (logsImg.IsOk()) btnLogs->SetIcon(ScaleIcon(logsImg, 24));

    wxFont btnFont(wxFontInfo(10).Bold());
    btnDashboard->SetFont(btnFont);
    btnWorkspace->SetFont(btnFont);
    btnSettings->SetFont(btnFont);
    btnLanguage->SetFont(btnFont);
    btnNetwork->SetFont(btnFont);
    btnLogs->SetFont(btnFont);

    sidebarSizer->Add(btnDashboard, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    sidebarSizer->Add(btnWorkspace, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    sidebarSizer->Add(btnSettings, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    sidebarSizer->Add(btnLanguage, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    sidebarSizer->Add(btnNetwork, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 10);
    sidebarSizer->Add(btnLogs, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);

    sidebarPanel->SetSizer(sidebarSizer);

    // Main Area
    mainPanel = new wxPanel(this, wxID_ANY);
    mainPanel->SetBackgroundColour(wxColour("#F4F7F6"));
    
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

    // Header
    wxBoxSizer* titleBox = new wxBoxSizer(wxVERTICAL);
    lblManager = new wxStaticText(mainPanel, wxID_ANY, "MCP SERVER MANAGER");
    lblManager->SetFont(wxFontInfo(20).Bold());
    lblManager->SetForegroundColour(wxColour("#13293D"));
    
    wxBoxSizer* statusBox = new wxBoxSizer(wxHORIZONTAL);
    lblStatus = new wxStaticText(mainPanel, wxID_ANY, "Status: ");
    lblStatus->SetFont(wxFontInfo(12));
    lblStatus->SetForegroundColour(wxColour("#13293D"));
    lblStatusValue = new wxStaticText(mainPanel, wxID_ANY, "OFFLINE");
    lblStatusValue->SetFont(wxFontInfo(12).Bold());
    lblStatusValue->SetForegroundColour(wxColour("#D62828"));
    
    statusBox->Add(lblStatus);
    statusBox->Add(lblStatusValue);
    
    wxBoxSizer* workspaceBox = new wxBoxSizer(wxHORIZONTAL);
    lblWorkspaceStatus = new wxStaticText(mainPanel, wxID_ANY, "Workspace: ");
    lblWorkspaceStatus->SetFont(wxFontInfo(10));
    lblWorkspaceStatus->SetForegroundColour(wxColour("#13293D"));
    lblWorkspaceValue = new wxStaticText(mainPanel, wxID_ANY, currentWorkspace);
    lblWorkspaceValue->SetFont(wxFontInfo(10));
    lblWorkspaceValue->SetForegroundColour(wxColour("#158E8A"));
    
    workspaceBox->Add(lblWorkspaceStatus);
    workspaceBox->Add(lblWorkspaceValue);
    
    titleBox->Add(lblManager, 0, wxBOTTOM, 5);
    titleBox->Add(statusBox, 0, wxBOTTOM, 2);
    titleBox->Add(workspaceBox, 0);

    // Initialize Simplebook
    contentBook = new wxSimplebook(mainPanel, wxID_ANY);
    
    // --- Page: Dashboard ---
    pageDashboard = new wxPanel(contentBook, wxID_ANY);
    pageDashboard->SetBackgroundColour(wxColour("#F4F7F6"));
    wxBoxSizer* dashSizer = new wxBoxSizer(wxVERTICAL);

    btnStartStop = new CustomButton(pageDashboard, ID_BTN_STARTSTOP, "", wxDefaultPosition, wxSize(200, 60));
    btnStartStop->SetBackgroundColour(wxColour("#158E8A"));
    btnStartStop->SetForegroundColour(wxColour(*wxWHITE));
    btnStartStop->SetFont(wxFontInfo(14).Bold());
    
    wxPanel* logsBgPanel = new wxPanel(pageDashboard, wxID_ANY);
    logsBgPanel->SetBackgroundColour(wxColour("#E8F1F2"));
    wxBoxSizer* logsSizer = new wxBoxSizer(wxVERTICAL);
    
    lblLogsHeader = new wxStaticText(logsBgPanel, wxID_ANY, "ЛОГИ СЕРВЕРА");
    lblLogsHeader->SetFont(wxFontInfo(16).Bold());
    lblLogsHeader->SetForegroundColour(wxColour("#13293D"));
    
    txtLogs = new wxTextCtrl(logsBgPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    txtLogs->SetBackgroundColour(wxColour(*wxWHITE));
    txtLogs->SetForegroundColour(wxColour(*wxBLACK));
    
    logsSizer->Add(lblLogsHeader, 0, wxALL, 15);
    logsSizer->Add(txtLogs, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
    logsBgPanel->SetSizer(logsSizer);
    
    wxPanel* statsBgPanel = new wxPanel(pageDashboard, wxID_ANY);
    statsBgPanel->SetBackgroundColour(wxColour("#E8F1F2"));
    wxGridSizer* gridSizer = new wxGridSizer(2, 2, 10, 10);
    
    statSessions = new wxStaticText(statsBgPanel, wxID_ANY, "Active Sessions: 0");
    statErrors = new wxStaticText(statsBgPanel, wxID_ANY, "Errors: 0");
    statTools = new wxStaticText(statsBgPanel, wxID_ANY, "Tools Calls: 0");
    statUptime = new wxStaticText(statsBgPanel, wxID_ANY, "Uptime: 0");
    
    statSessions->SetFont(wxFontInfo(12));
    statErrors->SetFont(wxFontInfo(12));
    statTools->SetFont(wxFontInfo(12));
    statUptime->SetFont(wxFontInfo(12));

    statSessions->SetForegroundColour(wxColour("#13293D"));
    statErrors->SetForegroundColour(wxColour("#13293D"));
    statTools->SetForegroundColour(wxColour("#13293D"));
    statUptime->SetForegroundColour(wxColour("#13293D"));

    gridSizer->Add(statSessions, 0, wxALL, 15);
    gridSizer->Add(statErrors, 0, wxALL, 15);
    gridSizer->Add(statTools, 0, wxALL, 15);
    gridSizer->Add(statUptime, 0, wxALL, 15);
    statsBgPanel->SetSizer(gridSizer);
    
    dashSizer->Add(btnStartStop, 0, wxALIGN_RIGHT | wxBOTTOM, 20);
    dashSizer->Add(logsBgPanel, 1, wxEXPAND | wxBOTTOM, 20);
    dashSizer->Add(statsBgPanel, 0, wxEXPAND);
    pageDashboard->SetSizer(dashSizer);
    contentBook->AddPage(pageDashboard, "Dashboard");

    // --- Page: Workspace ---
    pageWorkspace = new wxPanel(contentBook, wxID_ANY);
    pageWorkspace->SetBackgroundColour(wxColour("#F4F7F6"));
    wxBoxSizer* wsSizer = new wxBoxSizer(wxVERTICAL);
    
    lblWorkspaceTitle = new wxStaticText(pageWorkspace, wxID_ANY, "Select Workspace Directory");
    lblWorkspaceTitle->SetFont(wxFontInfo(16).Bold());
    lblWorkspaceTitle->SetForegroundColour(wxColour("#13293D"));
    
    wxBoxSizer* pathSizer = new wxBoxSizer(wxHORIZONTAL);
    txtWorkspacePath = new wxTextCtrl(pageWorkspace, wxID_ANY, currentWorkspace, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    txtWorkspacePath->SetBackgroundColour(wxColour(*wxWHITE));
    txtWorkspacePath->SetForegroundColour(wxColour(*wxBLACK));
    
    btnBrowseWorkspace = new CustomButton(pageWorkspace, ID_BTN_BROWSE_WS, "...", wxDefaultPosition, wxSize(50, 35));
    btnBrowseWorkspace->SetBackgroundColour(wxColour("#E8F1F2"));
    btnBrowseWorkspace->SetForegroundColour(wxColour("#13293D"));
    
    pathSizer->Add(txtWorkspacePath, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
    pathSizer->Add(btnBrowseWorkspace, 0, wxALIGN_CENTER_VERTICAL);
    
    chkCreateMissing = new wxCheckBox(pageWorkspace, wxID_ANY, "Create if missing");
    chkCreateMissing->SetValue(true);
    chkCreateMissing->SetForegroundColour(wxColour("#13293D"));
    
    btnSaveWorkspace = new CustomButton(pageWorkspace, ID_BTN_SAVE_WS, "SAVE", wxDefaultPosition, wxSize(200, 50));
    btnSaveWorkspace->SetBackgroundColour(wxColour("#158E8A"));
    btnSaveWorkspace->SetForegroundColour(wxColour(*wxWHITE));
    
    wsSizer->Add(lblWorkspaceTitle, 0, wxALL, 15);
    wsSizer->Add(pathSizer, 0, wxEXPAND | wxALL, 15);
    wsSizer->Add(chkCreateMissing, 0, wxALL, 15);
    wsSizer->Add(btnSaveWorkspace, 0, wxALIGN_CENTER | wxALL, 20);
    pageWorkspace->SetSizer(wsSizer);
    contentBook->AddPage(pageWorkspace, "Workspace");
    
    // --- Page: Language ---
    pageLanguage = new wxPanel(contentBook, wxID_ANY);
    pageLanguage->SetBackgroundColour(wxColour("#F4F7F6"));
    wxBoxSizer* langSizer = new wxBoxSizer(wxVERTICAL);
    
    btnLangRu = new CustomButton(pageLanguage, ID_BTN_LANG_RU, wxString::FromUTF8("Русский (RU)"), wxDefaultPosition, wxSize(250, 45));
    btnLangEn = new CustomButton(pageLanguage, ID_BTN_LANG_EN, "English (EN)", wxDefaultPosition, wxSize(250, 45));
    btnLangRu->SetFont(btnFont);
    btnLangEn->SetFont(btnFont);
    btnLangRu->SetBackgroundColour(wxColour("#E8F1F2"));
    btnLangEn->SetBackgroundColour(wxColour("#E8F1F2"));
    btnLangRu->SetForegroundColour(wxColour("#13293D"));
    btnLangEn->SetForegroundColour(wxColour("#13293D"));
    
    langSizer->Add(btnLangRu, 0, wxALIGN_CENTER | wxALL, 10);
    langSizer->Add(btnLangEn, 0, wxALIGN_CENTER | wxALL, 10);
    pageLanguage->SetSizer(langSizer);
    contentBook->AddPage(pageLanguage, "Language");

    // --- Page: Network ---
    pageNetwork = new wxPanel(contentBook, wxID_ANY);
    pageNetwork->SetBackgroundColour(wxColour("#F4F7F6"));
    wxBoxSizer* netSizer = new wxBoxSizer(wxVERTICAL);
    
    // Server Bind Info
    wxBoxSizer* bindSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblBindTitle = new wxStaticText(pageNetwork, wxID_ANY, "SERVER BIND INFO:");
    lblBindTitle->SetFont(wxFontInfo(12).Bold());
    lblBindTitle->SetForegroundColour(wxColour("#13293D"));
    lblBindInfo = new wxStaticText(pageNetwork, wxID_ANY, "Bind: 0.0.0.0 | Port: 3000 | Transport: SSE | Path: /sse");
    lblBindInfo->SetForegroundColour(wxColour("#13293D"));
    bindSizer->Add(lblBindTitle, 0, wxRIGHT, 10);
    bindSizer->Add(lblBindInfo, 0);
    
    // Connection Info
    wxBoxSizer* connSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* lblConnTitle = new wxStaticText(pageNetwork, wxID_ANY, "CONNECTION INFO");
    lblConnTitle->SetFont(wxFontInfo(14).Bold());
    lblConnTitle->SetForegroundColour(wxColour("#13293D"));
    
    wxBoxSizer* locSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblLoc = new wxStaticText(pageNetwork, wxID_ANY, "Local URL: ");
    lblLoc->SetForegroundColour(wxColour("#13293D"));
    lblLocalUrl = new wxStaticText(pageNetwork, wxID_ANY, "http://localhost:3000/sse");
    lblLocalUrl->SetForegroundColour(wxColour("#158E8A"));
    locSizer->Add(lblLoc, 0, wxRIGHT, 5);
    locSizer->Add(lblLocalUrl, 0);
    
    wxBoxSizer* extSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblExt = new wxStaticText(pageNetwork, wxID_ANY, "External URL: ");
    lblExt->SetForegroundColour(wxColour("#13293D"));
    lblExternalUrl = new wxStaticText(pageNetwork, wxID_ANY, "Not Detected");
    lblExternalUrl->SetForegroundColour(wxColour("#158E8A"));
    extSizer->Add(lblExt, 0, wxRIGHT, 5);
    extSizer->Add(lblExternalUrl, 0);
    
    wxBoxSizer* ipSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblIp = new wxStaticText(pageNetwork, wxID_ANY, "Detected Public IP: ");
    lblIp->SetForegroundColour(wxColour("#13293D"));
    lblPublicIP = new wxStaticText(pageNetwork, wxID_ANY, "Unknown");
    lblPublicIP->SetForegroundColour(wxColour("#13293D"));
    ipSizer->Add(lblIp, 0, wxRIGHT, 5);
    ipSizer->Add(lblPublicIP, 0);
    
    wxBoxSizer* domSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblDom = new wxStaticText(pageNetwork, wxID_ANY, "Custom Base URL: ");
    lblDom->SetForegroundColour(wxColour("#13293D"));
    txtCustomDomain = new wxTextCtrl(pageNetwork, ID_TXT_CUSTOM_DOMAIN, "", wxDefaultPosition, wxSize(200, -1));
    txtCustomDomain->SetHint("https://example.com");
    txtCustomDomain->SetBackgroundColour(wxColour(*wxWHITE));
    txtCustomDomain->SetForegroundColour(wxColour(*wxBLACK));
    domSizer->Add(lblDom, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    domSizer->Add(txtCustomDomain, 0);
    
    connSizer->Add(lblConnTitle, 0, wxBOTTOM, 10);
    connSizer->Add(locSizer, 0, wxBOTTOM, 5);
    connSizer->Add(ipSizer, 0, wxBOTTOM, 5);
    connSizer->Add(domSizer, 0, wxBOTTOM, 5);
    connSizer->Add(extSizer, 0, wxBOTTOM, 10);
    
    // Tokens
    wxBoxSizer* tokenSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* lblTokenTitle = new wxStaticText(pageNetwork, wxID_ANY, "ACCESS TOKENS");
    lblTokenTitle->SetFont(wxFontInfo(14).Bold());
    lblTokenTitle->SetForegroundColour(wxColour("#13293D"));
    
    gridTokens = new wxGrid(pageNetwork, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
    gridTokens->CreateGrid(0, 5);
    gridTokens->SetColLabelValue(0, "Name");
    gridTokens->SetColLabelValue(1, "ID");
    gridTokens->SetColLabelValue(2, "Token");
    gridTokens->SetColLabelValue(3, "Created");
    gridTokens->SetColLabelValue(4, "Status");
    gridTokens->SetColSize(0, 120);
    gridTokens->SetColSize(1, 150);
    gridTokens->SetColSize(2, 200);
    gridTokens->SetColSize(3, 160);
    gridTokens->SetColSize(4, 80);
    gridTokens->HideRowLabels();
    gridTokens->SetDefaultCellBackgroundColour(wxColour(*wxWHITE));
    gridTokens->SetDefaultCellTextColour(wxColour(*wxBLACK));
    gridTokens->SetLabelBackgroundColour(wxColour("#E0E0E0"));
    gridTokens->SetLabelTextColour(wxColour(*wxBLACK));
    
    wxBoxSizer* tbtnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnCreateToken = new CustomButton(pageNetwork, ID_BTN_CREATE_TOKEN, "Create Token", wxDefaultPosition, wxSize(120, 30));
    btnDeleteToken = new CustomButton(pageNetwork, ID_BTN_DELETE_TOKEN, "Delete Token", wxDefaultPosition, wxSize(120, 30));
    btnToggleToken = new CustomButton(pageNetwork, ID_BTN_TOGGLE_TOKEN, "Toggle Active", wxDefaultPosition, wxSize(120, 30));
    btnCreateToken->SetBackgroundColour(wxColour("#158E8A"));
    btnCreateToken->SetForegroundColour(wxColour(*wxWHITE));
    btnDeleteToken->SetBackgroundColour(wxColour("#D62828"));
    btnDeleteToken->SetForegroundColour(wxColour(*wxWHITE));
    btnToggleToken->SetBackgroundColour(wxColour("#E9C46A"));
    btnToggleToken->SetForegroundColour(wxColour("#13293D"));
    tbtnSizer->Add(btnCreateToken, 0, wxRIGHT, 10);
    tbtnSizer->Add(btnDeleteToken, 0, wxRIGHT, 10);
    tbtnSizer->Add(btnToggleToken, 0);
    
    tokenSizer->Add(lblTokenTitle, 0, wxBOTTOM, 10);
    tokenSizer->Add(gridTokens, 1, wxEXPAND | wxBOTTOM, 5);
    tokenSizer->Add(tbtnSizer, 0);
    
    // Connection string & Checker
    wxBoxSizer* botSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxBoxSizer* connStrSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* lblStrTitle = new wxStaticText(pageNetwork, wxID_ANY, "CONNECTION STRING");
    lblStrTitle->SetFont(wxFontInfo(12).Bold());
    lblStrTitle->SetForegroundColour(wxColour("#13293D"));
    txtConnString = new wxTextCtrl(pageNetwork, wxID_ANY, "", wxDefaultPosition, wxSize(300, 80), wxTE_MULTILINE | wxTE_READONLY);
    txtConnString->SetBackgroundColour(wxColour(*wxWHITE));
    txtConnString->SetForegroundColour(wxColour(*wxBLACK));
    btnCopyConn = new CustomButton(pageNetwork, ID_BTN_COPY_CONN, "Copy MCP Connection", wxDefaultPosition, wxSize(200, 40));
    btnCopyConn->SetBackgroundColour(wxColour("#13293D"));
    btnCopyConn->SetForegroundColour(wxColour(*wxWHITE));
    connStrSizer->Add(lblStrTitle, 0, wxBOTTOM, 5);
    connStrSizer->Add(txtConnString, 0, wxBOTTOM, 5);
    connStrSizer->Add(btnCopyConn, 0);
    
    wxBoxSizer* chkSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* lblChkTitle = new wxStaticText(pageNetwork, wxID_ANY, "NETWORK CHECKER");
    lblChkTitle->SetFont(wxFontInfo(11).Bold());
    lblChkTitle->SetForegroundColour(wxColour("#13293D"));
    btnCheckNetwork = new CustomButton(pageNetwork, ID_BTN_CHECK_NETWORK, "Check Network", wxDefaultPosition, wxSize(180, 40));
    btnCheckNetwork->SetBackgroundColour(wxColour("#158E8A"));
    btnCheckNetwork->SetForegroundColour(wxColour(*wxWHITE));
    btnCheckNetwork->SetFont(wxFontInfo(10).Bold());
    
    lblCheckResult = new CustomButton(pageNetwork, wxID_ANY, "Ready to check network", wxDefaultPosition, wxSize(250, 40));
    lblCheckResult->SetFont(wxFontInfo(11).Bold());
    lblCheckResult->SetForegroundColour(wxColour("#D62828"));
    lblCheckResult->SetBackgroundColour(wxColour(*wxWHITE));
    lblCheckResult->SetHoverColour(wxColour(*wxWHITE));
    
    wxBoxSizer* chkRow = new wxBoxSizer(wxHORIZONTAL);
    chkRow->Add(btnCheckNetwork, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 15);
    chkRow->Add(lblCheckResult, 0, wxALIGN_CENTER_VERTICAL);
    
    chkSizer->Add(lblChkTitle, 0, wxBOTTOM, 5);
    chkSizer->Add(chkRow, 0, wxBOTTOM, 10);
    
    botSizer->Add(connStrSizer, 1, wxEXPAND | wxRIGHT, 20);
    botSizer->Add(chkSizer, 1, wxEXPAND);
    
    // Assembly
    netSizer->Add(bindSizer, 0, wxALL, 15);
    netSizer->Add(new wxStaticLine(pageNetwork), 0, wxEXPAND | wxLEFT | wxRIGHT, 15);
    netSizer->Add(connSizer, 0, wxALL, 15);
    netSizer->Add(new wxStaticLine(pageNetwork), 0, wxEXPAND | wxLEFT | wxRIGHT, 15);
    netSizer->Add(tokenSizer, 1, wxEXPAND | wxALL, 15);
    netSizer->Add(new wxStaticLine(pageNetwork), 0, wxEXPAND | wxLEFT | wxRIGHT, 15);
    netSizer->Add(botSizer, 0, wxEXPAND | wxALL, 15);
    
    pageNetwork->SetSizer(netSizer);
    contentBook->AddPage(pageNetwork, "Network");

    RefreshTokenList();
    RefreshConnectionUrls();

    // --- Page: Stub ---
    pageStub = new wxPanel(contentBook, wxID_ANY);
    pageStub->SetBackgroundColour(wxColour("#F4F7F6"));
    wxBoxSizer* stubSizer = new wxBoxSizer(wxVERTICAL);
    lblStubMessage = new wxStaticText(pageStub, wxID_ANY, "This section is in development");
    lblStubMessage->SetFont(wxFontInfo(14));
    lblStubMessage->SetForegroundColour(wxColour("#13293D"));
    stubSizer->Add(lblStubMessage, 0, wxALIGN_CENTER | wxALL, 50);
    pageStub->SetSizer(stubSizer);
    contentBook->AddPage(pageStub, "Stub");

    // Assemble Main Panel
    rightSizer->Add(titleBox, 0, wxEXPAND | wxALL, 20);
    rightSizer->Add(contentBook, 1, wxEXPAND | wxALL, 20);
    mainPanel->SetSizer(rightSizer);

    // Assemble main frame
    mainSizer->Add(sidebarPanel, 0, wxEXPAND);
    mainSizer->Add(mainPanel, 1, wxEXPAND);
    this->SetSizer(mainSizer);
    
    // Setup initial routing explicitly
    UpdateSidebarSelection(btnDashboard);
    contentBook->ChangeSelection(0);
    
    // Bind server event
    Bind(wxEVT_SERVER_LOG, &Windows::OnServerLog, this);
}

void Windows::ApplyTealTheme() {
    wxColour sidebarBtnBg = wxColour("#E8F1F2");
    wxColour sidebarBtnFg = wxColour("#13293D");
    
    btnDashboard->SetBackgroundColour(sidebarBtnBg);
    btnDashboard->SetForegroundColour(sidebarBtnFg);
    btnWorkspace->SetBackgroundColour(sidebarBtnBg);
    btnWorkspace->SetForegroundColour(sidebarBtnFg);
    btnSettings->SetBackgroundColour(sidebarBtnBg);
    btnSettings->SetForegroundColour(sidebarBtnFg);
    btnLanguage->SetBackgroundColour(sidebarBtnBg);
    btnLanguage->SetForegroundColour(sidebarBtnFg);
    btnNetwork->SetBackgroundColour(sidebarBtnBg);
    btnNetwork->SetForegroundColour(sidebarBtnFg);
    btnLogs->SetBackgroundColour(sidebarBtnBg);
    btnLogs->SetForegroundColour(sidebarBtnFg);
}

void Windows::UpdateLanguage() {
    auto& lang = LanguageManager::Get();
    btnDashboard->SetLabel(lang.GetString("DASHBOARD"));
    btnWorkspace->SetLabel(lang.GetString("WORKSPACE"));
    btnSettings->SetLabel(lang.GetString("SETTINGS"));
    btnLanguage->SetLabel(lang.GetString("LANGUAGE"));
    btnNetwork->SetLabel(lang.GetString("NETWORK"));
    btnLogs->SetLabel(lang.GetString("LOGS"));

    lblManager->SetLabel(lang.GetString("SERVER_MANAGER"));
    lblStatus->SetLabel(lang.GetString("STATUS"));
    
    if (isServerRunning) {
        lblStatusValue->SetLabel(lang.GetString("ONLINE"));
        btnStartStop->SetLabel(lang.GetString("STOP_BTN"));
    } else {
        lblStatusValue->SetLabel(lang.GetString("OFFLINE"));
        btnStartStop->SetLabel(lang.GetString("START_BTN"));
    }

    lblLogsHeader->SetLabel(lang.GetString("LOGS_HEADER"));
    lblWorkspaceStatus->SetLabel(lang.GetString("CURRENT_WORKSPACE"));
    lblWorkspaceValue->SetLabel(wxString::FromUTF8(currentWorkspace.c_str()));
    statSessions->SetLabel(lang.GetString("STATS_SESSIONS") + wxString("0"));
    statErrors->SetLabel(lang.GetString("STATS_ERRORS") + wxString("0"));
    statTools->SetLabel(lang.GetString("STATS_TOOLS") + wxString("0"));
    statUptime->SetLabel(lang.GetString("STATS_UPTIME") + wxString::Format("%ds", uptimeSeconds.load()));

    lblWorkspaceTitle->SetLabel(lang.GetString("WORKSPACE_TITLE"));
    
    chkCreateMissing->SetLabel(lang.GetString("CREATE_IF_MISSING"));
    chkCreateMissing->InvalidateBestSize(); // Fix truncation on Linux
    
    // Network tab instructions removed, layout has its own labels

    lblStubMessage->SetLabel(lang.GetString("IN_DEV"));

    btnLangRu->SetBackgroundColour(lang.GetLanguage() == "RU" ? wxColour("#158E8A") : wxColour("#E8F1F2"));
    btnLangRu->SetForegroundColour(lang.GetLanguage() == "RU" ? wxColour(*wxWHITE) : wxColour("#13293D"));
    btnLangEn->SetBackgroundColour(lang.GetLanguage() == "EN" ? wxColour("#158E8A") : wxColour("#E8F1F2"));
    btnLangEn->SetForegroundColour(lang.GetLanguage() == "EN" ? wxColour(*wxWHITE) : wxColour("#13293D"));

    Layout();
}

void Windows::UpdateSidebarSelection(CustomButton* selected) {
    btnDashboard->SetSelected(btnDashboard == selected);
    btnWorkspace->SetSelected(btnWorkspace == selected);
    btnSettings->SetSelected(btnSettings == selected);
    btnLanguage->SetSelected(btnLanguage == selected);
    btnNetwork->SetSelected(btnNetwork == selected);
    btnLogs->SetSelected(btnLogs == selected);
}

void Windows::OnSidebarDashboard(wxCommandEvent& event) {
    UpdateSidebarSelection(btnDashboard);
    contentBook->ChangeSelection(0);
}
void Windows::OnSidebarWorkspace(wxCommandEvent& event) {
    UpdateSidebarSelection(btnWorkspace);
    contentBook->ChangeSelection(1);
}
void Windows::OnSidebarSettings(wxCommandEvent& event) {
    UpdateSidebarSelection(btnSettings);
    contentBook->ChangeSelection(4);
}
void Windows::OnSidebarLanguage(wxCommandEvent& event) {
    UpdateSidebarSelection(btnLanguage);
    contentBook->ChangeSelection(2);
}
void Windows::OnSidebarNetwork(wxCommandEvent& event) {
    UpdateSidebarSelection(btnNetwork);
    contentBook->ChangeSelection(3);
}
void Windows::OnSidebarLogs(wxCommandEvent& event) {
    UpdateSidebarSelection(btnLogs);
    contentBook->ChangeSelection(4); // Stub for now
}

void Windows::OnStartStop(wxCommandEvent& event) {
    auto& lang = LanguageManager::Get();
    if (!isServerRunning) {
        // Start Server
        isServerRunning = true;
        lblStatusValue->SetLabel(lang.GetString("ONLINE"));
        lblStatusValue->SetForegroundColour(wxColour("#2A9D8F"));
        btnStartStop->SetLabel(lang.GetString("STOP_BTN"));
        btnStartStop->SetBackgroundColour(wxColour("#D62828"));
        
        g_active_sessions = 0;
        g_tool_calls = 0;
        uptimeSeconds = 0;
        
        statUptime->SetLabel(lang.GetString("STATS_UPTIME") + wxString("0s"));
        statSessions->SetLabel(lang.GetString("STATS_SESSIONS") + wxString("0"));
        statTools->SetLabel(lang.GetString("STATS_TOOLS") + wxString("0"));
        
        m_timer->Start(1000); // 1 second intervals

        // Fetch active token
        std::string activeToken = "";
        auto tokens = NetworkUtils::LoadTokens();
        for (const auto& t : tokens) {
            if (t.active) {
                activeToken = t.raw_token;
                break;
            }
        }

        // Launch in background
        serverThread = std::thread([this, activeToken]() {
            run_mcp_server(3000, currentWorkspace, "mcp", activeToken);
        });
    } else {
        // Stop Server
        stop_mcp_server();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        isServerRunning = false;
        m_timer->Stop();
        
        lblStatusValue->SetLabel(lang.GetString("OFFLINE"));
        lblStatusValue->SetForegroundColour(wxColour("#D62828"));
        btnStartStop->SetLabel(lang.GetString("START_BTN"));
        btnStartStop->SetBackgroundColour(wxColour("#158E8A"));
    }
}

void Windows::OnServerLog(wxThreadEvent& event) {
    txtLogs->AppendText(event.GetString() + "\n");
}

void Windows::OnTimer(wxTimerEvent& event) {
    if (isServerRunning) {
        uptimeSeconds++;
        auto& lang = LanguageManager::Get();
        statUptime->SetLabel(lang.GetString("STATS_UPTIME") + wxString::Format("%ds", uptimeSeconds.load()));
        statSessions->SetLabel(lang.GetString("STATS_SESSIONS") + wxString::Format("%d", g_active_sessions.load()));
        statTools->SetLabel(lang.GetString("STATS_TOOLS") + wxString::Format("%d", g_tool_calls.load()));
    }
}

void Windows::OnBrowseWorkspace(wxCommandEvent& event) {
    wxDirDialog dialog(this, "Select Workspace Directory", currentWorkspace, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) {
        txtWorkspacePath->SetValue(dialog.GetPath());
    }
}

void Windows::OnSaveWorkspace(wxCommandEvent& event) {
    wxString newPath = txtWorkspacePath->GetValue();
    if (!std::filesystem::exists(newPath.ToStdString())) {
        if (chkCreateMissing->GetValue()) {
            std::error_code ec;
            if (!std::filesystem::create_directories(newPath.ToStdString(), ec)) {
                wxMessageBox("Failed to create directory: " + ec.message(), "Error", wxICON_ERROR);
                return;
            }
        } else {
            wxMessageBox("Directory does not exist", "Error", wxICON_ERROR);
            return;
        }
    }
    
    currentWorkspace = newPath.ToStdString();
    lblWorkspaceValue->SetLabel(wxString::FromUTF8(currentWorkspace.c_str()));
    Layout();
    
    wxThreadEvent* logEvent = new wxThreadEvent(wxEVT_SERVER_LOG);
    logEvent->SetString("[GUI] Workspace set to: " + currentWorkspace);
    wxQueueEvent(this, logEvent);
    
    wxMessageBox("Workspace updated successfully!", "Success", wxICON_INFORMATION);
}

void Windows::OnSelectLangRu(wxCommandEvent& event) {
    LanguageManager::Get().SetLanguage("RU");
    UpdateLanguage();
}

void Windows::OnSelectLangEn(wxCommandEvent& event) {
    LanguageManager::Get().SetLanguage("EN");
    UpdateLanguage();
}

void Windows::RefreshTokenList() {
    if (gridTokens->GetNumberRows() > 0) {
        gridTokens->DeleteRows(0, gridTokens->GetNumberRows());
    }
    auto tokens = NetworkUtils::LoadTokens();
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        gridTokens->AppendRows(1);
        const auto& t = tokens[i];
        gridTokens->SetCellValue(i, 0, t.name.empty() ? "New Token" : t.name);
        gridTokens->SetCellValue(i, 1, t.id);
        gridTokens->SetCellValue(i, 2, t.raw_token);
        gridTokens->SetCellValue(i, 3, t.creation_date);
        gridTokens->SetCellValue(i, 4, t.active ? "Active" : "Revoked");
        
        // Readonly for created and status
        gridTokens->SetReadOnly(i, 3, true);
        gridTokens->SetReadOnly(i, 4, true);
        
        if (!t.active) {
            gridTokens->SetCellTextColour(i, 4, wxColour("#D62828"));
        } else {
            gridTokens->SetCellTextColour(i, 4, wxColour("#2A9D8F"));
        }
    }
}

void Windows::RefreshConnectionUrls() {
    wxString custom = txtCustomDomain->GetValue();
    std::string baseUrl;
    if (!custom.IsEmpty()) {
        baseUrl = custom.ToStdString();
        if (baseUrl.back() == '/') baseUrl.pop_back();
    } else {
        std::string ip = lblPublicIP->GetLabel().ToStdString();
        if (ip == "Unknown" || ip.empty()) {
            baseUrl = "http://localhost:3000";
        } else {
            baseUrl = "http://" + ip + ":3000";
        }
    }
    
    std::string sseUrl = baseUrl + "/sse";
    lblExternalUrl->SetLabel(sseUrl);

    // Find active token or use placeholder
    auto tokens = NetworkUtils::LoadTokens();
    std::string activeToken = "<no_active_tokens_found>";
    for (const auto& t : tokens) {
        if (t.active) {
            activeToken = t.raw_token;
            break;
        }
    }
    
    std::string urlWithToken = sseUrl + "?token=" + activeToken;
    
    // Create a beautiful JSON string that the user can copy
    nlohmann::json connInfo = {
        {"mcpServers", {
            {"mcp-server", {
                {"command", "curl"}, // or your custom launch command
                {"url", urlWithToken},
                {"headers", {
                    {"Authorization", "Bearer " + activeToken}
                }}
            }}
        }}
    };
    
    txtConnString->SetValue(connInfo.dump(2));
}

void Windows::OnCreateToken(wxCommandEvent& event) {
    std::string rawToken = NetworkUtils::GenerateToken();
    
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    TokenInfo info;
    info.id = "tok_" + std::to_string(std::time(nullptr));
    info.name = "New Token";
    info.raw_token = rawToken;
    info.creation_date = oss.str();
    info.active = true;
    
    NetworkUtils::AddToken(info);
    RefreshTokenList();
    
    wxString msg = "New token created.\n\n" + rawToken;
    wxMessageBox(msg, "Token Created", wxOK | wxICON_INFORMATION);
    RefreshConnectionUrls(); // Update JSON string if needed
}

void Windows::SaveTokensFromGrid() {
    std::vector<TokenInfo> tokens;
    for (int i = 0; i < gridTokens->GetNumberRows(); ++i) {
        TokenInfo t;
        t.name = gridTokens->GetCellValue(i, 0).ToStdString();
        t.id = gridTokens->GetCellValue(i, 1).ToStdString();
        t.raw_token = gridTokens->GetCellValue(i, 2).ToStdString();
        t.creation_date = gridTokens->GetCellValue(i, 3).ToStdString();
        t.active = (gridTokens->GetCellValue(i, 4) == "Active");
        tokens.push_back(t);
    }
    NetworkUtils::SaveTokens(tokens);
}

void Windows::OnTokenCellChanged(wxGridEvent& event) {
    SaveTokensFromGrid();
    RefreshConnectionUrls();
}

void Windows::OnDeleteToken(wxCommandEvent& event) {
    wxArrayInt selRows = gridTokens->GetSelectedRows();
    int row = -1;
    if (selRows.IsEmpty()) {
        row = gridTokens->GetGridCursorRow();
    } else {
        row = selRows[0];
    }
    
    if (row < 0 || row >= gridTokens->GetNumberRows()) {
        wxMessageBox("Please select a token to delete.", "Error", wxICON_ERROR);
        return;
    }
    
    wxString id = gridTokens->GetCellValue(row, 1);
    NetworkUtils::DeleteToken(id.ToStdString());
    RefreshTokenList();
    RefreshConnectionUrls();
}

void Windows::OnToggleToken(wxCommandEvent& event) {
    wxArrayInt selRows = gridTokens->GetSelectedRows();
    int row = -1;
    if (selRows.IsEmpty()) {
        row = gridTokens->GetGridCursorRow();
    } else {
        row = selRows[0];
    }
    
    if (row < 0 || row >= gridTokens->GetNumberRows()) {
        wxMessageBox("Please select a token to toggle.", "Error", wxICON_ERROR);
        return;
    }
    
    wxString id = gridTokens->GetCellValue(row, 1);
    NetworkUtils::ToggleTokenActive(id.ToStdString());
    RefreshTokenList();
    RefreshConnectionUrls();
}

void Windows::OnCopyConnection(wxCommandEvent& event) {
    if (event.GetId() == ID_BTN_COPY_CONN) { // Only copy if actually clicked
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(txtConnString->GetValue()));
            wxTheClipboard->Close();
            wxMessageBox("Connection string copied to clipboard!", "Copied", wxICON_INFORMATION);
        }
    }
}

void Windows::OnCheckNetwork(wxCommandEvent& event) {
    lblCheckResult->SetLabel("Checking...");
    lblCheckResult->SetForegroundColour(wxColour("#13293D"));
    lblCheckResult->Refresh();
    pageNetwork->Update(); // Force paint immediately
    wxYield(); // Allow GTK to draw
    
    std::cout << "[Check] Getting Public IP..." << std::endl;
    std::string ip = NetworkUtils::GetPublicIP();
    std::cout << "[Check] IP = " << ip << std::endl;
    lblPublicIP->SetLabel(ip);
    RefreshConnectionUrls();
    pageNetwork->Update();
    wxYield();
    
    bool localOk = NetworkUtils::CheckLocalAccess(3000);
    pageNetwork->Update();
    wxYield();

    std::cout << "[Check] Checking External Access..." << std::endl;
    bool extOk = false;
    if (ip != "Unknown") {
        extOk = NetworkUtils::CheckExternalAccess(ip, 3000);
    }
    std::cout << "[Check] Ext = " << extOk << std::endl;
    
    if (extOk) {
        lblCheckResult->SetLabel("Reachable externally");
        lblCheckResult->SetForegroundColour(wxColour("#2A9D8F")); // Teal/Green
    } else if (localOk) {
        lblCheckResult->SetLabel("Local only");
        lblCheckResult->SetForegroundColour(wxColour("#E9C46A")); // Yellow/Orange
    } else {
        lblCheckResult->SetLabel("Not reachable");
        lblCheckResult->SetForegroundColour(wxColour("#D62828")); // Red
    }
    
    lblCheckResult->Refresh();
    lblCheckResult->Update();
}

void Windows::OnCustomDomainChanged(wxCommandEvent& event) {
    RefreshConnectionUrls();
}

wxBEGIN_EVENT_TABLE(Windows, wxFrame)
    EVT_BUTTON(ID_BTN_DASHBOARD, Windows::OnSidebarDashboard)
    EVT_BUTTON(ID_BTN_WORKSPACE, Windows::OnSidebarWorkspace)
    EVT_BUTTON(ID_BTN_SETTINGS,  Windows::OnSidebarSettings)
    EVT_BUTTON(ID_BTN_LANGUAGE,  Windows::OnSidebarLanguage)
    EVT_BUTTON(ID_BTN_NETWORK,   Windows::OnSidebarNetwork)
    EVT_BUTTON(ID_BTN_LOGS,      Windows::OnSidebarLogs)
    EVT_BUTTON(ID_BTN_STARTSTOP, Windows::OnStartStop)
    EVT_TIMER(ID_TIMER,          Windows::OnTimer)
    EVT_BUTTON(ID_BTN_BROWSE_WS, Windows::OnBrowseWorkspace)
    EVT_BUTTON(ID_BTN_SAVE_WS,   Windows::OnSaveWorkspace)
    EVT_BUTTON(ID_BTN_LANG_RU,   Windows::OnSelectLangRu)
    EVT_BUTTON(ID_BTN_LANG_EN,   Windows::OnSelectLangEn)
    EVT_BUTTON(ID_BTN_CREATE_TOKEN, Windows::OnCreateToken)
    EVT_BUTTON(ID_BTN_DELETE_TOKEN, Windows::OnDeleteToken)
    EVT_BUTTON(ID_BTN_TOGGLE_TOKEN, Windows::OnToggleToken)
    EVT_GRID_CELL_CHANGED(Windows::OnTokenCellChanged)
    EVT_BUTTON(ID_BTN_COPY_CONN,    Windows::OnCopyConnection)
    EVT_BUTTON(ID_BTN_CHECK_NETWORK,Windows::OnCheckNetwork)
    EVT_TEXT(ID_TXT_CUSTOM_DOMAIN,  Windows::OnCustomDomainChanged)
wxEND_EVENT_TABLE()
