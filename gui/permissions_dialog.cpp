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
    ID_TXT_WORKSPACE = 2005,
    ID_BTN_APPLY_PRESET = 2006,
    ID_CHK_TOOL_CONFIRM = 2007
};

wxBEGIN_EVENT_TABLE(PermissionsDialog, wxDialog)
    EVT_LIST_ITEM_SELECTED(ID_LIST_SERVERS, PermissionsDialog::OnServerSelected)
    EVT_LIST_ITEM_DESELECTED(ID_LIST_SERVERS, PermissionsDialog::OnServerSelected)
    EVT_LIST_ITEM_CHECKED(ID_LIST_TOOLS, PermissionsDialog::OnToolToggled)
    EVT_LIST_ITEM_UNCHECKED(ID_LIST_TOOLS, PermissionsDialog::OnToolToggled)
    EVT_TEXT(ID_TXT_WORKSPACE, PermissionsDialog::OnWorkspaceChanged)
    EVT_BUTTON(ID_BTN_SAVE, PermissionsDialog::OnSave)
    EVT_BUTTON(ID_BTN_CANCEL, PermissionsDialog::OnCancel)
    EVT_BUTTON(ID_BTN_APPLY_PRESET, PermissionsDialog::OnApplyPreset)
    EVT_LIST_ITEM_SELECTED(ID_LIST_TOOLS, PermissionsDialog::OnToolSelected)
    EVT_CHECKBOX(ID_CHK_TOOL_CONFIRM, PermissionsDialog::OnToolConfirmToggled)
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
    
    // Presets Sizer
    wxBoxSizer* presetSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lblPreset = new wxStaticText(this, wxID_ANY, lang.GetString("LBL_PRESETS"));
    
    m_internalRoles = {
        "Admin", "Developer", "Intern", "Observer", "Automation", "Restricted AI", "Custom"
    };
    
    wxArrayString presets;
    presets.Add(lang.GetString("ROLE_ADMIN"));
    presets.Add(lang.GetString("ROLE_DEVELOPER"));
    presets.Add(lang.GetString("ROLE_INTERN"));
    presets.Add(lang.GetString("ROLE_OBSERVER"));
    presets.Add(lang.GetString("ROLE_AUTOMATION"));
    presets.Add(lang.GetString("ROLE_RESTRICTED_AI"));
    presets.Add(lang.GetString("ROLE_CUSTOM"));
    
    choicePresets = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, presets);
    
    // Attempt to set selection based on token role
    int preset_idx = -1;
    for (size_t i = 0; i < m_internalRoles.size(); i++) {
        if (m_internalRoles[i] == m_tokenInfo.role) {
            preset_idx = i;
            break;
        }
    }
    if (preset_idx != wxNOT_FOUND) {
        choicePresets->SetSelection(preset_idx);
    } else {
        choicePresets->SetSelection(1); // Default Developer
    }

    btnApplyPreset = new wxButton(this, ID_BTN_APPLY_PRESET, lang.GetString("BTN_APPLY_ROLE"));
    
    wxStaticText* lblOverseer = new wxStaticText(this, wxID_ANY, lang.GetString("LBL_OVERSEER"));
    choiceOverseer = new wxChoice(this, wxID_ANY);
    
    presetSizer->Add(lblPreset, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    presetSizer->Add(choicePresets, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    presetSizer->Add(btnApplyPreset, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 20);
    presetSizer->Add(lblOverseer, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    presetSizer->Add(choiceOverseer, 1, wxALIGN_CENTER_VERTICAL, 0);
    
    mainSizer->Add(lblHeader, 0, wxALL, 15);
    mainSizer->Add(presetSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

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

    chkToolConfirm = new wxCheckBox(this, ID_CHK_TOOL_CONFIRM, "Requires Overseer Confirmation for this Tool");
    chkToolConfirm->Enable(false);
    rightSizer->Add(chkToolConfirm, 0, wxEXPAND | wxTOP, 5);

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

    // Populate Overseers
    // Populate Overseers with Server Nodes
    choiceOverseer->Clear();
    m_overseerNodeIds.clear();
    
    choiceOverseer->Append("None");
    m_overseerNodeIds.push_back("");
    choiceOverseer->Append(LanguageManager::Get().GetString("LOCAL_SERVER_COMBO"));
    m_overseerNodeIds.push_back("local");
    
    int sel_idx = 0;
    if (m_tokenInfo.overseer_node_id == "local") {
        sel_idx = 1;
    }
    
    int cur_idx = 2;
    for (const auto& node : nodes) {
        choiceOverseer->Append(wxString::FromUTF8(node.id.c_str()));
        m_overseerNodeIds.push_back(node.id);
        if (node.id == m_tokenInfo.overseer_node_id) {
            sel_idx = cur_idx;
        }
        cur_idx++;
    }
    choiceOverseer->SetSelection(sel_idx);
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
    
    std::vector<std::string> confirm_tools;
    if (m_tokenInfo.permissions.requires_confirmation.count(server_id)) {
        confirm_tools = m_tokenInfo.permissions.requires_confirmation[server_id];
    }
    
    bool has_asterisk = (std::find(allowed_tools.begin(), allowed_tools.end(), "*") != allowed_tools.end());
    bool confirm_asterisk = (std::find(confirm_tools.begin(), confirm_tools.end(), "*") != confirm_tools.end());

    long tool_idx = 0;
    
    // Add an option for "ALL TOOLS (*)"
    wxString ast_label = "*";
    if (confirm_asterisk) ast_label += " [Confirm]";
    listTools->InsertItem(tool_idx, ast_label);
    listTools->CheckItem(tool_idx, has_asterisk);
    tool_idx++;

    for (const auto& tool_name : all_tools) {
        wxString label = tool_name;
        if (confirm_asterisk || std::find(confirm_tools.begin(), confirm_tools.end(), tool_name) != confirm_tools.end()) {
            label += " [Confirm]";
        }
        
        listTools->InsertItem(tool_idx, label);
        
        bool is_allowed = has_asterisk || (std::find(allowed_tools.begin(), allowed_tools.end(), tool_name) != allowed_tools.end());
        listTools->CheckItem(tool_idx, is_allowed);
        tool_idx++;
    }
    
    chkToolConfirm->Enable(false);
    chkToolConfirm->SetValue(false);
}

void PermissionsDialog::OnToolToggled(wxListEvent& event) {
    long server_item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (server_item == -1) return;
    
    std::string server_id = listServers->GetItemText(server_item).ToStdString();
    
    // Collect checked tools
    std::vector<std::string> new_allowed;
    for (long i = 0; i < listTools->GetItemCount(); i++) {
        if (listTools->IsItemChecked(i)) {
            std::string label = listTools->GetItemText(i).ToStdString();
            size_t pos = label.find(" [Confirm]");
            if (pos != std::string::npos) {
                label = label.substr(0, pos);
            }
            new_allowed.push_back(label);
        }
    }
    
    m_tokenInfo.permissions.allowed_servers[server_id] = new_allowed;
    
    // Auto-select Custom preset
    auto it = std::find(m_internalRoles.begin(), m_internalRoles.end(), "Custom");
    if (it != m_internalRoles.end()) {
        choicePresets->SetSelection(std::distance(m_internalRoles.begin(), it));
    }
}

void PermissionsDialog::OnWorkspaceChanged(wxCommandEvent& event) {
    long server_item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (server_item == -1) return;
    std::string server_id = listServers->GetItemText(server_item).ToStdString();
    m_tokenInfo.permissions.server_workspaces[server_id] = txtWorkspace->GetValue().ToStdString();
    
    // Auto-select Custom preset
    auto it = std::find(m_internalRoles.begin(), m_internalRoles.end(), "Custom");
    if (it != m_internalRoles.end()) {
        choicePresets->SetSelection(std::distance(m_internalRoles.begin(), it));
    }
}

void PermissionsDialog::OnSave(wxCommandEvent& event) {
    int sel_idx = choicePresets->GetSelection();
    if (sel_idx >= 0 && sel_idx < m_internalRoles.size()) {
        m_tokenInfo.role = m_internalRoles[sel_idx];
    } else {
        m_tokenInfo.role = "Custom";
    }
    
    int over_idx = choiceOverseer->GetSelection();
    if (over_idx >= 0 && over_idx < m_overseerNodeIds.size()) {
        m_tokenInfo.overseer_node_id = m_overseerNodeIds[over_idx];
    }
    
    auto tokens = NetworkUtils::LoadTokens();
    for (auto& t : tokens) {
        if (t.id == m_tokenInfo.id) {
            t.permissions = m_tokenInfo.permissions;
            t.role = m_tokenInfo.role;
            t.overseer_node_id = m_tokenInfo.overseer_node_id;
            break;
        }
    }
    NetworkUtils::SaveTokens(tokens);
    EndModal(wxID_OK);
}

void PermissionsDialog::OnCancel(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}

void PermissionsDialog::OnToolSelected(wxListEvent& event) {
    long server_item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (server_item == -1) return;
    
    long tool_item = event.GetIndex();
    if (tool_item == -1) {
        chkToolConfirm->Enable(false);
        chkToolConfirm->SetValue(false);
        return;
    }
    
    std::string server_id = listServers->GetItemText(server_item).ToStdString();
    std::string tool_label = listTools->GetItemText(tool_item).ToStdString();
    
    std::string tool_name = tool_label;
    size_t pos = tool_name.find(" [Confirm]");
    if (pos != std::string::npos) {
        tool_name = tool_name.substr(0, pos);
    }
    
    std::vector<std::string>& confirm_tools = m_tokenInfo.permissions.requires_confirmation[server_id];
    bool is_confirmed = (std::find(confirm_tools.begin(), confirm_tools.end(), tool_name) != confirm_tools.end());
    
    chkToolConfirm->Enable(true);
    chkToolConfirm->SetValue(is_confirmed);
}

void PermissionsDialog::OnToolConfirmToggled(wxCommandEvent& event) {
    long server_item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (server_item == -1) return;
    
    long tool_item = listTools->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (tool_item == -1) return;
    
    std::string tool_label = listTools->GetItemText(tool_item).ToStdString();
    
    // Strip " [Confirm]" to get raw name
    std::string tool_name = tool_label;
    size_t pos = tool_name.find(" [Confirm]");
    if (pos != std::string::npos) {
        tool_name = tool_name.substr(0, pos);
    }
    
    std::string server_id = listServers->GetItemText(server_item).ToStdString();
    std::vector<std::string>& confirm_tools = m_tokenInfo.permissions.requires_confirmation[server_id];
    
    auto it = std::find(confirm_tools.begin(), confirm_tools.end(), tool_name);
    if (chkToolConfirm->GetValue()) {
        if (it == confirm_tools.end()) {
            confirm_tools.push_back(tool_name);
        }
        listTools->SetItemText(tool_item, tool_name + " [Confirm]");
    } else {
        if (it != confirm_tools.end()) {
            confirm_tools.erase(it);
        }
        listTools->SetItemText(tool_item, tool_name);
    }
    
    // Auto-select Custom preset
    auto role_it = std::find(m_internalRoles.begin(), m_internalRoles.end(), "Custom");
    if (role_it != m_internalRoles.end()) {
        choicePresets->SetSelection(std::distance(m_internalRoles.begin(), role_it));
    }
}

void PermissionsDialog::OnApplyPreset(wxCommandEvent& event) {
    int sel_idx = choicePresets->GetSelection();
    if (sel_idx == wxNOT_FOUND) return;
    
    std::string sel = m_internalRoles[sel_idx];
    
    m_tokenInfo.permissions.allowed_servers.clear();
    m_tokenInfo.permissions.requires_confirmation.clear();
    
    if (sel == "Admin") {
        m_tokenInfo.permissions.allowed_servers["*"] = {"*"};
    } else if (sel == "Developer") {
        m_tokenInfo.permissions.allowed_servers["local"] = {"*"};
        m_tokenInfo.permissions.requires_confirmation["local"] = {"mcp_my_local_cpp_server_execute_command", "mcp_my_local_cpp_server_write_file"};
    } else if (sel == "Observer") {
        m_tokenInfo.permissions.allowed_servers["local"] = {"mcp_my_local_cpp_server_list_directory", "mcp_my_local_cpp_server_read_file"};
    }
    
    // Refresh servers list to apply changes visually
    long item = listServers->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1) {
        wxListEvent dummy;
        OnServerSelected(dummy);
    }
    wxMessageBox("Preset applied! Please verify and save.", "Info", wxOK | wxICON_INFORMATION);
}
