#include "sse_handler.h"
#include "utils.h"

using json = nlohmann::json;

std::mutex sessions_mutex;
std::map<std::string, std::shared_ptr<Session>> active_sessions;

void send_to_session(const std::string& session_id, const json& response) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = active_sessions.find(session_id);
    if (it != active_sessions.end()) {
        auto session = it->second;
        {
            std::lock_guard<std::mutex> slock(session->mtx);
            session->messages.push(response.dump());
        }
        session->cv.notify_one();
    } else {
        mcp_log("[Warning] Attempt to send response to unknown session: " + session_id);
    }
}

void notify_tools_changed() {
    json notification = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/tools/list_changed"}
    };
    std::lock_guard<std::mutex> lock(sessions_mutex);
    for (const auto& [id, session] : active_sessions) {
        if (session->active) {
            std::lock_guard<std::mutex> slock(session->mtx);
            session->messages.push(notification.dump());
            session->cv.notify_one();
        }
    }
}
