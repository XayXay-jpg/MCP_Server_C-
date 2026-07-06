#include "sys_stats.h"
#include <fstream>
#include <string>
#include <sstream>
#include <cstdio>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <chrono>

static std::atomic<double> g_vramUsedGB{0.0};
static std::atomic<double> g_vramTotalGB{0.0};
static std::atomic<bool> g_hasVRAM{false};
static std::atomic<bool> g_vramMonitorRunning{false};
static std::thread g_vramThread;

#if defined(_WIN32)
#include <windows.h>
static FILE* popen_win_vram(const char* command, const char* mode) {
    return _popen(command, mode);
}
static int pclose_win_vram(FILE* file) {
    return _pclose(file);
}
#endif

void StartVramMonitor() {
    if (g_vramMonitorRunning.exchange(true)) return;
    
    g_vramThread = std::thread([]() {
        while (g_vramMonitorRunning) {
            unsigned long long totalUsedMB = 0;
            unsigned long long totalMaxMB = 0;
            bool foundGpu = false;
            std::array<char, 128> sysbuf;
            
#if defined(__linux__)
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader,nounits 2>/dev/null", "r"), pclose);
            if (pipe) {
                while (fgets(sysbuf.data(), sysbuf.size(), pipe.get()) != nullptr) {
                    unsigned long long u = 0, t = 0;
                    if (sscanf(sysbuf.data(), "%llu, %llu", &u, &t) == 2) {
                        totalUsedMB += u;
                        totalMaxMB += t;
                        foundGpu = true;
                    }
                }
            }
#elif defined(_WIN32)
            std::unique_ptr<FILE, decltype(&pclose_win_vram)> pipe(popen_win_vram("nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader,nounits 2>nul", "r"), pclose_win_vram);
            if (pipe) {
                while (fgets(sysbuf.data(), sysbuf.size(), pipe.get()) != nullptr) {
                    unsigned long long u = 0, t = 0;
                    if (sscanf(sysbuf.data(), "%llu, %llu", &u, &t) == 2) {
                        totalUsedMB += u;
                        totalMaxMB += t;
                        foundGpu = true;
                    }
                }
            }
#endif
            
            if (foundGpu && totalMaxMB > 0) {
                g_vramUsedGB = totalUsedMB / 1024.0;
                g_vramTotalGB = totalMaxMB / 1024.0;
                g_hasVRAM = true;
            } else {
                g_hasVRAM = false;
            }
            
            // Sleep for 10 seconds, but check exit condition frequently
            for (int i = 0; i < 100 && g_vramMonitorRunning; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
}

void StopVramMonitor() {
    if (g_vramMonitorRunning.exchange(false)) {
        if (g_vramThread.joinable()) {
            g_vramThread.join();
        }
    }
}

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

    // 4. VRAM Usage (from async monitor)
    if (g_hasVRAM) {
        stats.vramUsedGB = g_vramUsedGB.load();
        stats.vramTotalGB = g_vramTotalGB.load();
        if (stats.vramTotalGB > 0) {
            stats.vramPercent = (stats.vramUsedGB / stats.vramTotalGB) * 100.0;
        }
        stats.hasVRAM = true;
    }


    return stats;
}

#elif defined(__APPLE__)

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/statvfs.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>

static uint64_t s_prev_total = 0;
static uint64_t s_prev_idle  = 0;

SystemStats GetSystemStats() {
    SystemStats stats = {};
    stats.hasVRAM = false;

    // 1. CPU Usage via host_processor_info
    {
        natural_t cpuCount = 0;
        processor_info_array_t cpuInfo = nullptr;
        mach_msg_type_number_t cpuInfoCount = 0;

        if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                &cpuCount, &cpuInfo, &cpuInfoCount) == KERN_SUCCESS) {
            uint64_t totalUser = 0, totalSystem = 0, totalIdle = 0, totalNice = 0;
            for (natural_t i = 0; i < cpuCount; ++i) {
                processor_cpu_load_info_t load = (processor_cpu_load_info_t)cpuInfo;
                totalUser   += load[i].cpu_ticks[CPU_STATE_USER];
                totalSystem += load[i].cpu_ticks[CPU_STATE_SYSTEM];
                totalIdle   += load[i].cpu_ticks[CPU_STATE_IDLE];
                totalNice   += load[i].cpu_ticks[CPU_STATE_NICE];
            }
            uint64_t total = totalUser + totalSystem + totalIdle + totalNice;
            uint64_t idle  = totalIdle;

            if (s_prev_total != 0) {
                uint64_t diffTotal = total - s_prev_total;
                uint64_t diffIdle  = idle  - s_prev_idle;
                if (diffTotal > 0)
                    stats.cpuPercent = (double)(diffTotal - diffIdle) / diffTotal * 100.0;
            }
            s_prev_total = total;
            s_prev_idle  = idle;

            vm_deallocate(mach_task_self(), (vm_address_t)cpuInfo,
                          cpuInfoCount * sizeof(integer_t));
        }
    }

    // 2. RAM Usage via mach vm_statistics64
    {
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                              (host_info64_t)&vmStats, &vmCount) == KERN_SUCCESS) {
            uint64_t pageSize = 0;
            size_t psLen = sizeof(pageSize);
            sysctlbyname("hw.pagesize", &pageSize, &psLen, nullptr, 0);
            if (pageSize == 0) pageSize = 4096;

            uint64_t totalPages = 0;
            size_t tLen = sizeof(totalPages);
            sysctlbyname("hw.memsize", &totalPages, &tLen, nullptr, 0);
            uint64_t totalBytes = totalPages; // hw.memsize returns bytes directly

            uint64_t freeBytes  = (uint64_t)vmStats.free_count * pageSize;
            uint64_t activeBytes  = (uint64_t)vmStats.active_count * pageSize;
            uint64_t inactiveBytes= (uint64_t)vmStats.inactive_count * pageSize;
            uint64_t wireBytes  = (uint64_t)vmStats.wire_count * pageSize;
            uint64_t usedBytes  = activeBytes + wireBytes;

            stats.ramTotalGB = totalBytes  / (1024.0 * 1024.0 * 1024.0);
            stats.ramUsedGB  = usedBytes   / (1024.0 * 1024.0 * 1024.0);
            if (totalBytes > 0)
                stats.ramPercent = stats.ramUsedGB / stats.ramTotalGB * 100.0;
        }
    }

    // 3. Disk Usage
    {
        struct statvfs buffer;
        if (statvfs("/", &buffer) == 0) {
            uint64_t total = (uint64_t)buffer.f_blocks * buffer.f_frsize;
            uint64_t free  = (uint64_t)buffer.f_bavail * buffer.f_frsize;
            uint64_t used  = total - free;

            stats.diskTotalGB = total / (1024.0 * 1024.0 * 1024.0);
            stats.diskUsedGB  = used  / (1024.0 * 1024.0 * 1024.0);
            if (total > 0)
                stats.diskPercent = stats.diskUsedGB / stats.diskTotalGB * 100.0;
        }
    }

    return stats;
}

