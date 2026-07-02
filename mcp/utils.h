#pragma once
#include <string>
#include <filesystem>
#include <optional>
#include <nlohmann/json.hpp>
#include <functional>
#include <atomic>

// Global logger callback
extern std::function<void(const std::string&)> g_log_callback;
extern std::function<void(const std::string&, const std::string&)> g_notify_callback;
extern std::function<bool(const std::string&, const std::string&)> g_confirm_callback;
extern std::function<void()> g_refresh_cluster_callback;
void mcp_log(const std::string& message);
std::string GetLocalIP();

// Глобальное состояние
extern std::filesystem::path BASE_DIR;
extern std::atomic<int> g_active_sessions;
extern std::atomic<int> g_tool_calls;

// Формирование ошибки
nlohmann::json make_error(const nlohmann::json& id, int code, const std::string& message);

// Безопасность: Валидация пути для предотвращения Path Traversal
std::optional<std::filesystem::path> validate_path(const std::string& user_path, const std::filesystem::path& base_dir = BASE_DIR);
