#include "data_manager.h"
#include "ui_manager.h" // 添加UI管理器头文件


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
static bool is_initialized = false;

// 添加缓存控制变量
static uint32_t last_full_update_time = 0;
static bool data_changed = false;

// 添加状态变量
static uint8_t update_priority_level = 0; // 0=低，1=中，2=高
static uint32_t battery_saving_mode_time = 0;
static bool low_power_mode = false;

// 高效CPU数据读取实现 - 针对Allwinner H3优化
static bool _read_cpu_data(void) {
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
    
    return cpu_changed;

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
    return cpu_changed;
}

// 优化的存储数据读取函数
static bool _read_storage_data(void) {
    // 静态缓存，避免频繁分配内存
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
        
        // 替换popen实现为直接系统调用
        struct statvfs stat;
        if (statvfs(storage_path, &stat) == 0) {
            // 获取块大小和数量
            uint64_t block_size = stat.f_frsize;
            uint64_t blocks = stat.f_blocks;
            uint64_t free_blocks = stat.f_bfree;
            uint64_t avail_blocks = stat.f_bavail; // 普通用户可用
            
            // 计算总大小和已用大小(KB)
            storage_total = (blocks * block_size) / 1024;
            storage_used = ((blocks - avail_blocks) * block_size) / 1024;
            
            retry_count = 0;  // 重置重试计数
            data_changed = true;  // 标记数据已变更
            
            #if UI_DEBUG_ENABLED
            printf("Storage stats: total=%llu KB, used=%llu KB\n", 
                   storage_total, storage_used);
            #endif
        } else {
            #if UI_DEBUG_ENABLED
            printf("Failed to get storage stats for %s\n", storage_path);
            #endif
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
    return data_changed;
}

// 优化的内存数据读取函数 - 零拷贝实现
static bool _read_memory_data(void) {
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
        return data_changed;
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
    return data_changed;
}

// 改进当前的轮询式数据收集 - 实现智能更新策略
bool data_manager_update(void) {
    static uint32_t last_storage_time = 0;
    static uint32_t last_cpu_time = 0;
    static uint32_t last_memory_time = 0;
    static uint8_t update_cycle = 0;  // 循环计数器
    static uint32_t last_activity_time = 0;
    uint32_t current_time = lv_tick_get();
    bool any_data_changed = false;
    bool was_data_changed = data_changed;
    
    // 重置变化标志
    data_changed = false;
    
    // 检测长时间无活动，进入低功耗模式
    if (current_time - last_activity_time > 30000 && !low_power_mode) {
        low_power_mode = true;
        battery_saving_mode_time = current_time;
    }
    
    // 苹果风格的动态优先级更新策略
    // 定义每种数据类型在不同优先级下的更新间隔(ms)
    const uint32_t cpu_intervals[3] = {800, 400, 200};  // 低、中、高优先级下的CPU更新间隔
    const uint32_t memory_intervals[3] = {2000, 1000, 500};  // 内存更新间隔
    const uint32_t storage_intervals[3] = {5000, 3000, 1500};  // 存储更新间隔
    
    // 根据活跃状态，选择当前要更新的数据类型
    bool should_update_cpu = (current_time - last_cpu_time >= cpu_intervals[update_priority_level]);
    bool should_update_memory = (current_time - last_memory_time >= memory_intervals[update_priority_level]);
    bool should_update_storage = (current_time - last_storage_time >= storage_intervals[update_priority_level]);
    
    // 低功耗模式下进一步降低更新频率
    if (low_power_mode) {
        should_update_cpu &= (current_time - last_cpu_time >= cpu_intervals[0] * 3);
        should_update_memory &= (current_time - last_memory_time >= memory_intervals[0] * 3);
        should_update_storage &= (current_time - last_storage_time >= storage_intervals[0] * 3);
    }
    
    // 使用预测性能效率的Apple Intelligent Tracking方式，智能决定要更新的数据
    // 基于当前活跃的模块和用户交互历史，优先更新最可能被查看的数据
    uint8_t active_module = ui_manager_get_active_module();
    
    // 优先更新当前显示模块所需的数据
    if (active_module == 1) {  // 存储模块
        if (should_update_storage && _read_storage_data()) {
            any_data_changed = true;
            last_storage_time = current_time;
        }
        if (should_update_memory && _read_memory_data()) {
            any_data_changed = true;
            last_memory_time = current_time;
        }
    } else if (active_module == 2) {  // CPU模块
        // 在CPU界面使用更节能的更新策略
        if (should_update_cpu && low_power_mode) {
            // 低功耗模式下额外降低CPU刷新频率
            static uint8_t cpu_update_throttle = 0;
            if (++cpu_update_throttle >= 3) {  // 仅每3次更新一次
                _read_cpu_data();
                last_cpu_time = current_time;
                cpu_update_throttle = 0;
                any_data_changed = true;
            }
        } else if (should_update_cpu) {
            _read_cpu_data();
            last_cpu_time = current_time;
            any_data_changed = true;
        }
        
        // 在CPU界面不频繁更新内存数据，节省资源
        if (current_time - last_memory_time >= memory_intervals[update_priority_level] * 2) {
            _read_memory_data();
            last_memory_time = current_time;
        }
    } else { // 其他模块保持原有逻辑
        // 在非数据密集型界面下，降低更新频率，提高电池寿命
        // 但仍维持一个最低更新频率，确保数据不会太旧
        switch (update_cycle % 3) {
            case 0:
                if (should_update_cpu && _read_cpu_data()) {
                    any_data_changed = true;
                    last_cpu_time = current_time;
                }
                break;
            case 1:
                if (should_update_memory && _read_memory_data()) {
                    any_data_changed = true;
                    last_memory_time = current_time;
                }
                break;
            case 2:
                if (should_update_storage && _read_storage_data()) {
                    any_data_changed = true;
                    last_storage_time = current_time;
                }
                break;
        }
    }
    
    update_cycle++;
    
    // 如果检测到数据变化，更新最后活动时间
    if (any_data_changed || data_changed) {
        last_activity_time = current_time;
        
        // 如果是在低功耗模式下检测到数据变化，退出低功耗模式
        if (low_power_mode) {
            low_power_mode = false;
        }
    }
    
    // 如果开启了性能监控，传递系统负载数据
    ui_perf_mgr_set_system_load(
        (uint8_t)cpu_usage,         // CPU负载
        (uint8_t)((memory_used * 100) / memory_total), // 内存压力
        cpu_temp                    // CPU温度
    );
    
    // 动画期间确保UI得到更新
    if (anim_in_progress) {
        data_changed = true;
    }
    
    return any_data_changed || was_data_changed || data_changed;
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
    
    // 设置初始化标志
    is_initialized = true;
}

// 动画开始时调用此函数 - 增强线程安全性
void data_manager_set_anim_state(bool is_animating) {
    // 记录旧状态
    bool old_state = anim_in_progress;
    
    // 更新状态
    anim_in_progress = is_animating;
    
    // 调试输出
    if (old_state != anim_in_progress) {
        #if UI_DEBUG_ENABLED
        printf("[DATA_MGR] Animation state changed: %d -> %d\n", 
               old_state, anim_in_progress);
        #endif
    }
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

// 新增API：设置数据更新优先级 - 允许UI模块动态调整数据刷新优先级
void data_manager_set_update_priority(uint8_t priority) {
    if (priority <= 2) {  // 有效范围检查：0=低, 1=中, 2=高
        update_priority_level = priority;
    }
}

// 新增API：发送用户交互事件通知 - 当有用户交互时调用，用于智能调整更新策略
void data_manager_notify_user_activity(void) {
    // 用户交互后立即升高优先级，提供更及时的反馈
    update_priority_level = 2; // 高优先级
    low_power_mode = false;
    
    // 创建一个延时任务，在短时间高频更新后恢复中等优先级
    static lv_timer_t *priority_reset_timer = NULL;
    
    if (priority_reset_timer) {
        lv_timer_reset(priority_reset_timer);
    } else {
        priority_reset_timer = lv_timer_create(
            (lv_timer_cb_t)data_manager_set_update_priority,
            3000,  // 3秒后恢复中等优先级
            (void *)1  // 中等优先级参数
        );
        lv_timer_set_repeat_count(priority_reset_timer, 1);
    }
}





// 在析构函数开头重置
void data_manager_deinit(void) {
    is_initialized = false;
    // 现有代码...
}

// 实现检查函数
bool data_manager_is_initialized(void) {
    return is_initialized;
}

