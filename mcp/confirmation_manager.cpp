#include "confirmation_manager.h"

bool ConfirmationManager::RequestConfirmation(const std::string& token_name, const std::string& action_desc, DangerLevel level) {
    if (level == DangerLevel::LOW) return true; // Auto-approve LOW

    std::shared_ptr<ConfirmationRequest> req = std::make_shared<ConfirmationRequest>();
    req->token_name = token_name;
    req->action_description = action_desc;
    req->level = level;
    req->promise = std::make_shared<std::promise<bool>>();

    std::future<bool> future = req->promise->get_future();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        req->request_id = "req_" + std::to_string(++m_counter);
        m_pending_requests[req->request_id] = req;
    }

    if (m_notify_cb) {
        m_notify_cb(req->request_id);
    }

    // Block until GUI resolves the promise
    // To prevent infinite hanging if GUI is dead, maybe add a timeout? 
    // For now, we wait indefinitely as it's a security prompt.
    return future.get();
}

void ConfirmationManager::ResolveConfirmation(const std::string& request_id, bool approved) {
    std::shared_ptr<ConfirmationRequest> req = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pending_requests.find(request_id);
        if (it != m_pending_requests.end()) {
            req = it->second;
            m_pending_requests.erase(it);
        }
    }

    if (req && req->promise) {
        req->promise->set_value(approved);
    }
}

std::shared_ptr<ConfirmationRequest> ConfirmationManager::GetRequest(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pending_requests.find(request_id);
    if (it != m_pending_requests.end()) {
        return it->second;
    }
    return nullptr;
}