#elif defined(_WIN32)

#include <windows.h>
#include <array>
#include <string>
#include <memory>

static FILE* popen_win(const char* command, const char* mode) {
    return _popen(command, mode);
}
static int pclose_win(FILE* file) {
    return _pclose(file);
}

static unsigned long long FileTimeToULL(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

static unsigned long long prevTotalTime = 0;
static unsigned long long prevIdleTime = 0;

SystemStats GetSystemStats() {
    SystemStats stats = {};
    stats.hasVRAM = false;

    // 1. CPU Usage
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        unsigned long long idle = FileTimeToULL(idleTime);
        unsigned long long kernel = FileTimeToULL(kernelTime);
        unsigned long long user = FileTimeToULL(userTime);
        unsigned long long total = kernel + user;

        if (prevTotalTime != 0) {
            unsigned long long diffTotal = total - prevTotalTime;
            unsigned long long diffIdle = idle - prevIdleTime;
            if (diffTotal > 0) {
                stats.cpuPercent = (double)(diffTotal - diffIdle) / diffTotal * 100.0;
            }
        }
        prevTotalTime = total;
        prevIdleTime = idle;
    }

    // 2. RAM Usage
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        stats.ramTotalGB = memInfo.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
        unsigned long long usedRam = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        stats.ramUsedGB = usedRam / (1024.0 * 1024.0 * 1024.0);
        if (stats.ramTotalGB > 0) {
            stats.ramPercent = (stats.ramUsedGB / stats.ramTotalGB) * 100.0;
        }
    }

    // 3. Disk Usage (C:\ drive)
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        stats.diskTotalGB = totalNumberOfBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
        unsigned long long usedDisk = totalNumberOfBytes.QuadPart - totalNumberOfFreeBytes.QuadPart;
        stats.diskUsedGB = usedDisk / (1024.0 * 1024.0 * 1024.0);
        if (stats.diskTotalGB > 0) {
            stats.diskPercent = (stats.diskUsedGB / stats.diskTotalGB) * 100.0;
        }
    }

    // 4. VRAM Usage (from async monitor)
    if (g_hasVRAM) {
        stats.vramUsedGB = g_vramUsedGB.load();
        stats.vramTotalGB = g_vramTotalGB.load();
        if (stats.vramTotalGB > 0) {
            stats.vramPercent = (stats.vramUsedGB / stats.vramTotalGB) * 100.0;
        }
        stats.hasVRAM = true;
    }


    return stats;
}

#else

// Stubs for other platforms
SystemStats GetSystemStats() {
    SystemStats stats = {};
    return stats;
}

#endif
