#include "lvgl.h"
#include "core/data_manager.h" // 添加数据管理器头文件，解决data_manager_deinit隐式声明
#include "core/key355.h"       // 添加按键处理头文件
#include "modules/menu_ui.h"   // 添加菜单UI头文件
#include "modules/wifi_manager.h" // 添加WiFi管理器头文件
#include "modules/wifi_notification.h" // 添加WiFi通知头文件

// 添加全局更新定时器
static lv_timer_t *global_update_timer = NULL;
static uint32_t update_interval_ms = 200; // 默认更新间隔

// 全局更新函数 - 苹果风格智能更新
static void global_update_cb(lv_timer_t *timer) {
    static uint32_t idle_counter = 0;
    
    // 调用数据管理器更新 - 返回是否有实际变化
    bool data_changed = data_manager_update();
    
    // 如果数据发生变化，重置空闲计数
    if (data_changed) {
        idle_counter = 0;
    } else {
        // 数据未变化，增加空闲计数
        idle_counter++;
    }
    
    // 智能调整更新频率 - 苹果风格节能策略
    if (idle_counter > 50) {
        // 长时间无变化，切换到超低功耗模式，降低至2秒更新一次
        update_interval_ms = 2000;
    } else if (idle_counter > 30) {
        // 持续无变化，切换到低功耗模式，降低至1秒更新一次
        update_interval_ms = 1000;
    } else if (idle_counter > 10) {
        // 短期无变化，降低更新频率到500ms
        update_interval_ms = 500;
    } else {
        // 数据活跃期，保持较高更新频率
        update_interval_ms = 200;
    }
    
    // 动态调整定时器间隔
    lv_timer_set_period(global_update_timer, update_interval_ms);
    
    // 强制更新当前屏幕（如果有数据变化或长时间未更新）
    if (data_changed || idle_counter % 10 == 0) {
        lv_refr_now(lv_display_get_default());
    }
}

/**
 * 初始化并显示系统菜单UI
 */
void storage_monitor_init(const char *path) {
    // 初始化数据管理器
    data_manager_init(path);
        
    // 初始化按钮处理模块
    key355_init();

    // 创建初始菜单屏幕 - 直接使用新的屏幕创建函数
    menu_ui_create_screen();
    
    // 初始化wifi通知系统
    wifi_notification_init();
    
    // 初始化WiFi管理器
    wifi_manager_init();
        
    // 创建更智能的全局更新定时器
    if (global_update_timer == NULL) {
        global_update_timer = lv_timer_create(global_update_cb, update_interval_ms, NULL);
    }
}

/**
 * 关闭系统监控UI
 */
void storage_monitor_close(void) {
    // 停止全局更新定时器
    if (global_update_timer) {
        lv_timer_delete(global_update_timer);
        global_update_timer = NULL;
    }
    
    // 释放WiFi管理器资源 - 这会同时释放WiFi通知系统资源
    wifi_manager_deinit();
    
    // 释放按钮处理模块
    key355_deinit();
    
    // 调用正确声明的函数释放数据管理器资源
    data_manager_deinit();
}

// 为保持兼容性添加的函数
void read_storage_data(void) {
    data_manager_update();
}
