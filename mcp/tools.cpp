#include "tools.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <httplib.h>
#include <cstdlib>

#include "settings_manager.h"
#include "cluster_manager.h"
#include "crypto_utils.h"
#include "knowledge_layer.h"

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

json get_all_tools() {
    json tools = json::array({
        {
            {"name", "list_directory"},
            {"description", "List files and directories in the given path"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Relative path to directory"}}}
                }},
                {"required", {"path"}}
            }}
        },
        {
            {"name", "read_file"},
            {"description", "Read text content of a file"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Relative path to file"}}}
                }},
                {"required", {"path"}}
            }}
        },
        {
            {"name", "write_file"},
            {"description", "Write text content to a file (creates or overwrites)"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Relative path to file"}}},
                    {"content", {{"type", "string"}, {"description", "Text content to write"}}}
                }},
                {"required", {"path", "content"}}
            }}
        },
        {
            {"name", "start_script"},
            {"description", "Start a script, application, or game in the workspace"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Relative path to the executable or script"}}},
                    {"args", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Optional list of command line arguments"}}},
                    {"background", {{"type", "boolean"}, {"description", "Run in background without waiting for output (true for games/GUI apps, false for scripts)"}}}
                }},
                {"required", {"path"}}
            }}
        },
        {
            {"name", "search_files"},
            {"description", "Recursively search for files in the workspace matching a query string in their filename"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"query", {{"type", "string"}, {"description", "Substring to search for in filenames"}}}
                }},
                {"required", {"query"}}
            }}
        },
        {
            {"name", "execute_command"},
            {"description", "Execute an arbitrary terminal command in the workspace directory. Output is captured and returned."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"command", {{"type", "string"}, {"description", "Command to execute"}}}
                }},
                {"required", {"command"}}
            }}
        },
        {
            {"name", "take_screenshot"},
            {"description", "Take a screenshot of the main monitor and return the image data"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            }}
        },
        {
            {"name", "get_knowledge"},
            {"description", "Retrieves the digital twin knowledge base. Pass a section name to get specific data, or omit to get the full knowledge graph."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"section", {{"type", "string"}, {"description", "Optional section name (server, services, applications, policies, workflow, etc.)"}}}
                }}
            }}
        },
        {
            {"name", "update_knowledge"},
            {"description", "Updates or adds information to a specific section of the knowledge base."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"section", {{"type", "string"}, {"description", "Section name (e.g., 'services')"}}},
                    {"data", {{"type", "object"}, {"description", "JSON data to save into the section"}}}
                }},
                {"required", {"section", "data"}}
            }}
        }
    });
    
    for (auto& tool : tools) {
        tool["inputSchema"]["properties"]["target_server_id"] = {
            {"type", "string"},
            {"description", "Optional ID of the target child server to execute this tool on. If omitted, executes on the local parent server."}
        };
    }
    
    return tools;
}

json get_available_tools(const TokenInfo& token) {
    json all = get_all_tools();
    json filtered = json::array();
    auto& disabled = SettingsManager::Get().disabled_tools;
    
    for (auto& tool : all) {
        std::string name = tool["name"];
        if (std::find(disabled.begin(), disabled.end(), name) != disabled.end()) {
            continue;
        }
        
        // If token has NO permissions configured at all — treat it as having full local access
        bool no_permissions_configured = token.permissions.allowed_servers.empty();
        
        std::vector<std::string> allowed_nodes;
        if (no_permissions_configured || token.permissions.has_tool_access("local", name)) {
            allowed_nodes.push_back("local");
        }
        
        for (const auto& node : ClusterManager::GetInstance().GetNodes()) {
            if (node.status == "connected") {
                if (no_permissions_configured || token.permissions.has_tool_access(node.id, name)) {
                    allowed_nodes.push_back(node.id);
                }
            }
        }
        
        if (allowed_nodes.empty()) {
            continue; // The token has no access to this tool on ANY node
        }
        
        tool["inputSchema"]["properties"].erase("target_server_id");
        tool["inputSchema"]["properties"]["target_server_ids"] = {
            {"type", "array"},
            {"items", {
                {"type", "string"},
                {"enum", allowed_nodes}
            }},
            {"description", "Target servers to execute this tool on. CRITICAL: You can and SHOULD specify MULTIPLE servers in this array in a SINGLE tool call if you need to perform the same action across multiple devices simultaneously (e.g. [\"local\", \"Mac Server\"]). Do NOT make separate identical tool calls for each server! Allowed servers for your token: "}
        };
        
        std::string desc = tool["inputSchema"]["properties"]["target_server_ids"]["description"];
        desc += "[";
        for (size_t i = 0; i < allowed_nodes.size(); i++) {
            desc += allowed_nodes[i] + (i == allowed_nodes.size() - 1 ? "" : ", ");
        }
        desc += "].";
        tool["inputSchema"]["properties"]["target_server_ids"]["description"] = desc;
        
        filtered.push_back(tool);
    }
    return filtered;
}

