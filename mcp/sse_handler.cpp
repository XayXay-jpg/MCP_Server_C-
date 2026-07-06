#include "sse_handler.h"
#include "utils.h"
#include "knowledge_layer.h"
#include "tools.h"
#include "network_utils.h"

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

void notify_knowledge_changed(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    
    auto send_to_single_session = [&](std::shared_ptr<Session> session) {
        if (!session->active) return;
        
        json knowledge = KnowledgeLayer::GetInstance().GetFullKnowledge();
        TokenInfo token_info;
        for (const auto& t : NetworkUtils::LoadTokens()) {
            if (t.id == session->token_id) {
                token_info = t;
                break;
            }
        }
        
        if (!token_info.id.empty()) {
            json available_tools = get_available_tools(token_info);
            json simplified_tools = json::array();
            for(const auto& t : available_tools) {
                simplified_tools.push_back({
                    {"name", t.value("name", "")},
                    {"description", t.value("description", "")}
                });
            }
            knowledge["server_provided"]["available_tools"] = simplified_tools;
        }
        
        json notification = {
            {"jsonrpc", "2.0"},
            {"method", "notifications/knowledge/updated"},
            {"params", {
                {"data", knowledge}
            }}
        };
        
        std::lock_guard<std::mutex> slock(session->mtx);
        session->messages.push(notification.dump());
        session->cv.notify_one();
    };

    if (!session_id.empty()) {
        auto it = active_sessions.find(session_id);
        if (it != active_sessions.end()) {
            send_to_single_session(it->second);
        }
    } else {
        for (const auto& [id, session] : active_sessions) {
            send_to_single_session(session);
        }
    }
}

void notify_server_state_changed() {
    json notification = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/server/state_changed"}
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

int get_sessions_for_token(const std::string& token_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    int count = 0;
    for (const auto& [id, session] : active_sessions) {
        if (session->active && session->token_id == token_id) {
            count++;
        }
    }
    return count;
}

void stop_all_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    for (const auto& [id, session] : active_sessions) {
        std::lock_guard<std::mutex> slock(session->mtx);
        session->active = false;
        session->cv.notify_all();
    }
}
