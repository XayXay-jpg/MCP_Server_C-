#pragma once

struct SystemStats {
    double cpuPercent;
    
    double ramUsedGB;
    double ramTotalGB;
    double ramPercent;
    
    double diskUsedGB;
    double diskTotalGB;
    double diskPercent;
    
    double vramUsedGB;
    double vramTotalGB;
    double vramPercent;
    bool hasVRAM;
};

// Функция для получения полной статистики системы
SystemStats GetSystemStats();