json tool_take_screenshot(const json& id, const json& arguments, const std::filesystem::path& base_dir) {
    fs::path temp_dir = fs::temp_directory_path();
    fs::path file_path = temp_dir / "mcp_screenshot.png";
    std::string path_str = file_path.string();
    
    // Windows paths might contain backslashes, convert to forward slashes for Python script
    std::replace(path_str.begin(), path_str.end(), '\\', '/');

#ifdef _WIN32
    std::string python_cmd = "python";
#else
    std::string python_cmd = "python3";
#endif

    std::string cmd = python_cmd + " -c \"from PIL import ImageGrab; img=ImageGrab.grab(); img.save('" + path_str + "')\"";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        return make_error(id, -32000, "Failed to take screenshot using python PIL");
    }

    std::ifstream file(file_path.string(), std::ios::binary);
    if (!file) {
        return make_error(id, -32000, "Failed to read screenshot file");
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string base64 = httplib::detail::base64_encode(content);

    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", json::array({
                {
                    {"type", "image"},
                    {"data", base64},
                    {"mimeType", "image/png"}
                }
            })}
        }}
    };
}

json tool_list_directory(const json& id, const json& arguments, const std::filesystem::path& base_dir) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>(), base_dir);
    if (!safe_path) {
        return make_error(id, -32000, "Access denied: Path is outside the sandbox");
    }
    if (!fs::exists(*safe_path) || !fs::is_directory(*safe_path)) {
        return make_error(id, -32000, "Directory does not exist or is not a directory");
    }
    
    try {
        json files = json::array();
        for (const auto& entry : fs::directory_iterator(*safe_path)) {
            files.push_back({
                {"name", entry.path().filename().string()},
                {"is_directory", entry.is_directory()}
            });
        }
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"content", json::array({
                    {
                        {"type", "text"},
                        {"text", files.dump(2)}
                    }
                })}
            }}
        };
    } catch (const std::exception& e) {
        mcp_log("[Error] List directory failed: " + std::string(e.what()));
        return make_error(id, -32000, "Internal error reading directory");
    }
}

json tool_read_file(const json& id, const json& arguments, const std::filesystem::path& base_dir) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>(), base_dir);
    if (!safe_path) {
        return make_error(id, -32000, "Access denied: Path is outside the sandbox");
    }
    if (!fs::exists(*safe_path) || !fs::is_regular_file(*safe_path)) {
        return make_error(id, -32000, "File does not exist or is not a regular file");
    }
    
    try {
        std::ifstream ifs(*safe_path);
        if (!ifs.is_open()) {
            return make_error(id, -32000, "Failed to open file for reading");
        }
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"content", json::array({
                    {{"type", "text"}, {"text", std::move(content)}}
                })}
            }}
        };
    } catch (const std::exception& e) {
        mcp_log("[Error] Read file failed: " + std::string(e.what()));
        return make_error(id, -32000, "Internal error reading file");
    }
}

