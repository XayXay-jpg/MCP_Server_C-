#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>

struct Session {
    std::queue<std::string> messages;
    std::mutex mtx;
    std::condition_variable cv;
    bool active = true;
};

extern std::mutex sessions_mutex;
extern std::map<std::string, std::shared_ptr<Session>> active_sessions;

void send_to_session(const std::string& session_id, const nlohmann::json& response);
void notify_tools_changed();
