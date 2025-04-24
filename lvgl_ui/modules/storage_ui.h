#ifndef STORAGE_UI_H
#define STORAGE_UI_H

#include "../common.h"
#include "../core/data_manager.h"
#include "../utils/ui_utils.h"

/**
 * 创建存储屏幕
 * 使用LVGL原生屏幕管理替代模块化架构
 */
void storage_ui_create_screen(void);

#endif // STORAGE_UI_H