json tool_write_file(const json& id, const json& arguments, const std::filesystem::path& base_dir) {
    if (!arguments.contains("path") || !arguments["path"].is_string() || 
        !arguments.contains("content") || !arguments["content"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' or 'content' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>(), base_dir);
    if (!safe_path) {
        return make_error(id, -32000, "Access denied: Path is outside the sandbox");
    }
    
    try {
        fs::create_directories(safe_path->parent_path());
    } catch (const std::exception& e) {
        mcp_log("[Error] Failed to create directories: " + std::string(e.what()));
        return make_error(id, -32000, "Failed to create parent directories");
    }
    
    try {
        std::ofstream ofs(*safe_path);
        if (!ofs.is_open()) {
            return make_error(id, -32000, "Failed to open file for writing");
        }
        ofs << arguments["content"].get<std::string>();
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"content", json::array({
                    {{"type", "text"}, {"text", "File written successfully"}}
                })}
            }}
        };
    } catch (const std::exception& e) {
        mcp_log("[Error] Write file failed: " + std::string(e.what()));
        return make_error(id, -32000, "Internal error writing file");
    }
}

json tool_start_script(const json& id, const json& arguments, const std::filesystem::path& base_dir) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>(), base_dir);
    if (!safe_path) {
        return make_error(id, -32000, "Access denied: Path is outside the sandbox");
    }
    if (!fs::exists(*safe_path)) {
        return make_error(id, -32000, "Script or executable does not exist");
    }

    std::string cmd = "cd \"" + base_dir.string() + "\" && chmod +x \"" + safe_path->string() + "\" && \"" + safe_path->string() + "\"";
    
    if (arguments.contains("args") && arguments["args"].is_array()) {
        for (const auto& arg : arguments["args"]) {
            cmd += " \"" + arg.get<std::string>() + "\"";
        }
    }

    bool background = arguments.value("background", false);
    
    if (background) {
        cmd += " > /dev/null 2>&1 &";
        int ret = std::system(cmd.c_str());
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"content", json::array({
                    {{"type", "text"}, {"text", "Process started in background (return code: " + std::to_string(ret) + ")" }}
                })}
            }}
        };
    } else {
        cmd += " 2>&1"; 
        std::string output;
        try {
            FILE* pipe = POPEN(cmd.c_str(), "r");
            if (!pipe) {
                return make_error(id, -32000, "Failed to start process");
            }
            char buffer[4096];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            int status = PCLOSE(pipe);
            std::string final_output = "Process exited with code " + std::to_string(status) + "\nOutput:\n" + output;
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", {
                    {"content", json::array({
                        {{"type", "text"}, {"text", std::move(final_output)}}
                    })}
                }}
            };
        } catch (const std::exception& e) {
            return make_error(id, -32000, std::string("Internal error executing script: ") + e.what());
        }
    }
}

json tool_search_files(const json& id, const json& arguments, const std::filesystem::path& base_dir) {
    if (!arguments.contains("query") || !arguments["query"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'query' argument");
    }
    std::string query = arguments["query"].get<std::string>();
    
    try {
        json results = json::array();
        for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
            std::string filename = entry.path().filename().string();
            if (filename.find(query) != std::string::npos) {
                std::string rel_path = fs::relative(entry.path(), base_dir).string();
                results.push_back({
                    {"path", rel_path},
                    {"is_directory", entry.is_directory()}
                });
            }
        }
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"content", json::array({
                    {
                        {"type", "text"},
                        {"text", results.dump(2)}
                    }
                })}
            }}
        };
    } catch (const std::exception& e) {
        mcp_log("[Error] Search files failed: " + std::string(e.what()));
        return make_error(id, -32000, "Internal error searching files");
    }
}
json tool_execute_command(const json& id, const json& arguments, const std::filesystem::path& base_dir) {
    if (!arguments.contains("command") || !arguments["command"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'command' argument");
    }
    
    std::string command = arguments["command"].get<std::string>();
    std::string cmd = "cd \"" + base_dir.string() + "\" && " + command + " 2>&1";
    std::string output;
    
    try {
        FILE* pipe = POPEN(cmd.c_str(), "r");
        if (!pipe) {
            return make_error(id, -32000, "Failed to execute command");
        }
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        int status = PCLOSE(pipe);
        
        std::string final_output = "Command exited with code " + std::to_string(status) + "\nOutput:\n" + output;
        
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"content", json::array({
                    {{"type", "text"}, {"text", std::move(final_output)}}
                })}
            }}
        };
    } catch (const std::exception& e) {
        mcp_log("[Error] Execute command failed: " + std::string(e.what()));
        return make_error(id, -32000, std::string("Internal error executing command: ") + e.what());
    }
}

