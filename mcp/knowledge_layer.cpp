#include "knowledge_layer.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "../gui/sys_stats.h"

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

KnowledgeLayer& KnowledgeLayer::GetInstance() {
    static KnowledgeLayer instance;
    return instance;
}

void KnowledgeLayer::Load(const std::string& workspace_dir) {
    std::lock_guard<std::mutex> lock(mtx);
    filepath = workspace_dir + "/mcp_knowledge.json";
    
    if (std::filesystem::exists(filepath)) {
        std::ifstream file(filepath);
        if (file.is_open()) {
            try {
                file >> knowledgeData;
            } catch (...) {
                knowledgeData = nlohmann::json::object();
            }
        }
    }
    
    // Ensure default sections exist
    std::vector<std::string> default_sections = {
        "server", "applications", "policies", 
        "workflow", "incidents", "notes", "cluster"
    };
    
    bool changed = false;
    for (const auto& sec : default_sections) {
        if (!knowledgeData.contains(sec)) {
            knowledgeData[sec] = nlohmann::json::object();
            if (sec == "server") knowledgeData[sec] = {{"description", "Main server info"}};
            if (sec == "cluster") knowledgeData[sec] = {{"description", "Child servers and family nodes configuration"}};
            changed = true;
        }
    }
    
    if (changed || !std::filesystem::exists(filepath)) {
        Save();
    }
}

void KnowledgeLayer::Save() {
    if (filepath.empty()) return;
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << knowledgeData.dump(4);
    }
}

static std::vector<std::string> GetRunningITPrograms() {
    std::vector<std::string> target_programs = {
        "docker", "dockerd", "kubelet", "nginx", "apache2", "httpd", "mysqld", "postgres", 
        "mongod", "redis-server", "python", "python3", "node", "java", "go", "rustc", 
        "php", "ruby", "code", "idea", "eclipse", "gitlab-runner", "jenkins", 
        "ansible", "terraform", "tmux", "vim", "nvim", "htop", "sshd", "wireshark", 
        "nmap", "tcpdump", "kubectl", "helm", "prometheus", "grafana-server", 
        "elasticsearch", "logstash", "kibana", "kafka", "rabbitmq-server", "sqlite3",
        "containerd", "podman", "vagrant", "virtualbox", "qemu-system", "minikube", "xamp"
    };

    std::string cmd = "";
#ifdef _WIN32
    cmd = "tasklist /FO CSV /NH";
#else
    cmd = "ps -e -o comm=";
#endif

    std::vector<std::string> running;
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) return running;

    char buffer[512];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    PCLOSE(pipe);

    std::transform(output.begin(), output.end(), output.begin(), ::tolower);

    for (const auto& prog : target_programs) {
        if (output.find(prog) != std::string::npos) {
            running.push_back(prog);
        }
    }
    return running;
}

nlohmann::json KnowledgeLayer::GetFullKnowledge() {
    std::lock_guard<std::mutex> lock(mtx);
    
    nlohmann::json full_knowledge;
    full_knowledge["user_provided"] = knowledgeData;
    
    SystemStats stats = GetSystemStats();
    
    nlohmann::json server_provided;
    server_provided["hardware"] = {
        {"cpu_usage_percent", stats.cpuPercent},
        {"ram_used_gb", stats.ramUsedGB},
        {"ram_total_gb", stats.ramTotalGB},
        {"ram_usage_percent", stats.ramPercent},
        {"disk_used_gb", stats.diskUsedGB},
        {"disk_total_gb", stats.diskTotalGB},
        {"disk_usage_percent", stats.diskPercent}
    };
    
#ifdef _WIN32
    server_provided["os"] = "Windows";
#elif __APPLE__
    server_provided["os"] = "macOS";
#elif __linux__
    server_provided["os"] = "Linux";
#else
    server_provided["os"] = "Unknown";
#endif

    server_provided["running_it_programs"] = GetRunningITPrograms();

    full_knowledge["server_provided"] = server_provided;
    
    return full_knowledge;
}

nlohmann::json KnowledgeLayer::GetSection(const std::string& section) {
    std::lock_guard<std::mutex> lock(mtx);
    if (knowledgeData.contains(section)) {
        return knowledgeData[section];
    }
    return nlohmann::json(nullptr);
}

void KnowledgeLayer::UpdateSection(const std::string& section, const nlohmann::json& data) {
    std::lock_guard<std::mutex> lock(mtx);
    knowledgeData[section] = data;
    Save();
}

std::vector<std::string> KnowledgeLayer::GetSections() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> sections;
    for (auto& el : knowledgeData.items()) {
        sections.push_back(el.key());
    }
    return sections;
}
