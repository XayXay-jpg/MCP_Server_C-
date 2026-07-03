#include "knowledge_layer.h"
#include <fstream>
#include <filesystem>
#include <iostream>

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
    } else {
        // Initialize default digital twin structure
        knowledgeData = {
            {"server", {{"description", "Main server info"}}},
            {"services", {}},
            {"applications", {}},
            {"policies", {}},
            {"workflow", {}},
            {"incidents", {}},
            {"notes", {}}
        };
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

nlohmann::json KnowledgeLayer::GetFullKnowledge() {
    std::lock_guard<std::mutex> lock(mtx);
    return knowledgeData;
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