json tool_get_knowledge(const json& id, const json& arguments) {
    json data;
    if (arguments.contains("section") && arguments["section"].is_string()) {
        std::string section = arguments["section"].get<std::string>();
        data = KnowledgeLayer::GetInstance().GetSection(section);
        if (data.is_null()) {
            return make_error(id, -32000, "Section '" + section + "' not found in knowledge base.");
        }
    } else {
        data = KnowledgeLayer::GetInstance().GetFullKnowledge();
    }
    
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", json::array({
                {{"type", "text"}, {"text", data.dump(2)}}
            })}
        }}
    };
}

json tool_update_knowledge(const json& id, const json& arguments) {
    if (!arguments.contains("section") || !arguments["section"].is_string() || !arguments.contains("data")) {
        return make_error(id, -32602, "Missing 'section' (string) or 'data' (object/array)");
    }
    
    std::string section = arguments["section"].get<std::string>();
    json data = arguments["data"];
    
    KnowledgeLayer::GetInstance().UpdateSection(section, data);
    
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", json::array({
                {{"type", "text"}, {"text", "Knowledge section '" + section + "' updated successfully."}}
            })}
        }}
    };
}

json handle_tools_call(const json& request, const TokenInfo& token) {
    auto id = request.value("id", json(nullptr));
    if (!request.contains("params") || !request["params"].contains("name")) {
        return make_error(id, -32602, "Missing tool name in params");
    }
    
    std::string name = request["params"]["name"];
    json arguments = request["params"].value("arguments", json::object());
    
    std::vector<std::string> target_servers;
    if (arguments.contains("target_server_ids") && arguments["target_server_ids"].is_array()) {
        for (const auto& s : arguments["target_server_ids"]) {
            target_servers.push_back(s.get<std::string>());
        }
    }
    if (target_servers.empty() && arguments.contains("target_server_id")) {
        target_servers.push_back(arguments.value("target_server_id", "local"));
    }
    if (target_servers.empty()) {
        target_servers.push_back("local");
    }
    
    json combined_content = json::array();
    bool has_error = false;
    
    for (const std::string& target_server : target_servers) {
        mcp_log("[Tool] Executing: " + name + " on server: " + target_server);
        g_tool_calls++;
        
        bool is_forwarded = request.value("_is_forwarded", false);
        bool no_permissions_configured = token.permissions.allowed_servers.empty();
        bool has_access = is_forwarded || no_permissions_configured || token.permissions.has_tool_access(target_server, name);
        
        if (!has_access) {
            mcp_log("[Access Denied] Token '" + token.name + "' tried to use '" + name + "' on '" + target_server + "' — permission denied.");
            combined_content.push_back({{"type", "text"}, {"text", "[Server " + target_server + "] Error: Access Denied to tool '" + name + "' for this token."}});
            continue;
        }
        
        json res_json;
        if (target_server != "local") {
            ClusterNode targetNode;
            if (!ClusterManager::GetInstance().GetNode(target_server, targetNode) || targetNode.status != "connected") {
                combined_content.push_back({{"type", "text"}, {"text", "[Server " + target_server + "] Error: Target node not found or not connected"}});
                continue;
            }
            
            mcp_log("[Proxy] Forwarding request to child node: " + targetNode.ip_address);
            
            json child_req = request;
            child_req["params"]["arguments"].erase("target_server_ids");
            child_req["params"]["arguments"]["target_server_id"] = "local";
            child_req["_forwarded_token"] = token.raw_token; // Put inside encrypted payload
            
            std::string plain_body = child_req.dump();
            std::string encrypted_body = encrypt_aes256(targetNode.encryption_key, plain_body);
            
            json enc_req = {
                {"encrypted_payload", encrypted_body}
            };
            std::string req_body = enc_req.dump();
            std::string signature = generate_hmac_sha256(targetNode.encryption_key, req_body);
            
            mcp_log("[Proxy] targetNode.id: " + targetNode.id);
            mcp_log("[Proxy] targetNode.master_token: " + targetNode.master_token);
            mcp_log("[Proxy] targetNode.encryption_key: " + targetNode.encryption_key);
            mcp_log("[Proxy] req_body length: " + std::to_string(req_body.length()));
            mcp_log("[Proxy] generated signature: " + signature);
            
            std::string ip = targetNode.ip_address;
            if (ip.find("http://") == 0) ip = ip.substr(7);
            if (ip.find("https://") == 0) ip = ip.substr(8);
            
            httplib::Client cli("http://" + ip);
            cli.set_connection_timeout(5, 0);
            cli.set_read_timeout(30, 0);
            
            httplib::Headers headers = {
                {"Authorization", "Bearer " + targetNode.master_token},
                {"X-MCP-Signature", signature},
                {"Content-Type", "application/json"}
            };
            
            std::string proxy_url = "/mcp";
            if (!token.raw_token.empty()) {
                proxy_url += "?token=" + token.raw_token;
            }
            
            auto res = cli.Post(proxy_url, headers, req_body, "application/json");
            if (res && res->status == 200) {
                try {
                    json parsed = json::parse(res->body);
                    if (parsed.contains("encrypted_payload")) {
                        std::string dec = decrypt_aes256(targetNode.encryption_key, parsed.value("encrypted_payload", ""));
                        if (!dec.empty()) {
                            res_json = json::parse(dec);
                        } else {
                            res_json = parsed;
                        }
                    } else {
                        res_json = parsed;
                    }
                } catch (const std::exception& e) {
                    combined_content.push_back({{"type", "text"}, {"text", "[Server " + target_server + "] Error parsing response: " + std::string(e.what())}});
                    continue;
                }
            } else {
                combined_content.push_back({{"type", "text"}, {"text", "[Server " + target_server + "] Error: HTTP " + (res ? std::to_string(res->status) : "Connection Error")}});
                continue;
            }
        } else {
            fs::path base_dir = BASE_DIR;
            auto it = token.permissions.server_workspaces.find("local");
            if (it != token.permissions.server_workspaces.end() && !it->second.empty()) {
                base_dir = it->second;
            }
            
            if (name == "list_directory") {
                res_json = tool_list_directory(id, arguments, base_dir);
            } else if (name == "read_file") {
                res_json = tool_read_file(id, arguments, base_dir);
            } else if (name == "write_file") {
                res_json = tool_write_file(id, arguments, base_dir);
            } else if (name == "start_script") {
                res_json = tool_start_script(id, arguments, base_dir);
            } else if (name == "search_files") {
                res_json = tool_search_files(id, arguments, base_dir);
            } else if (name == "execute_command") {
                res_json = tool_execute_command(id, arguments, base_dir);
            } else if (name == "take_screenshot") {
                res_json = tool_take_screenshot(id, arguments, base_dir);
            } else if (name == "get_knowledge") {
                res_json = tool_get_knowledge(id, arguments);
            } else if (name == "update_knowledge") {
                res_json = tool_update_knowledge(id, arguments);
            } else {
                res_json = make_error(id, -32601, "Tool not found");
            }
        }
        
        bool is_error_response = false;
        if (res_json.contains("result") && res_json["result"].contains("content") && res_json["result"]["content"].is_array()) {
            for (const auto& item : res_json["result"]["content"]) {
                if (item.contains("text")) {
                    std::string original_text = item["text"].get<std::string>();
                    if (target_servers.size() > 1 || target_server != "local") {
                        combined_content.push_back({{"type", "text"}, {"text", "[Server " + target_server + "]\n" + original_text}});
                    } else {
                        combined_content.push_back({{"type", "text"}, {"text", original_text}});
                    }
                } else {
                    combined_content.push_back(item);
                }
            }
            if (res_json["result"].contains("isError") && res_json["result"]["isError"].get<bool>()) {
                is_error_response = true;
            }
        } else if (res_json.contains("error")) {
            std::string err_msg = res_json["error"].value("message", "Unknown error");
            if (target_servers.size() > 1 || target_server != "local") {
                combined_content.push_back({{"type", "text"}, {"text", "[Server " + target_server + "] Error: " + err_msg}});
            } else {
                combined_content.push_back({{"type", "text"}, {"text", "Error: " + err_msg}});
            }
            is_error_response = true;
        }
        
        if (is_error_response) has_error = true;
    }
    
    json final_result = {
        {"content", combined_content}
    };
    if (has_error) {
        final_result["isError"] = true;
    }
    
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", final_result}
    };
}
