#include "settings_manager.h"
#include <fstream>
#include <iostream>
#include <wx/stdpaths.h>
#include <wx/filename.h>

SettingsManager::SettingsManager() {
    Load();
}

std::filesystem::path SettingsManager::GetConfigDir() const {
    wxString configDir = wxStandardPaths::Get().GetUserDataDir();
    if (!wxFileName::DirExists(configDir)) {
        wxFileName::Mkdir(configDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }
    return std::filesystem::path(configDir.ToStdString());
}

std::filesystem::path SettingsManager::GetSettingsFilePath() const {
    return GetConfigDir() / "settings.json";
}

void SettingsManager::Load() {
    auto path = GetSettingsFilePath();
    if (std::filesystem::exists(path)) {
        try {
            std::ifstream f(path);
            nlohmann::json j;
            f >> j;

            launchOnStartup = j.value("launchOnStartup", false);
            autoStartServer = j.value("autoStartServer", false);
            minimizeToTray = j.value("minimizeToTray", false);
            showNotifications = j.value("showNotifications", false);
            appMode = j.value("appMode", "Parent (Cluster Manager)");
            parentMasterToken = j.value("parentMasterToken", "");
            parentEncryptionKey = j.value("parentEncryptionKey", "");
            disabled_tools = j.value("disabled_tools", std::vector<std::string>{});
            serverPort = j.value("serverPort", 3000);
            logRetention = j.value("logRetention", "30 Days");
            theme = j.value("theme", "Dark");
            language = j.value("language", "EN");
            compactMode = j.value("compactMode", false);
            appLock = j.value("appLock", false);
            appPasswordHash = j.value("appPasswordHash", "");
            maskSecrets = j.value("maskSecrets", true);
            autoUpdate = j.value("autoUpdate", false);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load settings: " << e.what() << std::endl;
        }
    }
}

void SettingsManager::Save() {
    try {
        nlohmann::json j;
        j["launchOnStartup"] = launchOnStartup;
        j["autoStartServer"] = autoStartServer;
        j["minimizeToTray"] = minimizeToTray;
        j["showNotifications"] = showNotifications;
        j["appMode"] = appMode;
        j["parentMasterToken"] = parentMasterToken;
        j["parentEncryptionKey"] = parentEncryptionKey;
        j["disabled_tools"] = disabled_tools;
        j["serverPort"] = serverPort;
        j["logRetention"] = logRetention;
        j["theme"] = theme;
        j["language"] = language;
        j["compactMode"] = compactMode;
        j["appLock"] = appLock;
        j["appPasswordHash"] = appPasswordHash;
        j["maskSecrets"] = maskSecrets;
        j["autoUpdate"] = autoUpdate;

        std::ofstream f(GetSettingsFilePath());
        f << j.dump(4);
    } catch (const std::exception& e) {
        std::cerr << "Failed to save settings: " << e.what() << std::endl;
    }
}
