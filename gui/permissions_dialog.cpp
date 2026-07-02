#include "permissions_dialog.h"
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include "../mcp/cluster_manager.h"
#include "../mcp/tools.h"
#include "../mcp/utils.h"
#include "LanguageManager.h"

enum {
    ID_LIST_SERVERS = 2001,
    ID_LIST_TOOLS = 2002,
    ID_BTN_SAVE = 2003,
    ID_BTN_CANCEL = 2004,
    ID_TXT_WORKSPACE = 2005
};

wxBEGIN_EVENT_TABLE(PermissionsDialog, wxDialog)
    EVT_LIST_ITEM_SELECTED(ID_LIST_SERVERS, PermissionsDialog::OnServerSelected)
    EVT_LIST_ITEM_DESELECTED(ID_LIST_SERVERS, PermissionsDialog::OnServerSelected)
    EVT_LIST_ITEM_CHECKED(ID_LIST_TOOLS, PermissionsDialog::OnToolToggled)
    EVT_LIST_ITEM_UNCHECKED(ID_LIST_TOOLS, PermissionsDialog::OnToolToggled)
    EVT_TEXT(ID_TXT_WORKSPACE, PermissionsDialog::OnWorkspaceChanged)
    EVT_BUTTON(ID_BTN_SAVE, PermissionsDialog::OnSave)
    EVT_BUTTON(ID_BTN_CANCEL, PermissionsDialog::OnCancel)
wxEND_EVENT_TABLE()

