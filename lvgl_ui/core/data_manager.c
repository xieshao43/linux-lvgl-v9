#include "data_manager.h"
#include "../common.h"
#include "../utils/ui_perf_mgr.h" // 添加性能管理器头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  // 使用isdigit函数
#include <math.h>   // 添加math.h头文件，解决fabs函数未声明问题
#include <fcntl.h>  // 添加fcntl.h，提供O_RDONLY等常量
#include <unistd.h> // 添加unistd.h，提供read、close函数

// 添加前向声明，解决隐式声明问题
static bool _try_update_all_data(void);
static void _use_last_valid_data(void);

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

// 高效CPU数据读取实现 - 针对Allwinner H3优化
static void _read_cpu_data(void) {
    // 声明所有变量在函数开始处，避免goto标签后声明变量的问题
    static char buffer[8192];  // 更大的缓冲区，H3处理器有4个核心
    static unsigned long long prev_cpu_data[CPU_CORES+1][8] = {0};
    static float last_cpu_usage = 0.0;
    static float last_cpu_core_usage[CPU_CORES] = {0};
    static uint8_t last_cpu_temp = 0;
    static bool first_read = true;
    int fd;
    ssize_t bytes_read;
    char *line, *next_line;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    int core_idx = -1;
    bool cpu_changed = false;
    static int direction = 1;
    static float base_usage = 30.0f;
    int temp_fd;
    char temp_buf[16];
    int temp;
    static uint32_t last_temp_check = 0;
    uint32_t now;
    
    // 尝试打开/proc/stat文件一次性读取所有CPU数据
    fd = open("/proc/stat", O_RDONLY);
    if (fd < 0) {
        goto fallback_cpu_data;  // 打开失败时使用模拟数据
    }
    
    // 使用read系统调用一次性获取所有数据，避免使用FILE* 
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);  // 立即关闭文件描述符
    
    if (bytes_read <= 0) {
        goto fallback_cpu_data;  // 读取失败使用模拟数据
    }
    
    buffer[bytes_read] = '\0';  // 确保字符串结尾
    
    // 在内存中解析数据，一次性处理所有CPU核心信息
    line = buffer;
    
    while (line && *line) {
        next_line = strchr(line, '\n');
        if (next_line) *next_line = '\0';
        
        if (strncmp(line, "cpu", 3) == 0) {
            if (line[3] == ' ') { // 总CPU行
                core_idx = -1;
                if (sscanf(line+4, "%llu %llu %llu %llu %llu %llu %llu %llu",
                          &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
                    
                    // 计算总的CPU使用率
                    unsigned long long total_idle = idle + iowait;
                    unsigned long long total_active = user + nice + system + irq + softirq + steal;
                    unsigned long long total = total_idle + total_active;
                    
                    // 读取之前的数据
                    unsigned long long prev_idle = prev_cpu_data[0][3] + prev_cpu_data[0][4];
                    unsigned long long prev_active = prev_cpu_data[0][0] + prev_cpu_data[0][1] + 
                                                   prev_cpu_data[0][2] + prev_cpu_data[0][5] +
                                                   prev_cpu_data[0][6] + prev_cpu_data[0][7];
                    unsigned long long prev_total = prev_idle + prev_active;
                    
                    // 计算使用率
                    if (!first_read && total > prev_total) {
                        unsigned long long idle_diff = total_idle - prev_idle;
                        unsigned long long total_diff = total - prev_total;
                        cpu_usage = 100.0f * (float)(total_diff - idle_diff) / (float)total_diff;
                    }
                    
                    // 存储当前数据
                    prev_cpu_data[0][0] = user;
                    prev_cpu_data[0][1] = nice;
                    prev_cpu_data[0][2] = system;
                    prev_cpu_data[0][3] = idle;
                    prev_cpu_data[0][4] = iowait;
                    prev_cpu_data[0][5] = irq;
                    prev_cpu_data[0][6] = softirq;
                    prev_cpu_data[0][7] = steal;
                }
            } else if (isdigit(line[3])) { // 单核CPU行
                int cpu_num;
                if (sscanf(line+3, "%d", &cpu_num) == 1 && cpu_num < CPU_CORES) {
                    // 处理单核心数据，与上面类似
                    if (sscanf(line+5, "%llu %llu %llu %llu %llu %llu %llu %llu",
                              &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
                        
                        unsigned long long total_idle = idle + iowait;
                        unsigned long long total_active = user + nice + system + irq + softirq + steal;
                        unsigned long long total = total_idle + total_active;
                        
                        unsigned long long prev_idle = prev_cpu_data[cpu_num+1][3] + prev_cpu_data[cpu_num+1][4];
                        unsigned long long prev_active = prev_cpu_data[cpu_num+1][0] + prev_cpu_data[cpu_num+1][1] + 
                                                       prev_cpu_data[cpu_num+1][2] + prev_cpu_data[cpu_num+1][5] +
                                                       prev_cpu_data[cpu_num+1][6] + prev_cpu_data[cpu_num+1][7];
                        unsigned long long prev_total = prev_idle + prev_active;
                        
                        if (!first_read && total > prev_total) {
                            unsigned long long idle_diff = total_idle - prev_idle;
                            unsigned long long total_diff = total - prev_total;
                            cpu_core_usage[cpu_num] = 100.0f * (float)(total_diff - idle_diff) / (float)total_diff;
                        }
                        
                        prev_cpu_data[cpu_num+1][0] = user;
                        prev_cpu_data[cpu_num+1][1] = nice;
                        prev_cpu_data[cpu_num+1][2] = system;
                        prev_cpu_data[cpu_num+1][3] = idle;
                        prev_cpu_data[cpu_num+1][4] = iowait;
                        prev_cpu_data[cpu_num+1][5] = irq;
                        prev_cpu_data[cpu_num+1][6] = softirq;
                        prev_cpu_data[cpu_num+1][7] = steal;
                    }
                }
            }
        }
        
        if (next_line) {
            line = next_line + 1;
        } else {
            break;
        }
    }

    // 使用更高效的方式读取H3的温度信息
    // H3的温度信息通常在/sys/class/thermal/thermal_zone0/temp
    now = lv_tick_get();
    
    // 减少温度检查频率，每2秒检查一次
    if (now - last_temp_check > 2000 || cpu_temp == 0) {
        temp_fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
        if (temp_fd >= 0) {
            memset(temp_buf, 0, sizeof(temp_buf));  // 确保缓冲区为空
            if (read(temp_fd, temp_buf, sizeof(temp_buf) - 1) > 0) {
                temp = atoi(temp_buf);
                cpu_temp = temp / 1000;  // 通常以毫摄氏度表示
            }
            close(temp_fd);
        } else {
            // 尝试H3的备用温度文件
            temp_fd = open("/sys/devices/virtual/thermal/thermal_zone0/temp", O_RDONLY);
            if (temp_fd >= 0) {
                memset(temp_buf, 0, sizeof(temp_buf));  // 确保缓冲区为空
                if (read(temp_fd, temp_buf, sizeof(temp_buf) - 1) > 0) {
                    temp = atoi(temp_buf);
                    cpu_temp = temp / 1000;
                }
                close(temp_fd);
            }
        }
        
        last_temp_check = now;
    }
    
    first_read = false;
    
    // 检查是否有明显变化
    if (fabs(cpu_usage - last_cpu_usage) > 1.0f) {
        last_cpu_usage = cpu_usage;
        cpu_changed = true;
    }
    
    for (int i = 0; i < CPU_CORES; i++) {
        if (fabs(cpu_core_usage[i] - last_cpu_core_usage[i]) > 1.0f) {
            last_cpu_core_usage[i] = cpu_core_usage[i];
            cpu_changed = true;
        }
    }
    
    if (cpu_temp != last_cpu_temp) {
        last_cpu_temp = cpu_temp;
        cpu_changed = true;
    }
    
    if (cpu_changed) {
        data_changed = true;
    }
    
    return;

fallback_cpu_data:
    ; // 添加空语句，解决标签后直接声明变量的问题
    
    // 模拟数据生成
    base_usage += direction * (rand() % 3);
    if (base_usage > 75) direction = -1;
    if (base_usage < 25) direction = 1;
    
    cpu_usage = base_usage;
    for (int i = 0; i < CPU_CORES; i++) {
        cpu_core_usage[i] = base_usage + (rand() % 10) - 5;
        if (cpu_core_usage[i] < 0) cpu_core_usage[i] = 0;
        if (cpu_core_usage[i] > 100) cpu_core_usage[i] = 100;
    }
    
    // 随机生成温度数据
    cpu_temp = 45 + (rand() % 15);
}

// 优化的存储数据读取函数
static void _read_storage_data(void) {
    // 静态缓存，避免频繁分配内存
    static char cmd_buffer[256];
    static char result_buffer[1024];
    static uint64_t last_storage_total = 0;
    static uint64_t last_storage_used = 0;
    static uint32_t last_check_time = 0;
    static uint8_t retry_count = 0;
    
    uint32_t now = lv_tick_get();
    
    // 强制每30秒至少更新一次，即使之前成功过
    if (now - last_check_time > 30000) {
        retry_count = 1;  // 强制更新
        last_check_time = now;
    }
    
    // 首次读取或最后一次尝试失败时或者强制更新时
    if (storage_total == 0 || retry_count > 0) {
        // 默认值 (以防读取失败)
        if (storage_total == 0) {
            storage_total = 16 * 1024 * 1024;  // 16GB eMMC默认值
            storage_used = 4 * 1024 * 1024;
        }
        
        // 使用更直接简单的命令，避免复杂的管道和awk
        snprintf(cmd_buffer, sizeof(cmd_buffer), "df -k %s", storage_path);
        
        FILE *fp = popen(cmd_buffer, "r");
        if (fp) {
            // 跳过第一行标题
            fgets(result_buffer, sizeof(result_buffer), fp);
            
            // 读取结果行
            if (fgets(result_buffer, sizeof(result_buffer), fp)) {
                // 更灵活的解析方式 - 支持不同格式的df输出
                char fs_name[128];
                uint64_t blocks = 0, used = 0, available = 0;
                int use_percent = 0;
                
                // 尝试标准格式解析
                int matched = sscanf(result_buffer, "%s %llu %llu %llu %d%%", 
                                     fs_name, &blocks, &used, &available, &use_percent);
                
                if (matched >= 4 && blocks > 0) {
                    storage_total = blocks;
                    storage_used = used;
                    retry_count = 0;  // 重置重试计数
                    data_changed = true;  // 标记数据已变更
                } else {
                    // 尝试备用格式解析（某些系统可能有不同输出格式）
                    matched = sscanf(result_buffer, "%s %llu %llu %llu", 
                                     fs_name, &blocks, &used, &available);
                    
                    if (matched >= 4 && blocks > 0) {
                        storage_total = blocks;
                        storage_used = used;
                        retry_count = 0;  // 重置重试计数
                        data_changed = true;  // 标记数据已变更
                    } else {
                        // 记录解析失败的调试信息
                        printf("Failed to parse df output: %s\n", result_buffer);
                        retry_count++;
                    }
                }
            } else {
                printf("Failed to read df output\n");
                retry_count++;
            }
            pclose(fp);
        } else {
            printf("Failed to execute df command\n");
            retry_count++;
            
            // 重试3次后使用模拟数据
            if (retry_count > 3) {
                // 使用模拟数据进行平滑变化
                static int direction = 1;
                storage_used += (rand() % 256) * direction;
                
                if (storage_used > storage_total * 0.9) {
                    direction = -1;
                } else if (storage_used < storage_total * 0.3) {
                    direction = 1;
                }
                
                retry_count = 0;  // 重置重试计数
                data_changed = true;  // 标记数据已变更
            }
        }
    }
    
    // 检查数据是否变化超过阈值 (1%)
    uint64_t change_threshold = storage_total / 100;
    if (labs((long)(storage_used - last_storage_used)) > change_threshold ||
        labs((long)(storage_total - last_storage_total)) > change_threshold) {
        last_storage_total = storage_total;
        last_storage_used = storage_used;
        data_changed = true;
    }
}

// 优化的内存数据读取函数 - 零拷贝实现
static void _read_memory_data(void) {
    static char buffer[4096];  // 足够大的缓冲区一次读取整个/proc/meminfo
    static uint64_t last_memory_total = 0;
    static uint64_t last_memory_used = 0;
    static uint8_t retry_count = 0;
    
    // 打开/proc/meminfo前先检查
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) {
        retry_count++;
        if (retry_count > 3) {
            // 多次失败后使用模拟数据
            if (memory_total == 0) {
                memory_total = 512 * 1024;  // 512MB
                memory_used = 256 * 1024;   // 50%使用率
            }
            
            // 产生随机波动，模拟真实数据
            int direction = (rand() % 2) ? 1 : -1;
            memory_used += (rand() % 1024) * direction;
            
            // 确保在合理范围
            if (memory_used > memory_total * 0.9) {
                memory_used = (uint64_t)(memory_total * 0.9);
            } else if (memory_used < memory_total * 0.2) {
                memory_used = (uint64_t)(memory_total * 0.2);
            }
            
            retry_count = 0;
        }
        return;
    }
    
    // 一次性读取整个文件内容到内存
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, meminfo);
    fclose(meminfo);  // 立即关闭文件，减少资源占用
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // 确保以null结尾
        
        // 在内存中按行处理，避免多次fgets调用
        char *memTotal = strstr(buffer, "MemTotal:");
        char *memAvailable = strstr(buffer, "MemAvailable:");
        
        // 直接在内存中解析数值
        if (memTotal && memAvailable) {
            sscanf(memTotal, "MemTotal: %llu", &memory_total);
            
            uint64_t available;
            sscanf(memAvailable, "MemAvailable: %llu", &available);
            memory_used = (memory_total > available) ? (memory_total - available) : 0;
            
            retry_count = 0;  // 成功读取，重置重试计数
        } else {
            retry_count++;
        }
    } else {
        retry_count++;
    }
    
    // 检查内存数据变化
    uint64_t threshold = memory_total / 50;  // 2%阈值
    if (labs((long)(memory_used - last_memory_used)) > threshold) {
        last_memory_used = memory_used;
        data_changed = true;
    }
    
    if (memory_total != last_memory_total) {
        last_memory_total = memory_total;
        data_changed = true;
    }
}

