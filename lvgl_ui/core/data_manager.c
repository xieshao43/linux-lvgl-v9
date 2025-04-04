#include "data_manager.h"
#include "../common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  // 使用isdigit函数
#include <math.h>   // 添加math.h头文件，解决fabs函数未声明问题

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

// 添加优化标志，检测是否有动画正在进行
static bool anim_in_progress = false;

// 添加缓存控制变量
static uint32_t last_full_update_time = 0;
static bool data_changed = false;

// 读取存储数据 - 优化I/O并增加缓存
static void _read_storage_data(void) {
    // 静态缓存值，避免无变化时的重复计算
    static uint64_t last_storage_total = 0;
    static uint64_t last_storage_used = 0;
    
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
    
    // 检查数据是否变化
    if (storage_total != last_storage_total || storage_used != last_storage_used) {
        last_storage_total = storage_total;
        last_storage_used = storage_used;
        data_changed = true;
    }
}

// 读取内存数据 - 优化处理逻辑
static void _read_memory_data(void) {
    // 静态缓存变量
    static uint64_t last_memory_total = 0;
    static uint64_t last_memory_used = 0;
    
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
    
    // 检查数据是否变化
    if (memory_total != last_memory_total || memory_used != last_memory_used) {
        last_memory_total = memory_total;
        last_memory_used = memory_used;
        data_changed = true;
    }
}

