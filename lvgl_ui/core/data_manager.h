#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <stdint.h>
#include "../common.h"

#include "../common.h"
#include "../utils/ui_perf_mgr.h" // 添加性能管理器头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  // 使用isdigit函数
#include <math.h>   // 添加math.h头文件，解决fabs函数未声明问题
#include <fcntl.h>  // 添加fcntl.h，提供O_RDONLY等常量
#include <unistd.h> // 添加unistd.h，提供read、close函数
#include <sys/statvfs.h>

/**
 * 初始化数据管理器
 * @param path 存储路径
 */
void data_manager_init(const char *path);

/**
 * 更新所有数据
 * @return 是否有数据更新
 */
bool data_manager_update(void);

/**
 * 设置数据更新优先级
 * @param priority 优先级(0=低，1=中，2=高)
 */
void data_manager_set_update_priority(uint8_t priority);

/**
 * 通知用户活动，触发更高频率的数据更新
 */
void data_manager_notify_user_activity(void);

/**
 * 检查数据是否有变化
 */
bool data_manager_has_changes(void);

/**
 * 获取存储数据
 * @param used 已用空间(KB)
 * @param total 总空间(KB)
 * @param percent 使用百分比
 */
void data_manager_get_storage(uint64_t *used, uint64_t *total, uint8_t *percent);

/**
 * 获取内存数据
 * @param used 已用内存(KB)
 * @param total 总内存(KB)
 * @param percent 使用百分比
 */
void data_manager_get_memory(uint64_t *used, uint64_t *total, uint8_t *percent);

/**
 * 获取CPU总体数据
 * @param usage CPU使用率(%)
 * @param temp CPU温度(℃)
 */
void data_manager_get_cpu(float *usage, uint8_t *temp);

/**
 * 获取CPU核心数据
 * @param core_idx 核心索引
 * @param usage 核心使用率(%)
 */
void data_manager_get_cpu_core(uint8_t core_idx, float *usage);

/**
 * 获取CPU核心数量
 * @return 核心数量
 */
uint8_t data_manager_get_cpu_core_count(void);

/**
 * 设置动画状态以优化性能
 * @param is_animating 是否正在动画
 */
void data_manager_set_anim_state(bool is_animating);

bool data_manager_is_initialized(void);

/**
 * 释放数据管理器资源
 */
void data_manager_deinit(void);

#endif // DATA_MANAGER_H
