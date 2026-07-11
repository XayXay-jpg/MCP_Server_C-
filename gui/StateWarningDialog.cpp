#include "StateWarningDialog.h"
#include "CustomButton.h"
#include <wx/sizer.h>

enum {
    ID_BTN_PRIMARY = 3001,
    ID_BTN_SECONDARY = 3002,
    ID_BTN_CANCEL = 3003
};

wxBEGIN_EVENT_TABLE(StateWarningDialog, wxDialog)
    EVT_BUTTON(ID_BTN_PRIMARY, StateWarningDialog::OnPrimary)
    EVT_BUTTON(ID_BTN_SECONDARY, StateWarningDialog::OnSecondary)
    EVT_BUTTON(ID_BTN_CANCEL, StateWarningDialog::OnCancel)
wxEND_EVENT_TABLE()

StateWarningDialog::StateWarningDialog(wxWindow* parent,
                                       const wxString& title,
                                       const wxString& message,
                                       const std::vector<std::string>& files,
                                       const wxString& primaryLabel,
                                       const wxString& secondaryLabel,
                                       bool isRollbackWarning)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(480, 280), wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
{
    SetBackgroundColour(wxColour("#09090B")); // Sleek Zinc-950

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Header Title
    wxStaticText* lblTitle = new wxStaticText(this, wxID_ANY, title);
    lblTitle->SetFont(wxFontInfo(13).Bold());
    lblTitle->SetForegroundColour(wxColour("#FFFFFF")); // White
    mainSizer->Add(lblTitle, 0, wxALL, 20);

    // Message Body
    wxStaticText* lblMessage = new wxStaticText(this, wxID_ANY, message);
    lblMessage->SetFont(wxFontInfo(10));
    lblMessage->SetForegroundColour(wxColour("#A1A1AA")); // Zinc-400
    lblMessage->Wrap(440);
    mainSizer->Add(lblMessage, 0, wxLEFT | wxRIGHT | wxBOTTOM, 20);

    // Files List
    if (!files.empty()) {
        wxBoxSizer* filesSizer = new wxBoxSizer(wxVERTICAL);
        for (const auto& file : files) {
            wxStaticText* lblFile = new wxStaticText(this, wxID_ANY, "  •  " + wxString(file));
            lblFile->SetFont(wxFontInfo(9).Bold());
            // Amber for warning, Red for danger
            lblFile->SetForegroundColour(isRollbackWarning ? wxColour("#EF4444") : wxColour("#F59E0B"));
            filesSizer->Add(lblFile, 0, wxBOTTOM, 4);
        }
        mainSizer->Add(filesSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 20);
    }

    mainSizer->AddStretchSpacer(1);

    // Action Buttons
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);

    // Cancel (Ghost button)
    CustomButton* btnCancel = new CustomButton(this, ID_BTN_CANCEL, "Cancel", wxDefaultPosition, wxSize(100, 36));
    btnCancel->SetBackgroundColour(wxColour(0,0,0,0)); // Transparent
    btnCancel->SetForegroundColour(wxColour("#A1A1AA")); // Zinc-400
    btnCancel->SetHoverColour(wxColour("#F4F4F5"));
    btnCancel->SetAlignment(wxALIGN_CENTER);

    // Secondary Action (Neutral/Danger)
    CustomButton* btnSecondary = new CustomButton(this, ID_BTN_SECONDARY, secondaryLabel, wxDefaultPosition, wxSize(150, 36));
    if (isRollbackWarning) {
        // Rollback anyway is a danger action -> red
        btnSecondary->SetBackgroundColour(wxColour("#EF4444"));
        btnSecondary->SetForegroundColour(wxColour("#FFFFFF"));
    } else {
        // Commit staged only -> neutral dark grey
        btnSecondary->SetBackgroundColour(wxColour("#18181B"));
        btnSecondary->SetForegroundColour(wxColour("#F4F4F5"));
    }
    btnSecondary->SetAlignment(wxALIGN_CENTER);

    // Primary Action (Accent, e.g. blue/green)
    CustomButton* btnPrimary = new CustomButton(this, ID_BTN_PRIMARY, primaryLabel, wxDefaultPosition, wxSize(150, 36));
    btnPrimary->SetBackgroundColour(wxColour("#6366F1")); // Indigo Brand Accent
    btnPrimary->SetForegroundColour(wxColour("#FFFFFF"));
    btnPrimary->SetAlignment(wxALIGN_CENTER);

    btnSizer->Add(btnCancel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    btnSizer->Add(btnSecondary, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    btnSizer->Add(btnPrimary, 0, wxALIGN_CENTER_VERTICAL);

    mainSizer->Add(btnSizer, 0, wxALIGN_RIGHT | wxALL, 20);

    SetSizer(mainSizer);
    Layout();
    CenterOnParent();
}

void StateWarningDialog::OnPrimary(wxCommandEvent& event) {
    m_choice = SW_ACTION_PRIMARY;
    EndModal(wxID_OK);
}

void StateWarningDialog::OnSecondary(wxCommandEvent& event) {
    m_choice = SW_ACTION_SECONDARY;
    EndModal(wxID_OK);
}

void StateWarningDialog::OnCancel(wxCommandEvent& event) {
    m_choice = SW_CANCEL;
    EndModal(wxID_CANCEL);
}