// 优化后的CPU数据读取函数 - 更高效的数据解析
static void _read_cpu_data(void) {
    // 保存上一次的值以检测变化
    static float last_cpu_usage = 0.0;
    static float last_cpu_core_usage[CPU_CORES] = {0};
    static uint8_t last_cpu_temp = 0;
    
    // 保持历史数据结构以确保兼容性
    static unsigned long long prev_cpu_data[CPU_CORES+1][8] = {0};
    char buffer[4096] = {0};  // 更大的缓冲区，一次读取所有数据
    
    // 一次性读取/proc/stat文件
    FILE *proc_stat = fopen("/proc/stat", "r");
    if (!proc_stat) {
        // 模拟数据部分保持不变
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
    
    // 一次性读取全部内容到内存
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, proc_stat);
    fclose(proc_stat);  // 立即关闭文件，减少文件句柄占用时间
    
    if (bytes_read == 0) {
        return;  // 读取失败
    }
    buffer[bytes_read] = '\0';  // 确保字符串结束
    
    // 在内存中处理数据
    char *line = buffer;
    char *next_line = NULL;
    int core_idx = 0;
    
    while (line && *line) {
        next_line = strchr(line, '\n');
        if (next_line) *next_line = '\0';  // 将每行结束符替换为字符串结束符
        
        if (strncmp(line, "cpu ", 4) == 0) {
            // 处理总体CPU数据
            unsigned long long values[8] = {0};
            sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                  &values[0], &values[1], &values[2], &values[3],
                  &values[4], &values[5], &values[6], &values[7]);
            
            unsigned long long idle_time = values[3] + values[4];  // idle + iowait
            unsigned long long non_idle = values[0] + values[1] + values[2] + values[5] + values[6] + values[7];
            unsigned long long total = idle_time + non_idle;
            
            // 计算总体CPU使用率
            unsigned long long prev_total = 0;
            unsigned long long prev_idle = 0;
            
            for (int i = 0; i < 8; i++) {
                prev_total += prev_cpu_data[0][i];
                if (i == 3 || i == 4) prev_idle += prev_cpu_data[0][i];
            }
            
            unsigned long long total_diff = total - prev_total;
            unsigned long long idle_diff = idle_time - prev_idle;
            
            if (total_diff > 0) {
                cpu_usage = 100.0f * (float)(total_diff - idle_diff) / total_diff;
            }
            
            // 保存当前值
            for (int i = 0; i < 8; i++) {
                prev_cpu_data[0][i] = values[i];
            }
        } 
        else if (strncmp(line, "cpu", 3) == 0 && isdigit(line[3])) {
            // 处理单核CPU数据
            int core_num = 0;
            sscanf(line, "cpu%d", &core_num);
            
            if (core_num < CPU_CORES) {
                unsigned long long values[8] = {0};
                sscanf(line, "cpu%*d %llu %llu %llu %llu %llu %llu %llu %llu",
                      &values[0], &values[1], &values[2], &values[3],
                      &values[4], &values[5], &values[6], &values[7]);
                
                unsigned long long idle_time = values[3] + values[4];
                unsigned long long non_idle = values[0] + values[1] + values[2] + values[5] + values[6] + values[7];
                unsigned long long total = idle_time + non_idle;
                
                // 计算CPU核心使用率
                unsigned long long prev_total = 0;
                unsigned long long prev_idle = 0;
                
                for (int i = 0; i < 8; i++) {
                    prev_total += prev_cpu_data[core_num+1][i];
                    if (i == 3 || i == 4) prev_idle += prev_cpu_data[core_num+1][i];
                }
                
                unsigned long long total_diff = total - prev_total;
                unsigned long long idle_diff = idle_time - prev_idle;
                
                if (total_diff > 0) {
                    cpu_core_usage[core_num] = 100.0f * (float)(total_diff - idle_diff) / total_diff;
                }
                
                // 保存当前值
                for (int i = 0; i < 8; i++) {
                    prev_cpu_data[core_num+1][i] = values[i];
                }
            }
        }
        
        if (next_line) {
            line = next_line + 1;
        } else {
            break;
        }
    }
    
    // 优化CPU温度读取
    static uint8_t last_valid_temp = 45;  // 保留最后一个有效温度值
    FILE *temp_file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    
    if (temp_file) {
        if (fgets(buffer, sizeof(buffer), temp_file)) {
            int temp = atoi(buffer);
            cpu_temp = temp / 1000;  // 通常以毫摄氏度表示
            last_valid_temp = cpu_temp;  // 更新有效温度
        }
        fclose(temp_file);
    } else {
        // 尝试备选温度文件
        temp_file = fopen("/sys/devices/virtual/thermal/thermal_zone0/temp", "r");
        if (temp_file) {
            if (fgets(buffer, sizeof(buffer), temp_file)) {
                int temp = atoi(buffer);
                cpu_temp = temp / 1000;
                last_valid_temp = cpu_temp;
            }
            fclose(temp_file);
        } else {
            // 使用上次有效温度
            cpu_temp = last_valid_temp;
        }
    }
    
    // 检测数据变化
    bool cpu_changed = (fabs(cpu_usage - last_cpu_usage) > 0.5f);
    last_cpu_usage = cpu_usage;
    
    for (int i = 0; i < CPU_CORES; i++) {
        if (fabs(cpu_core_usage[i] - last_cpu_core_usage[i]) > 0.5f) {
            cpu_changed = true;
            last_cpu_core_usage[i] = cpu_core_usage[i];
        }
    }
    
    if (cpu_temp != last_cpu_temp) {
        cpu_changed = true;
        last_cpu_temp = cpu_temp;
    }
    
    if (cpu_changed) {
        data_changed = true;
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

// 动画开始时调用此函数 
void data_manager_set_anim_state(bool is_animating) {
    anim_in_progress = is_animating;
}

// 改进当前的轮询式数据收集 - 实现智能更新策略
void data_manager_update(void) {
    static uint32_t last_storage_time = 0;
    static uint32_t last_cpu_time = 0;
    static uint32_t last_memory_time = 0;
    uint32_t current_time = lv_tick_get();
    
    // 重置变化标志
    data_changed = false;
    
    // 如果正在动画，减少更新频率但不要完全停止更新
    if (anim_in_progress) {
        // 每1000ms更新所有数据，确保动画后有正确数据
        if (current_time - last_storage_time > 1000) {
            _read_storage_data();
            last_storage_time = current_time;
        }
        
        if (current_time - last_memory_time > 1000) {
            _read_memory_data();
            last_memory_time = current_time;
        }
        
        if (current_time - last_cpu_time > 1000) {
            _read_cpu_data();
            last_cpu_time = current_time;
        }
        
        // 强制设置数据变化标志，确保UI更新
        data_changed = true;
        return;
    }
    
    // 内存数据更新 - 500ms
    if (current_time - last_memory_time > 500) {
        _read_memory_data();
        last_memory_time = current_time;
    }
    
    // CPU数据更新 - 750ms
    if (current_time - last_cpu_time > 750) {
        _read_cpu_data();
        last_cpu_time = current_time;
    }
    
    // 存储数据更新 - 2000ms (降低到2秒以提高响应性)
    if (current_time - last_storage_time > 2000) {
        _read_storage_data();
        last_storage_time = current_time;
    }
}

// 新增函数：检查数据是否有变化
bool data_manager_has_changes(void) {
    return data_changed;
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
        // 确保至少有最小值，避免UI显示为空
        if (*usage < 1.0f) *usage = 1.0f;
    }
}

// 获取CPU核心数量
uint8_t data_manager_get_cpu_core_count(void) {
    return CPU_CORES;
}