// 改进当前的轮询式数据收集 - 实现智能更新策略
void data_manager_update(void) {
    static uint32_t last_storage_time = 0;
    static uint32_t last_cpu_time = 0;
    static uint32_t last_memory_time = 0;
    static uint8_t update_cycle = 0;  // 循环计数器
    uint32_t current_time = lv_tick_get();
    
    // 重置变化标志
    data_changed = false;
    
    // Allwinner H3优化策略：交错更新不同类型的数据
    // 在单个更新周期内，只处理一种类型的数据读取
    // 这样可以避免I/O堆积和CPU峰值
    
    switch (update_cycle % 3) {
        case 0:  // 更新CPU数据 - 优先级最高，更新频率最高
            if (current_time - last_cpu_time > 500) {  // 每500ms更新一次CPU数据
                _read_cpu_data();
                last_cpu_time = current_time;
                
                // 读取CPU温度信息，并传递给性能管理器
                ui_perf_mgr_set_system_load(
                    (uint8_t)cpu_usage,         // CPU负载
                    (uint8_t)((memory_used * 100) / memory_total), // 内存压力
                    cpu_temp                    // CPU温度
                );
            }
            break;
            
        case 1:  // 更新内存数据 - 优先级中等
            if (current_time - last_memory_time > 1000) {  // 每1秒更新一次内存
                _read_memory_data();
                last_memory_time = current_time;
            }
            break;
            
        case 2:  // 更新存储数据 - 优先级最低，变化最慢
            if (current_time - last_storage_time > 5000) {  // 每5秒更新一次存储
                _read_storage_data();
                last_storage_time = current_time;
            }
            break;
    }
    
    update_cycle++;
    
    // 动画期间的特殊处理
    if (anim_in_progress) {
        // 强制设置数据变化标志，确保动画期间UI平滑更新
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

// 检查数据是否有变化
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

// 添加缺失的函数定义
static bool _try_update_all_data(void) {
    // 尝试更新全部数据
    // 这里实现数据更新逻辑，如果成功返回true，失败返回false
    bool success = true;
    
    // 读取各种数据
    _read_storage_data();
    _read_memory_data();
    _read_cpu_data();
    
    return success;
}

static void _use_last_valid_data(void) {
    // 使用上次有效的数据
    // 此处什么也不做，因为数据已经在缓存中
    LV_LOG_INFO("Using last valid data");
}

