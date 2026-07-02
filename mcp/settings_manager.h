#pragma once
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

class SettingsManager {
public:
    static SettingsManager& Get() {
        static SettingsManager instance;
        return instance;
    }

    void Load();
    void Save();

    // System & Behavior
    bool launchOnStartup = false;
    bool autoStartServer = false;
    bool minimizeToTray = false;
    bool showNotifications = false;
    std::string appMode = "Parent (Cluster Manager)";
    std::string parentMasterToken = "";
    std::string parentEncryptionKey = "";
    std::vector<std::string> disabled_tools;

    // Storage & Logs
    std::string logRetention = "30 Days"; // "7 Days", "30 Days", "Keep All"

    // Appearance
    std::string theme = "Dark"; // "System", "Dark", "Light"
    std::string language = "RU";
    bool compactMode = false;

    // Security
    bool appLock = false;
    std::string appPasswordHash = "";
    bool maskSecrets = true;

    // Updates
    bool autoUpdate = false;

    std::filesystem::path GetConfigDir() const;
    std::filesystem::path GetSettingsFilePath() const;
private:
    SettingsManager();
};
