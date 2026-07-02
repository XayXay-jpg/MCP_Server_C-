#pragma once
#include <string>

int run_mcp_server(int port, const std::string& default_workspace, const std::string& auth_user, const std::string& auth_pass);
void stop_mcp_server();
void notify_tools_changed();
