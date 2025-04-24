#ifndef MENU_UI_H
#define MENU_UI_H

#include "../common.h"
#include "../utils/ui_utils.h"
#include "../core/key355.h"

/**
 * 创建菜单屏幕
 * 使用LVGL原生屏幕管理替代模块化架构
 */
void menu_ui_create_screen(void);

/**
 * 设置菜单为活动状态
 * 当从其他页面返回菜单时调用
 */
void menu_ui_set_active(void);

#endif // MENU_UI_H