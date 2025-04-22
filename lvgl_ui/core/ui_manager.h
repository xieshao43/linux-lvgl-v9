#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "../common.h"

// 初始化UI管理器
void ui_manager_init(lv_color_t bg_color);

// 显示指定模块
void ui_manager_show_module(uint8_t index);

// 更新UI
void ui_manager_update(void);

// 获取当前活动模块索引
uint8_t ui_manager_get_active_module(void);

// 获取已注册的模块数量
uint8_t ui_manager_get_module_count(void);

// 注册模块
void ui_manager_register_module(ui_module_t *module);

// 反初始化UI管理器
void ui_manager_deinit(void);

// 设置动画类型
void ui_manager_set_anim_type(ui_anim_type_t type);

#endif // UI_MANAGER_H

