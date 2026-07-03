#include "auto_discovery.h"
#include <wx/wx.h>
#include <wx/utils.h>
#include <sstream>

namespace AutoDiscovery {

std::vector<nlohmann::json> DiscoverDockerContainers() {
    std::vector<nlohmann::json> containers;
    wxArrayString output;
    wxArrayString errors;
    
    // Attempt to list docker containers
    // Format: name|image|status
    long res = wxExecute("docker ps --format \"{{.Names}}|{{.Image}}|{{.Status}}\"", output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE);
    
    if (res == 0) {
        for (const wxString& line : output) {
            std::string s = line.ToStdString();
            if (s.empty()) continue;
            
            size_t pos1 = s.find('|');
            if (pos1 != std::string::npos) {
                size_t pos2 = s.find('|', pos1 + 1);
                if (pos2 != std::string::npos) {
                    std::string name = s.substr(0, pos1);
                    std::string image = s.substr(pos1 + 1, pos2 - pos1 - 1);
                    std::string status = s.substr(pos2 + 1);
                    
                    nlohmann::json containerData;
                    containerData["name"] = name;
                    containerData["image"] = image;
                    containerData["status"] = status;
                    containers.push_back(containerData);
                }
            }
        }
    }
    
    return containers;
}

std::vector<std::string> DiscoverRunningDatabases() {
    std::vector<std::string> db_list;
    
    struct DbProcess {
        std::string name;
        std::string win_proc;
        std::string lin_proc;
    };
    
    std::vector<DbProcess> databases = {
        {"PostgreSQL", "postgres.exe", "postgres"},
        {"MySQL", "mysqld.exe", "mysqld"},
        {"MongoDB", "mongod.exe", "mongod"},
        {"Redis", "redis-server.exe", "redis-server"},
        {"Nginx", "nginx.exe", "nginx"},
        {"Apache", "httpd.exe", "apache2"},
        {"SQLite", "", "sqlite3"} // Usually embedded, but we can check CLI
    };

    for (const auto& db : databases) {
        wxArrayString output;
        wxArrayString errors;
        long res = -1;
        
#ifdef _WIN32
        if (!db.win_proc.empty()) {
            res = wxExecute("tasklist /FI \"IMAGENAME eq " + db.win_proc + "\"", output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE);
            // tasklist always returns 0, so we must check output for the process name
            bool found = false;
            for (const auto& line : output) {
                if (line.Lower().Contains(wxString(db.win_proc).Lower())) {
                    found = true;
                    break;
                }
            }
            if (found) {
                db_list.push_back(db.name);
            }
        }
#else
        if (!db.lin_proc.empty()) {
            res = wxExecute("pgrep -x " + db.lin_proc, output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE);
            // pgrep returns 0 if at least one process matched
            if (res == 0) {
                db_list.push_back(db.name);
            }
        }
#endif
    }
    
    return db_list;
}

nlohmann::json GetDiscoveredApplications() {
    nlohmann::json result = nlohmann::json::object();
    
    auto containers = DiscoverDockerContainers();
    if (!containers.empty()) {
        result["docker_containers"] = containers;
        result["docker_active"] = true;
    } else {
        result["docker_active"] = false;
    }
    
    auto dbs = DiscoverRunningDatabases();
    result["system_services"] = dbs;
    
    return result;
}

} // namespace AutoDiscovery
