/**
 * @file ui_perf_mgr.h
 * @brief UI性能管理器 - 苹果风格高效率优化
 */

#ifndef UI_PERF_MGR_H
#define UI_PERF_MGR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * 初始化UI性能管理器
 */
void ui_perf_mgr_init(void);

/**
 * 开始UI批处理操作
 * 将多个UI操作合并为一次更新，减少重绘次数
 */
void ui_perf_mgr_batch_begin(void);

/**
 * 结束UI批处理操作并执行更新
 */
void ui_perf_mgr_batch_end(void);

/**
 * 暂停复杂动画效果 - 用于低性能设备
 * @param pause 是否暂停
 */
void ui_perf_mgr_pause_animations(bool pause);

/**
 * 设置UI更新节流阀值
 * @param ms 毫秒，最小允许的两次更新间隔
 */
void ui_perf_mgr_set_throttle(uint32_t ms);

/**
 * 判断是否应该更新UI
 * @return 是否应该更新
 */
bool ui_perf_mgr_should_update(void);

/**
 * 记录UI更新开始
 */
void ui_perf_mgr_update_begin(void);

/**
 * 记录UI更新结束
 */
void ui_perf_mgr_update_end(void);

/**
 * 获取UI每秒更新次数
 * @return 每秒更新次数
 */
float ui_perf_mgr_get_fps(void);

/**
 * 设置系统负载信息，供自适应优化使用
 * @param cpu_load CPU负载百分比
 * @param memory_pressure 内存压力百分比
 * @param cpu_temp CPU温度(℃)
 */
void ui_perf_mgr_set_system_load(uint8_t cpu_load, uint8_t memory_pressure, uint8_t cpu_temp);

/**
 * 检查是否处于低功耗模式
 * @return 是否处于低功耗模式
 */
bool ui_perf_mgr_is_low_power_mode(void);

#endif // UI_PERF_MGR_H
