#ifndef CPU_UI_H
#define CPU_UI_H

#include "../common.h"
#include "../core/data_manager.h"
#include "../core/key355.h"
#include "../utils/ui_utils.h"

/**
 * 创建CPU监控屏幕
 * 使用LVGL原生屏幕管理替代模块化架构
 */
void cpu_ui_create_screen(void);

#endif // CPU_UI_H