PermissionsDialog::PermissionsDialog(wxWindow* parent, const std::string& token_id)
    : wxDialog(parent, wxID_ANY, "Token Permissions", wxDefaultPosition, wxSize(800, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_tokenId(token_id)
{
    // Load current token info
    auto tokens = NetworkUtils::LoadTokens();
    auto it = std::find_if(tokens.begin(), tokens.end(), [&](const TokenInfo& t) { return t.id == token_id; });
    if (it != tokens.end()) {
        m_tokenInfo = *it;
    } else {
        m_tokenInfo.id = token_id;
        m_tokenInfo.name = "Unknown Token";
        m_tokenInfo.permissions.allowed_servers["local"] = {}; // Default empty
    }

    SetupUI();
    LoadData();
}

void PermissionsDialog::SetupUI() {
    auto& lang = LanguageManager::Get();
    SetBackgroundColour(wxColour("#09090B"));

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Header
    wxStaticText* lblHeader = new wxStaticText(this, wxID_ANY, "Permissions for: " + m_tokenInfo.name);
    lblHeader->SetFont(wxFontInfo(16).Bold());
    lblHeader->SetForegroundColour(wxColour("#FFFFFF"));
    mainSizer->Add(lblHeader, 0, wxALL, 15);

    // Splitter or horizontal sizer for Servers and Tools
    wxBoxSizer* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    // Servers List
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* lblServers = new wxStaticText(this, wxID_ANY, "Available Servers");
    lblServers->SetForegroundColour(wxColour("#A1A1AA"));
    leftSizer->Add(lblServers, 0, wxBOTTOM, 5);

    listServers = new wxListCtrl(this, ID_LIST_SERVERS, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
    listServers->InsertColumn(0, "Server ID", wxLIST_FORMAT_LEFT, 150);
    listServers->InsertColumn(1, "Status", wxLIST_FORMAT_LEFT, 100);
    listServers->SetBackgroundColour(wxColour("#141417"));
    listServers->SetTextColour(wxColour("#F4F4F5"));
    leftSizer->Add(listServers, 1, wxEXPAND, 0);
    
    contentSizer->Add(leftSizer, 1, wxEXPAND | wxLEFT | wxBOTTOM, 15);

    // Right Side: Workspace & Tools
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    
    // Workspace overrides
    wxStaticText* lblWorkspace = new wxStaticText(this, wxID_ANY, "Server Workspace Override (Leave empty for default)");
    lblWorkspace->SetForegroundColour(wxColour("#A1A1AA"));
    rightSizer->Add(lblWorkspace, 0, wxBOTTOM, 5);
    
    txtWorkspace = new wxTextCtrl(this, ID_TXT_WORKSPACE, "", wxDefaultPosition, wxDefaultSize);
    txtWorkspace->SetBackgroundColour(wxColour("#141417"));
    txtWorkspace->SetForegroundColour(wxColour("#FFFFFF"));
    txtWorkspace->SetHint(BASE_DIR.string());
    rightSizer->Add(txtWorkspace, 0, wxEXPAND | wxBOTTOM, 10);
    
    // Tools List
    wxStaticText* lblTools = new wxStaticText(this, wxID_ANY, "Allowed Tools on Selected Server");
    lblTools->SetForegroundColour(wxColour("#A1A1AA"));
    rightSizer->Add(lblTools, 0, wxBOTTOM, 5);

    listTools = new wxListCtrl(this, ID_LIST_TOOLS, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxBORDER_NONE);
    listTools->EnableCheckBoxes(true);
    listTools->InsertColumn(0, "Tool Name", wxLIST_FORMAT_LEFT, 200);
    listTools->SetBackgroundColour(wxColour("#141417"));
    listTools->SetTextColour(wxColour("#F4F4F5"));
    rightSizer->Add(listTools, 1, wxEXPAND, 0);

    contentSizer->Add(rightSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    mainSizer->Add(contentSizer, 1, wxEXPAND, 0);

    // Buttons
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSave = new wxButton(this, ID_BTN_SAVE, "Save Permissions");
    btnSave->SetBackgroundColour(wxColour("#2563EB"));
    btnSave->SetForegroundColour(wxColour("#FFFFFF"));
    
    btnCancel = new wxButton(this, ID_BTN_CANCEL, "Cancel");
    
    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnCancel, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
    btnSizer->Add(btnSave, 0, wxALIGN_CENTER_VERTICAL, 0);
    
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 15);

    SetSizer(mainSizer);
}

std::vector<std::string> PermissionsDialog::GetAllTools() {
    std::vector<std::string> tools;
    auto available_tools = get_all_tools(); // from mcp/tools.h
    for (const auto& t : available_tools) {
        if (t.contains("name")) {
            tools.push_back(t["name"].get<std::string>());
        }
    }
    return tools;
}

void PermissionsDialog::LoadData() {
    listServers->DeleteAllItems();
    long idx = 0;

    // Add Local Server
    listServers->InsertItem(idx, "local");
    listServers->SetItem(idx, 1, "Online");
    idx++;

    // Add Cluster Nodes
    auto nodes = ClusterManager::GetInstance().GetNodes();
    for (const auto& node : nodes) {
        listServers->InsertItem(idx, node.id);
        listServers->SetItem(idx, 1, node.status);
        idx++;
    }
    
    // Select first server by default if exists
    if (listServers->GetItemCount() > 0) {
        listServers->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
}

void PermissionsDialog::OnServerSelected(wxListEvent& event) {
    long item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    
    listTools->DeleteAllItems();
    
    if (item == -1) return; // Nothing selected

    std::string server_id = listServers->GetItemText(item).ToStdString();
    
    if (m_tokenInfo.permissions.server_workspaces.count(server_id)) {
        txtWorkspace->SetValue(m_tokenInfo.permissions.server_workspaces[server_id]);
    } else {
        txtWorkspace->SetValue("");
    }
    
    auto all_tools = GetAllTools();
    
    // Get allowed tools for this server
    std::vector<std::string> allowed_tools;
    if (m_tokenInfo.permissions.allowed_servers.count(server_id)) {
        allowed_tools = m_tokenInfo.permissions.allowed_servers[server_id];
    }
    
    bool has_asterisk = (std::find(allowed_tools.begin(), allowed_tools.end(), "*") != allowed_tools.end());

    long tool_idx = 0;
    
    // Add an option for "ALL TOOLS (*)"
    listTools->InsertItem(tool_idx, "*");
    listTools->CheckItem(tool_idx, has_asterisk);
    tool_idx++;

    for (const auto& tool_name : all_tools) {
        listTools->InsertItem(tool_idx, tool_name);
        
        bool is_allowed = has_asterisk || (std::find(allowed_tools.begin(), allowed_tools.end(), tool_name) != allowed_tools.end());
        listTools->CheckItem(tool_idx, is_allowed);
        tool_idx++;
    }
}

void PermissionsDialog::OnToolToggled(wxListEvent& event) {
    long server_item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (server_item == -1) return;
    
    std::string server_id = listServers->GetItemText(server_item).ToStdString();
    
    // Collect checked tools
    std::vector<std::string> new_allowed;
    for (long i = 0; i < listTools->GetItemCount(); i++) {
        if (listTools->IsItemChecked(i)) {
            new_allowed.push_back(listTools->GetItemText(i).ToStdString());
        }
    }
    
    m_tokenInfo.permissions.allowed_servers[server_id] = new_allowed;
}

void PermissionsDialog::OnWorkspaceChanged(wxCommandEvent& event) {
    long server_item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (server_item == -1) return;
    std::string server_id = listServers->GetItemText(server_item).ToStdString();
    m_tokenInfo.permissions.server_workspaces[server_id] = txtWorkspace->GetValue().ToStdString();
}

void PermissionsDialog::OnSave(wxCommandEvent& event) {
    auto tokens = NetworkUtils::LoadTokens();
    for (auto& t : tokens) {
        if (t.id == m_tokenInfo.id) {
            t.permissions = m_tokenInfo.permissions;
            break;
        }
    }
    NetworkUtils::SaveTokens(tokens);
    EndModal(wxID_OK);
}

void PermissionsDialog::OnCancel(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}
