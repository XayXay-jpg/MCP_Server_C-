#include "windows.h"
#include "LanguageManager.h"
#include "../mcp/server.h"
#include "../mcp/utils.h"
#include "../mcp/network_utils.h"
#include "CustomButton.h"
#include "../mcp/settings_manager.h"
#include "../mcp/cluster_manager.h"
#include "icons.h"
#include "permissions_dialog.h"
#include <wx/mstream.h>
#include <wx/base64.h>
#include <wx/dcgraph.h>
#include <wx/statline.h>
#include <future>
#include <fstream>
#include <wx/textdlg.h>
#include <wx/notifmsg.h>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include "sys_stats.h"

std::string HashPassword(const std::string& input) {
    uint64_t hash = 14695981039346656037ull;
    for (char c : input) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ull;
    }
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

wxBitmap GetBitmapFromBase64(const std::string& b64) {
    wxLogNull logNo; // Suppress errors
    wxMemoryBuffer buf = wxBase64Decode(b64);
    if (buf.GetDataLen() == 0) return wxNullBitmap;
    wxMemoryInputStream stream(buf.GetData(), buf.GetDataLen());
    wxImage img(stream, wxBITMAP_TYPE_PNG);
    if (!img.IsOk()) return wxNullBitmap;
    return wxBitmap(img);
}
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <wx/simplebook.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <filesystem>

#if defined(__WXMSW__) || defined(_WIN32)
#include <wx/msw/registry.h>
#endif

const std::string APP_VERSION = "1.0.0-beta";

wxDEFINE_EVENT(wxEVT_SERVER_LOG, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_SERVER_NOTIFY, wxThreadEvent);

wxBEGIN_EVENT_TABLE(AnimatedLogo, wxPanel)
    EVT_PAINT(AnimatedLogo::OnPaint)
wxEND_EVENT_TABLE()

AnimatedLogo::AnimatedLogo(wxWindow* parent, const wxBitmap& bmp) 
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE) {
    m_img = bmp.ConvertToImage();
    m_progress = 1.0;
    m_bgColour = wxColour("#141417"); // sidebar color
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(bmp.GetWidth(), bmp.GetHeight()));
}

void AnimatedLogo::SetProgress(double progress) {
    m_progress = progress;
    int targetW = m_img.GetWidth() * progress;
    SetMinSize(wxSize(targetW, m_img.GetHeight()));
    Refresh();
}

void AnimatedLogo::OnPaint(wxPaintEvent& event) {
    wxPaintDC paintDC(this);
    wxGCDC dc(paintDC);
    dc.SetBrush(wxBrush(m_bgColour));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(GetClientRect());
    
    if (m_progress <= 0.0) return;
    
    double angle = (1.0 - m_progress) * M_PI; // Rotate up to 180 deg while hiding
    wxPoint center(m_img.GetWidth()/2, m_img.GetHeight()/2);
    wxImage rotated = m_img.Rotate(angle, center, true);
    
    if (!rotated.HasAlpha()) {
        rotated.InitAlpha();
    }
    
    for (int y = 0; y < rotated.GetHeight(); y++) {
        for (int x = 0; x < rotated.GetWidth(); x++) {
            rotated.SetAlpha(x, y, rotated.GetAlpha(x, y) * m_progress);
        }
    }
    
    int x = (GetClientSize().GetWidth() - rotated.GetWidth()) / 2;
    int y = (GetClientSize().GetHeight() - rotated.GetHeight()) / 2;
    dc.DrawBitmap(wxBitmap(rotated), x, y, true);
}

wxBEGIN_EVENT_TABLE(AnimatedText, wxPanel)
    EVT_PAINT(AnimatedText::OnPaint)
wxEND_EVENT_TABLE()

AnimatedText::AnimatedText(wxWindow* parent, const wxString& text)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE) {
    m_text = text;
    m_progress = 1.0;
    m_bgColour = wxColour("#141417"); // sidebar color
    m_font = wxFontInfo(20).Bold();
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    
    wxClientDC dc(this);
    dc.SetFont(m_font);
    wxSize textSize = dc.GetTextExtent(m_text);
    SetMinSize(textSize);
}

void AnimatedText::SetProgress(double progress) {
    m_progress = progress;
    wxClientDC dc(this);
    dc.SetFont(m_font);
    wxSize textSize = dc.GetTextExtent(m_text);
    SetMinSize(wxSize(textSize.GetWidth() * progress, textSize.GetHeight()));
    Refresh();
}

void AnimatedText::OnPaint(wxPaintEvent& event) {
    wxPaintDC paintDC(this);
    wxGCDC dc(paintDC);
    dc.SetBrush(wxBrush(m_bgColour));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(GetClientRect());
    
    if (m_progress <= 0.0) return;
    
    dc.SetFont(m_font);
    dc.SetTextForeground(wxColour(244, 244, 245, (unsigned char)(255 * m_progress))); // #F4F4F5 with alpha
    dc.DrawText(m_text, 0, 0);
}

enum {
    ID_BTN_SERVER_LOCAL = 1001,
    ID_BTN_CLUSTER,
    ID_BTN_TOOLS,
    ID_BTN_GLOBAL_SETTINGS,
    ID_BTN_TAB_OVERVIEW,
    ID_BTN_TAB_CONNECTIONS,
    ID_BTN_TAB_LOGS,
    ID_BTN_STARTSTOP,
    ID_TIMER,
    ID_BTN_BROWSE_WS,
    ID_BTN_SAVE_WS,
    ID_BTN_LANG_RU,
    ID_BTN_LANG_EN,
    ID_BTN_CREATE_TOKEN,
    ID_BTN_DELETE_TOKEN,
    ID_BTN_TOGGLE_TOKEN,
    ID_BTN_EDIT_PERMISSIONS,
    ID_BTN_COPY_TOKEN_URL,
    ID_BTN_CHECK_NETWORK,
    ID_TXT_CUSTOM_DOMAIN,
    ID_BTN_APPROVE_NODE,
    ID_BTN_REJECT_NODE,
    ID_BTN_ADD_NODE,
    ID_BTN_RECONNECT_NODE,
    ID_BTN_TOGGLE_COMPACT,
    ID_BTN_TOGGLE_THEME,
    ID_ANIM_TIMER
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

    g_notify_callback = [this](const std::string& title, const std::string& msg) {
        wxThreadEvent* event = new wxThreadEvent(wxEVT_SERVER_NOTIFY);
        event->SetString(title + "|||" + msg);
        wxQueueEvent(this, event);
    };
    
    g_refresh_cluster_callback = [this]() {
        CallAfter([this]() {
            RefreshNodesList();
        });
    };
    
    g_confirm_callback = [this](const std::string& title, const std::string& msg) -> bool {
        std::promise<bool> promise;
        auto future = promise.get_future();
        
        CallAfter([&promise, title, msg, this]() {
            int res = wxMessageBox(msg, title, wxYES_NO | wxICON_QUESTION, this);
            promise.set_value(res == wxYES);
        });
        
        return future.get();
    };
    
    Bind(wxEVT_SERVER_NOTIFY, &Windows::OnServerNotify, this);

    LanguageManager::Get().SetLanguage(SettingsManager::Get().language);

    SetupUI();
    ApplyTheme();
    UpdateLanguage();

    
    // Perform log retention cleanup
    std::string retention = SettingsManager::Get().logRetention;
    if (retention != "Keep All") {
        int days = (retention == "7 Days") ? 7 : 30;
        std::string logPath = currentWorkspace + "/logs";
        try {
            if (std::filesystem::exists(logPath)) {
                auto now = std::filesystem::file_time_type::clock::now();
                for (const auto& entry : std::filesystem::directory_iterator(logPath)) {
                    auto ftime = std::filesystem::last_write_time(entry);
                    auto diff = std::chrono::duration_cast<std::chrono::hours>(now - ftime).count();
                    if (diff > days * 24) {
                        std::filesystem::remove(entry.path());
                    }
                }
            }
        } catch (...) {}
    }
    m_timer = new wxTimer(this, ID_TIMER);
    m_animTimer = new wxTimer(this, ID_ANIM_TIMER);
    
    // Initialize Tray Icon
    trayIcon = new wxTaskBarIcon();
    wxIcon appIcon;
    appIcon.CopyFromBitmap(GetBitmapFromBase64(icon_app_png_base64));
    trayIcon->SetIcon(appIcon, "MCP Server Manager");
    
    Bind(wxEVT_CLOSE_WINDOW, &Windows::OnCloseWindow, this);
    trayIcon->Bind(wxEVT_TASKBAR_LEFT_DCLICK, &Windows::OnTrayIconActivated, this);
    
    // Check App Lock
    if (SettingsManager::Get().appLock && !SettingsManager::Get().appPasswordHash.empty()) {
        wxPasswordEntryDialog dialog(nullptr, "Please enter your Master Password to unlock MCP Server Manager", "App Lock");
        if (dialog.ShowModal() == wxID_OK) {
            if (HashPassword(dialog.GetValue().ToStdString()) != SettingsManager::Get().appPasswordHash) {
                wxMessageBox("Incorrect password.", "Error", wxICON_ERROR);
                exit(1);
            }
        } else {
            exit(0);
        }
    }
    
