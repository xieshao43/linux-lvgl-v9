#include "lv_examples.h"
#include "core/ui_manager.h"
#include "core/data_manager.h"
#include "modules/storage_ui.h"
#include "modules/cpu_ui.h"
#include "modules/wifi_notification.h" // Add WiFi notification header
#include <stdio.h>
#include <stdlib.h> 

/**
 * 初始化并显示系统监控UI
 */
void storage_monitor_init(const char *path) {
    // 隐藏光标
    system("echo -e \"\\033[?25l\" > /dev/tty1");
    
    // 初始化数据管理器
    data_manager_init(path);

    // 初始化UI管理器
    ui_manager_init(lv_color_hex(COLOR_BG));
    
    // 设置过渡动画类型
    ui_manager_set_anim_type(ANIM_SLIDE_LEFT); // 或者其他动画类型

    // 注册存储模块
    ui_manager_register_module(storage_ui_get_module());

    // 注册CPU模块
    ui_manager_register_module(cpu_ui_get_module());

    // 初始化WiFi通知系统
    wifi_notification_init();

    // 显示第一个模块
    ui_manager_show_module(0);
    
    // 可以在这里触发一个WiFi通知作为示例
    wifi_notification_show(WIFI_STATE_CONNECTED, "Quark 2.4G");
}

/**
 * 关闭存储监控UI
 */
void storage_monitor_close(void) {
    // 清理UI管理器
    ui_manager_deinit();
}

/**
 * 为保持兼容性添加的函数
 */
void read_storage_data(void) {
    data_manager_update();
}
