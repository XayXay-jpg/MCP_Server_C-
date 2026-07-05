#pragma once

#include <string>
#include <future>
#include <mutex>
#include <map>
#include <memory>

enum class DangerLevel {
    LOW,
    HIGH,
    MAX
};

struct ConfirmationRequest {
    std::string request_id;
    std::string token_name;
    std::string action_description;
    DangerLevel level;
    std::shared_ptr<std::promise<bool>> promise;
};

class ConfirmationManager {
public:
    static ConfirmationManager& GetInstance() {
        static ConfirmationManager instance;
        return instance;
    }

    // Called by MCP tools thread. Blocks until GUI thread resolves it.
    bool RequestConfirmation(const std::string& token_name, const std::string& action_desc, DangerLevel level);

    // Called by GUI thread to resolve a pending request
    void ResolveConfirmation(const std::string& request_id, bool approved);

    // Get a request by ID (for the GUI thread)
    std::shared_ptr<ConfirmationRequest> GetRequest(const std::string& request_id);

    // Callback setup
    using NotificationCallback = std::function<void(const std::string&)>;
    void SetNotificationCallback(NotificationCallback cb) { m_notify_cb = cb; }

private:
    ConfirmationManager() = default;
    ~ConfirmationManager() = default;

    std::mutex m_mutex;
    std::map<std::string, std::shared_ptr<ConfirmationRequest>> m_pending_requests;
    int m_counter = 0;
    NotificationCallback m_notify_cb;
};
