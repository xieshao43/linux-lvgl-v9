#ifndef LVGL9_UI_EXAMPLE_H
#define LVGL9_UI_EXAMPLE_H

#include "lvgl/lvgl.h"

/**
 * 初始化并显示系统监控UI
 */
void ui_example_init(void);

/**
 * 关闭系统监控UI并清理资源
 */
void ui_example_close(void);

/**
 * 手动更新系统数据（可选）
 * @param mem_total 总内存(KB)
 * @param mem_used 已用内存(KB)
 * @param cpu_usage CPU使用率(0-100)
 */
void ui_example_update_data(uint32_t mem_total, uint32_t mem_used, uint8_t cpu_usage);

#endif /* LVGL9_UI_EXAMPLE_H */