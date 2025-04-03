#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "../common.h"
#include "data_manager.h" 
#include <stdio.h>

/**
 * 初始化UI管理器
 * @param bg_color 背景颜色
 */
void ui_manager_init(lv_color_t bg_color);

/**
 * 注册UI模块
 * @param module UI模块接口
 */
void ui_manager_register_module(ui_module_t *module);

/**
 * 显示指定索引的模块
 * @param index 模块索引
 */
void ui_manager_show_module(uint8_t index);

/**
 * 清理UI管理器
 */
void ui_manager_deinit(void);

/**
 * 获取主容器
 * @return 主容器对象
 */
lv_obj_t* ui_manager_get_container(void);

#endif // UI_MANAGER_H

