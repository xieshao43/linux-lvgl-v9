#include "data_manager.h"
#include "../common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 存储相关数据
static uint64_t storage_total = 0;
static uint64_t storage_used = 0;
static char storage_path[64] = "/";

// 内存相关数据
static uint64_t memory_total = 0;
static uint64_t memory_used = 0;

// CPU相关数据
static float cpu_usage = 0.0;
static float cpu_core_usage[CPU_CORES] = {0};
static uint8_t cpu_temp = 0;

// 读取存储数据 - 从原有代码中提取
static void _read_storage_data(void) {
    FILE *cmd_output;
    char buffer[256];
    
    // 默认值
    storage_total = 10 * 1024 * 1024;  // 10 GB
    storage_used = 4 * 1024 * 1024;    // 4 GB
    
    // 执行df命令获取存储信息
    snprintf(buffer, sizeof(buffer), "df -k %s | tail -n 1", storage_path);
    cmd_output = popen(buffer, "r");
    
    if (cmd_output) {
        if (fgets(buffer, sizeof(buffer), cmd_output) != NULL) {
            char fs[64], mount[64];
            uint64_t blocks, used, available;
            int percent;
            
            if (sscanf(buffer, "%s %llu %llu %llu %d%% %s", 
                      fs, &blocks, &used, &available, &percent, mount) >= 5) {
                storage_total = blocks;
                storage_used = used;
            }
        }
        pclose(cmd_output);
    } else {
        // 模拟数据
        static int direction = 1;
        storage_used += (rand() % 1024) * 100 * direction;
        
        if (storage_used > storage_total * 0.95) {
            direction = -1;
        } else if (storage_used < storage_total * 0.3) {
            direction = 1;
        }
        
        if (storage_used > storage_total) {
            storage_used = storage_total;
        }
    }
}

// 读取内存数据
static void _read_memory_data(void) {
    FILE *meminfo = fopen("/proc/meminfo", "r");
    
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (strstr(line, "MemTotal:")) {
                sscanf(line, "MemTotal: %llu kB", &memory_total);
            } else if (strstr(line, "MemAvailable:")) {
                uint64_t available;
                sscanf(line, "MemAvailable: %llu kB", &available);
                memory_used = memory_total - available;
            }
        }
        fclose(meminfo);
    } else {
        // 模拟数据
        memory_total = 2048 * 1024;  // 2 GB
        memory_used = 512 * 1024;    // 512 MB
        memory_used += (rand() % 256) - 128;  // 随机波动
    }
}

