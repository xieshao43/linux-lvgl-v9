#ifndef STORAGE_UI_H
#define STORAGE_UI_H

#include "../common.h"
#include "../core/data_manager.h"
#include "../utils/ui_utils.h"
#include "../utils/ui_rounded.h"

/**
 * 获取存储监控模块接口
 * @return 存储监控模块接口
 */
ui_module_t* storage_ui_get_module(void);

#endif // STORAGE_UI_H
