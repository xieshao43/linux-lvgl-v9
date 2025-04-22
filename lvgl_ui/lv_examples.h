#ifndef LV_EXAMPLES_H
#define LV_EXAMPLES_H

//使用的模块
#include "core/ui_manager.h"
#include "core/data_manager.h"
#include "modules/storage_ui.h"
#include "modules/cpu_ui.h"
#include "modules/wifi_notification.h" // WiFi通知头文件
#include "modules/wifi_manager.h"      // 添加WiFi管理器头文件
#include "modules/menu_ui.h"          // 添加菜单模块头文件
#include <stdio.h>
#include <stdlib.h> 

/**
 * 初始化并显示系统监控UI
 * @param path 存储路径
 */
void storage_monitor_init(const char *path);

/**
 * 关闭系统监控UI
 */
void storage_monitor_close(void);

#endif // LV_EXAMPLES_H