// 读取CPU数据
static void _read_cpu_data(void) {
    FILE *proc_stat;
    static unsigned long long prev_user[CPU_CORES+1] = {0};
    static unsigned long long prev_nice[CPU_CORES+1] = {0};
    static unsigned long long prev_system[CPU_CORES+1] = {0};
    static unsigned long long prev_idle[CPU_CORES+1] = {0};
    static unsigned long long prev_iowait[CPU_CORES+1] = {0};
    static unsigned long long prev_irq[CPU_CORES+1] = {0};
    static unsigned long long prev_softirq[CPU_CORES+1] = {0};
    static unsigned long long prev_steal[CPU_CORES+1] = {0};
    
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    unsigned long long total, idle_time, non_idle;
    char buffer[256];
    
    proc_stat = fopen("/proc/stat", "r");
    if (!proc_stat) {
        // 文件打开失败，使用轻微变化的模拟数据
        static int direction = 1;
        static float base_usage = 30.0f;
        
        base_usage += direction * (rand() % 5);
        if (base_usage > 80) direction = -1;
        if (base_usage < 20) direction = 1;
        
        cpu_usage = base_usage;
        for (int i = 0; i < CPU_CORES; i++) {
            cpu_core_usage[i] = base_usage + (rand() % 15) - 7;
            if (cpu_core_usage[i] < 0) cpu_core_usage[i] = 0;
            if (cpu_core_usage[i] > 100) cpu_core_usage[i] = 100;
        }
        return;
    }
    
    // 读取总体CPU使用率
    if (fgets(buffer, sizeof(buffer), proc_stat)) {
        if (sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", 
                &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 8) {
            
            idle_time = idle + iowait;
            non_idle = user + nice + system + irq + softirq + steal;
            total = idle_time + non_idle;
            
            // 计算差值
            unsigned long long total_diff = total - 
                (prev_user[0] + prev_nice[0] + prev_system[0] + prev_idle[0] + 
                 prev_iowait[0] + prev_irq[0] + prev_softirq[0] + prev_steal[0]);
            
            unsigned long long idle_diff = idle_time - (prev_idle[0] + prev_iowait[0]);
            
            if (total_diff > 0) {
                cpu_usage = 100.0f * (float)(total_diff - idle_diff) / total_diff;
            }
            
            // 保存当前值用于下次计算
            prev_user[0] = user;
            prev_nice[0] = nice;
            prev_system[0] = system;
            prev_idle[0] = idle;
            prev_iowait[0] = iowait;
            prev_irq[0] = irq;
            prev_softirq[0] = softirq;
            prev_steal[0] = steal;
        }
    }
    
    // 读取各核心CPU使用率
    for (int i = 0; i < CPU_CORES; i++) {
        char cpu_label[10];
        snprintf(cpu_label, sizeof(cpu_label), "cpu%d", i);
        
        // 重置文件位置
        rewind(proc_stat);
        
        while (fgets(buffer, sizeof(buffer), proc_stat)) {
            if (strncmp(buffer, cpu_label, strlen(cpu_label)) == 0) {
                if (sscanf(buffer, "cpu%*d %llu %llu %llu %llu %llu %llu %llu %llu", 
                        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 8) {
                    
                    idle_time = idle + iowait;
                    non_idle = user + nice + system + irq + softirq + steal;
                    total = idle_time + non_idle;
                    
                    // 计算差值
                    unsigned long long total_diff = total - 
                        (prev_user[i+1] + prev_nice[i+1] + prev_system[i+1] + prev_idle[i+1] + 
                         prev_iowait[i+1] + prev_irq[i+1] + prev_softirq[i+1] + prev_steal[i+1]);
                    
                    unsigned long long idle_diff = idle_time - (prev_idle[i+1] + prev_iowait[i+1]);
                    
                    if (total_diff > 0) {
                        cpu_core_usage[i] = 100.0f * (float)(total_diff - idle_diff) / total_diff;
                    }
                    
                    // 保存当前值用于下次计算
                    prev_user[i+1] = user;
                    prev_nice[i+1] = nice;
                    prev_system[i+1] = system;
                    prev_idle[i+1] = idle;
                    prev_iowait[i+1] = iowait;
                    prev_irq[i+1] = irq;
                    prev_softirq[i+1] = softirq;
                    prev_steal[i+1] = steal;
                    
                    break;
                }
            }
        }
    }
    
    fclose(proc_stat);
    
    // 读取CPU温度
    FILE *temp_file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (temp_file) {
        if (fgets(buffer, sizeof(buffer), temp_file)) {
            int temp = atoi(buffer);
            cpu_temp = temp / 1000;  // 通常以毫摄氏度表示
        }
        fclose(temp_file);
    } else {
        // 尝试其他可能的温度文件
        temp_file = fopen("/sys/devices/virtual/thermal/thermal_zone0/temp", "r");
        if (temp_file) {
            if (fgets(buffer, sizeof(buffer), temp_file)) {
                int temp = atoi(buffer);
                cpu_temp = temp / 1000;
            }
            fclose(temp_file);
        } else {
            // 保持上一次的温度值或使用默认值
            if (cpu_temp == 0) {
                cpu_temp = 45;  // 默认温度值
            }
        }
    }
}

// 初始化数据管理器
void data_manager_init(const char *path) {
    if (path) {
        strncpy(storage_path, path, sizeof(storage_path) - 1);
        storage_path[sizeof(storage_path) - 1] = '\0';
    }
    
    // 初始读取数据
    _read_storage_data();
    _read_memory_data();
    _read_cpu_data();
}

// 更新所有数据
void data_manager_update(void) {
    _read_storage_data();
    _read_memory_data();
    _read_cpu_data();
}

// 获取存储数据
void data_manager_get_storage(uint64_t *used, uint64_t *total, uint8_t *percent) {
    if (used) *used = storage_used;
    if (total) *total = storage_total;
    if (percent) {
        *percent = (storage_total > 0) ? 
            (uint8_t)((storage_used * 100) / storage_total) : 0;
    }
}

// 获取内存数据
void data_manager_get_memory(uint64_t *used, uint64_t *total, uint8_t *percent) {
    if (used) *used = memory_used;
    if (total) *total = memory_total;
    if (percent) {
        *percent = (memory_total > 0) ? 
            (uint8_t)((memory_used * 100) / memory_total) : 0;
    }
}

// 获取CPU总体数据
void data_manager_get_cpu(float *usage, uint8_t *temp) {
    if (usage) *usage = cpu_usage;
    if (temp) *temp = cpu_temp;
}

// 获取CPU核心数据
void data_manager_get_cpu_core(uint8_t core_idx, float *usage) {
    if (usage && core_idx < CPU_CORES) {
        *usage = cpu_core_usage[core_idx];
    }
}

// 获取CPU核心数量
uint8_t data_manager_get_cpu_core_count(void) {
    return CPU_CORES;
}

