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
    std::string token_id = ""; // ID of the token used to authenticate
};

extern std::mutex sessions_mutex;
extern std::map<std::string, std::shared_ptr<Session>> active_sessions;

void send_to_session(const std::string& session_id, const nlohmann::json& response);
void notify_tools_changed();
void notify_knowledge_changed(const std::string& session_id = "");
void notify_server_state_changed();
int get_sessions_for_token(const std::string& token_id);
void stop_all_sessions();
