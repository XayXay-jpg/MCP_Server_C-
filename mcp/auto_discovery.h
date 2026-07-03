#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace AutoDiscovery {

    // Main entry point for auto-discovering all applications on the server
    nlohmann::json GetDiscoveredApplications();

    // Specific discovery modules
    std::vector<nlohmann::json> DiscoverDockerContainers();
    std::vector<std::string> DiscoverRunningDatabases();

} // namespace AutoDiscovery
