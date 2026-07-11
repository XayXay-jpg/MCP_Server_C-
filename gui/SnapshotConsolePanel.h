#pragma once
#include <wx/wx.h>

/**
 * @brief Interactive terminal console panel in the State History GUI.
 * 
 * Consists of a read-only multiline terminal output log (styled with monospaced font
 * and dark colors) and a single-line text input field with a "mcp > " command prompt.
 */
class SnapshotConsolePanel : public wxPanel {
public:
    /**
     * @brief Constructor for the SnapshotConsolePanel.
     */
    SnapshotConsolePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
    virtual ~SnapshotConsolePanel() = default;

    /**
     * @brief Appends text output to the terminal screen.
     * @param text The response string to print.
     */
    void AppendOutput(const wxString& text);

    /**
     * @brief Clears the terminal display window.
     */
    void ClearOutput();

private:
    wxTextCtrl* m_txtOutput = nullptr;       ///< Multi-line scrollable output console
    wxTextCtrl* m_txtInput = nullptr;        ///< Single-line command input field
    wxStaticText* m_lblPrompt = nullptr;     ///< Prompt indicator (e.g. "mcp > ")

    /**
     * @brief Triggered when the user presses Enter in the command input box.
     * 
     * Reads the command, sends it to CommandParser in a background thread,
     * and appends results to the console output.
     */
    void OnCommandEnter(wxCommandEvent& event);

    /**
     * @brief Adjusts child control sizes when the panel size changes.
     */
    void OnSize(wxSizeEvent& event);

    wxDECLARE_EVENT_TABLE();
};
