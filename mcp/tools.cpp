#include "tools.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <httplib.h>
#include <cstdlib>

namespace fs = std::filesystem;
using json = nlohmann::json;

json get_available_tools() {
    return json::array({
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
        }
    });
}

json tool_take_screenshot(const json& id, const json& arguments) {
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

json tool_list_directory(const json& id, const json& arguments) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>());
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

json tool_read_file(const json& id, const json& arguments) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>());
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

json tool_write_file(const json& id, const json& arguments) {
    if (!arguments.contains("path") || !arguments["path"].is_string() || 
        !arguments.contains("content") || !arguments["content"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' or 'content' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>());
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

json tool_start_script(const json& id, const json& arguments) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'path' argument");
    }
    
    auto safe_path = validate_path(arguments["path"].get<std::string>());
    if (!safe_path) {
        return make_error(id, -32000, "Access denied: Path is outside the sandbox");
    }
    if (!fs::exists(*safe_path)) {
        return make_error(id, -32000, "Script or executable does not exist");
    }

    std::string cmd = "cd \"" + BASE_DIR.string() + "\" && chmod +x \"" + safe_path->string() + "\" && \"" + safe_path->string() + "\"";
    
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
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                return make_error(id, -32000, "Failed to start process");
            }
            char buffer[4096];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            int status = pclose(pipe);
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

json tool_search_files(const json& id, const json& arguments) {
    if (!arguments.contains("query") || !arguments["query"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'query' argument");
    }
    std::string query = arguments["query"].get<std::string>();
    
    try {
        json results = json::array();
        for (const auto& entry : fs::recursive_directory_iterator(BASE_DIR)) {
            std::string filename = entry.path().filename().string();
            if (filename.find(query) != std::string::npos) {
                std::string rel_path = fs::relative(entry.path(), BASE_DIR).string();
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
json tool_execute_command(const json& id, const json& arguments) {
    if (!arguments.contains("command") || !arguments["command"].is_string()) {
        return make_error(id, -32602, "Invalid or missing 'command' argument");
    }
    
    std::string command = arguments["command"].get<std::string>();
    std::string cmd = "cd \"" + BASE_DIR.string() + "\" && " + command + " 2>&1";
    std::string output;
    
    try {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return make_error(id, -32000, "Failed to execute command");
        }
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        int status = pclose(pipe);
        
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

json handle_tools_call(const json& request) {
    auto id = request.value("id", json(nullptr));
    if (!request.contains("params") || !request["params"].contains("name")) {
        return make_error(id, -32602, "Missing tool name in params");
    }
    
    std::string name = request["params"]["name"];
    json arguments = request["params"].value("arguments", json::object());
    
    mcp_log("[Tool] Executing: " + name);
    g_tool_calls++;
    
    if (name == "list_directory") {
        return tool_list_directory(id, arguments);
    } else if (name == "read_file") {
        return tool_read_file(id, arguments);
    } else if (name == "write_file") {
        return tool_write_file(id, arguments);
    } else if (name == "start_script") {
        return tool_start_script(id, arguments);
    } else if (name == "search_files") {
        return tool_search_files(id, arguments);
    } else if (name == "execute_command") {
        return tool_execute_command(id, arguments);
    } else if (name == "take_screenshot") {
        return tool_take_screenshot(id, arguments);
    } else {
        return make_error(id, -32601, "Tool not found");
    }
}
