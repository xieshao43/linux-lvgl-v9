#ifndef CPU_UI_H
#define CPU_UI_H

#include "../common.h"
#include "../core/key355.h"  // 添加此行

/**
 * 获取CPU监控模块接口
 * @return CPU监控模块接口
 */
ui_module_t* cpu_ui_get_module(void);

#endif // CPU_UI_H