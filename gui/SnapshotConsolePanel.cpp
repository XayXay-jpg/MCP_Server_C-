#include "SnapshotConsolePanel.h"
#include "../mcp/CommandParser.h"
#include "../mcp/thread_pool.h"
#include "SnapshotGraphPanel.h"
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>

wxBEGIN_EVENT_TABLE(SnapshotConsolePanel, wxPanel)
    EVT_TEXT_ENTER(wxID_ANY, SnapshotConsolePanel::OnCommandEnter)
    EVT_SIZE(SnapshotConsolePanel::OnSize)
wxEND_EVENT_TABLE()

SnapshotConsolePanel::SnapshotConsolePanel(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    
    SetBackgroundColour(wxColour("#090D16")); // Deep terminal black/blue

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Multiline read-only console output
    m_txtOutput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                 wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxBORDER_NONE);
    m_txtOutput->SetBackgroundColour(wxColour("#090D16"));
    m_txtOutput->SetForegroundColour(wxColour("#10B981")); // Cyberpunk console green
    
    // Choose monospace font
    wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_txtOutput->SetFont(font);

    // Input prompt row
    wxPanel* inputPanel = new wxPanel(this, wxID_ANY);
    inputPanel->SetBackgroundColour(wxColour("#0F172A"));
    wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);

    m_lblPrompt = new wxStaticText(inputPanel, wxID_ANY, " mcp > ");
    m_lblPrompt->SetForegroundColour(wxColour("#6366F1")); // Indigo highlight for prompt
    m_lblPrompt->SetFont(font.Bold());

    m_txtInput = new wxTextCtrl(inputPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxTE_PROCESS_ENTER | wxBORDER_NONE);
    m_txtInput->SetBackgroundColour(wxColour("#0F172A"));
    m_txtInput->SetForegroundColour(wxColour("#F4F4F5"));
    m_txtInput->SetFont(font);

    inputSizer->Add(m_lblPrompt, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    inputSizer->Add(m_txtInput, 1, wxEXPAND | wxALL, 5);
    inputPanel->SetSizer(inputSizer);

    sizer->Add(m_txtOutput, 1, wxEXPAND);
    sizer->Add(inputPanel, 0, wxEXPAND);
    SetSizer(sizer);

    // Print welcome message
    AppendOutput("MCP Console Interface. Type 'mcp help' or 'git help' for list of commands.\n\n");
}

void SnapshotConsolePanel::AppendOutput(const wxString& text) {
    m_txtOutput->AppendText(text);
    m_txtOutput->ShowPosition(m_txtOutput->GetLastPosition());
}

void SnapshotConsolePanel::ClearOutput() {
    m_txtOutput->Clear();
}

void SnapshotConsolePanel::OnCommandEnter(wxCommandEvent& event) {
    wxString rawCmd = m_txtInput->GetValue();
    if (rawCmd.Trim().IsEmpty()) {
        return;
    }

    m_txtInput->Clear();
    AppendOutput("mcp > " + rawCmd + "\n");

    std::string cmd_std = rawCmd.ToStdString();

    // Run command in background ThreadPool
    ThreadPool::GetInstance().enqueue([this, cmd_std]() {
        std::string result = CommandParser::Execute(cmd_std);
        
        // Push print update back to GUI thread
        this->GetEventHandler()->CallAfter([this, result, cmd_std]() {
            this->AppendOutput(wxString::FromUTF8(result) + "\n\n");
            
            // If the command altered states, trigger refreshing the sibling DAG graph panel
            std::string cmd_clean = cmd_std;
            std::transform(cmd_clean.begin(), cmd_clean.end(), cmd_clean.begin(), ::tolower);
            
            if (cmd_clean.find("commit") != std::string::npos ||
                cmd_clean.find("checkout") != std::string::npos ||
                cmd_clean.find("reset") != std::string::npos ||
                cmd_clean.find("restore") != std::string::npos) {
                
                // Find sibling GraphPanel
                if (GetParent()) {
                    wxWindowList& children = GetParent()->GetChildren();
                    for (wxWindow* child : children) {
                        SnapshotGraphPanel* graph = dynamic_cast<SnapshotGraphPanel*>(child);
                        if (graph) {
                            graph->LoadGraphData();
                        }
                    }
                }
            }
        });
    });
}

void SnapshotConsolePanel::OnSize(wxSizeEvent& event) {
    Layout();
    event.Skip();
}
