#include "main_gui.h"
#include "gui/windows.h"

bool soft::OnInit() {
    wxInitAllImageHandlers();
    Windows *windows = new Windows(wxT("MCP Server Manager"));
    windows -> Show(true); return true;
}

IMPLEMENT_APP(soft);