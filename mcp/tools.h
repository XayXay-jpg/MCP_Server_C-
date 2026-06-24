#pragma once
#include <nlohmann/json.hpp>

// Возвращает список доступных инструментов
nlohmann::json get_available_tools();

// Обрабатывает вызов инструмента
nlohmann::json handle_tools_call(const nlohmann::json& request);

// Отдельные реализации инструментов
nlohmann::json tool_list_directory(const nlohmann::json& id, const nlohmann::json& arguments);
nlohmann::json tool_read_file(const nlohmann::json& id, const nlohmann::json& arguments);
nlohmann::json tool_write_file(const nlohmann::json& id, const nlohmann::json& arguments);
nlohmann::json tool_start_script(const nlohmann::json& id, const nlohmann::json& arguments);
nlohmann::json tool_search_files(const nlohmann::json& id, const nlohmann::json& arguments);
nlohmann::json tool_execute_command(const nlohmann::json& id, const nlohmann::json& arguments);
