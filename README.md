<div align="center">

# MCP Server Manager

**A professional cross-platform desktop application for managing a Model Context Protocol (MCP) server, providing secure access for AI models (LLMs) to your local environment.**

[![Language](https://img.shields.io/badge/language-C++17-blue.svg)](https://isocpp.org/)
[![GUI](https://img.shields.io/badge/GUI-wxWidgets-green.svg)](https://wxwidgets.org/)
[![Build System](https://img.shields.io/badge/build-CMake-red.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-informational.svg)]()

[English](#english) · [Русский](#русский)

</div>

---

<a name="english"></a>
## English (Official Manual)

### Project Overview

**MCP Server Manager** is a specialized software suite acting as a secure "bridge" between modern Large Language Models (LLMs) and your local operating system.

The Model Context Protocol (MCP) allows neural networks to go beyond text generation and **take action**: read local files, run scripts, execute console commands, and analyze directory structures. This application provides a fully-featured server implementing this protocol, equipped with a user-friendly GUI for monitoring and strict access control.

### Architecture & Mechanics

The program operates on a "client-server" model:
1. The **Client** is the AI interface (e.g., Claude Desktop, Cursor, or any MCP-compatible app).
2. The **Server** is our application, running quietly in the background on your machine.

Communication is handled via **HTTP(S)** utilizing **Server-Sent Events (SSE)**. This ensures real-time data streaming, which is critical for fast, responsive AI interactions.

To guarantee security, the application does not expose ports without protection. All requests pass through a strict authorization layer using **Bearer Tokens**. The client must provide a unique cryptographic key with every request; otherwise, the server immediately drops the connection, rejecting access.

### Multi-Node Architecture (Server Families)

One of the most powerful features of MCP technology is the ability for the AI to work with a **family of servers simultaneously**. 

You can run instances of MCP Server Manager on your workstation, your personal laptop, and a remote cloud server. By adding all these endpoints to your LLM client's configuration, you create a unified infrastructure. 

The neural network acts as an orchestrator. It sees the combined toolkit of all connected nodes and routes tasks autonomously. For example, within a single conversation, the AI could read error logs from your remote Linux server, write a code patch on your Windows workstation, and trigger a deployment script from your macOS laptop. MCP Server Manager is specifically engineered for stability in these distributed multi-node clusters.

### Installation (Pre-built Binaries)

Thanks to the integrated cross-platform CI/CD pipeline (GitHub Actions), manual compilation is no longer required.

1. Navigate to the **Releases** page in this GitHub repository.
2. Download the installer for your OS from the latest release:
   * **Windows:** Download `windows-installer.zip`, extract, and run the `.exe`.
   * **macOS:** Download `macos-installer.zip`, extract the `.dmg`, and drag the app to Applications.
   * **Linux:** Download `linux-installer.zip`, extract the `.deb`, and install it via your package manager (e.g., `sudo apt install ./mcp-server-manager.deb`).
3. The app will be integrated into your system menu with its native icon.

### Detailed User Guide

The interface is divided into logical tabs, each managing a specific aspect of the server.

#### 1. Workspace
This is the foundation of your system's security. Here, you define the **strict boundaries** within which the AI can operate.
* Click "Choose Directory" and point it to the folder you want the LLM to access (e.g., your current project folder).
* **Crucial:** The server inherently blocks any attempts by the LLM to read, modify, or delete files outside this folder. Path traversal attempts (like `../../windows/system32`) are strictly rejected.

#### 2. Network & Access
The control center for security and routing.
* **Token Generation:** Click to generate a cryptographically secure, random 64-character access key.
* **Connection String:** The app automatically builds a ready-to-use URL containing your IP, port (default 3000), and active token. This string is the only "key" to your server.
* **Smart IP Detection (VPN Bypass):** If you use a VPN, virtual adapters can hijack routing. The application queries physical network interfaces directly to determine your actual local network IP, preventing connection failures.

#### 3. Dashboard
The main monitoring hub.
* **START Button:** Initializes the HTTP server and opens the port. Until the server is started here (or via Auto-start), external connections are physically impossible.
* **Live Metrics:** Displays the number of active SSE sessions (how many clients are connected), total executed tool calls, and server uptime.

#### 4. Settings
Global application parameters:
* **Auto-start:** Enable this to automatically launch the server and open the port as soon as the app opens.
* **Language:** Switch between English and Russian instantly at runtime.

### Integration (Connecting your LLM)

To allow the AI to interact with your PC:
1. Start the server on the **Dashboard** tab.
2. Go to the **Network** tab and copy the **Connection String**.
3. Open your MCP client's configuration file (e.g., `claude_desktop_config.json`).
4. Add the server config using the `sse` transport type and paste the copied URL.

Example configuration for Claude Desktop:
```json
{
  "mcpServers": {
    "my_workstation": {
      "command": "sse",
      "url": "http://192.168.1.100:3000/sse?token=YOUR_GENERATED_TOKEN"
    }
  }
}
```

### Capabilities (Available Tools)

Once connected, the neural network gains access to specific tools it can call autonomously to solve your tasks:

| Tool | Description & Use Case |
|---|---|
| `list_directory` | Allows the LLM to browse the workspace, understanding the project architecture and file tree. |
| `read_file` | Used by the AI for deep-reading source code, system logs, or text documents. |
| `write_file` | Enables the LLM to write code autonomously, create new files, or apply patches. |
| `start_script` | A versatile tool for executing scripts (Python, Node.js), starting compilers, or launching apps. Supports background execution for long tasks. |
| `search_files` | Full-text recursive search. The LLM can find all files containing specific functions or text. |
| `execute_command` | Direct access to the system shell (cmd/powershell/bash) to run OS commands, install packages (npm, pip), and use Git. Terminal output is returned directly to the AI. |
| `take_screenshot` | A unique visual tool allowing the LLM to "see" your screen (e.g., to review the layout of a website it just coded). *Requires Python.* |

### Security Recommendations

* **Treat the token like a bank PIN.** Anyone or any program possessing your token and local IP can execute arbitrary commands on your PC.
* **Project Isolation:** It's highly recommended to use a specific, dedicated folder for each project in the Workspace settings, rather than exposing the entire `C:\` or `/home/` drive.
* **Instant Revocation:** If you suspect a token leak or have finished working, delete the token in the Network tab. The server will immediately sever all active client sessions.

### Building from Source (For Developers)
1. Install dependencies: `CMake` (>= 3.14), C++17 compiler, `wxWidgets 3.2+`, and `OpenSSL`.
2. Clone and build:
```bash
git clone https://github.com/XayXay-jpg/MCP_Server_C-.git
cd MCP_Server_C-
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

<a name="русский"></a>
## Русский (Официальное руководство)

### Описание проекта

**MCP Server Manager** — это специализированный программный комплекс, выступающий в роли защищенного "моста" между современными большими языковыми моделями (LLM) и вашей локальной операционной системой. 

Технология Model Context Protocol (MCP) позволяет нейросетям не просто генерировать текст, но и **действовать**: читать локальные файлы, запускать скрипты, выполнять консольные команды и анализировать структуру директорий. Данное приложение предоставляет полнофункциональный сервер, реализующий этот протокол, снабженный удобным графическим интерфейсом для мониторинга и жесткого контроля доступа.

### Принцип работы (Архитектура)

Программа работает по принципу "клиент-сервер", где:
1. **Клиентом** выступает интерфейс нейросети (например, Claude Desktop, Cursor или любая другая программа, поддерживающая MCP).
2. **Сервером** является наше приложение, работающее на вашем ПК в фоновом режиме.

Связь осуществляется по протоколу **HTTP(S)** с использованием технологии **Server-Sent Events (SSE)**. Это обеспечивает потоковую передачу данных в реальном времени, что критически важно для быстрых ответов нейросети (стриминг). 

Для обеспечения безопасности приложение не использует открытые порты напрямую. Все запросы проходят через строгую систему авторизации по технологии **Bearer Token**. Клиент обязан предоставить уникальный криптографический ключ при каждом запросе, иначе сервер немедленно разорвет соединение, отклонив доступ.

### Мульти-серверная архитектура (Семейство серверов)

Одной из самых мощных особенностей технологии MCP является возможность ИИ работать с **семейством серверов одновременно**. 

Вы можете запустить экземпляры MCP Server Manager на вашем рабочем ПК, домашнем ноутбуке и удаленном облачном сервере. Добавив все эти узлы в конфигурацию вашего LLM-клиента, вы создаете единую мощную инфраструктуру.

Нейросеть в данном случае выступает в роли "оркестратора". Она видит объединенный набор инструментов всех серверов и самостоятельно распределяет задачи. Например, в рамках одного диалога ИИ может прочитать логи с удаленного сервера на Linux, написать патч для кода на вашей Windows-станции и запустить скрипт деплоя на MacBook. Приложение MCP Server Manager специально спроектировано для максимальной стабильности при работе в таких распределенных кластерах.

### Установка (Готовые пакеты)

Благодаря встроенной системе автоматизированной кроссплатформенной сборки (CI/CD на базе GitHub Actions), вам больше не нужно компилировать программу вручную.

1. Перейдите на страницу **Releases** в этом репозитории на GitHub.
2. Выберите последнюю версию и скачайте установщик для вашей операционной системы:
   * **Windows:** Скачайте архив `windows-installer.zip`, извлеките и запустите установщик `.exe`. 
   * **macOS:** Скачайте `macos-installer.zip`, извлеките образ `.dmg` и перетащите иконку приложения в папку Applications.
   * **Linux:** Скачайте `linux-installer.zip`, извлеките пакет `.deb` и установите его через системный менеджер пакетов (например, командой `sudo apt install ./mcp-server-manager.deb`).
3. Приложение будет автоматически интегрировано в вашу систему и добавлено в меню приложений ОС с оригинальной иконкой.

### Подробное руководство пользователя

Интерфейс программы разделен на логические вкладки, каждая из которых отвечает за отдельный аспект управления сервером.

#### 1. Вкладка "Рабочая среда" (Workspace)
Это фундамент безопасности вашей системы. Здесь вы определяете **строгие границы**, за которые нейросеть не сможет выйти.
* Нажмите кнопку "Выбрать директорию" и укажите папку, с которой будет работать LLM (например, папка вашего текущего проекта кода).
* **Важно:** Сервер на программном уровне блокирует любые попытки LLM прочитать, изменить или удалить файлы, находящиеся за пределами этой папки. Запросы, содержащие попытки обхода путей (например, `../../windows/system32`), будут автоматически отклонены.

#### 2. Вкладка "Сеть и Доступ" (Network)
Центр управления безопасностью и сетевыми подключениями.
* **Генерация токенов:** Нажмите кнопку создания токена. Программа сгенерирует криптографически стойкий случайный 64-символьный ключ доступа.
* **Строка подключения (Connection String):** Программа автоматически формирует готовый URL-адрес, включающий ваш IP, порт (по умолчанию 3000) и активный токен. Эта строка — единственный "ключ" от вашего сервера.
* **Обход VPN (Smart IP Detection):** Если вы используете VPN, виртуальные адаптеры могут перехватывать сетевой трафик. Приложение умеет обращаться напрямую к физическим сетевым интерфейсам ОС, чтобы определить ваш реальный локальный IP-адрес для локальной сети, предотвращая ошибки маршрутизации.

#### 3. Вкладка "Панель управления" (Dashboard)
Главный экран мониторинга состояния сервера.
* **Кнопка Запуска:** Инициализирует HTTP сервер и открывает порт. Пока сервер не запущен из этого меню (или через автозапуск), никакие внешние подключения невозможны физически.
* **Живые метрики (Live Metrics):** Отображает количество активных сессий (сколько раз LLM подключалась к вам в данный момент), количество выполненных команд ("вызовов инструментов") и общее время бесперебойной работы (аптайм).

#### 4. Вкладка "Настройки" (Settings)
Здесь располагаются глобальные параметры конфигурации:
* Вы можете включить **Автозапуск сервера** (чтобы сервер автоматически стартовал и открывал порт при запуске приложения).
* Выбор языка интерфейса (смена происходит мгновенно в рантайме, без необходимости перезагружать программу).

### Интеграция с клиентами (Как подключить LLM)

Чтобы нейросеть начала взаимодействовать с вашим ПК (в рамках дозволенного), необходимо:
1. Запустить сервер на вкладке **Dashboard**.
2. Перейти на вкладку **Network** и скопировать **Строку подключения (Connection String)**.
3. Открыть настройки конфигурации вашего MCP-клиента (например, файл `claude_desktop_config.json` для Claude Desktop).
4. Добавить конфигурацию внешнего сервера, указав тип соединения `sse` и вставив скопированный URL.

Пример конфигурации для Claude Desktop:
```json
{
  "mcpServers": {
    "my_local_pc": {
      "command": "sse",
      "url": "http://192.168.1.100:3000/sse?token=ВАШ_СГЕНЕРИРОВАННЫЙ_ТОКЕН"
    }
  }
}
```

### Доступные инструменты (Capabilities)

После успешного подключения, нейросеть получит доступ к набору инструментов (Tools), которые она может вызывать автономно для решения поставленных вами задач:

| Инструмент | Описание и сценарии использования |
|---|---|
| `list_directory` | Позволяет LLM осмотреться в рабочей папке, получить полную структуру файлов и подпапок для понимания архитектуры проекта. |
| `read_file` | Используется нейросетью для глубокого чтения исходного кода, системных логов или текстовых документов. |
| `write_file` | Позволяет LLM автономно писать код, создавать новые файлы или вносить патчи/исправления в существующие документы. |
| `start_script` | Универсальный инструмент для запуска скриптов (Python, Node.js), запуска компиляции кода или открытия других программ. Поддерживает фоновый режим для долгих задач. |
| `search_files` | Полнотекстовый рекурсивный поиск. LLM может попросить сервер найти все файлы, содержащие определенную функцию, переменную или текст. |
| `execute_command` | Прямой доступ к системной консоли (cmd/powershell/bash) для выполнения команд ОС, установки пакетов (npm, pip), работы с Git и настройки ОС. Весь текстовый вывод консоли возвращается обратно в нейросеть. |
| `take_screenshot` | Уникальный визуальный инструмент, позволяющий LLM "посмотреть" на ваш экран (например, чтобы оценить верстку сайта, который она только что написала, или проанализировать графики). *Требует Python.* |

### Рекомендации по безопасности

* **Относитесь к токену как к банковскому пин-коду.** Любой человек, программа или веб-сайт, обладающие вашим токеном и знающие ваш IP, смогут выполнять любые команды на вашем ПК от вашего имени.
* **Изоляция проектов:** Рекомендуется выделять отдельную пустую папку под каждый новый проект и указывать её в настройках Workspace, а не предоставлять доступ ко всему системному диску (например, `C:\` или `/home/`).
* **Моментальный отзыв доступа:** Если вы подозреваете утечку токена или завершили работу, просто удалите токен на вкладке Network. Все существующие активные сессии клиентов будут немедленно принудительно разорваны сервером.

### Сборка из исходников (Для разработчиков)

1. Установите системные зависимости: `CMake` (>= 3.14), компилятор с поддержкой C++17, `wxWidgets 3.2+` и `OpenSSL`.
2. Выполните клонирование и сборку:
```bash
git clone https://github.com/XayXay-jpg/MCP_Server_C-.git
cd MCP_Server_C-
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---
<div align="center">
<i>Разработано для обеспечения надежной, производительной и безопасной связи между человеком, машиной и искусственным интеллектом.</i>
</div>
