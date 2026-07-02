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
        dictionary["EN"]["SERVER_LOCAL"] = wxString::FromUTF8("LOCAL SERVER");
        dictionary["EN"]["CLUSTER_NODES"] = wxString::FromUTF8("Cluster Nodes");
        dictionary["EN"]["TOOLS"] = wxString::FromUTF8("Tools");
        dictionary["EN"]["GLOBAL_SETTINGS"] = wxString::FromUTF8("GLOBAL SETTINGS");
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
        dictionary["EN"]["CLUSTER_NODES"] = wxString::FromUTF8("Cluster Nodes");
        dictionary["EN"]["CLUSTER_MANAGEMENT"] = wxString::FromUTF8("Cluster Management");
        dictionary["EN"]["CLUSTER_SUB"] = wxString::FromUTF8("Manage connected child nodes and handle incoming requests");
        dictionary["EN"]["NODE_ID"] = wxString::FromUTF8("Node ID");
        dictionary["EN"]["HOSTNAME"] = wxString::FromUTF8("Hostname");
        dictionary["EN"]["IP_ADDRESS"] = wxString::FromUTF8("IP Address");
        dictionary["EN"]["PLATFORM"] = wxString::FromUTF8("Platform");
        dictionary["EN"]["BTN_APPROVE"] = wxString::FromUTF8("Approve Request");
        dictionary["EN"]["BTN_REJECT"] = wxString::FromUTF8("Remove Node");
        dictionary["EN"]["BTN_ADD_NODE"] = wxString::FromUTF8("Add Manual Node");
        dictionary["EN"]["TOOLS_MANAGEMENT"] = wxString::FromUTF8("TOOLS MANAGEMENT");
        dictionary["EN"]["TOOLS_SUB"] = wxString::FromUTF8("Configure and manage active tool integrations across the system");
        dictionary["EN"]["GS_APP_MODE"] = wxString::FromUTF8("Application Mode:");
        dictionary["EN"]["GS_MODE_PARENT"] = wxString::FromUTF8("Parent (Cluster Manager)");
        dictionary["EN"]["GS_MODE_CHILD"] = wxString::FromUTF8("Child (Worker Node)");
        
        dictionary["EN"]["GS_SYSTEM"] = wxString::FromUTF8("SYSTEM & BEHAVIOR");
        dictionary["EN"]["GS_WORKSPACE"] = wxString::FromUTF8("WORKSPACE DIRECTORY");
        dictionary["EN"]["GS_LAUNCH_STARTUP"] = wxString::FromUTF8("Launch on System Startup");
        dictionary["EN"]["GS_AUTO_START_SERVER"] = wxString::FromUTF8("Auto-start Server on App Launch");
        dictionary["EN"]["GS_MINIMIZE_TRAY"] = wxString::FromUTF8("Minimize to System Tray");
        dictionary["EN"]["GS_NOTIFICATIONS"] = wxString::FromUTF8("Show System Notifications");
        
        dictionary["EN"]["GS_STORAGE"] = wxString::FromUTF8("STORAGE & LOGS");
        dictionary["EN"]["GS_LOG_RETENTION"] = wxString::FromUTF8("Log Retention:");
        dictionary["EN"]["GS_CLEAR_CACHE"] = wxString::FromUTF8("Clear Application Cache");
        
        dictionary["EN"]["GS_APPEARANCE"] = wxString::FromUTF8("APPEARANCE");
        dictionary["EN"]["GS_THEME"] = wxString::FromUTF8("Theme:");
        dictionary["EN"]["GS_COMPACT_MODE"] = wxString::FromUTF8("Enable Compact Mode");
        
        dictionary["EN"]["GS_SECURITY"] = wxString::FromUTF8("SECURITY");
        dictionary["EN"]["GS_APP_LOCK"] = wxString::FromUTF8("Require Password on App Launch");
        dictionary["EN"]["GS_MASK_SECRETS"] = wxString::FromUTF8("Mask Secrets in UI");
        
        dictionary["EN"]["GS_UPDATES"] = wxString::FromUTF8("UPDATES");
        dictionary["EN"]["GS_AUTO_UPDATE"] = wxString::FromUTF8("Automatically Install Updates");
        dictionary["EN"]["GS_CHECK_UPDATES"] = wxString::FromUTF8("Check for Updates");
        dictionary["EN"]["GS_LANGUAGE"] = wxString::FromUTF8("LANGUAGE PREFERENCE");
        
        // New translations
        dictionary["EN"]["TAB_OVERVIEW"] = wxString::FromUTF8("Overview");
        dictionary["EN"]["TAB_CONNECTIONS"] = wxString::FromUTF8("Connections");
        dictionary["EN"]["TAB_LOGS"] = wxString::FromUTF8("Logs");
        dictionary["EN"]["SYSTEM_LOAD"] = wxString::FromUTF8("SYSTEM LOAD");
        dictionary["EN"]["LOCAL_SERVER_COMBO"] = wxString::FromUTF8("Local Server");
        dictionary["EN"]["CPU_LABEL"] = wxString::FromUTF8("CPU: 0%");
        dictionary["EN"]["RAM_LABEL"] = wxString::FromUTF8("RAM: 0%");
        dictionary["EN"]["DISK_LABEL"] = wxString::FromUTF8("Disk: 0%");
        dictionary["EN"]["NET_LABEL"] = wxString::FromUTF8("Net: 0 B/s");
        dictionary["EN"]["COL_TOKEN_NAME"] = wxString::FromUTF8("Token Name");
        dictionary["EN"]["COL_NAME"] = wxString::FromUTF8("Name");
        dictionary["EN"]["COL_ID"] = wxString::FromUTF8("ID");
        dictionary["EN"]["COL_TOKEN"] = wxString::FromUTF8("Token");
        dictionary["EN"]["COL_CREATED"] = wxString::FromUTF8("Created");
        
        dictionary["EN"]["BTN_CREATE_TOKEN"] = wxString::FromUTF8("Create Token");
        dictionary["EN"]["BTN_DELETE_TOKEN"] = wxString::FromUTF8("Delete Token");
        dictionary["EN"]["BTN_TOGGLE_TOKEN"] = wxString::FromUTF8("Toggle Active");
        dictionary["EN"]["BTN_EDIT_PERMISSIONS"] = wxString::FromUTF8("Edit Permissions");
        
        dictionary["EN"]["LBL_CPU"] = wxString::FromUTF8("CPU");
        dictionary["EN"]["LBL_RAM"] = wxString::FromUTF8("RAM");
        dictionary["EN"]["LBL_DISK"] = wxString::FromUTF8("DISK");
        dictionary["EN"]["LBL_VRAM"] = wxString::FromUTF8("VRAM");
        
        dictionary["EN"]["LBL_BIND_INFO"] = wxString::FromUTF8("SERVER BIND INFO:");
        dictionary["EN"]["LBL_CONN_INFO"] = wxString::FromUTF8("CONNECTION INFO");
        dictionary["EN"]["LBL_LOC_URL"] = wxString::FromUTF8("Local URL: ");
        dictionary["EN"]["LBL_EXT_URL"] = wxString::FromUTF8("External URL: ");
        dictionary["EN"]["LBL_PUB_IP"] = wxString::FromUTF8("Detected Public IP: ");
        dictionary["EN"]["LBL_CUST_URL"] = wxString::FromUTF8("Custom Base URL: ");
        dictionary["EN"]["LBL_TOKENS"] = wxString::FromUTF8("ACCESS TOKENS");
        dictionary["EN"]["BTN_CREATE_TOKEN"] = wxString::FromUTF8("Create Token");
        dictionary["EN"]["BTN_TOGGLE_TOKEN"] = wxString::FromUTF8("Toggle Active");
        dictionary["EN"]["BTN_EDIT_PERMISSIONS"] = wxString::FromUTF8("Edit Permissions");
        dictionary["EN"]["BTN_COPY_TOKEN_URL"] = wxString::FromUTF8("Copy URL for Token");
        dictionary["EN"]["LBL_NET_CHECK"] = wxString::FromUTF8("NETWORK CHECKER");
        dictionary["EN"]["BTN_CHECK"] = wxString::FromUTF8("Check");
        dictionary["EN"]["LBL_READY"] = wxString::FromUTF8("Ready");
        dictionary["EN"]["LBL_CHECKING"] = wxString::FromUTF8("Checking...");
        dictionary["EN"]["LBL_REACH_EXT"] = wxString::FromUTF8("Reachable externally");
        dictionary["EN"]["LBL_REACH_LOC"] = wxString::FromUTF8("Local only");
        dictionary["EN"]["LBL_NOT_REACH"] = wxString::FromUTF8("Not reachable");
        dictionary["EN"]["LBL_SERVER_LOGS"] = wxString::FromUTF8("SERVER LOGS");
        dictionary["EN"]["LBL_UNKNOWN"] = wxString::FromUTF8("Unknown");
        dictionary["EN"]["CLUSTER_MANAGEMENT"] = wxString::FromUTF8("CLUSTER MANAGEMENT");
        dictionary["EN"]["CLUSTER_SUB"] = wxString::FromUTF8("Manage connected child nodes and handle incoming requests");
        dictionary["EN"]["BTN_ADD_NODE"] = wxString::FromUTF8("Add Manual Node");
        dictionary["EN"]["BTN_APPROVE"] = wxString::FromUTF8("Approve Request");
        dictionary["EN"]["BTN_REJECT"] = wxString::FromUTF8("Remove Node");
        dictionary["EN"]["TOOLS_MANAGEMENT"] = wxString::FromUTF8("TOOLS MANAGEMENT");
        dictionary["EN"]["TOOLS_SUB"] = wxString::FromUTF8("Configure and manage active tool integrations across the system");

        // RU
        dictionary["RU"]["DASHBOARD"] = wxString::FromUTF8("ДЭШБОРД");
        dictionary["RU"]["WORKSPACE"] = wxString::FromUTF8("ДИРЕКТОРИЯ");
        dictionary["RU"]["SETTINGS"] = wxString::FromUTF8("НАСТРОЙКИ");
        dictionary["RU"]["LANGUAGE"] = wxString::FromUTF8("ЯЗЫК");
        dictionary["RU"]["NETWORK"] = wxString::FromUTF8("СЕТЬ");
        dictionary["RU"]["LOGS"] = wxString::FromUTF8("ЛОГИ");
        dictionary["RU"]["SERVER_MANAGER"] = wxString::FromUTF8("МЕНЕДЖЕР MCP СЕРВЕРА");
        dictionary["RU"]["SERVER_LOCAL"] = wxString::FromUTF8("ЛОКАЛЬНЫЙ СЕРВЕР");
        dictionary["RU"]["CLUSTER_NODES"] = wxString::FromUTF8("Узлы Кластера");
        dictionary["RU"]["TOOLS"] = wxString::FromUTF8("Инструменты");
        dictionary["RU"]["GLOBAL_SETTINGS"] = wxString::FromUTF8("НАСТРОЙКИ");
        dictionary["RU"]["STATUS"] = wxString::FromUTF8("Статус: ");
        dictionary["RU"]["OFFLINE"] = wxString::FromUTF8("ВЫКЛ");
        dictionary["RU"]["ONLINE"] = wxString::FromUTF8("ВКЛ");
        dictionary["RU"]["START_BTN"] = wxString::FromUTF8("ЗАПУСТИТЬ");
        dictionary["RU"]["STOP_BTN"] = wxString::FromUTF8("ОСТАНОВИТЬ");
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
        dictionary["RU"]["CLUSTER_NODES"] = wxString::FromUTF8("Узлы кластера");
        dictionary["RU"]["CLUSTER_MANAGEMENT"] = wxString::FromUTF8("УПРАВЛЕНИЕ КЛАСТЕРОМ");
        dictionary["RU"]["CLUSTER_SUB"] = wxString::FromUTF8("Управление подключенными дочерними узлами и обработка запросов");
        dictionary["RU"]["NODE_ID"] = wxString::FromUTF8("Идентификатор");
        dictionary["RU"]["HOSTNAME"] = wxString::FromUTF8("Имя хоста");
        dictionary["RU"]["IP_ADDRESS"] = wxString::FromUTF8("IP адрес");
        dictionary["RU"]["PLATFORM"] = wxString::FromUTF8("Платформа");
        dictionary["RU"]["BTN_APPROVE"] = wxString::FromUTF8("Одобрить запрос");
        dictionary["RU"]["BTN_REJECT"] = wxString::FromUTF8("Удалить узел");
        dictionary["RU"]["BTN_ADD_NODE"] = wxString::FromUTF8("Добавить узел вручную");
        dictionary["RU"]["TOOLS_MANAGEMENT"] = wxString::FromUTF8("УПРАВЛЕНИЕ ИНСТРУМЕНТАМИ");
        dictionary["RU"]["TOOLS_SUB"] = wxString::FromUTF8("Настройка и управление активными интеграциями инструментов");
        dictionary["RU"]["GS_APP_MODE"] = wxString::FromUTF8("Режим приложения:");
        dictionary["RU"]["GS_MODE_PARENT"] = wxString::FromUTF8("Родитель (Главный узел)");
        dictionary["RU"]["GS_MODE_CHILD"] = wxString::FromUTF8("Дочерний (Рабочий узел)");
        
        dictionary["RU"]["GS_SYSTEM"] = wxString::FromUTF8("СИСТЕМА И ПОВЕДЕНИЕ");
        dictionary["RU"]["GS_WORKSPACE"] = wxString::FromUTF8("РАБОЧАЯ ДИРЕКТОРИЯ");
        dictionary["RU"]["GS_LAUNCH_STARTUP"] = wxString::FromUTF8("Запускать при старте системы");
        dictionary["RU"]["GS_AUTO_START_SERVER"] = wxString::FromUTF8("Автоматически запускать сервер");
        dictionary["RU"]["GS_MINIMIZE_TRAY"] = wxString::FromUTF8("Сворачивать в системный трей");
        dictionary["RU"]["GS_NOTIFICATIONS"] = wxString::FromUTF8("Показывать системные уведомления");
        
        dictionary["RU"]["GS_STORAGE"] = wxString::FromUTF8("ПАМЯТЬ И ЛОГИ");
        dictionary["RU"]["GS_LOG_RETENTION"] = wxString::FromUTF8("Хранение логов:");
        dictionary["RU"]["GS_CLEAR_CACHE"] = wxString::FromUTF8("Очистить кэш приложения");
        
        dictionary["RU"]["GS_APPEARANCE"] = wxString::FromUTF8("ВНЕШНИЙ ВИД");
        dictionary["RU"]["GS_THEME"] = wxString::FromUTF8("Тема:");
        dictionary["RU"]["GS_COMPACT_MODE"] = wxString::FromUTF8("Включить компактный режим");
        
        dictionary["RU"]["GS_SECURITY"] = wxString::FromUTF8("БЕЗОПАСНОСТЬ");
        dictionary["RU"]["GS_APP_LOCK"] = wxString::FromUTF8("Запрашивать пароль при запуске");
        dictionary["RU"]["GS_MASK_SECRETS"] = wxString::FromUTF8("Скрывать секреты в интерфейсе");
        
        dictionary["RU"]["GS_UPDATES"] = wxString::FromUTF8("ОБНОВЛЕНИЯ");
        dictionary["RU"]["GS_AUTO_UPDATE"] = wxString::FromUTF8("Автоматически устанавливать обновления");
        dictionary["RU"]["GS_CHECK_UPDATES"] = wxString::FromUTF8("Проверить обновления");
        dictionary["RU"]["GS_LANGUAGE"] = wxString::FromUTF8("ВЫБОР ЯЗЫКА");
        
        // New translations
        dictionary["RU"]["TAB_OVERVIEW"] = wxString::FromUTF8("Обзор");
        dictionary["RU"]["TAB_CONNECTIONS"] = wxString::FromUTF8("Подключения");
        dictionary["RU"]["TAB_LOGS"] = wxString::FromUTF8("Логи");
        dictionary["RU"]["SYSTEM_LOAD"] = wxString::FromUTF8("СИСТЕМНАЯ НАГРУЗКА");
        dictionary["RU"]["LOCAL_SERVER_COMBO"] = wxString::FromUTF8("Локальный сервер");
        dictionary["RU"]["CPU_LABEL"] = wxString::FromUTF8("ЦП: 0%");
        dictionary["RU"]["RAM_LABEL"] = wxString::FromUTF8("ОЗУ: 0%");
        dictionary["RU"]["DISK_LABEL"] = wxString::FromUTF8("Диск: 0%");
        dictionary["RU"]["NET_LABEL"] = wxString::FromUTF8("Сеть: 0 Б/с");
        dictionary["RU"]["COL_NAME"] = wxString::FromUTF8("Имя");
        dictionary["RU"]["COL_ID"] = wxString::FromUTF8("ID");
        dictionary["RU"]["COL_TOKEN"] = wxString::FromUTF8("Токен");
        dictionary["RU"]["COL_CREATED"] = wxString::FromUTF8("Создан");
        dictionary["RU"]["COL_STATUS"] = wxString::FromUTF8("Статус");
        
        dictionary["RU"]["BTN_CREATE_TOKEN"] = wxString::FromUTF8("Создать Токен");
        dictionary["RU"]["BTN_DELETE_TOKEN"] = wxString::FromUTF8("Удалить Токен");
        dictionary["RU"]["BTN_TOGGLE_TOKEN"] = wxString::FromUTF8("Вкл/Выкл");
        dictionary["RU"]["BTN_EDIT_PERMISSIONS"] = wxString::FromUTF8("Права Доступа");
        
        dictionary["RU"]["LBL_CPU"] = wxString::FromUTF8("ЦП");
        dictionary["RU"]["LBL_RAM"] = wxString::FromUTF8("ОЗУ");
        dictionary["RU"]["LBL_DISK"] = wxString::FromUTF8("Диск");
        dictionary["RU"]["LBL_VRAM"] = wxString::FromUTF8("Видеопамять");
        
        dictionary["RU"]["LBL_BIND_INFO"] = wxString::FromUTF8("ПАРАМЕТРЫ ЗАПУСКА СЕРВЕРА:");
        dictionary["RU"]["LBL_CONN_INFO"] = wxString::FromUTF8("ИНФОРМАЦИЯ О ПОДКЛЮЧЕНИИ");
        dictionary["RU"]["LBL_LOC_URL"] = wxString::FromUTF8("Локальный URL: ");
        dictionary["RU"]["LBL_EXT_URL"] = wxString::FromUTF8("Внешний URL: ");
        dictionary["RU"]["LBL_PUB_IP"] = wxString::FromUTF8("Публичный IP: ");
        dictionary["RU"]["LBL_CUST_URL"] = wxString::FromUTF8("Свой базовый URL: ");
        dictionary["RU"]["LBL_TOKENS"] = wxString::FromUTF8("ТОКЕНЫ ДОСТУПА");
        dictionary["RU"]["BTN_CREATE_TOKEN"] = wxString::FromUTF8("Создать Токен");
        dictionary["RU"]["BTN_TOGGLE_TOKEN"] = wxString::FromUTF8("Вкл/Выкл");
        dictionary["RU"]["BTN_EDIT_PERMISSIONS"] = wxString::FromUTF8("Права Доступа");
        dictionary["RU"]["BTN_COPY_TOKEN_URL"] = wxString::FromUTF8("Копировать URL Токена");
        dictionary["RU"]["LBL_NET_CHECK"] = wxString::FromUTF8("ПРОВЕРКА СЕТИ");
        dictionary["RU"]["BTN_CHECK"] = wxString::FromUTF8("Проверить");
        dictionary["RU"]["LBL_READY"] = wxString::FromUTF8("Готово");
        dictionary["RU"]["LBL_CHECKING"] = wxString::FromUTF8("Проверка...");
        dictionary["RU"]["LBL_REACH_EXT"] = wxString::FromUTF8("Доступен извне");
        dictionary["RU"]["LBL_REACH_LOC"] = wxString::FromUTF8("Только локально");
        dictionary["RU"]["LBL_NOT_REACH"] = wxString::FromUTF8("Недоступен");
        dictionary["RU"]["LBL_SERVER_LOGS"] = wxString::FromUTF8("СЕРВЕРНЫЕ ЛОГИ");
        dictionary["RU"]["LBL_UNKNOWN"] = wxString::FromUTF8("Неизвестно");
        dictionary["RU"]["LBL_NOT_DETECTED"] = wxString::FromUTF8("Не обнаружено");
    }

    std::string current_lang = "RU";
    std::map<std::string, std::map<std::string, wxString>> dictionary;
};
