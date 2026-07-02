<div align="center">

# MCP Server Manager

**A cross-platform desktop application for hosting a Model Context Protocol server, built in C++ with wxWidgets.**

[![Language](https://img.shields.io/badge/language-C++17-blue.svg)](https://isocpp.org/)
[![GUI](https://img.shields.io/badge/GUI-wxWidgets-green.svg)](https://wxwidgets.org/)
[![Build System](https://img.shields.io/badge/build-CMake-red.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-informational.svg)]()

[English](#english) · [Русский](#русский)

</div>

---

<a name="english"></a>
## English

### Overview

MCP Server Manager is a production-ready desktop application that runs a local [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) server and exposes it to Large Language Models or remote clients over HTTP with SSE transport. It provides a native GUI for controlling the server, managing access tokens, monitoring live metrics, and configuring the workspace — all without requiring any command-line interaction.

The server uses Bearer Token authentication and supports selective VPN bypass to expose the correct public IP.

---

### Features

| Feature | Description |
|---|---|
| Native Desktop GUI | Built with wxWidgets; no Electron, no web UI overhead |
| Bearer Token Auth | Generate, revoke, and toggle access tokens from the GUI |
| Live Monitoring | Real-time session count, tool call counter, and uptime |
| Workspace Management | Select any directory to expose; auto-creates if missing |
| Public IP Detection | Binds to the physical interface to bypass active VPN tunnels |
| Multilingual UI | Switch between English and Russian at runtime |
| SSE Transport | Full Server-Sent Events support for streaming MCP responses |
| Auto Dependencies | `nlohmann/json` and `cpp-httplib` are fetched via CMake FetchContent |

---

### Exposed MCP Tools

The server implements the Model Context Protocol and provides the following tools to connected LLM clients:

| Tool | Description |
|---|---|
| `list_directory` | List files and directories at a given path |
| `read_file` | Read the text content of any file in the workspace |
| `write_file` | Write or overwrite a file |
| `start_script` | Launch a script, application, or game (supports background mode) |
| `search_files` | Recursively search files by name or content |
| `execute_command` | Run arbitrary shell commands and capture output |
| `take_screenshot` | Capture the screen and return it as a base64-encoded image |

---

### Requirements

- CMake >= 3.14
- C++17 compiler (GCC 9+, Clang 10+)
- wxWidgets >= 3.0 (Core + Base)
- Python 3 (required only for `take_screenshot`)
- `curl` (required for public IP detection)

Install wxWidgets on Debian/Ubuntu:
```bash
sudo apt install libwxgtk3.2-dev
```

---

### Building from Source

```bash
git clone https://github.com/XayXay-jpg/MCP_Server_C-.git
cd MCP_Server_C-
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

All C++ dependencies (`nlohmann/json`, `cpp-httplib`) are automatically fetched during the CMake configure step. No manual installation required.

---

### Usage

1. Launch the `mcp_manager` executable.
2. In the **WORKSPACE** tab — select the directory you want to expose to the LLM.
3. In the **NETWORK** tab — create a Bearer Token and copy the connection string.
4. On the **DASHBOARD** — click **START** to bring the server online.
5. Paste the generated connection string into your LLM client configuration (e.g., Claude Desktop, llama.cpp, or any MCP-compatible client).

#### Connecting from a remote network

Configure port forwarding on your router (port `3000` → your machine's local IP).  
The **Network** tab displays your detected public IP and generates a ready-to-paste connection string with the active token included.

---

### Project Structure

```
MCP_Server_C-/
├── mcp/
│   ├── main.cpp          # HTTP server core, SSE handling, MCP dispatcher
│   ├── tools.cpp         # MCP tool implementations
│   ├── network_utils.cpp # Token management, public IP detection
│   └── utils.cpp         # Shared utilities
├── gui/
│   ├── windows.cpp       # Main application window
│   ├── CustomButton.cpp  # Styled wxButton subclass
│   └── LanguageManager.h # Runtime i18n (EN/RU)
├── icons/                # Application icons
├── CMakeLists.txt
└── README.md
```

---

---

<a name="русский"></a>
## Русский

### Описание

MCP Server Manager — десктопное приложение для запуска локального [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) сервера с нативным графическим интерфейсом. Написано на C++ с использованием wxWidgets.

Приложение позволяет управлять сервером, токенами доступа, рабочим пространством и наблюдать за метриками в реальном времени — без использования командной строки.

---

### Возможности

| Функция | Описание |
|---|---|
| Нативный GUI | На базе wxWidgets, без Electron и браузерных движков |
| Bearer Token аутентификация | Создание, отзыв и переключение токенов из интерфейса |
| Живой мониторинг | Счётчики сессий, вызовов инструментов и аптайм в реальном времени |
| Управление директорией | Любая папка на диске; создаётся автоматически при отсутствии |
| Определение публичного IP | Запрос идёт напрямую через физический интерфейс, минуя VPN |
| Мультиязычный интерфейс | Переключение между русским и английским без перезапуска |
| SSE транспорт | Поддержка Server-Sent Events для потоковой передачи ответов MCP |
| Авто-зависимости | `nlohmann/json` и `cpp-httplib` загружаются через CMake FetchContent |

---

### Инструменты MCP

| Инструмент | Описание |
|---|---|
| `list_directory` | Список файлов и папок по указанному пути |
| `read_file` | Чтение содержимого файла |
| `write_file` | Запись или перезапись файла |
| `start_script` | Запуск скрипта, приложения или игры (в т.ч. в фоне) |
| `search_files` | Рекурсивный поиск файлов по имени или содержимому |
| `execute_command` | Выполнение команд в оболочке с возвратом вывода |
| `take_screenshot` | Снимок экрана в формате base64 |

---

### Требования

- CMake >= 3.14
- Компилятор C++17 (GCC 9+, Clang 10+)
- wxWidgets >= 3.0 (Core + Base)
- Python 3 (только для `take_screenshot`)
- `curl` (для определения публичного IP)

Установка wxWidgets на Debian/Ubuntu:
```bash
sudo apt install libwxgtk3.2-dev
```

---

### Сборка из исходников

```bash
git clone https://github.com/XayXay-jpg/MCP_Server_C-.git
cd MCP_Server_C-
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

Зависимости C++ (`nlohmann/json`, `cpp-httplib`) загружаются автоматически при конфигурировании CMake.

---

### Использование

1. Запустите исполняемый файл `mcp_manager`.
2. Во вкладке **ДИРЕКТОРИЯ** — выберите папку, которую хотите открыть для LLM.
3. Во вкладке **СЕТЬ** — создайте Bearer Token и скопируйте строку подключения.
4. На **ДЭШБОРДЕ** — нажмите **ЗАПУСТИТЬ**.
5. Вставьте строку подключения в конфигурацию вашего LLM-клиента.

#### Подключение из другой сети

Настройте проброс порта `3000` на своём роутере на локальный IP этой машины.  
Вкладка **СЕТЬ** автоматически определяет публичный IP и формирует готовую строку подключения с активным токеном.

---

### Структура проекта

```
MCP_Server_C-/
├── mcp/
│   ├── main.cpp          # HTTP сервер, SSE, диспетчер MCP
│   ├── tools.cpp         # Реализация инструментов MCP
│   ├── network_utils.cpp # Токены, определение публичного IP
│   └── utils.cpp         # Общие утилиты
├── gui/
│   ├── windows.cpp       # Главное окно приложения
│   ├── CustomButton.cpp  # Стилизованный подкласс wxButton
│   └── LanguageManager.h # Локализация (RU/EN)
├── icons/                # Иконки приложения
├── CMakeLists.txt
└── README.md
```

---


