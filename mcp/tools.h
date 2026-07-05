#pragma once
#include <nlohmann/json.hpp>
#include "network_utils.h"

// Возвращает список всех доступных в коде инструментов
nlohmann::json get_all_tools();

// Возвращает список включенных инструментов
nlohmann::json get_available_tools(const TokenInfo& token);

// Обрабатывает вызов инструмента
nlohmann::json handle_tools_call(const nlohmann::json& request, const TokenInfo& token);

// Отдельные реализации инструментов
nlohmann::json tool_list_directory(const nlohmann::json& id, const nlohmann::json& arguments);
nlohmann::json tool_read_file(const nlohmann::json& id, const nlohmann::json& arguments);
nlohmann::json tool_execute_command(const nlohmann::json& id, const nlohmann::json& arguments, const std::filesystem::path& base_dir);
nlohmann::json tool_take_screenshot(const nlohmann::json& id, const nlohmann::json& arguments, const std::filesystem::path& base_dir);
nlohmann::json tool_update_knowledge(const nlohmann::json& id, const nlohmann::json& arguments);