    if (SettingsManager::Get().autoStartServer) {
        wxCommandEvent dummy;
        OnStartStop(dummy);
    }
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
    sidebarPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(240, -1));
    sidebarPanel->SetBackgroundColour(wxColour("#141417")); // Surface Elevated
    
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxFileName f(exePath);
    wxString rootPath = f.GetPath() + "/../";

    // Sidebar Logo
    wxBoxSizer* logoSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxBitmap logoBmp = GetBitmapFromBase64(icon_logo_small_png_base64);
    logoImg = new AnimatedLogo(sidebarPanel, logoBmp);
    logoSizer->Add(logoImg, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    
    logoText = new AnimatedText(sidebarPanel, "MCP");
    logoSizer->Add(logoText, 0, wxALIGN_CENTER_VERTICAL);
    
    logoSizer->AddStretchSpacer();
    
    btnToggleTheme = new wxBitmapButton(sidebarPanel, ID_BTN_TOGGLE_THEME, wxNullBitmap, wxDefaultPosition, wxSize(24,24), wxBORDER_NONE);
    btnToggleTheme->SetBackgroundColour(wxColour("#141417"));
    // Since we don't have a theme icon, just leave it blank or we could draw text on it later
    logoSizer->Add(btnToggleTheme, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    
    wxBitmap bmpCompact = GetBitmapFromBase64(icon_compact_png_base64);
    compactIconImg = bmpCompact.ConvertToImage();
    btnToggleCompact = new wxBitmapButton(sidebarPanel, ID_BTN_TOGGLE_COMPACT, bmpCompact, wxDefaultPosition, wxSize(24,24), wxBORDER_NONE);
    btnToggleCompact->SetBackgroundColour(wxColour("#141417"));
    logoSizer->Add(btnToggleCompact, 0, wxALIGN_CENTER_VERTICAL);
    
    sidebarSizer->Add(logoSizer, 0, wxEXPAND | wxALL, 20);

    // Left Sidebar Items
    btnServerLocal = new CustomButton(sidebarPanel, ID_BTN_SERVER_LOCAL, _("Local Server"));
    btnCluster = new CustomButton(sidebarPanel, ID_BTN_CLUSTER, _("Cluster Nodes"));
    btnTools = new CustomButton(sidebarPanel, ID_BTN_TOOLS, _("Tools"));
    btnGlobalSettings = new CustomButton(sidebarPanel, ID_BTN_GLOBAL_SETTINGS, _("Global Settings"));

    // Ensure they have decent height
    btnServerLocal->SetMinSize(wxSize(-1, 40));
    btnCluster->SetMinSize(wxSize(-1, 40));
    btnTools->SetMinSize(wxSize(-1, 40));
    btnGlobalSettings->SetMinSize(wxSize(-1, 40));

    btnServerLocal->SetIndent(15);
    btnCluster->SetIndent(15);
    btnTools->SetIndent(15);
    btnGlobalSettings->SetIndent(15);

    sidebarSizer->Add(btnServerLocal, 0, wxEXPAND | wxBOTTOM, 5);
    sidebarSizer->Add(btnCluster, 0, wxEXPAND | wxBOTTOM, 5);
    sidebarSizer->Add(btnTools, 0, wxEXPAND | wxBOTTOM, 5);
    sidebarSizer->Add(btnGlobalSettings, 0, wxEXPAND | wxBOTTOM, 5);

    // Set Icons
    wxInitAllImageHandlers();
    wxBitmap serverIcon = GetBitmapFromBase64(icon_server_png_base64);
    btnServerLocal->SetIcon(serverIcon);
    
    wxBitmap clusterIcon = GetBitmapFromBase64(icon_cluster_png_base64);
    btnCluster->SetIcon(clusterIcon);

    wxBitmap toolsIcon = GetBitmapFromBase64(icon_tools_png_base64);
    btnTools->SetIcon(toolsIcon);

    wxBitmap settingsIcon = GetBitmapFromBase64(icon_settings_new_png_base64);
    btnGlobalSettings->SetIcon(settingsIcon);

    wxBitmap appIconBmp = GetBitmapFromBase64(icon_logo_small_png_base64);
    wxIcon appIcon;
    appIcon.CopyFromBitmap(appIconBmp);
    this->SetIcon(appIcon);

    wxFont btnFont(wxFontInfo(10).Bold());
    btnServerLocal->SetFont(btnFont);
    btnCluster->SetFont(btnFont);
    btnTools->SetFont(btnFont);
    btnGlobalSettings->SetFont(btnFont);

    sidebarSizer->AddStretchSpacer();
    sidebarSizer->Add(btnGlobalSettings, 0, wxEXPAND | wxBOTTOM, 20);

    sidebarPanel->SetSizer(sidebarSizer);

    // Main Area
    mainPanel = new wxPanel(this, wxID_ANY);
    mainPanel->SetBackgroundColour(wxColour("#0A0A0B")); // Surface
    
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    auto& lang = LanguageManager::Get();
    
    // Root Book to switch between Server and Settings
    rootBook = new SlideBook(mainPanel, wxID_ANY);
    rootBook->SetSlideDirection(SlideBook::SLIDE_VERTICAL);
    
    // === PAGE 0: SERVER CONTAINER ===
    serverContainer = new wxPanel(rootBook, wxID_ANY);
    serverContainer->SetBackgroundColour(wxColour("#0A0A0B"));
    wxBoxSizer* serverSizer = new wxBoxSizer(wxVERTICAL);

    // Header Panel
    headerPanel = new wxPanel(serverContainer, wxID_ANY);
    headerPanel->SetBackgroundColour(wxColour("#0A0A0B"));
    wxBoxSizer* headerSizer = new wxBoxSizer(wxVERTICAL);
    
    wxBoxSizer* titleBox = new wxBoxSizer(wxHORIZONTAL);
    lblManager = new wxStaticText(headerPanel, wxID_ANY, lang.GetString("SERVER_MANAGER"));
    lblManager->SetFont(wxFontInfo(20).Bold());
    lblManager->SetForegroundColour(wxColour("#F4F4F5"));
    
    lblStatusValue = new wxStaticText(headerPanel, wxID_ANY, "OFFLINE");
    lblStatusValue->SetFont(wxFontInfo(10).Bold());
    lblStatusValue->SetForegroundColour(wxColour("#EF4444")); // Status Error / Offline
    
    titleBox->Add(lblManager, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    titleBox->Add(lblStatusValue, 0, wxALIGN_CENTER_VERTICAL);
    
    // Fake status label for translations
    lblStatus = new wxStaticText(headerPanel, wxID_ANY, ""); 
    lblStatus->Hide();
    
    // Tab bar (horizontal)
    wxBoxSizer* tabBarSizer = new wxBoxSizer(wxHORIZONTAL);
    btnTabOverview = new CustomButton(headerPanel, ID_BTN_TAB_OVERVIEW, lang.GetString("TAB_OVERVIEW"), wxDefaultPosition, wxSize(100, 30));
    btnTabConnections = new CustomButton(headerPanel, ID_BTN_TAB_CONNECTIONS, lang.GetString("TAB_CONNECTIONS"), wxDefaultPosition, wxSize(110, 30));
    btnTabLogs = new CustomButton(headerPanel, ID_BTN_TAB_LOGS, lang.GetString("TAB_LOGS"), wxDefaultPosition, wxSize(100, 30));
    
    wxFont tabFont(wxFontInfo(10));
    btnTabOverview->SetFont(tabFont);
    btnTabConnections->SetFont(tabFont);
    btnTabLogs->SetFont(tabFont);
    
    btnTabOverview->SetAlignment(wxALIGN_CENTER);
    btnTabConnections->SetAlignment(wxALIGN_CENTER);
    btnTabLogs->SetAlignment(wxALIGN_CENTER);
    
    tabBarSizer->Add(btnTabOverview, 0, wxRIGHT, 5);
    tabBarSizer->Add(btnTabConnections, 0, wxRIGHT, 5);
    tabBarSizer->Add(btnTabLogs, 0);

    headerSizer->Add(titleBox, 0, wxBOTTOM, 15);
    headerSizer->Add(tabBarSizer, 0);
    headerPanel->SetSizer(headerSizer);

    // Initialize ContentBook for Server Tabs
    contentBook = new SlideBook(serverContainer, wxID_ANY);
    
    // --- Page: Overview ---
    pageOverview = new wxPanel(contentBook, wxID_ANY);
    pageOverview->SetBackgroundColour(wxColour("#0A0A0B"));
    wxBoxSizer* dashSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* topControlsSizer = new wxBoxSizer(wxHORIZONTAL);
    btnStartStop = new CustomButton(pageOverview, ID_BTN_STARTSTOP, "", wxDefaultPosition, wxSize(150, 40));
    btnStartStop->SetBackgroundColour(wxColour("#F4F4F5")); // Primary inverted button
    btnStartStop->SetForegroundColour(wxColour("#0A0A0B"));
    btnStartStop->SetFont(wxFontInfo(11).Bold());
    btnStartStop->SetAlignment(wxALIGN_CENTER);
    topControlsSizer->Add(btnStartStop, 0);
    
    wxBoxSizer* statsHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblStatsTitle = new wxStaticText(pageOverview, wxID_ANY, lang.GetString("SYSTEM_LOAD"));
    lblStatsTitle->SetFont(wxFontInfo(11).Bold());
    lblStatsTitle->SetForegroundColour(wxColour("#A1A1AA"));
    
    comboServerStats = new wxComboBox(pageOverview, wxID_ANY, lang.GetString("LOCAL_SERVER_COMBO"), wxDefaultPosition, wxSize(200, -1), 0, NULL, wxCB_READONLY | wxCB_DROPDOWN);
    comboServerStats->Append(lang.GetString("LOCAL_SERVER_COMBO"));
    comboServerStats->SetSelection(0);
    
    statsHeaderSizer->Add(lblStatsTitle, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    statsHeaderSizer->AddStretchSpacer();
    statsHeaderSizer->Add(comboServerStats, 0, wxALIGN_CENTER_VERTICAL);
    
    sysStatsPanel = new wxPanel(pageOverview, wxID_ANY);
    sysStatsPanel->SetBackgroundColour(wxColour("#141417"));
    wxGridSizer* sysStatsGrid = new wxGridSizer(2, 2, 10, 10);
    
    wxFont statFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
    
    wxBoxSizer* cpuBox = new wxBoxSizer(wxVERTICAL);
    lblSysCpu = new wxStaticText(sysStatsPanel, wxID_ANY, lang.GetString("CPU_LABEL"));
    lblSysCpu->SetForegroundColour(wxColour("#F4F4F5"));
    lblSysCpu->SetFont(statFont);
    gaugeSysCpu = new wxGauge(sysStatsPanel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 4));
    cpuBox->Add(lblSysCpu, 0, wxBOTTOM, 5);
    cpuBox->Add(gaugeSysCpu, 0, wxEXPAND);
    
    wxBoxSizer* ramBox = new wxBoxSizer(wxVERTICAL);
    lblSysRam = new wxStaticText(sysStatsPanel, wxID_ANY, "RAM: 0.0 / 0.0 GB (0%)");
    lblSysRam->SetForegroundColour(wxColour("#F4F4F5"));
    lblSysRam->SetFont(statFont);
    gaugeSysRam = new wxGauge(sysStatsPanel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 4));
    ramBox->Add(lblSysRam, 0, wxBOTTOM, 5);
    ramBox->Add(gaugeSysRam, 0, wxEXPAND);
    
    wxBoxSizer* diskBox = new wxBoxSizer(wxVERTICAL);
    lblSysDisk = new wxStaticText(sysStatsPanel, wxID_ANY, "DISK: 0.0 / 0.0 GB (0%)");
    lblSysDisk->SetForegroundColour(wxColour("#F4F4F5"));
    lblSysDisk->SetFont(statFont);
    gaugeSysDisk = new wxGauge(sysStatsPanel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 4));
    diskBox->Add(lblSysDisk, 0, wxBOTTOM, 5);
    diskBox->Add(gaugeSysDisk, 0, wxEXPAND);
    
    wxBoxSizer* vramBox = new wxBoxSizer(wxVERTICAL);
    lblSysVram = new wxStaticText(sysStatsPanel, wxID_ANY, "VRAM: N/A");
    lblSysVram->SetForegroundColour(wxColour("#F4F4F5"));
    lblSysVram->SetFont(statFont);
    gaugeSysVram = new wxGauge(sysStatsPanel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 4));
    vramBox->Add(lblSysVram, 0, wxBOTTOM, 5);
    vramBox->Add(gaugeSysVram, 0, wxEXPAND);
    
    sysStatsGrid->Add(cpuBox, 1, wxEXPAND | wxALL, 15);
    sysStatsGrid->Add(ramBox, 1, wxEXPAND | wxALL, 15);
    sysStatsGrid->Add(diskBox, 1, wxEXPAND | wxALL, 15);
    sysStatsGrid->Add(vramBox, 1, wxEXPAND | wxALL, 15);
    sysStatsPanel->SetSizer(sysStatsGrid);

    statsBgPanel = new wxPanel(pageOverview, wxID_ANY);
    statsBgPanel->SetBackgroundColour(wxColour("#141417")); // Surface Elevated
    wxGridSizer* gridSizer = new wxGridSizer(2, 2, 10, 10);
    
    statSessions = new wxStaticText(statsBgPanel, wxID_ANY, "Active Sessions: 0");
    statErrors = new wxStaticText(statsBgPanel, wxID_ANY, "Errors: 0");
    statTools = new wxStaticText(statsBgPanel, wxID_ANY, "Tools Calls: 0");
    statUptime = new wxStaticText(statsBgPanel, wxID_ANY, "Uptime: 0");
    
    // Tabular numerals font for data
    wxFont dataFont(wxFontInfo(12).Family(wxFONTFAMILY_TELETYPE));
    statSessions->SetFont(dataFont);
    statErrors->SetFont(dataFont);
    statTools->SetFont(dataFont);
    statUptime->SetFont(dataFont);

    statSessions->SetForegroundColour(wxColour("#F4F4F5"));
    statErrors->SetForegroundColour(wxColour("#F4F4F5"));
    statTools->SetForegroundColour(wxColour("#F4F4F5"));
    statUptime->SetForegroundColour(wxColour("#F4F4F5"));

    gridSizer->Add(statSessions, 0, wxALL, 15);
    gridSizer->Add(statErrors, 0, wxALL, 15);
    gridSizer->Add(statTools, 0, wxALL, 15);
    gridSizer->Add(statUptime, 0, wxALL, 15);
    statsBgPanel->SetSizer(gridSizer);
    
    dashSizer->Add(topControlsSizer, 0, wxBOTTOM, 20);
    dashSizer->Add(statsHeaderSizer, 0, wxEXPAND | wxBOTTOM, 10);
    dashSizer->Add(sysStatsPanel, 0, wxEXPAND | wxBOTTOM, 20);
    dashSizer->Add(statsBgPanel, 0, wxEXPAND);
    pageOverview->SetSizer(dashSizer);
    contentBook->AddPage(pageOverview, "Overview");

    // --- Page: Connections ---
    pageConnections = new wxPanel(contentBook, wxID_ANY);
    pageConnections->SetBackgroundColour(wxColour("#0A0A0B"));
    wxBoxSizer* netSizer = new wxBoxSizer(wxVERTICAL);
    
    // Server Bind Info
    wxBoxSizer* bindSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblBindTitle = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_BIND_INFO"));
    lblBindTitle->SetFont(wxFontInfo(11).Bold());
    lblBindTitle->SetForegroundColour(wxColour("#A1A1AA"));
    lblBindInfo = new wxStaticText(pageConnections, wxID_ANY, "Bind: 0.0.0.0 | Port: 3000 | Transport: SSE | Path: /sse");
    lblBindInfo->SetForegroundColour(wxColour("#F4F4F5"));
    bindSizer->Add(lblBindTitle, 0, wxRIGHT, 10);
    bindSizer->Add(lblBindInfo, 0);
    
    // Connection Info
    wxBoxSizer* connSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* lblConnTitle = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_CONN_INFO"));
    lblConnTitle->SetFont(wxFontInfo(11).Bold());
    lblConnTitle->SetForegroundColour(wxColour("#A1A1AA"));
    
    wxBoxSizer* locSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblLoc = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_LOC_URL"));
    lblLoc->SetForegroundColour(wxColour("#F4F4F5"));
    lblLocalUrl = new wxStaticText(pageConnections, wxID_ANY, "http://localhost:3000/sse");
    lblLocalUrl->SetForegroundColour(wxColour("#5E6AD2"));
    locSizer->Add(lblLoc, 0, wxRIGHT, 5);
    locSizer->Add(lblLocalUrl, 0);
    
    wxBoxSizer* extSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblExt = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_EXT_URL"));
    lblExt->SetForegroundColour(wxColour("#F4F4F5"));
    lblExternalUrl = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_NOT_DETECTED"));
    lblExternalUrl->SetForegroundColour(wxColour("#5E6AD2"));
    extSizer->Add(lblExt, 0, wxRIGHT, 5);
    extSizer->Add(lblExternalUrl, 0);
    
    wxBoxSizer* ipSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblIp = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_PUB_IP"));
    lblIp->SetForegroundColour(wxColour("#F4F4F5"));
    lblPublicIP = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_UNKNOWN"));
    lblPublicIP->SetForegroundColour(wxColour("#F4F4F5"));
    ipSizer->Add(lblIp, 0, wxRIGHT, 5);
    ipSizer->Add(lblPublicIP, 0);
    
    wxBoxSizer* domSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblDom = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_CUST_URL"));
    lblDom->SetForegroundColour(wxColour("#F4F4F5"));
    txtCustomDomain = new wxTextCtrl(pageConnections, ID_TXT_CUSTOM_DOMAIN, "", wxDefaultPosition, wxSize(200, -1), wxBORDER_NONE);
    txtCustomDomain->SetBackgroundColour(wxColour("#141417"));
    txtCustomDomain->SetForegroundColour(wxColour("#F4F4F5"));
    domSizer->Add(lblDom, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    domSizer->Add(txtCustomDomain, 0);
    
    connSizer->Add(lblConnTitle, 0, wxBOTTOM, 10);
    connSizer->Add(locSizer, 0, wxBOTTOM, 5);
    connSizer->Add(ipSizer, 0, wxBOTTOM, 5);
    connSizer->Add(domSizer, 0, wxBOTTOM, 5);
    connSizer->Add(extSizer, 0, wxBOTTOM, 10);
    
    // Tokens
    wxBoxSizer* tokenSizer = new wxBoxSizer(wxVERTICAL);
    lblTokenTitle = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_TOKENS"));
    lblTokenTitle->SetFont(wxFontInfo(11).Bold());
    lblTokenTitle->SetForegroundColour(wxColour("#A1A1AA"));
    
    gridTokens = new wxGrid(pageConnections, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
    gridTokens->CreateGrid(0, 5);
    gridTokens->SetColLabelValue(0, lang.GetString("COL_NAME"));
    gridTokens->SetColLabelValue(1, lang.GetString("COL_ID"));
    gridTokens->SetColLabelValue(2, lang.GetString("COL_TOKEN"));
    gridTokens->SetColLabelValue(3, lang.GetString("COL_CREATED"));
    gridTokens->SetColLabelValue(4, lang.GetString("COL_STATUS"));
    gridTokens->SetColSize(0, 120);
    gridTokens->SetColSize(1, 150);
    gridTokens->SetColSize(2, 200);
    gridTokens->SetColSize(3, 160);
    gridTokens->SetColSize(4, 80);
    gridTokens->HideRowLabels();
    gridTokens->DisableDragColSize();
    gridTokens->DisableDragRowSize();
    gridTokens->SetSelectionMode(wxGrid::wxGridSelectRows);
    gridTokens->SetDefaultCellBackgroundColour(wxColour("#141417"));
    gridTokens->SetDefaultCellTextColour(wxColour("#F4F4F5"));
    gridTokens->SetLabelBackgroundColour(wxColour("#0A0A0B"));
    gridTokens->SetLabelTextColour(wxColour("#A1A1AA"));
    gridTokens->SetGridLineColour(wxColour("#27272A"));
    gridTokens->Bind(wxEVT_SIZE, &Windows::OnGridSize, this);
    
    wxBoxSizer* tbtnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnCreateToken = new CustomButton(pageConnections, ID_BTN_CREATE_TOKEN, lang.GetString("BTN_CREATE_TOKEN"), wxDefaultPosition, wxSize(120, 30));
    btnDeleteToken = new CustomButton(pageConnections, ID_BTN_DELETE_TOKEN, lang.GetString("BTN_DELETE_TOKEN"), wxDefaultPosition, wxSize(120, 30));
    btnToggleToken = new CustomButton(pageConnections, ID_BTN_TOGGLE_TOKEN, lang.GetString("BTN_TOGGLE_TOKEN"), wxDefaultPosition, wxSize(120, 30));
    btnEditPermissions = new CustomButton(pageConnections, ID_BTN_EDIT_PERMISSIONS, lang.GetString("BTN_EDIT_PERMISSIONS"), wxDefaultPosition, wxSize(140, 30));
    
    btnCreateToken->SetBackgroundColour(wxColour("#F4F4F5"));
    btnCreateToken->SetForegroundColour(wxColour("#0A0A0B"));
    btnDeleteToken->SetBackgroundColour(wxColour("#EF4444"));
    btnDeleteToken->SetForegroundColour(wxColour("#F4F4F5"));
    btnToggleToken->SetBackgroundColour(wxColour("#141417"));
    btnToggleToken->SetForegroundColour(wxColour("#F4F4F5"));
    btnEditPermissions->SetBackgroundColour(wxColour("#2563EB"));
    btnEditPermissions->SetForegroundColour(wxColour("#FFFFFF"));
    
    btnCreateToken->SetAlignment(wxALIGN_CENTER);
    btnDeleteToken->SetAlignment(wxALIGN_CENTER);
    btnToggleToken->SetAlignment(wxALIGN_CENTER);
    btnEditPermissions->SetAlignment(wxALIGN_CENTER);
    btnCopyTokenUrl = new CustomButton(pageConnections, ID_BTN_COPY_TOKEN_URL, lang.GetString("BTN_COPY_TOKEN_URL"), wxDefaultPosition, wxSize(150, 30));
    btnCopyTokenUrl->SetBackgroundColour(wxColour("#3B82F6"));
    btnCopyTokenUrl->SetForegroundColour(wxColour("#FFFFFF"));
    btnCopyTokenUrl->SetAlignment(wxALIGN_CENTER);

    tbtnSizer->Add(btnCreateToken, 0, wxRIGHT, 10);
    tbtnSizer->Add(btnDeleteToken, 0, wxRIGHT, 10);
    tbtnSizer->Add(btnToggleToken, 0, wxRIGHT, 10);
    tbtnSizer->Add(btnEditPermissions, 0, wxRIGHT, 10);
    tbtnSizer->Add(btnCopyTokenUrl, 0);
    
    tokenSizer->Add(lblTokenTitle, 0, wxBOTTOM, 10);
    tokenSizer->Add(gridTokens, 1, wxEXPAND | wxBOTTOM, 10);
    tokenSizer->Add(tbtnSizer, 0);
    
    // Connection string & Checker
    wxBoxSizer* botSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxBoxSizer* chkSizer = new wxBoxSizer(wxVERTICAL);
    lblChkTitle = new wxStaticText(pageConnections, wxID_ANY, lang.GetString("LBL_NET_CHECK"));
    lblChkTitle->SetFont(wxFontInfo(11).Bold());
    lblChkTitle->SetForegroundColour(wxColour("#A1A1AA"));
    btnCheckNetwork = new CustomButton(pageConnections, ID_BTN_CHECK_NETWORK, lang.GetString("BTN_CHECK"), wxDefaultPosition, wxSize(100, 30));
    btnCheckNetwork->SetBackgroundColour(wxColour("#141417"));
    btnCheckNetwork->SetForegroundColour(wxColour("#F4F4F5"));
    btnCheckNetwork->SetAlignment(wxALIGN_CENTER);
    
    lblCheckResult = new CustomButton(pageConnections, wxID_ANY, lang.GetString("LBL_READY"), wxDefaultPosition, wxSize(200, 30));
    lblCheckResult->SetForegroundColour(wxColour("#EF4444"));
    lblCheckResult->SetBackgroundColour(wxColour(0,0,0,0)); // Transparent
    lblCheckResult->SetHoverColour(wxColour(0,0,0,0));
    
    wxBoxSizer* chkRow = new wxBoxSizer(wxHORIZONTAL);
    chkRow->Add(btnCheckNetwork, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
    chkRow->Add(lblCheckResult, 0, wxALIGN_CENTER_VERTICAL);
    
    chkSizer->Add(lblChkTitle, 0, wxBOTTOM, 5);
    chkSizer->Add(chkRow, 0, wxBOTTOM, 10);
    
    botSizer->Add(chkSizer, 1, wxEXPAND);
    
    // Assembly
    netSizer->Add(bindSizer, 0, wxALL, 0);
    netSizer->Add(new wxStaticLine(pageConnections), 0, wxEXPAND | wxTOP | wxBOTTOM, 15);
    netSizer->Add(connSizer, 0, wxALL, 0);
    netSizer->Add(new wxStaticLine(pageConnections), 0, wxEXPAND | wxTOP | wxBOTTOM, 15);
    netSizer->Add(tokenSizer, 1, wxEXPAND | wxALL, 0);
    netSizer->Add(new wxStaticLine(pageConnections), 0, wxEXPAND | wxTOP | wxBOTTOM, 15);
    netSizer->Add(botSizer, 0, wxEXPAND | wxALL, 0);
    
    pageConnections->SetSizer(netSizer);
    contentBook->AddPage(pageConnections, "Connections");

    RefreshTokenList();
    RefreshConnectionUrls();

    // --- Page: Logs ---
    pageLogs = new wxPanel(contentBook, wxID_ANY);
    pageLogs->SetBackgroundColour(wxColour("#0A0A0B"));
    wxBoxSizer* logsSizer = new wxBoxSizer(wxVERTICAL);
    
    lblLogsHeader = new wxStaticText(pageLogs, wxID_ANY, lang.GetString("LBL_SERVER_LOGS"));
    lblLogsHeader->SetFont(wxFontInfo(11).Bold());
    lblLogsHeader->SetForegroundColour(wxColour("#A1A1AA"));
    
    txtLogs = new wxTextCtrl(pageLogs, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    txtLogs->SetBackgroundColour(wxColour("#141417"));
    txtLogs->SetForegroundColour(wxColour("#A1A1AA")); // Muted ink for logs
    txtLogs->SetFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
    
    logsSizer->Add(lblLogsHeader, 0, wxBOTTOM, 10);
    logsSizer->Add(txtLogs, 1, wxEXPAND, 0);
    pageLogs->SetSizer(logsSizer);
    contentBook->AddPage(pageLogs, "Logs");

    // Combine header and contentBook into serverContainer
    serverSizer->Add(headerPanel, 0, wxEXPAND | wxBOTTOM, 20);
    serverSizer->Add(contentBook, 1, wxEXPAND);
    serverContainer->SetSizer(serverSizer);
    rootBook->AddPage(serverContainer, "ServerContainer");
    
    // === CLUSTER CONTAINER ===
    clusterContainer = new wxPanel(rootBook, wxID_ANY);
    clusterContainer->SetBackgroundColour(wxColour("#09090B")); // Darker modern background
    
    wxBoxSizer* clusterMainSizer = new wxBoxSizer(wxVERTICAL);
    
    // Header Panel
    wxPanel* clusterHeaderPanel = new wxPanel(clusterContainer, wxID_ANY);
    clusterHeaderPanel->SetBackgroundColour(wxColour("#141417")); // Sleek top header
    wxBoxSizer* clusterHeaderSizer = new wxBoxSizer(wxVERTICAL);
    
    lblClusterTitle = new wxStaticText(clusterHeaderPanel, wxID_ANY, lang.GetString("CLUSTER_MANAGEMENT"));
    lblClusterTitle->SetFont(wxFontInfo(20).Bold());
    lblClusterTitle->SetForegroundColour(wxColour("#FFFFFF"));
    
    lblClusterSub = new wxStaticText(clusterHeaderPanel, wxID_ANY, lang.GetString("CLUSTER_SUB"));
    lblClusterSub->SetFont(wxFontInfo(10));
    lblClusterSub->SetForegroundColour(wxColour("#A1A1AA")); // Zinc 400
    
    clusterHeaderSizer->Add(lblClusterTitle, 0, wxLEFT | wxTOP | wxRIGHT, 20);
    clusterHeaderSizer->Add(lblClusterSub, 0, wxLEFT | wxRIGHT | wxBOTTOM, 20);
    clusterHeaderPanel->SetSizer(clusterHeaderSizer);
    
    clusterMainSizer->Add(clusterHeaderPanel, 0, wxEXPAND | wxBOTTOM, 20);
    
    // Content area with padding
    wxBoxSizer* clusterContentSizer = new wxBoxSizer(wxVERTICAL);
    // Create a card for the list
    wxPanel* clusterCard = new wxPanel(clusterContainer, wxID_ANY);
    clusterCard->SetBackgroundColour(wxColour("#18181B")); // Zinc 900 Card
    wxBoxSizer* clusterCardSizer = new wxBoxSizer(wxVERTICAL);
    
    listNodes = new wxListCtrl(clusterCard, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxBORDER_NONE | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_EDIT_LABELS);
    listNodes->Bind(wxEVT_LIST_END_LABEL_EDIT, &Windows::OnNodeIdEdited, this);
    listNodes->InsertColumn(0, lang.GetString("NODE_ID"), wxLIST_FORMAT_LEFT, 150);
    listNodes->InsertColumn(1, lang.GetString("HOSTNAME"), wxLIST_FORMAT_LEFT, 150);
    listNodes->InsertColumn(2, lang.GetString("IP_ADDRESS"), wxLIST_FORMAT_LEFT, 130);
    listNodes->InsertColumn(3, lang.GetString("COL_STATUS"), wxLIST_FORMAT_LEFT, 100);
    listNodes->InsertColumn(4, lang.GetString("PLATFORM"), wxLIST_FORMAT_LEFT, 120);
    listNodes->SetBackgroundColour(wxColour("#18181B")); 
    listNodes->SetTextColour(wxColour("#E4E4E7")); // Zinc 200
    listNodes->SetFont(wxFontInfo(10));
    
    // Action buttons using standard CustomButton
    wxBoxSizer* clusterBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnAddNode = new CustomButton(clusterCard, ID_BTN_ADD_NODE, lang.GetString("BTN_ADD_NODE"), wxDefaultPosition, wxSize(170, 36));
    btnAddNode->SetBackgroundColour(wxColour("#27272A"));
    btnAddNode->SetForegroundColour(wxColour("#F4F4F5"));
    
    btnApproveNode = new CustomButton(clusterCard, ID_BTN_APPROVE_NODE, lang.GetString("BTN_APPROVE"), wxDefaultPosition, wxSize(170, 36));
    btnApproveNode->SetBackgroundColour(wxColour("#2563EB")); // Primary Blue
    btnApproveNode->SetForegroundColour(wxColour("#FFFFFF"));
    
    btnRejectNode = new CustomButton(clusterCard, ID_BTN_REJECT_NODE, lang.GetString("BTN_REJECT"), wxDefaultPosition, wxSize(170, 36));
    btnRejectNode->SetBackgroundColour(wxColour("#EF4444")); // Destructive Red
    btnRejectNode->SetForegroundColour(wxColour("#FFFFFF"));
    
    clusterBtnSizer->Add(btnAddNode, 0, wxRIGHT, 10);
    clusterBtnSizer->Add(btnApproveNode, 0, wxRIGHT, 10);
    
    btnReconnectNode = new CustomButton(clusterCard, ID_BTN_RECONNECT_NODE, lang.GetString("BTN_RECONNECT"), wxDefaultPosition, wxSize(170, 36));
    btnReconnectNode->SetBackgroundColour(wxColour("#F59E0B")); // Amber 500
    btnReconnectNode->SetForegroundColour(wxColour("#FFFFFF"));
    
    clusterBtnSizer->Add(btnReconnectNode, 0, wxRIGHT, 10);
    clusterBtnSizer->Add(btnRejectNode, 0, 0, 0);
    
    clusterCardSizer->Add(clusterBtnSizer, 0, wxALL, 15);
    clusterCardSizer->Add(listNodes, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);
    clusterCard->SetSizer(clusterCardSizer);
    
    clusterContentSizer->Add(clusterCard, 1, wxEXPAND | wxALL, 20);
    clusterMainSizer->Add(clusterContentSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
    
    clusterContainer->SetSizer(clusterMainSizer);
    rootBook->AddPage(clusterContainer, "ClusterContainer");
    
    // === TOOLS CONTAINER ===
    toolsContainer = new wxPanel(rootBook, wxID_ANY);
    toolsContainer->SetBackgroundColour(wxColour("#0A0A0B"));
    wxBoxSizer* toolsSizer = new wxBoxSizer(wxVERTICAL);
    lblToolsHeader = new wxStaticText(toolsContainer, wxID_ANY, lang.GetString("TOOLS_MANAGEMENT"));
    lblToolsHeader->SetFont(wxFontInfo(20).Bold());
    lblToolsHeader->SetForegroundColour(wxColour("#F4F4F5"));
    toolsSizer->Add(lblToolsHeader, 0, wxALL, 30);
    
    wxStaticText* lblToolsSub = new wxStaticText(toolsContainer, wxID_ANY, lang.GetString("TOOLS_SUB"));
    lblToolsSub->SetFont(wxFontInfo(10));
    lblToolsSub->SetForegroundColour(wxColour("#A1A1AA")); // Zinc 400
    toolsSizer->Add(lblToolsSub, 0, wxLEFT | wxRIGHT | wxBOTTOM, 30);
    
    wxPanel* toolsCard = new wxPanel(toolsContainer, wxID_ANY);
    toolsCard->SetBackgroundColour(wxColour("#18181B"));
    wxBoxSizer* toolsCardSizer = new wxBoxSizer(wxVERTICAL);
    
    scrollTools = new wxScrolledWindow(toolsCard, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    scrollTools->SetBackgroundColour(wxColour("#18181B"));
    scrollTools->SetScrollRate(5, 5);
    scrollToolsSizer = new wxBoxSizer(wxVERTICAL);
    scrollTools->SetSizer(scrollToolsSizer);
    
    toolsCardSizer->Add(scrollTools, 1, wxEXPAND | wxALL, 15);
    toolsCard->SetSizer(toolsCardSizer);
    
    toolsSizer->Add(toolsCard, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 30);
    
    toolsContainer->SetSizer(toolsSizer);
    rootBook->AddPage(toolsContainer, "ToolsContainer");
    
    RefreshToolsList();

    // === PAGE 1: GLOBAL SETTINGS CONTAINER ===
    globalSettingsContainer = new wxPanel(rootBook, wxID_ANY);
    globalSettingsContainer->SetBackgroundColour(wxColour("#0A0A0B"));
    wxBoxSizer* gsMainSizer = new wxBoxSizer(wxVERTICAL);
    
    lblGsHeader = new wxStaticText(globalSettingsContainer, wxID_ANY, lang.GetString("GLOBAL_SETTINGS"));
    lblGsHeader->SetFont(wxFontInfo(20).Bold());
    lblGsHeader->SetForegroundColour(wxColour("#F4F4F5"));
    gsMainSizer->Add(lblGsHeader, 0, wxBOTTOM, 30);
    
    pageGlobalSettings = new wxScrolledWindow(globalSettingsContainer, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    pageGlobalSettings->SetBackgroundColour(wxColour("#0A0A0B"));
    pageGlobalSettings->SetScrollRate(5, 5);
    wxBoxSizer* wsSizer = new wxBoxSizer(wxVERTICAL);
    
    SettingsManager& sm = SettingsManager::Get();
    wxFont sectionFont = wxFontInfo(14).Bold(); // Make headers much larger for better readability
    wxColour sectionColor = wxColour("#FFFFFF"); // Bright white for strong contrast
    wxColour fgColor = wxColour("#A1A1AA"); // Muted zinc for normal text
    wxColour bgPanelColor = wxColour("#141417");

    auto addSectionHeader = [&](const wxString& title, wxStaticText*& outPtr) {
        wxStaticText* lbl = new wxStaticText(pageGlobalSettings, wxID_ANY, title);
        lbl->SetFont(sectionFont);
        lbl->SetForegroundColour(sectionColor);
        wsSizer->Add(lbl, 0, wxTOP | wxBOTTOM, 15);
        outPtr = lbl;
    };
    

    // --- System & Behavior ---
    addSectionHeader(lang.GetString("GS_SYSTEM"), lblSecSystem);
    
    wxBoxSizer* appModeSizer = new wxBoxSizer(wxHORIZONTAL);
    lblAppMode = new wxStaticText(pageGlobalSettings, wxID_ANY, lang.GetString("GS_APP_MODE"));
    lblAppMode->SetForegroundColour(fgColor);
    wxArrayString modeChoices;
    modeChoices.Add(lang.GetString("GS_MODE_PARENT"));
    modeChoices.Add(lang.GetString("GS_MODE_CHILD"));
    choiceAppMode = new wxChoice(pageGlobalSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, modeChoices);
    choiceAppMode->SetStringSelection(sm.appMode);
    appModeSizer->Add(lblAppMode, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    appModeSizer->Add(choiceAppMode, 0, wxALIGN_CENTER_VERTICAL);
    wsSizer->Add(appModeSizer, 0, wxBOTTOM, 15);
    
    chkLaunchOnStartup = new wxCheckBox(pageGlobalSettings, wxID_ANY, lang.GetString("GS_LAUNCH_STARTUP"));
    chkLaunchOnStartup->SetValue(sm.launchOnStartup);
    chkLaunchOnStartup->SetForegroundColour(fgColor);
    
    chkAutoStartServer = new wxCheckBox(pageGlobalSettings, wxID_ANY, lang.GetString("GS_AUTO_START_SERVER"));
    chkAutoStartServer->SetValue(sm.autoStartServer);
    chkAutoStartServer->SetForegroundColour(fgColor);
    
    chkMinimizeToTray = new wxCheckBox(pageGlobalSettings, wxID_ANY, lang.GetString("GS_MINIMIZE_TRAY"));
    chkMinimizeToTray->SetValue(sm.minimizeToTray);
    chkMinimizeToTray->SetForegroundColour(fgColor);
    
    chkShowNotifications = new wxCheckBox(pageGlobalSettings, wxID_ANY, lang.GetString("GS_NOTIFICATIONS"));
    chkShowNotifications->SetValue(sm.showNotifications);
    chkShowNotifications->SetForegroundColour(fgColor);

    wsSizer->Add(chkLaunchOnStartup, 0, wxBOTTOM, 10);
    wsSizer->Add(chkAutoStartServer, 0, wxBOTTOM, 10);
    wsSizer->Add(chkMinimizeToTray, 0, wxBOTTOM, 10);
    wsSizer->Add(chkShowNotifications, 0, wxBOTTOM, 10);

    // --- Storage & Logs ---
    addSectionHeader(lang.GetString("GS_STORAGE"), lblSecStorage);
    wxBoxSizer* logRetentionSizer = new wxBoxSizer(wxHORIZONTAL);
    lblLogRetention = new wxStaticText(pageGlobalSettings, wxID_ANY, lang.GetString("GS_LOG_RETENTION"));
    lblLogRetention->SetForegroundColour(fgColor);
    wxArrayString retentionChoices;
    retentionChoices.Add("7 Days"); retentionChoices.Add("30 Days"); retentionChoices.Add("Keep All");
    choiceLogRetention = new wxChoice(pageGlobalSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, retentionChoices);
    choiceLogRetention->SetStringSelection(sm.logRetention);
    logRetentionSizer->Add(lblLogRetention, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    logRetentionSizer->Add(choiceLogRetention, 0);
    
    btnClearCache = new CustomButton(pageGlobalSettings, wxID_ANY, lang.GetString("GS_CLEAR_CACHE"), wxDefaultPosition, wxSize(200, 36));
    btnClearCache->SetBackgroundColour(wxColour("#EF4444"));
    btnClearCache->SetForegroundColour(wxColour("#FFFFFF"));
    btnClearCache->SetAlignment(wxALIGN_CENTER);

    wsSizer->Add(logRetentionSizer, 0, wxBOTTOM, 10);
    wsSizer->Add(btnClearCache, 0, wxBOTTOM, 10);

    // Appearance section removed as per user request (moved to header)

    // --- Security ---
    addSectionHeader(lang.GetString("GS_SECURITY"), lblSecSecurity);
    chkAppLock = new wxCheckBox(pageGlobalSettings, wxID_ANY, lang.GetString("GS_APP_LOCK"));
    chkAppLock->SetValue(sm.appLock);
    chkAppLock->SetForegroundColour(fgColor);

    chkMaskSecrets = new wxCheckBox(pageGlobalSettings, wxID_ANY, lang.GetString("GS_MASK_SECRETS"));
    chkMaskSecrets->SetValue(sm.maskSecrets);
    chkMaskSecrets->SetForegroundColour(fgColor);

    wsSizer->Add(chkAppLock, 0, wxBOTTOM, 10);
    wsSizer->Add(chkMaskSecrets, 0, wxBOTTOM, 10);

    // --- Updates ---
    addSectionHeader(lang.GetString("GS_UPDATES"), lblSecUpdates);
    
    lblCurrentVersion = new wxStaticText(pageGlobalSettings, wxID_ANY, lang.GetString("LBL_VERSION") + APP_VERSION);
    lblCurrentVersion->SetForegroundColour(wxColour("#A1A1AA"));
    
    chkAutoUpdate = new wxCheckBox(pageGlobalSettings, wxID_ANY, lang.GetString("GS_AUTO_UPDATE"));
    chkAutoUpdate->SetValue(sm.autoUpdate);
    chkAutoUpdate->SetForegroundColour(fgColor);

    wxBoxSizer* updRow = new wxBoxSizer(wxHORIZONTAL);
    btnCheckUpdates = new CustomButton(pageGlobalSettings, wxID_ANY, lang.GetString("GS_CHECK_UPDATES"), wxDefaultPosition, wxSize(200, 36));
    btnCheckUpdates->SetBackgroundColour(bgPanelColor);
    btnCheckUpdates->SetForegroundColour(wxColour("#FFFFFF"));
    btnCheckUpdates->SetAlignment(wxALIGN_CENTER);

    lblUpdateStatus = new wxStaticText(pageGlobalSettings, wxID_ANY, "");
    lblUpdateStatus->SetForegroundColour(sectionColor);

    updRow->Add(btnCheckUpdates, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
    updRow->Add(lblUpdateStatus, 0, wxALIGN_CENTER_VERTICAL);

    wsSizer->Add(lblCurrentVersion, 0, wxBOTTOM, 5);
    wsSizer->Add(chkAutoUpdate, 0, wxBOTTOM, 10);
    wsSizer->Add(updRow, 0, wxBOTTOM, 10);

    // --- Language (From original) ---
    addSectionHeader(lang.GetString("GS_LANGUAGE"), lblSecLanguage);
    wxBoxSizer* langRowSizer = new wxBoxSizer(wxHORIZONTAL);
    btnLangRu = new CustomButton(pageGlobalSettings, ID_BTN_LANG_RU, wxString::FromUTF8("Русский (RU)"), wxDefaultPosition, wxSize(150, 40));
    btnLangEn = new CustomButton(pageGlobalSettings, ID_BTN_LANG_EN, "English (EN)", wxDefaultPosition, wxSize(150, 40));
    btnLangRu->SetAlignment(wxALIGN_CENTER);
    btnLangEn->SetAlignment(wxALIGN_CENTER);
    btnLangRu->SetBackgroundColour(bgPanelColor);
    btnLangEn->SetBackgroundColour(bgPanelColor);
    btnLangRu->SetForegroundColour(fgColor);
    btnLangEn->SetForegroundColour(fgColor);
    langRowSizer->Add(btnLangRu, 0, wxRIGHT, 10);
    langRowSizer->Add(btnLangEn, 0);
    wsSizer->Add(langRowSizer, 0, wxBOTTOM, 10);

    // --- Workspace (From original) ---
    addSectionHeader(lang.GetString("GS_WORKSPACE"), lblSecWorkspace);
    wxBoxSizer* pathSizer = new wxBoxSizer(wxHORIZONTAL);
    txtWorkspacePath = new wxTextCtrl(pageGlobalSettings, wxID_ANY, currentWorkspace, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxBORDER_NONE);
    txtWorkspacePath->SetBackgroundColour(bgPanelColor);
    txtWorkspacePath->SetForegroundColour(fgColor);
    
    btnBrowseWorkspace = new CustomButton(pageGlobalSettings, ID_BTN_BROWSE_WS, "...", wxDefaultPosition, wxSize(40, 30));
    btnBrowseWorkspace->SetBackgroundColour(bgPanelColor);
    btnBrowseWorkspace->SetForegroundColour(fgColor);
    btnBrowseWorkspace->SetAlignment(wxALIGN_CENTER);
    
    pathSizer->Add(txtWorkspacePath, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
    pathSizer->Add(btnBrowseWorkspace, 0, wxALIGN_CENTER_VERTICAL);
    
    chkCreateMissing = new wxCheckBox(pageGlobalSettings, wxID_ANY, "Create if missing");
    chkCreateMissing->SetValue(true);
    chkCreateMissing->SetForegroundColour(fgColor);
    
    btnSaveWorkspace = new CustomButton(pageGlobalSettings, ID_BTN_SAVE_WS, "Save Workspace", wxDefaultPosition, wxSize(150, 40));
    btnSaveWorkspace->SetBackgroundColour(fgColor);
    btnSaveWorkspace->SetForegroundColour(wxColour("#0A0A0B"));
    btnSaveWorkspace->SetAlignment(wxALIGN_CENTER);
    
    wsSizer->Add(pathSizer, 0, wxEXPAND | wxBOTTOM, 10);
    wsSizer->Add(chkCreateMissing, 0, wxBOTTOM, 10);
    wsSizer->Add(btnSaveWorkspace, 0, wxBOTTOM, 30);

    pageGlobalSettings->SetSizer(wsSizer);
    
    // Bind Setting events
    chkLaunchOnStartup->Bind(wxEVT_CHECKBOX, &Windows::OnSettingChanged, this);
    chkAutoStartServer->Bind(wxEVT_CHECKBOX, &Windows::OnSettingChanged, this);
    choiceAppMode->Bind(wxEVT_CHOICE, &Windows::OnSettingChanged, this);
    chkMinimizeToTray->Bind(wxEVT_CHECKBOX, &Windows::OnSettingChanged, this);
    chkShowNotifications->Bind(wxEVT_CHECKBOX, &Windows::OnSettingChanged, this);
    choiceLogRetention->Bind(wxEVT_CHOICE, &Windows::OnSettingChanged, this);
    chkAppLock->Bind(wxEVT_CHECKBOX, &Windows::OnSettingChanged, this);
    chkMaskSecrets->Bind(wxEVT_CHECKBOX, &Windows::OnSettingChanged, this);
    chkAutoUpdate->Bind(wxEVT_CHECKBOX, &Windows::OnSettingChanged, this);
    btnClearCache->Bind(wxEVT_BUTTON, &Windows::OnClearCache, this);
    btnCheckUpdates->Bind(wxEVT_BUTTON, &Windows::OnCheckUpdates, this);
    
    gsMainSizer->Add(pageGlobalSettings, 1, wxEXPAND);
    globalSettingsContainer->SetSizer(gsMainSizer);
    rootBook->AddPage(globalSettingsContainer, "GlobalSettingsContainer");

    // Assemble Main Panel
    rightSizer->Add(rootBook, 1, wxEXPAND | wxALL, 20);
    mainPanel->SetSizer(rightSizer);

    // Assemble main frame
    mainSizer->Add(sidebarPanel, 0, wxEXPAND);
    mainSizer->Add(mainPanel, 1, wxEXPAND);
    this->SetSizer(mainSizer);
    
    // Setup initial routing explicitly
    UpdateSidebarSelection(btnServerLocal);
    rootBook->ChangeSelection(0); // Server Container
    contentBook->ChangeSelection(0); // Overview
    UpdateTabSelection(btnTabOverview);
    
    // Bind server event
    Bind(wxEVT_SERVER_LOG, &Windows::OnServerLog, this);
}
wxBitmap RecolorIconBmp(const std::string& base64, wxColour targetColor) {
    wxImage img = GetBitmapFromBase64(base64).ConvertToImage();
    if (!img.IsOk()) return wxNullBitmap;
    if (!img.HasAlpha()) img.InitAlpha();
    unsigned char* data = img.GetData();
    unsigned char* alpha = img.GetAlpha();
    int pixelCount = img.GetWidth() * img.GetHeight();
    for (int i = 0; i < pixelCount; i++) {
        if (alpha && alpha[i] > 0) {
            data[i*3] = targetColor.Red();
            data[i*3+1] = targetColor.Green();
            data[i*3+2] = targetColor.Blue();
        }
    }
    return wxBitmap(img);
}

void Windows::ApplyTheme() {
    std::string theme = SettingsManager::Get().theme;
    
    wxColour fgColor, bgColor, panelColor, mutedColor, transparentBg;
    transparentBg = wxColour(0, 0, 0, 0);
    
    if (theme == "Light") {
        bgColor = wxColour("#F4F4F5");
        panelColor = wxColour("#FFFFFF");
        fgColor = wxColour("#18181B");
        mutedColor = wxColour("#71717A");
    } else {
        bgColor = wxColour("#0A0A0B");
        panelColor = wxColour("#141417");
        fgColor = wxColour("#F4F4F5");
        mutedColor = wxColour("#A1A1AA");
    }

    this->SetBackgroundColour(bgColor);
    mainPanel->SetBackgroundColour(bgColor);
    sidebarPanel->SetBackgroundColour(panelColor);
    
    if (globalSettingsContainer) globalSettingsContainer->SetBackgroundColour(bgColor);
    if (serverContainer) serverContainer->SetBackgroundColour(bgColor);
    if (headerPanel) headerPanel->SetBackgroundColour(bgColor);
    if (statsPanel) statsPanel->SetBackgroundColour(bgColor);
    
    if (lblGsHeader) lblGsHeader->SetForegroundColour(fgColor);
    if (logoText) logoText->SetForegroundColour(fgColor);
    if (lblStatusValue) lblStatusValue->SetForegroundColour(fgColor);
    
    if (statSessions) statSessions->SetForegroundColour(fgColor);
    if (statErrors) statErrors->SetForegroundColour(fgColor);
    if (statTools) statTools->SetForegroundColour(fgColor);
    if (statUptime) statUptime->SetForegroundColour(fgColor);
    
    if (sysStatsPanel) sysStatsPanel->SetBackgroundColour(panelColor);
    if (lblSysCpu) lblSysCpu->SetForegroundColour(fgColor);
    if (lblSysRam) lblSysRam->SetForegroundColour(fgColor);
    if (lblSysDisk) lblSysDisk->SetForegroundColour(fgColor);
    if (lblSysVram) lblSysVram->SetForegroundColour(fgColor);
    
    if (lblNetworkInstructions) lblNetworkInstructions->SetForegroundColour(mutedColor);
    if (lblBindInfo) lblBindInfo->SetForegroundColour(mutedColor);
    if (lblLocalUrl) lblLocalUrl->SetForegroundColour(fgColor);
    if (lblExternalUrl) lblExternalUrl->SetForegroundColour(fgColor);
    if (lblPublicIP) lblPublicIP->SetForegroundColour(fgColor);
    if (lblLogsHeader) lblLogsHeader->SetForegroundColour(mutedColor);

    
    btnServerLocal->SetBackgroundColour(transparentBg);
    btnServerLocal->SetForegroundColour(mutedColor);
    btnServerLocal->SetHoverColour(wxColour("#1F1F24")); // Darker gray hover
    
    btnCluster->SetBackgroundColour(transparentBg);
    btnCluster->SetForegroundColour(mutedColor);
    btnCluster->SetHoverColour(wxColour("#1F1F24"));
    
    btnTools->SetBackgroundColour(transparentBg);
    btnTools->SetForegroundColour(mutedColor);
    btnTools->SetHoverColour(wxColour("#1F1F24"));
    
    btnGlobalSettings->SetBackgroundColour(transparentBg);
    btnGlobalSettings->SetForegroundColour(mutedColor);
    btnGlobalSettings->SetHoverColour(wxColour("#1F1F24"));
    
    wxColour iconColor = (theme == "Light") ? mutedColor : wxColour("#F4F4F5");
    btnServerLocal->SetIcon(RecolorIconBmp(icon_server_png_base64, iconColor));
    btnCluster->SetIcon(RecolorIconBmp(icon_cluster_png_base64, iconColor));
    btnTools->SetIcon(RecolorIconBmp(icon_tools_png_base64, iconColor));
    btnGlobalSettings->SetIcon(RecolorIconBmp(icon_settings_png_base64, iconColor));
    
    btnTabOverview->SetBackgroundColour(transparentBg);
    btnTabOverview->SetForegroundColour(mutedColor);
    btnTabConnections->SetBackgroundColour(transparentBg);
    btnTabConnections->SetForegroundColour(mutedColor);
    btnTabLogs->SetBackgroundColour(transparentBg);
    btnTabLogs->SetForegroundColour(mutedColor);

    lblManager->SetForegroundColour(fgColor);
    lblStatus->SetForegroundColour(fgColor);
    
    pageOverview->SetBackgroundColour(bgColor);
    pageConnections->SetBackgroundColour(bgColor);
    pageLogs->SetBackgroundColour(bgColor);
    pageGlobalSettings->SetBackgroundColour(bgColor);
    
    // Global Settings Page foregrounds
    if (lblSecSystem) lblSecSystem->SetForegroundColour(mutedColor);
    if (lblSecStorage) lblSecStorage->SetForegroundColour(mutedColor);
    if (lblSecAppearance) lblSecAppearance->SetForegroundColour(mutedColor);
    if (lblSecSecurity) lblSecSecurity->SetForegroundColour(mutedColor);
    if (lblSecUpdates) lblSecUpdates->SetForegroundColour(mutedColor);
    if (lblSecLanguage) lblSecLanguage->SetForegroundColour(mutedColor);
    if (lblSecWorkspace) lblSecWorkspace->SetForegroundColour(mutedColor);
    
    if (chkLaunchOnStartup) chkLaunchOnStartup->SetForegroundColour(fgColor);
    if (chkAutoStartServer) chkAutoStartServer->SetForegroundColour(fgColor);
    if (chkMinimizeToTray) chkMinimizeToTray->SetForegroundColour(fgColor);
    if (chkShowNotifications) chkShowNotifications->SetForegroundColour(fgColor);
    if (lblLogRetention) lblLogRetention->SetForegroundColour(fgColor);
    if (lblThemeLabel) lblThemeLabel->SetForegroundColour(fgColor);
    if (chkCompactMode) chkCompactMode->SetForegroundColour(fgColor);
    if (chkAppLock) chkAppLock->SetForegroundColour(fgColor);
    if (chkMaskSecrets) chkMaskSecrets->SetForegroundColour(fgColor);
    if (chkAutoUpdate) chkAutoUpdate->SetForegroundColour(fgColor);
    if (chkCreateMissing) chkCreateMissing->SetForegroundColour(fgColor);
    
    if (txtWorkspacePath) {
        txtWorkspacePath->SetBackgroundColour(panelColor);
        txtWorkspacePath->SetForegroundColour(fgColor);
    }
    if (txtLogs) {
        txtLogs->SetBackgroundColour(panelColor);
        txtLogs->SetForegroundColour(fgColor);
    }
    if (txtCustomDomain) {
        txtCustomDomain->SetBackgroundColour(panelColor);
        txtCustomDomain->SetForegroundColour(fgColor);
    }
    
    if (gridTokens) {
        gridTokens->SetDefaultCellBackgroundColour(panelColor);
        gridTokens->SetDefaultCellTextColour(fgColor);
        gridTokens->SetLabelBackgroundColour(bgColor);
        gridTokens->SetLabelTextColour(mutedColor);
    }
    
    bool compact = SettingsManager::Get().compactMode;
    int sidebarWidth = compact ? 80 : 240;
    int btnWidth = compact ? 60 : 220;
    
    currentSidebarWidth = sidebarWidth;
    targetSidebarWidth = sidebarWidth;
    currentIconAngle = compact ? M_PI : 0.0;
    targetIconAngle = currentIconAngle;
    
    sidebarPanel->SetMinSize(wxSize(sidebarWidth, -1));
    btnServerLocal->SetMinSize(wxSize(btnWidth, 40));
    btnTools->SetMinSize(wxSize(btnWidth, 40));
    btnGlobalSettings->SetMinSize(wxSize(btnWidth, 40));
    
    if (compact) {
        logoImg->Hide();
        logoText->Hide();
        btnToggleTheme->Hide();
        btnServerLocal->SetIndent(0);
        btnCluster->SetIndent(0);
        btnTools->SetIndent(0);
        btnGlobalSettings->SetIndent(0);
    } else {
        logoImg->Show();
        logoText->Show();
        btnToggleTheme->Show();
        btnServerLocal->SetIndent(15);
        btnCluster->SetIndent(15);
        btnTools->SetIndent(15);
        btnGlobalSettings->SetIndent(15);
    }
    
    this->Layout();
    this->Refresh(true);
    this->Update();
}

void Windows::UpdateLanguage() {
    auto& lang = LanguageManager::Get();
    bool compact = SettingsManager::Get().compactMode;
    
    btnServerLocal->SetLabel(compact ? "" : lang.GetString("SERVER_LOCAL"));
    btnCluster->SetLabel(compact ? "" : lang.GetString("CLUSTER_NODES"));
    btnTools->SetLabel(compact ? "" : lang.GetString("TOOLS"));
    btnGlobalSettings->SetLabel(compact ? "" : lang.GetString("GLOBAL_SETTINGS"));
    
    btnTabOverview->SetLabel(lang.GetString("OVERVIEW"));
    btnTabConnections->SetLabel(lang.GetString("CONNECTIONS"));
    btnTabLogs->SetLabel(lang.GetString("LOGS"));

    lblManager->SetLabel(lang.GetString("SERVER_MANAGER"));
    lblStatus->SetLabel(lang.GetString("STATUS"));
    
    if (isServerRunning) {
        lblStatusValue->SetLabel(lang.GetString("ONLINE"));
        btnStartStop->SetLabel(lang.GetString("STOP_BTN"));
    } else {
        lblStatusValue->SetLabel(lang.GetString("OFFLINE"));
        btnStartStop->SetLabel(lang.GetString("START_BTN"));
    }

    // lblLogsHeader->SetLabel(lang.GetString("LOGS_HEADER")); // Now handled by section headers
    // lblWorkspaceStatus and lblWorkspaceValue are no longer used here
    statSessions->SetLabel(lang.GetString("STATS_SESSIONS") + wxString("0"));
    statErrors->SetLabel(lang.GetString("STATS_ERRORS") + wxString("0"));
    statTools->SetLabel(lang.GetString("STATS_TOOLS") + wxString("0"));
    statUptime->SetLabel(lang.GetString("STATS_UPTIME") + wxString::Format("%ds", uptimeSeconds.load()));

    // lblWorkspaceTitle->SetLabel(lang.GetString("WORKSPACE_TITLE")); // Now handled by section headers
    
    chkCreateMissing->SetLabel(lang.GetString("CREATE_IF_MISSING"));
    chkCreateMissing->InvalidateBestSize(); // Fix truncation on Linux
    
    lblSecSystem->SetLabel(lang.GetString("GS_SYSTEM"));
    lblSecStorage->SetLabel(lang.GetString("GS_STORAGE"));
    if (lblSecAppearance) lblSecAppearance->SetLabel(lang.GetString("GS_APPEARANCE"));
    if (lblSecSecurity) lblSecSecurity->SetLabel(lang.GetString("GS_SECURITY"));
    if (lblSecUpdates) lblSecUpdates->SetLabel(lang.GetString("GS_UPDATES"));
    if (lblSecLanguage) lblSecLanguage->SetLabel(lang.GetString("GS_LANGUAGE"));
    if (lblSecWorkspace) lblSecWorkspace->SetLabel(lang.GetString("GS_WORKSPACE"));
    
    chkLaunchOnStartup->SetLabel(lang.GetString("GS_LAUNCH_STARTUP"));
    if (chkAutoStartServer) chkAutoStartServer->SetLabel(lang.GetString("GS_AUTO_START_SERVER"));
    chkMinimizeToTray->SetLabel(lang.GetString("GS_MINIMIZE_TRAY"));
    chkShowNotifications->SetLabel(lang.GetString("GS_NOTIFICATIONS"));
    
    lblLogRetention->SetLabel(lang.GetString("GS_LOG_RETENTION"));
    btnClearCache->SetLabel(lang.GetString("GS_CLEAR_CACHE"));
    
    if (lblThemeLabel) lblThemeLabel->SetLabel(lang.GetString("GS_THEME"));
    if (chkCompactMode) chkCompactMode->SetLabel(lang.GetString("GS_COMPACT_MODE"));
    
    chkAppLock->SetLabel(lang.GetString("GS_APP_LOCK"));
    chkMaskSecrets->SetLabel(lang.GetString("GS_MASK_SECRETS"));
    
    chkAutoUpdate->SetLabel(lang.GetString("GS_AUTO_UPDATE"));
    if (lblCurrentVersion) lblCurrentVersion->SetLabel(lang.GetString("LBL_VERSION") + APP_VERSION);
    btnCheckUpdates->SetLabel(lang.GetString("GS_CHECK_UPDATES"));
    
    if (lblAppMode) lblAppMode->SetLabel(lang.GetString("GS_APP_MODE"));
    if (choiceAppMode) {
        int sel = choiceAppMode->GetSelection();
        choiceAppMode->SetString(0, lang.GetString("GS_MODE_PARENT"));
        choiceAppMode->SetString(1, lang.GetString("GS_MODE_CHILD"));
        choiceAppMode->SetSelection(sel);
    }
    
    // Cluster UI
    if (lblClusterTitle) lblClusterTitle->SetLabel(lang.GetString("CLUSTER_MANAGEMENT"));
    if (lblClusterSub) lblClusterSub->SetLabel(lang.GetString("CLUSTER_SUB"));
    if (btnAddNode) btnAddNode->SetLabel(lang.GetString("BTN_ADD_NODE"));
    if (btnApproveNode) btnApproveNode->SetLabel(lang.GetString("BTN_APPROVE"));
    if (btnRejectNode) btnRejectNode->SetLabel(lang.GetString("BTN_REJECT"));
    
    // Tools UI
    if (lblToolsHeader) lblToolsHeader->SetLabel(lang.GetString("TOOLS_MANAGEMENT"));
    
    if (listNodes && listNodes->GetColumnCount() == 5) {
        wxListItem col;
        col.SetMask(wxLIST_MASK_TEXT);
        col.SetText(lang.GetString("NODE_ID"));
        listNodes->SetColumn(0, col);
        col.SetText(lang.GetString("HOSTNAME"));
        listNodes->SetColumn(1, col);
        col.SetText(lang.GetString("IP_ADDRESS"));
        listNodes->SetColumn(2, col);
        col.SetText(lang.GetString("STATUS"));
        listNodes->SetColumn(3, col);
        col.SetText(lang.GetString("PLATFORM"));
        listNodes->SetColumn(4, col);
    }
    
    pageGlobalSettings->Layout();
    
    // Network tab instructions removed, layout has its own labels

    btnLangRu->SetBackgroundColour(lang.GetLanguage() == "RU" ? wxColour("#1F1F24") : wxColour("#141417"));
    btnLangRu->SetForegroundColour(lang.GetLanguage() == "RU" ? wxColour(*wxWHITE) : wxColour("#A1A1AA"));
    btnLangEn->SetBackgroundColour(lang.GetLanguage() == "EN" ? wxColour("#1F1F24") : wxColour("#141417"));
    btnLangEn->SetForegroundColour(lang.GetLanguage() == "EN" ? wxColour(*wxWHITE) : wxColour("#A1A1AA"));

    Layout();
}

void Windows::UpdateSidebarSelection(CustomButton* selected) {
    btnServerLocal->SetSelected(btnServerLocal == selected);
    btnCluster->SetSelected(btnCluster == selected);
    btnTools->SetSelected(btnTools == selected);
    btnGlobalSettings->SetSelected(btnGlobalSettings == selected);
}

void Windows::UpdateTabSelection(CustomButton* selected) {
    if (btnTabOverview) btnTabOverview->SetSelected(btnTabOverview == selected);
    if (btnTabConnections) btnTabConnections->SetSelected(btnTabConnections == selected);
    if (btnTabLogs) btnTabLogs->SetSelected(btnTabLogs == selected);
}

void Windows::OnSidebarServerLocal(wxCommandEvent& event) {
    UpdateSidebarSelection(btnServerLocal);
    rootBook->ChangeSelection(0); // ServerContainer
    contentBook->ChangeSelection(0); // Overview
    UpdateTabSelection(btnTabOverview);
}

void Windows::OnSidebarTools(wxCommandEvent& event) {
    UpdateSidebarSelection(btnTools);
    rootBook->ChangeSelection(2); // ToolsContainer
    UpdateTabSelection(nullptr);
}

void Windows::OnSidebarGlobalSettings(wxCommandEvent& event) {
    UpdateSidebarSelection(btnGlobalSettings);
    rootBook->ChangeSelection(3); // GlobalSettingsContainer
    UpdateTabSelection(nullptr);
}

void Windows::OnSidebarCluster(wxCommandEvent& event) {
    UpdateSidebarSelection(btnCluster);
    rootBook->ChangeSelection(1); // ClusterContainer
    UpdateTabSelection(nullptr);
    RefreshNodesList();
}

void Windows::OnApproveNode(wxCommandEvent& event) {
    long item = listNodes->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1) {
        wxMessageBox("Please select a node first.", "Error", wxICON_ERROR);
        return;
    }
    
    std::string nodeId = listNodes->GetItemText(item).ToStdString();
    ClusterManager::GetInstance().ApproveNode(nodeId);
    RefreshNodesList();
}

void Windows::OnRejectNode(wxCommandEvent& event) {
    long item = listNodes->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1) {
        wxMessageBox("Please select a node first.", "Error", wxICON_ERROR);
        return;
    }
    
    std::string nodeId = listNodes->GetItemText(item).ToStdString();
    ClusterManager::GetInstance().RemoveNode(nodeId);
    RefreshNodesList();
}

void Windows::OnAddNode(wxCommandEvent& event) {
    // Show a dialog to manually add a node
    wxTextEntryDialog dlg(this, "Enter Child Node URL (e.g. http://192.168.1.100:3000):", "Add Manual Node");
    if (dlg.ShowModal() == wxID_OK) {
        std::string url = dlg.GetValue().ToStdString();
        // Generate a random ID for manual nodes or ask for it
        std::string nodeId = "manual_" + std::to_string(rand() % 10000);
        
        ClusterManager::GetInstance().RegisterNodeRequest(nodeId, url, url, "manual");
        ClusterManager::GetInstance().ApproveNode(nodeId); // Auto approve manual
        RefreshNodesList();
    }
}

void Windows::OnReconnectNode(wxCommandEvent& event) {
    long item = -1;
    item = listNodes->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1) {
        std::string id = listNodes->GetItemText(item).ToStdString();
        ClusterManager::GetInstance().SetNodeStatus(id, "connecting...");
        RefreshNodesList();
        
        std::thread([id]() {
            ClusterManager::GetInstance().ApproveNode(id);
        }).detach();
    } else {
        std::thread([]() {
            ClusterManager::GetInstance().ReconnectAllNodes();
        }).detach();
    }
}

void Windows::OnNodeIdEdited(wxListEvent& event) {
    long item = event.GetIndex();
    wxString newId = event.GetLabel();
    if (newId.IsEmpty() || item == -1) {
        event.Veto();
        return;
    }
    
    // Get the old ID which is currently in the list before the edit is accepted
    std::string oldId = listNodes->GetItemText(item).ToStdString();
    
    // Update the ID in the ClusterManager
    if (!ClusterManager::GetInstance().UpdateNodeId(oldId, newId.ToStdString())) {
        wxMessageBox("Node ID already exists or invalid.", "Error", wxICON_ERROR);
        event.Veto();
        return;
    }
    
    // Let wxWidgets accept the new label
    event.Allow();
}

void Windows::RefreshNodesList() {
    listNodes->DeleteAllItems();
    
    auto nodes = ClusterManager::GetInstance().GetNodes();
    long idx = 0;
    
    for (const auto& node : nodes) {
        listNodes->InsertItem(idx, node.id);
        listNodes->SetItem(idx, 1, node.hostname);
        listNodes->SetItem(idx, 2, node.ip_address);
        listNodes->SetItem(idx, 3, node.status);
        listNodes->SetItem(idx, 4, node.platform);
        listNodes->SetItemBackgroundColour(idx, wxColour("#18181B"));
        idx++;
    }
}

void Windows::OnTabOverview(wxCommandEvent& event) {
    contentBook->ChangeSelection(0);
    UpdateTabSelection(btnTabOverview);
}

void Windows::OnTabConnections(wxCommandEvent& event) {
    contentBook->ChangeSelection(1);
    UpdateTabSelection(btnTabConnections);
}

void Windows::OnTabLogs(wxCommandEvent& event) {
    contentBook->ChangeSelection(2);
    UpdateTabSelection(btnTabLogs);
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
        
        m_timer->Start(1000);
        
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

void Windows::OnServerNotify(wxThreadEvent& event) {
    if (SettingsManager::Get().showNotifications) {
        wxString payload = event.GetString();
        int sepIdx = payload.Find("|||");
        wxString title = "Notification";
        wxString msg = payload;
        if (sepIdx != wxNOT_FOUND) {
            title = payload.Mid(0, sepIdx);
            msg = payload.Mid(sepIdx + 3);
        }
        wxNotificationMessage notif(title, msg);
        notif.Show();
    }
}

void Windows::OnServerLog(wxThreadEvent& event) {
    txtLogs->AppendText(event.GetString() + "\n");
}

void Windows::OnTimer(wxTimerEvent& event) {
    auto& lang = LanguageManager::Get();
    if (isServerRunning) {
        uptimeSeconds++;
        statUptime->SetLabel(lang.GetString("STATS_UPTIME") + wxString::Format("%ds", uptimeSeconds.load()));
        statSessions->SetLabel(lang.GetString("STATS_SESSIONS") + wxString::Format("%d", g_active_sessions.load()));
        statTools->SetLabel(lang.GetString("STATS_TOOLS") + wxString::Format("%d", g_tool_calls.load()));
    }
    
    if (sysStatsPanel && sysStatsPanel->IsShownOnScreen()) {
        SystemStats stats = GetSystemStats();
        
        lblSysCpu->SetLabel(wxString::Format("%s: %.1f%%", lang.GetString("LBL_CPU").c_str(), stats.cpuPercent));
        gaugeSysCpu->SetValue(std::min((int)stats.cpuPercent, 100));
        
        lblSysRam->SetLabel(wxString::Format("%s: %.1f / %.1f GB (%.0f%%)", lang.GetString("LBL_RAM").c_str(), stats.ramUsedGB, stats.ramTotalGB, stats.ramPercent));
        gaugeSysRam->SetValue(std::min((int)stats.ramPercent, 100));
        
        lblSysDisk->SetLabel(wxString::Format("%s: %.1f / %.1f GB (%.0f%%)", lang.GetString("LBL_DISK").c_str(), stats.diskUsedGB, stats.diskTotalGB, stats.diskPercent));
        gaugeSysDisk->SetValue(std::min((int)stats.diskPercent, 100));
        
        if (stats.hasVRAM) {
            lblSysVram->SetLabel(wxString::Format("%s: %.1f / %.1f GB (%.0f%%)", lang.GetString("LBL_VRAM").c_str(), stats.vramUsedGB, stats.vramTotalGB, stats.vramPercent));
            gaugeSysVram->SetValue(std::min((int)stats.vramPercent, 100));
        } else {
            lblSysVram->SetLabel(wxString::Format("%s: N/A", lang.GetString("LBL_VRAM").c_str()));
            gaugeSysVram->SetValue(0);
        }
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
    txtWorkspacePath->SetValue(wxString::FromUTF8(currentWorkspace.c_str()));
    Layout();
    
    wxThreadEvent* logEvent = new wxThreadEvent(wxEVT_SERVER_LOG);
    logEvent->SetString("[GUI] Workspace set to: " + currentWorkspace);
    wxQueueEvent(this, logEvent);
    
    wxMessageBox("Workspace updated successfully!", "Success", wxICON_INFORMATION);
}

void Windows::OnCopyTokenUrl(wxCommandEvent& event) {
    wxArrayInt selections = gridTokens->GetSelectedRows();
    int row = -1;
    if (selections.IsEmpty()) {
        row = gridTokens->GetGridCursorRow();
    } else {
        row = selections[0];
    }
    
    if (row < 0 || row >= gridTokens->GetNumberRows()) {
        wxMessageBox("Please select a token from the list first.", "Information", wxICON_INFORMATION);
        return;
    }
    wxString raw_token = gridTokens->GetCellValue(row, 2);
    if (raw_token.Contains("*")) {
        // If it's masked, we need to get it from network utils
        auto tokens = NetworkUtils::LoadTokens();
        std::string id = gridTokens->GetCellValue(row, 1).ToStdString();
        for (const auto& t : tokens) {
            if (t.id == id) {
                raw_token = t.raw_token;
                break;
            }
        }
    }
    
    std::string ip = NetworkUtils::GetPublicIP();
    if (ip == "127.0.0.1") {
        wxMessageBox("Could not determine public IP. Copying localhost URL instead.", "Warning", wxICON_WARNING);
    }
    
    std::string url = txtCustomDomain ? txtCustomDomain->GetValue().ToStdString() : "";
    if (url.empty()) {
        url = "http://" + ip + ":3000/sse";
    }
    
    url += "?token=" + raw_token.ToStdString();
    
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(url));
        wxTheClipboard->Close();
        
        wxThreadEvent* logEvent = new wxThreadEvent(wxEVT_SERVER_LOG);
        logEvent->SetString("[GUI] Token URL copied to clipboard.");
        wxQueueEvent(this, logEvent);
        
        wxMessageBox("Connection URL copied to clipboard!", "Success", wxICON_INFORMATION);
    }
}

void Windows::OnSelectLangRu(wxCommandEvent& event) {
    LanguageManager::Get().SetLanguage("RU");
    SettingsManager::Get().language = "RU";
    SettingsManager::Get().Save();
    UpdateLanguage();
}

void Windows::OnSelectLangEn(wxCommandEvent& event) {
    LanguageManager::Get().SetLanguage("EN");
    SettingsManager::Get().language = "EN";
    SettingsManager::Get().Save();
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
        if (SettingsManager::Get().maskSecrets && t.raw_token.length() > 8) {
            std::string masked = t.raw_token.substr(0, 4) + "****************" + t.raw_token.substr(t.raw_token.length() - 4);
            gridTokens->SetCellValue(i, 2, masked);
        } else {
            gridTokens->SetCellValue(i, 2, t.raw_token);
        }
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

#include "../mcp/tools.h"

void Windows::RefreshToolsList() {
    scrollToolsSizer->Clear(true);
    
    try {
        nlohmann::json tools = get_all_tools();
        if (tools.is_array()) {
            for (const auto& tool : tools) {
                std::string name = tool.value("name", "Unknown");
                std::string desc = tool.value("description", "");
                
                auto& disabled = SettingsManager::Get().disabled_tools;
                bool is_enabled = std::find(disabled.begin(), disabled.end(), name) == disabled.end();
                
                wxPanel* toolRow = new wxPanel(scrollTools, wxID_ANY);
                toolRow->SetBackgroundColour(wxColour("#27272A")); // Zinc 800 for cards inside
                
                wxBoxSizer* rowSizer = new wxBoxSizer(wxHORIZONTAL);
                
                wxCheckBox* chk = new wxCheckBox(toolRow, wxID_ANY, "");
                chk->SetValue(is_enabled);
                chk->Bind(wxEVT_CHECKBOX, &Windows::OnToolToggled, this);
                chk->SetName(name);
                
                wxBoxSizer* textSizer = new wxBoxSizer(wxVERTICAL);
                wxStaticText* lblName = new wxStaticText(toolRow, wxID_ANY, name);
                lblName->SetFont(wxFontInfo(11).Bold());
                lblName->SetForegroundColour(wxColour("#F4F4F5"));
                
                wxStaticText* lblDesc = new wxStaticText(toolRow, wxID_ANY, desc);
                lblDesc->SetFont(wxFontInfo(10));
                lblDesc->SetForegroundColour(wxColour("#A1A1AA"));
                
                textSizer->Add(lblName, 0, wxBOTTOM, 4);
                textSizer->Add(lblDesc, 0);
                
                rowSizer->Add(chk, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 15);
                rowSizer->Add(textSizer, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 10);
                
                toolRow->SetSizer(rowSizer);
                
                scrollToolsSizer->Add(toolRow, 0, wxEXPAND | wxBOTTOM, 10);
            }
        }
    } catch (...) {}
    
    scrollTools->SetSizer(scrollToolsSizer);
    scrollTools->Layout();
    scrollTools->FitInside();
}


void Windows::OnToolToggled(wxCommandEvent& event) {
    wxCheckBox* chk = dynamic_cast<wxCheckBox*>(event.GetEventObject());
    if (!chk) return;
    
    std::string tool_name = chk->GetName().ToStdString();
    bool isChecked = chk->GetValue();
    
    auto& disabled = SettingsManager::Get().disabled_tools;
    if (isChecked) {
        auto it = std::find(disabled.begin(), disabled.end(), tool_name);
        if (it != disabled.end()) disabled.erase(it);
    } else {
        if (std::find(disabled.begin(), disabled.end(), tool_name) == disabled.end()) {
            disabled.push_back(tool_name);
        }
    }
    SettingsManager::Get().Save();
}

void Windows::RefreshConnectionUrls() {
    wxString custom = txtCustomDomain->GetValue();
    std::string baseUrl;
    if (!custom.IsEmpty()) {
        baseUrl = custom.ToStdString();
        if (baseUrl.back() == '/') baseUrl.pop_back();
    } else {
        std::string ip = lblPublicIP->GetLabel().ToStdString();
        if (ip == LanguageManager::Get().GetString("LBL_UNKNOWN").ToStdString() || ip.empty() || ip == "Unknown") {
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
    std::vector<TokenInfo> originalTokens = NetworkUtils::LoadTokens();
    std::vector<TokenInfo> tokens;
    for (int i = 0; i < gridTokens->GetNumberRows(); ++i) {
        TokenInfo t;
        t.name = gridTokens->GetCellValue(i, 0).ToStdString();
        t.id = gridTokens->GetCellValue(i, 1).ToStdString();
        std::string rawFromGrid = gridTokens->GetCellValue(i, 2).ToStdString();
        
        auto it = std::find_if(originalTokens.begin(), originalTokens.end(), [&](const TokenInfo& o) { return o.id == t.id; });
        if (it != originalTokens.end()) {
            if (rawFromGrid.find("***") != std::string::npos) {
                t.raw_token = it->raw_token; // keep original unmasked
            } else {
                t.raw_token = rawFromGrid;
            }
            t.permissions = it->permissions;
        } else {
            t.raw_token = rawFromGrid;
        }
        
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

void Windows::OnEditPermissions(wxCommandEvent& event) {
    wxArrayInt selRows = gridTokens->GetSelectedRows();
    int row = -1;
    if (selRows.IsEmpty()) {
        row = gridTokens->GetGridCursorRow();
    } else {
        row = selRows[0];
    }
    
    if (row < 0 || row >= gridTokens->GetNumberRows()) {
        wxMessageBox("Please select a token to edit permissions.", "Error", wxICON_ERROR);
        return;
    }
    
    wxString token_id = gridTokens->GetCellValue(row, 1);
    
    // Find actual token string from id
    auto tokens = NetworkUtils::LoadTokens();
    std::string token_str = "";
    for (const auto& t : tokens) {
        if (t.id == token_id.ToStdString()) {
            token_str = t.id; // Passing ID to permissions dialog!
            break;
        }
    }
    
    if (!token_str.empty()) {
        PermissionsDialog dlg(this, token_str);
        if (dlg.ShowModal() == wxID_OK) {
            RefreshTokenList();
        }
    } else {
        wxMessageBox("Token not found.", "Error", wxICON_ERROR);
    }
}

void Windows::OnGridSize(wxSizeEvent& event) {
    if (!gridTokens) {
        event.Skip();
        return;
    }
    int w, h;
    gridTokens->GetClientSize(&w, &h);
    int fixedWidths = 120 + 150 + 160 + 80;
    int tokenWidth = w - fixedWidths;
    if (tokenWidth < 150) tokenWidth = 150;
    gridTokens->SetColSize(2, tokenWidth);
    event.Skip();
}

void Windows::OnCheckNetwork(wxCommandEvent& event) {
    lblCheckResult->SetLabel(LanguageManager::Get().GetString("LBL_CHECKING"));
    lblCheckResult->SetForegroundColour(wxColour("#13293D"));
    lblCheckResult->Refresh();
    pageConnections->Update(); // Force paint immediately
    wxYield(); // Allow GTK to draw
    
    std::cout << "[Check] Getting Public IP..." << std::endl;
    std::string ip = NetworkUtils::GetPublicIP();
    std::cout << "[Check] IP = " << ip << std::endl;
    lblPublicIP->SetLabel(ip == "Unknown" ? LanguageManager::Get().GetString("LBL_UNKNOWN") : wxString(ip));
    RefreshConnectionUrls();
    pageConnections->Update();
    wxYield();
    
    bool localOk = NetworkUtils::CheckLocalAccess(3000);
    pageConnections->Update();
    wxYield();

    std::cout << "[Check] Checking External Access..." << std::endl;
    bool extOk = false;
    if (ip != "Unknown") {
        extOk = NetworkUtils::CheckExternalAccess(ip, 3000);
    }
    std::cout << "[Check] Ext = " << extOk << std::endl;
    
    if (extOk) {
        lblCheckResult->SetLabel(LanguageManager::Get().GetString("LBL_REACH_EXT"));
        lblCheckResult->SetForegroundColour(wxColour("#2A9D8F")); // Teal/Green
    } else if (localOk) {
        lblCheckResult->SetLabel(LanguageManager::Get().GetString("LBL_REACH_LOC"));
        lblCheckResult->SetForegroundColour(wxColour("#E9C46A")); // Yellow/Orange
    } else {
        lblCheckResult->SetLabel(LanguageManager::Get().GetString("LBL_NOT_REACH"));
        lblCheckResult->SetForegroundColour(wxColour("#D62828")); // Red
    }
    
    lblCheckResult->Refresh();
    lblCheckResult->Update();
}

void Windows::OnCustomDomainChanged(wxCommandEvent& event) {
    RefreshConnectionUrls();
}


void Windows::OnSettingChanged(wxCommandEvent& event) {
    SettingsManager& sm = SettingsManager::Get();
    
    // Check if AppLock is being enabled
    if (chkAppLock->GetValue() && !sm.appLock) {
        wxPasswordEntryDialog dialog(this, "Set a Master Password for the Dashboard", "Set Password");
        if (dialog.ShowModal() == wxID_OK && !dialog.GetValue().IsEmpty()) {
            sm.appPasswordHash = HashPassword(dialog.GetValue().ToStdString());
            sm.appLock = true;
        } else {
            chkAppLock->SetValue(false);
            sm.appLock = false;
        }
    } else if (!chkAppLock->GetValue()) {
        sm.appLock = false;
        sm.appPasswordHash = "";
    }
    
    sm.launchOnStartup = chkLaunchOnStartup->GetValue();
    if (chkAutoStartServer) sm.autoStartServer = chkAutoStartServer->GetValue();
    sm.minimizeToTray = chkMinimizeToTray->GetValue();
    sm.showNotifications = chkShowNotifications->GetValue();
    sm.logRetention = choiceLogRetention->GetStringSelection().ToStdString();
    sm.appMode = choiceAppMode->GetStringSelection().ToStdString();
    
    sm.appLock = chkAppLock->GetValue();
    sm.maskSecrets = chkMaskSecrets->GetValue();
    sm.autoUpdate = chkAutoUpdate->GetValue();
    
    sm.Save();
    
    // Immediate applies
    ApplyTheme(); // in case theme or compact mode changed
    UpdateLanguage(); // to refresh labels based on compact mode
    RefreshTokenList(); // in case mask changed
    
    // Launch on startup logic
#if defined(__linux__)
    std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
    if (!homeDir.empty()) {
        std::filesystem::path autostartDir = std::filesystem::path(homeDir) / ".config" / "autostart";
        std::filesystem::path desktopPath = autostartDir / "mcp_manager.desktop";
        
        if (sm.launchOnStartup) {
            std::error_code ec;
            if (!std::filesystem::exists(autostartDir)) {
                std::filesystem::create_directories(autostartDir, ec);
            }
            wxString exe = wxStandardPaths::Get().GetExecutablePath();
            std::ofstream f(desktopPath.string());
            if (f.is_open()) {
                f << "[Desktop Entry]\nType=Application\nName=MCP Server Manager\nExec=" << exe.ToStdString() << "\nTerminal=false\n";
                f.close();
            }
        } else {
            std::error_code ec;
            if (std::filesystem::exists(desktopPath)) {
                std::filesystem::remove(desktopPath, ec);
            }
        }
    }
#elif defined(__WXMSW__) || defined(_WIN32)
    wxRegKey key(wxRegKey::HKCU, "Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    if (sm.launchOnStartup) {
        wxString exe = wxStandardPaths::Get().GetExecutablePath();
        key.SetValue("MCPServerManager", exe);
    } else {
        if (key.HasValue("MCPServerManager")) {
            key.DeleteValue("MCPServerManager");
        }
    }
#elif defined(__APPLE__)
    std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
    if (!homeDir.empty()) {
        std::filesystem::path launchAgentsDir = std::filesystem::path(homeDir) / "Library" / "LaunchAgents";
        std::filesystem::path plistPath = launchAgentsDir / "com.mcp.manager.plist";
        
        if (sm.launchOnStartup) {
            std::error_code ec;
            if (!std::filesystem::exists(launchAgentsDir)) {
                std::filesystem::create_directories(launchAgentsDir, ec);
            }
            wxString exe = wxStandardPaths::Get().GetExecutablePath();
            std::ofstream f(plistPath.string());
            if (f.is_open()) {
                f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
                f << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
                f << "<plist version=\"1.0\">\n";
                f << "<dict>\n";
                f << "    <key>Label</key>\n";
                f << "    <string>com.mcp.manager</string>\n";
                f << "    <key>ProgramArguments</key>\n";
                f << "    <array>\n";
                f << "        <string>" << exe.ToStdString() << "</string>\n";
                f << "    </array>\n";
                f << "    <key>RunAtLoad</key>\n";
                f << "    <true/>\n";
                f << "</dict>\n";
                f << "</plist>\n";
                f.close();
            }
        } else {
            std::error_code ec;
            if (std::filesystem::exists(plistPath)) {
                std::filesystem::remove(plistPath, ec);
            }
        }
    }
#endif
}

void Windows::OnClearCache(wxCommandEvent& event) {
    if (txtLogs) txtLogs->Clear();
    
    std::string logPath = currentWorkspace + "/logs";
    try {
        if (std::filesystem::exists(logPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(logPath)) {
                std::filesystem::remove_all(entry.path());
            }
        }
    } catch (...) {}
    
    if (SettingsManager::Get().showNotifications) {
        wxNotificationMessage notif("MCP Server Manager", "Cache and Logs cleared successfully.");
        notif.Show();
    } else {
        wxMessageBox("Cache and Logs cleared successfully.", "Success", wxICON_INFORMATION);
    }
}

void Windows::OnCheckUpdates(wxCommandEvent& event) {
    auto& lang = LanguageManager::Get();
    lblUpdateStatus->SetLabel(lang.GetString("LBL_CHECKING"));
    wxYield();

    wxArrayString output;
    wxArrayString errors;
    long res = wxExecute("curl -s https://api.github.com/repos/XayXay-jpg/MCP_Server_C-/releases/latest", output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE);

    std::string jsonStr;
    for (const auto& line : output) {
        jsonStr += line.ToStdString() + "\n";
    }

    try {
        if (!jsonStr.empty()) {
            nlohmann::json j = nlohmann::json::parse(jsonStr);
            if (j.contains("tag_name")) {
                std::string latest_ver = j["tag_name"].get<std::string>();
                if (latest_ver == "v" + APP_VERSION || latest_ver == APP_VERSION) {
                    lblUpdateStatus->SetLabel(lang.GetString("UPD_LATEST"));
                } else {
                    lblUpdateStatus->SetLabel(lang.GetString("UPD_AVAILABLE") + latest_ver);
                }
            } else if (j.contains("message") && j["message"] == "Not Found") {
                lblUpdateStatus->SetLabel(lang.GetString("UPD_NONE"));
            } else {
                lblUpdateStatus->SetLabel(lang.GetString("UPD_ERR_PARSE"));
            }
        } else {
            lblUpdateStatus->SetLabel(lang.GetString("UPD_ERR_NET"));
        }
    } catch (...) {
        lblUpdateStatus->SetLabel(lang.GetString("UPD_ERR_PARSE"));
    }
}

void Windows::OnCloseWindow(wxCloseEvent& event) {
    if (event.CanVeto() && SettingsManager::Get().minimizeToTray) {
        event.Veto();
        this->Hide();
    } else {
        event.Skip();
    }
}

void Windows::OnTrayIconActivated(wxTaskBarIconEvent& event) {
    this->Show();
    this->Raise();
}

wxBEGIN_EVENT_TABLE(Windows, wxFrame)
    EVT_BUTTON(ID_BTN_SERVER_LOCAL, Windows::OnSidebarServerLocal)
    EVT_BUTTON(ID_BTN_CLUSTER,      Windows::OnSidebarCluster)
    EVT_BUTTON(ID_BTN_TOOLS,        Windows::OnSidebarTools)
    EVT_BUTTON(ID_BTN_GLOBAL_SETTINGS, Windows::OnSidebarGlobalSettings)
    EVT_BUTTON(ID_BTN_TAB_OVERVIEW,  Windows::OnTabOverview)
    EVT_BUTTON(ID_BTN_TAB_CONNECTIONS,Windows::OnTabConnections)
    EVT_BUTTON(ID_BTN_TAB_LOGS,      Windows::OnTabLogs)
    EVT_BUTTON(ID_BTN_STARTSTOP, Windows::OnStartStop)
    EVT_TIMER(ID_TIMER,          Windows::OnTimer)
    EVT_BUTTON(ID_BTN_BROWSE_WS, Windows::OnBrowseWorkspace)
    EVT_BUTTON(ID_BTN_SAVE_WS,   Windows::OnSaveWorkspace)
    EVT_BUTTON(ID_BTN_LANG_RU,   Windows::OnSelectLangRu)
    EVT_BUTTON(ID_BTN_LANG_EN,   Windows::OnSelectLangEn)
    EVT_BUTTON(ID_BTN_CREATE_TOKEN, Windows::OnCreateToken)
    EVT_BUTTON(ID_BTN_DELETE_TOKEN,  Windows::OnDeleteToken)
    EVT_BUTTON(ID_BTN_TOGGLE_TOKEN,  Windows::OnToggleToken)
    EVT_BUTTON(ID_BTN_EDIT_PERMISSIONS, Windows::OnEditPermissions)
    EVT_BUTTON(ID_BTN_COPY_TOKEN_URL, Windows::OnCopyTokenUrl)
    EVT_BUTTON(ID_BTN_CHECK_NETWORK, Windows::OnCheckNetwork)
    EVT_GRID_CELL_CHANGED(Windows::OnTokenCellChanged)

    EVT_TEXT(ID_TXT_CUSTOM_DOMAIN,  Windows::OnCustomDomainChanged)
    EVT_BUTTON(ID_BTN_APPROVE_NODE, Windows::OnApproveNode)
    EVT_BUTTON(ID_BTN_REJECT_NODE,  Windows::OnRejectNode)
    EVT_BUTTON(ID_BTN_ADD_NODE,     Windows::OnAddNode)
    EVT_BUTTON(ID_BTN_RECONNECT_NODE, Windows::OnReconnectNode)
    EVT_BUTTON(ID_BTN_TOGGLE_COMPACT, Windows::OnToggleCompact)
    EVT_BUTTON(ID_BTN_TOGGLE_THEME, Windows::OnToggleTheme)
    EVT_TIMER(ID_ANIM_TIMER, Windows::OnAnimTimer)
wxEND_EVENT_TABLE()

void Windows::OnToggleCompact(wxCommandEvent& event) {
    SettingsManager& sm = SettingsManager::Get();
    sm.compactMode = !sm.compactMode;
    sm.Save();
    
    targetSidebarWidth = sm.compactMode ? 80 : 240;
    targetIconAngle = sm.compactMode ? M_PI : 0.0; // Rotate 180 degrees
    
    // Setup initial state before animation
    if (sm.compactMode) {
        btnToggleTheme->Hide();
        
        btnServerLocal->SetLabel("");
        btnCluster->SetLabel("");
        btnTools->SetLabel("");
        btnGlobalSettings->SetLabel("");
        btnServerLocal->SetIndent(0);
        btnCluster->SetIndent(0);
        btnTools->SetIndent(0);
        btnGlobalSettings->SetIndent(0);
    } else {
        auto& lang = LanguageManager::Get();
        btnServerLocal->SetLabel(lang.GetString("SERVER_LOCAL"));
        btnCluster->SetLabel(lang.GetString("CLUSTER_NODES"));
        btnTools->SetLabel(lang.GetString("TOOLS"));
        btnGlobalSettings->SetLabel(lang.GetString("GLOBAL_SETTINGS"));
        btnServerLocal->SetIndent(15);
        btnCluster->SetIndent(15);
        btnTools->SetIndent(15);
        btnGlobalSettings->SetIndent(15);
    }
    
    m_animTimer->Start(8); // ~120fps
}

void Windows::OnAnimTimer(wxTimerEvent& event) {
    int diff = targetSidebarWidth - currentSidebarWidth;
    double angleDiff = targetIconAngle - currentIconAngle;
    
    if (abs(diff) <= 2 && abs(angleDiff) <= 0.05) {
        currentSidebarWidth = targetSidebarWidth;
        currentIconAngle = targetIconAngle;
        m_animTimer->Stop();
    } else {
        int step = diff / 12; // Even slower, softer easing
        if (step == 0) step = (diff > 0) ? 1 : -1;
        currentSidebarWidth += step;
        currentIconAngle += angleDiff / 12;
        
        double progress = (currentSidebarWidth - 80.0) / (240.0 - 80.0);
        if (progress < 0.0) progress = 0.0;
        if (progress > 1.0) progress = 1.0;
        
        logoImg->SetProgress(progress);
        logoText->SetProgress(progress);
    }
    
    // Rotate the image
    wxPoint center(compactIconImg.GetWidth()/2, compactIconImg.GetHeight()/2);
    wxImage rotated = compactIconImg.Rotate(currentIconAngle, center, true);
    
    // Crop back to original size to avoid button resizing
    int w = rotated.GetWidth();
    int h = rotated.GetHeight();
    wxRect cropRect((w - 24)/2, (h - 24)/2, 24, 24);
    rotated = rotated.GetSubImage(cropRect);
    
    // Create new bitmap with same size
    wxBitmap newBmp(rotated);
    btnToggleCompact->SetBitmapLabel(newBmp);
    
    sidebarPanel->SetMinSize(wxSize(currentSidebarWidth, -1));
    this->Layout();
    this->Refresh(true);
    this->Update();
    
    if (currentSidebarWidth == targetSidebarWidth) {
        double finalProgress = SettingsManager::Get().compactMode ? 0.0 : 1.0;
        logoImg->SetProgress(finalProgress);
        logoText->SetProgress(finalProgress);
        
        if (!SettingsManager::Get().compactMode) {
            btnToggleTheme->Show();
        }
        this->Layout();
        this->Refresh(true);
        this->Update();
    }
}

void Windows::OnToggleTheme(wxCommandEvent& event) {
    SettingsManager& sm = SettingsManager::Get();
    if (sm.theme == "Dark") sm.theme = "Light";
    else sm.theme = "Dark";
    sm.Save();
    // In a real app we'd refresh the entire UI palette here.
    // For now we just save the setting since full dynamic theming in wxWidgets is complex.
    wxMessageBox("Theme changed to " + sm.theme + ". Please restart the application to apply full theme changes.", "Theme Changed", wxOK | wxICON_INFORMATION);
}
