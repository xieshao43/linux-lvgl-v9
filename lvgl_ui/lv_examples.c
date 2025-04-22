#include "lv_examples.h"

// 添加全局更新定时器
static lv_timer_t *global_update_timer = NULL;
static uint32_t update_interval_ms = 200; // 默认更新间隔

// 全局更新函数 - 苹果风格智能更新
static void global_update_cb(lv_timer_t *timer) {
    static uint32_t idle_counter = 0;
    static uint8_t active_module_last = 255;
    uint8_t current_module = ui_manager_get_active_module();
    
    // 检测模块切换 - 模块切换后立即进行高频率更新
    if (current_module != active_module_last) {
        // 模块切换后重置为高频更新
        idle_counter = 0;
        update_interval_ms = 100; // 切换后高频更新
        lv_timer_set_period(global_update_timer, update_interval_ms);
        active_module_last = current_module;
    }
    
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
    
    // 更新当前UI模块
    ui_manager_update();
}

/**
 * 初始化并显示系统菜单UI
 */
void storage_monitor_init(const char *path) {
    // 初始化数据管理器
    data_manager_init(path);
        
    // 初始化按钮处理模块
    key355_init();

    // 初始化UI管理器
    ui_manager_init(lv_color_hex(COLOR_BG));
    

    // 设置过渡动画类型
    ui_manager_set_anim_type(ANIM_FADE_SCALE);

    // 1. 注册菜单模块 (索引0)
    ui_manager_register_module(menu_ui_get_module());
    
    // 2. 注册存储模块 (索引1)
    ui_manager_register_module(storage_ui_get_module());

    // 3. 注册CPU模块 (索引2)
    ui_manager_register_module(cpu_ui_get_module());
    
    // 显示菜单模块 (索引0)
    ui_manager_show_module(0);
    
     // 初始化wifi通知系统
     wifi_notification_init();
     // 初始化WiFi管理器
     wifi_manager_init();
        
    // 创建更智能的全局更新定时器
    if (global_update_timer == NULL) {
        global_update_timer = lv_timer_create(global_update_cb, update_interval_ms, NULL);
        // 移除可能不兼容的优先级设置
        // LVGL 9.0可能不支持此API或常量定义发生了变化
        // lv_timer_set_prio(global_update_timer, LV_TIMER_PRIO_MID);
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
    
    // 清理UI管理器
    ui_manager_deinit();
}

// 为保持兼容性添加的函数
void read_storage_data(void) {
    data_manager_update();
}
