#ifndef MENU_UI_H
#define MENU_UI_H

#include "../common.h"
#include "../utils/ui_utils.h"
#include "../core/ui_manager.h"
#include "../core/key355.h"

/**
 * 获取菜单模块接口
 * @return 菜单模块接口
 */
ui_module_t* menu_ui_get_module(void);

#endif // MENU_UI_H