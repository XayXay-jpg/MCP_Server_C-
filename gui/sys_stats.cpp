#include "sys_stats.h"
#include <fstream>
#include <string>
#include <sstream>
#include <cstdio>
#include <memory>
#include <array>

#if defined(__linux__)
#include <sys/statvfs.h>

static unsigned long long lastTotalUser = 0, lastTotalUserLow = 0;
static unsigned long long lastTotalSys = 0, lastTotalIdle = 0;

SystemStats GetSystemStats() {
    SystemStats stats = {0};
    stats.hasVRAM = false;

    // 1. CPU Usage
    std::ifstream cpuFile("/proc/stat");
    if (cpuFile.is_open()) {
        std::string line;
        if (std::getline(cpuFile, line)) {
            if (line.compare(0, 3, "cpu") == 0) {
                std::istringstream ss(line);
                std::string cpuLabel;
                unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
                ss >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

                if (lastTotalUser != 0 || lastTotalUserLow != 0 || lastTotalSys != 0 || lastTotalIdle != 0) {
                    unsigned long long totalUser = user;
                    unsigned long long totalUserLow = nice;
                    unsigned long long totalSys = system;
                    unsigned long long totalIdle = idle;

                    unsigned long long diffUser = totalUser - lastTotalUser;
                    unsigned long long diffUserLow = totalUserLow - lastTotalUserLow;
                    unsigned long long diffSys = totalSys - lastTotalSys;
                    unsigned long long diffIdle = totalIdle - lastTotalIdle;

                    unsigned long long totalDiff = diffUser + diffUserLow + diffSys + diffIdle;
                    if (totalDiff > 0) {
                        stats.cpuPercent = (double)(diffUser + diffUserLow + diffSys) / totalDiff * 100.0;
                    }
                }
                lastTotalUser = user;
                lastTotalUserLow = nice;
                lastTotalSys = system;
                lastTotalIdle = idle;
            }
        }
    }

    // 2. RAM Usage
    std::ifstream memFile("/proc/meminfo");
    if (memFile.is_open()) {
        std::string line;
        unsigned long long memTotal = 0, memFree = 0, buffers = 0, cached = 0, sreclaimable = 0, shmem = 0;
        
        while (std::getline(memFile, line)) {
            std::istringstream ss(line);
            std::string key;
            unsigned long long value;
            std::string unit;
            ss >> key >> value >> unit;
            
            if (key == "MemTotal:") memTotal = value;
            else if (key == "MemFree:") memFree = value;
            else if (key == "Buffers:") buffers = value;
            else if (key == "Cached:") cached = value;
            else if (key == "SReclaimable:") sreclaimable = value;
            else if (key == "Shmem:") shmem = value;
        }

        if (memTotal > 0) {
            unsigned long long memAvailable = memFree + buffers + cached + sreclaimable - shmem;
            unsigned long long memUsed = memTotal - memAvailable;
            
            stats.ramTotalGB = memTotal / (1024.0 * 1024.0);
            stats.ramUsedGB = memUsed / (1024.0 * 1024.0);
            stats.ramPercent = (stats.ramUsedGB / stats.ramTotalGB) * 100.0;
        }
    }

    // 3. Disk Usage (Root partition)
    struct statvfs buffer;
    if (statvfs("/", &buffer) == 0) {
        unsigned long long total = (unsigned long long)buffer.f_blocks * buffer.f_frsize;
        unsigned long long free = (unsigned long long)buffer.f_bavail * buffer.f_frsize;
        unsigned long long used = total - free;
        
        stats.diskTotalGB = total / (1024.0 * 1024.0 * 1024.0);
        stats.diskUsedGB = used / (1024.0 * 1024.0 * 1024.0);
        if (total > 0) {
            stats.diskPercent = (stats.diskUsedGB / stats.diskTotalGB) * 100.0;
        }
    }

    // 4. VRAM Usage (via nvidia-smi)
    std::array<char, 128> sysbuf;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader,nounits 2>/dev/null", "r"), pclose);
    if (pipe) {
        while (fgets(sysbuf.data(), sysbuf.size(), pipe.get()) != nullptr) {
            result += sysbuf.data();
        }
        
        unsigned long long vramUsedMB = 0, vramTotalMB = 0;
        if (sscanf(result.c_str(), "%llu, %llu", &vramUsedMB, &vramTotalMB) == 2) {
            stats.vramUsedGB = vramUsedMB / 1024.0;
            stats.vramTotalGB = vramTotalMB / 1024.0;
            if (vramTotalMB > 0) {
                stats.vramPercent = (double)vramUsedMB / vramTotalMB * 100.0;
                stats.hasVRAM = true;
            }
        }
    }

    return stats;
}

#else

// Stubs for non-Linux platforms
SystemStats GetSystemStats() { 
    SystemStats stats = {0};
    return stats;
}

#endif
