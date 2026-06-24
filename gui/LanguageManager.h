#pragma once
#include <string>
#include <map>

class LanguageManager {
public:
    static LanguageManager& Get() {
        static LanguageManager instance;
        return instance;
    }

    void SetLanguage(const std::string& lang) {
        current_lang = lang;
    }
    
    std::string GetLanguage() const { return current_lang; }

    wxString GetString(const std::string& key) {
        if (dictionary.count(current_lang) && dictionary[current_lang].count(key)) {
            return dictionary[current_lang][key];
        }
        return wxString::FromUTF8(key.c_str());
    }

private:
    LanguageManager() {
        // EN
        dictionary["EN"]["DASHBOARD"] = wxString::FromUTF8("DASHBOARD");
        dictionary["EN"]["WORKSPACE"] = wxString::FromUTF8("WORKSPACE");
        dictionary["EN"]["SETTINGS"] = wxString::FromUTF8("SETTINGS");
        dictionary["EN"]["LANGUAGE"] = wxString::FromUTF8("LANGUAGE");
        dictionary["EN"]["NETWORK"] = wxString::FromUTF8("NETWORK");
        dictionary["EN"]["LOGS"] = wxString::FromUTF8("LOGS");
        dictionary["EN"]["SERVER_MANAGER"] = wxString::FromUTF8("MCP SERVER MANAGER");
        dictionary["EN"]["STATUS"] = wxString::FromUTF8("Status: ");
        dictionary["EN"]["OFFLINE"] = wxString::FromUTF8("OFFLINE");
        dictionary["EN"]["ONLINE"] = wxString::FromUTF8("ONLINE");
        dictionary["EN"]["START_BTN"] = wxString::FromUTF8("START");
        dictionary["EN"]["STOP_BTN"] = wxString::FromUTF8("STOP");
        dictionary["EN"]["LOGS_HEADER"] = wxString::FromUTF8("SERVER LOGS");
        dictionary["EN"]["STATS_SESSIONS"] = wxString::FromUTF8("Active Sessions: ");
        dictionary["EN"]["STATS_ERRORS"] = wxString::FromUTF8("Errors: ");
        dictionary["EN"]["STATS_TOOLS"] = wxString::FromUTF8("Tools Calls: ");
        dictionary["EN"]["STATS_UPTIME"] = wxString::FromUTF8("Uptime: ");
        dictionary["EN"]["IN_DEV"] = wxString::FromUTF8("This section is in development (Visual stub)");
        dictionary["EN"]["WORKSPACE_TITLE"] = wxString::FromUTF8("Select Workspace Directory");
        dictionary["EN"]["CREATE_IF_MISSING"] = wxString::FromUTF8("Create if missing");
        dictionary["EN"]["CURRENT_WORKSPACE"] = wxString::FromUTF8("Workspace: ");
        dictionary["EN"]["NETWORK_INSTRUCTIONS"] = wxString::FromUTF8(
            "Port Forwarding Instructions:\n\n"
            "To make your local MCP Server accessible from the internet (e.g., for remote LLMs):\n"
            "1. Use ngrok: run 'ngrok http 3000' in your terminal.\n"
            "2. Or configure port forwarding on your router to forward port 3000 to this PC.\n"
            "3. Update your remote client configuration with the external URL/IP.\n"
            "Make sure your firewall allows incoming connections on the specified port."
        );

        // RU
        dictionary["RU"]["DASHBOARD"] = wxString::FromUTF8("DASHBOARD");
        dictionary["RU"]["WORKSPACE"] = wxString::FromUTF8("WORKSPACE");
        dictionary["RU"]["SETTINGS"] = wxString::FromUTF8("SETTINGS");
        dictionary["RU"]["LANGUAGE"] = wxString::FromUTF8("LANGUAGE");
        dictionary["RU"]["NETWORK"] = wxString::FromUTF8("NETWORK");
        dictionary["RU"]["LOGS"] = wxString::FromUTF8("LOGS");
        dictionary["RU"]["SERVER_MANAGER"] = wxString::FromUTF8("MCP SERVER MANAGER");
        dictionary["RU"]["STATUS"] = wxString::FromUTF8("Статус: ");
        dictionary["RU"]["OFFLINE"] = wxString::FromUTF8("OFFLINE");
        dictionary["RU"]["ONLINE"] = wxString::FromUTF8("ONLINE");
        dictionary["RU"]["START_BTN"] = wxString::FromUTF8("START");
        dictionary["RU"]["STOP_BTN"] = wxString::FromUTF8("STOP");
        dictionary["RU"]["LOGS_HEADER"] = wxString::FromUTF8("ЛОГИ СЕРВЕРА");
        dictionary["RU"]["STATS_SESSIONS"] = wxString::FromUTF8("Активные сессии: ");
        dictionary["RU"]["STATS_ERRORS"] = wxString::FromUTF8("Ошибки: ");
        dictionary["RU"]["STATS_TOOLS"] = wxString::FromUTF8("Вызовов tools: ");
        dictionary["RU"]["STATS_UPTIME"] = wxString::FromUTF8("Аптайм: ");
        dictionary["RU"]["IN_DEV"] = wxString::FromUTF8("Данный раздел находится в разработке (Визуальная заглушка)");
        dictionary["RU"]["WORKSPACE_TITLE"] = wxString::FromUTF8("Выбор рабочей директории");
        dictionary["RU"]["CREATE_IF_MISSING"] = wxString::FromUTF8("Создать при отсутствии");
        dictionary["RU"]["CURRENT_WORKSPACE"] = wxString::FromUTF8("Директория: ");
        dictionary["RU"]["NETWORK_INSTRUCTIONS"] = wxString::FromUTF8(
            "Инструкция по пробросу портов:\n\n"
            "Чтобы сделать локальный MCP-сервер доступным из интернета (например, для удаленных LLM):\n"
            "1. Используйте ngrok: запустите 'ngrok http 3000' в терминале.\n"
            "2. Или настройте проброс порта 3000 на вашем роутере на этот ПК.\n"
            "3. Обновите конфигурацию удаленного клиента, указав внешний URL/IP.\n"
            "Убедитесь, что ваш брандмауэр разрешает входящие соединения на указанном порту."
        );
    }

    std::string current_lang = "RU";
    std::map<std::string, std::map<std::string, wxString>> dictionary;
};
