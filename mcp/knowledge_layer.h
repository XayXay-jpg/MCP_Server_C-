#pragma once
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

class KnowledgeLayer {
public:
    static KnowledgeLayer& GetInstance();
    
    void Load(const std::string& workspace_dir);
    void Save();
    
    nlohmann::json GetFullKnowledge();
    nlohmann::json GetSection(const std::string& section);
    void UpdateSection(const std::string& section, const nlohmann::json& data);
    std::vector<std::string> GetSections();

private:
    KnowledgeLayer() = default;
    
    std::string filepath;
    nlohmann::json knowledgeData;
    std::mutex mtx;
};
