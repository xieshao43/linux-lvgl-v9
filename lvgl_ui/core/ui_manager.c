#include "ui_manager.h"
#include <stdio.h>
#include "../core/data_manager.h" // 添加数据管理器头文件

// 模块管理
#define MAX_MODULES 10
static ui_module_t* modules[MAX_MODULES];
static uint8_t module_count = 0;
static uint8_t active_module = 0;
static lv_obj_t* ui_container = NULL;
static ui_anim_type_t current_anim_type = ANIM_NONE;  // 添加动画类型变量

// 新增：记录上次重绘时间，用于智能节流
static uint32_t last_refresh_time = 0;

// 初始化UI管理器
void ui_manager_init(lv_color_t bg_color) {
    // 创建一个容器作为所有UI的父对象
    ui_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(ui_container);
    lv_obj_set_size(ui_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(ui_container, 0, 0);
    
    // 设置背景色
    lv_obj_set_style_bg_color(ui_container, bg_color, 0);
    lv_obj_set_style_bg_opa(ui_container, LV_OPA_COVER, 0);
    
    // 初始化模块计数
    module_count = 0;
    active_module = 0;
}

// 注册模块
void ui_manager_register_module(ui_module_t* module) {
    if (module_count < MAX_MODULES) {
        modules[module_count++] = module;
        
        // 为每个模块创建UI组件
        module->create(ui_container);
        module->hide();
    }
}

// 设置动画类型
void ui_manager_set_anim_type(ui_anim_type_t type) {
    current_anim_type = type;
}

// 修改：显示指定模块 - 兼容LVGL内置屏幕切换
void ui_manager_show_module(uint8_t module_idx) {
    // 安全检查 - 防止无效模块索引
    if (module_idx >= module_count) {
        printf("[UI_MANAGER] Invalid module index: %d\n", module_idx);
        return;
    }
    
    // 先判断是否是当前模块 - 避免不必要的切换
    if (module_idx == active_module && active_module < module_count) {
        return;
    }
    
    // 记录要切换到的模块
    uint8_t target_module = module_idx;
    
    // 仅更新内部状态，不执行实际的模块显示/隐藏操作
    // 这使UI管理器能够跟踪当前活动模块，而不干扰LVGL的屏幕切换
    active_module = target_module;
    
    #if UI_DEBUG_ENABLED
    printf("[UI_MANAGER] Module state updated to: %d\n", active_module);
    #endif
}

// 添加：与LVGL屏幕切换配合的函数
void ui_manager_notify_screen_change(lv_obj_t *new_screen) {
    // 当使用LVGL原生屏幕切换时调用此函数
    // 确保UI管理器知道屏幕已经切换
    
    #if UI_DEBUG_ENABLED
    printf("[UI_MANAGER] Received screen change notification\n");
    #endif
    
    // 这里可以添加额外的状态同步逻辑
}

// 更新UI - 增强版
void ui_manager_update(void) {
    uint32_t now = lv_tick_get();
    
    // 只更新当前活动模块
    if (module_count > 0 && active_module < module_count) {
        if (modules[active_module] && modules[active_module]->update) {
            modules[active_module]->update();
        }
    }
    
    // 智能重绘策略 - 避免过于频繁的屏幕刷新
    // 在静态场景下降低刷新率，动画场景保持高刷新率
    bool force_refresh = false;
    
    if (now - last_refresh_time >= 250) {  // 最低250ms刷新一次
        force_refresh = true;
    } else if (data_manager_has_changes()) {  // 有数据变化时刷新
        force_refresh = true;
    }
    
    // 需要时才重绘屏幕
    if (force_refresh) {
        lv_display_t *disp = lv_display_get_default();
        if (disp) {
            lv_refr_now(disp);
            last_refresh_time = now;
        }
    }
}

// 获取当前活动模块索引
uint8_t ui_manager_get_active_module(void) {
    return active_module;
}

// 获取已注册的模块数量
uint8_t ui_manager_get_module_count(void) {
    return module_count;
}

// 反初始化UI管理器
void ui_manager_deinit(void) {
    // 停止所有动画
    lv_anim_del_all();
    
    // 延迟一小段时间，确保动画停止完成
    lv_task_handler();
    
    // 删除所有模块
    for (uint8_t i = 0; i < module_count; i++) {
        ui_module_t *module = modules[i];
        if (module && module->delete) {
            // 先将模块隐藏
            if (module->hide) {
                module->hide();
            }
            
            // 执行一次任务处理，确保隐藏动作完成
            lv_task_handler();
            
            // 安全删除模块
            module->delete();
        }
    }
    
    // 删除容器
    if (ui_container) {
        lv_obj_delete(ui_container);
        ui_container = NULL;
    }
    
    module_count = 0;
    active_module = 0;
}
