# MCP Server Manager (C++ / wxWidgets)

[English](#english) | [Русский](#русский)

---

<a name="english"></a>
## 🇬🇧 English

### Overview
**MCP Server Manager** is a robust, desktop-based graphical interface and backend server for the Model Context Protocol (MCP). It is written in C++ and uses wxWidgets for its cross-platform Graphical User Interface (GUI). The server securely exposes a local workspace environment to Large Language Models (LLMs) or remote clients, allowing them to interact with the file system, execute commands, and run scripts in a sandboxed manner.

### Key Features
* **Modern GUI**: Built with wxWidgets and styled with a sleek Teal theme.
* **Workspace Management**: Easily select, create, and manage the active directory available to the LLM.
* **Multilingual Support**: Switch seamlessly between English and Russian on the fly.
* **Live Server Logs**: Monitor tool calls, errors, and system events in real-time.
* **Port Forwarding Guidance**: Built-in instructions on how to expose the local server using ngrok or router configurations.
* **Background Processing**: Execute scripts or GUI apps asynchronously.

### Exposed Tools for LLMs
The server implements the Model Context Protocol and exposes the following tools via JSON-RPC over HTTP:
1. `list_directory`: List files and directories within a specific path.
2. `read_file`: Read the text content of a file.
3. `write_file`: Write or overwrite text content to a file.
4. `start_script`: Launch a script, application, or game. Supports background execution (e.g., for GUI apps).
5. `search_files`: Recursively search for files matching a specific query string.
6. `execute_command`: Execute arbitrary terminal commands in the workspace and return captured output.
7. `take_screenshot`: Capture a screenshot of the main monitor (uses Python PIL internally) and return it as a base64 encoded image.

### Building from Source

**Prerequisites:**
- CMake (>= 3.14)
- A modern C++17 compiler (GCC, Clang, or MSVC)
- wxWidgets (Core & Base)
- Python 3 (required for the `take_screenshot` tool)

**Build Steps:**
```bash
mkdir build && cd build
cmake ..
cmake --build .
```
Dependencies such as `nlohmann/json` and `cpp-httplib` are automatically fetched via CMake FetchContent.

### Running the Server
Launch the compiled `mcp_manager` executable. From the GUI:
1. Navigate to the **Workspace** tab to select or create your working directory.
2. Go to the **Dashboard** and click **START** to initialize the MCP Server.
3. Monitor active sessions, tools calls, and logs directly from the Dashboard.

### Remote Access (Port Forwarding)
To make your local MCP Server accessible from the internet (e.g., for remote LLM clients):
1. Use ngrok: run `ngrok http 3000` in your terminal.
2. Alternatively, configure port forwarding on your home router to forward port 3000 to your PC's local IP address.
3. Update your LLM client configuration with the provided external URL/IP.

---

<a name="русский"></a>
## 🇷🇺 Русский

### Описание
**MCP Server Manager** — это надежное графическое приложение и сервер для работы с протоколом Model Context Protocol (MCP). Проект написан на C++ и использует библиотеку wxWidgets для кроссплатформенного пользовательского интерфейса (GUI). Сервер предоставляет большим языковым моделям (LLM) или удаленным клиентам безопасный доступ к локальной рабочей среде, позволяя взаимодействовать с файловой системой, выполнять команды и запускать скрипты в песочнице.

### Основные возможности
* **Современный GUI**: Построен на базе wxWidgets и оформлен в стильной цветовой палитре (Teal theme).
* **Управление рабочим пространством**: Удобный выбор, создание и управление активной директорией, доступной для LLM.
* **Мультиязычность**: Мгновенное переключение между английским и русским интерфейсом.
* **Живые логи сервера**: Отслеживание вызовов инструментов, ошибок и системных событий в реальном времени.
* **Проброс портов**: Встроенные инструкции по предоставлению доступа к локальному серверу через ngrok или настройки роутера.
* **Фоновые процессы**: Асинхронный запуск скриптов и приложений с графическим интерфейсом.

### Доступные инструменты для LLM
Сервер реализует протокол MCP и предоставляет следующие инструменты через JSON-RPC поверх HTTP:
1. `list_directory`: Просмотр файлов и папок по указанному пути.
2. `read_file`: Чтение текстового содержимого файла.
3. `write_file`: Запись или перезапись текстового содержимого в файл.
4. `start_script`: Запуск скрипта, приложения или игры. Поддерживает фоновое выполнение.
5. `search_files`: Рекурсивный поиск файлов, содержащих определенную строку в названии.
6. `execute_command`: Выполнение произвольных терминальных команд в рабочей директории с возвратом вывода.
7. `take_screenshot`: Создание скриншота основного монитора (использует Python PIL) и возврат в формате base64.

### Сборка из исходников

**Требования:**
- CMake (>= 3.14)
- Современный компилятор C++17 (GCC, Clang или MSVC)
- wxWidgets (Core & Base)
- Python 3 (требуется для работы инструмента `take_screenshot`)

**Шаги сборки:**
```bash
mkdir build && cd build
cmake ..
cmake --build .
```
Зависимости, такие как `nlohmann/json` и `cpp-httplib`, будут автоматически загружены через CMake FetchContent.

### Запуск сервера
Запустите скомпилированный файл `mcp_manager`. В графическом интерфейсе:
1. Перейдите на вкладку **WORKSPACE** (ДИРЕКТОРИЯ) для выбора или создания рабочей папки.
2. На вкладке **DASHBOARD** (ДЭШБОРД) нажмите **START** (СТАРТ), чтобы запустить MCP сервер.
3. Отслеживайте активные сессии, вызовы инструментов и логи прямо из дэшборда.

### Удаленный доступ (Проброс портов)
Чтобы сделать ваш локальный MCP-сервер доступным из интернета (например, для удаленных клиентов LLM):
1. Используйте ngrok: выполните команду `ngrok http 3000` в вашем терминале.
2. Или настройте проброс порта 3000 на вашем домашнем роутере на локальный IP-адрес этого ПК.
3. Обновите конфигурацию вашего клиента LLM, указав полученный внешний URL/IP.
