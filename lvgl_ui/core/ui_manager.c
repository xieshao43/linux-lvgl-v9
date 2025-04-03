#include "ui_manager.h"


#define MAX_MODULES 8

typedef struct {
    lv_obj_t *main_cont;        // 主容器
    ui_module_t **modules;      // UI模块数组
    uint8_t module_count;       // 模块总数
    uint8_t current_module;     // 当前显示的模块
    lv_timer_t *page_timer;     // 页面切换定时器
    lv_timer_t *update_timer;   // 数据更新定时器
} ui_manager_t;

static ui_manager_t *ui_mgr = NULL;

// 页面切换回调
static void _page_switch_cb(lv_timer_t *timer) {
    if (!ui_mgr || ui_mgr->module_count == 0) return;
    
    // 隐藏当前页面
    if (ui_mgr->modules[ui_mgr->current_module]->hide) {
        ui_mgr->modules[ui_mgr->current_module]->hide();
    }
    
    // 切换到下一页
    ui_mgr->current_module = (ui_mgr->current_module + 1) % ui_mgr->module_count;
    
    // 显示新页面
    if (ui_mgr->modules[ui_mgr->current_module]->show) {
        ui_mgr->modules[ui_mgr->current_module]->show();
    }
}

// 数据更新回调
static void _update_data_cb(lv_timer_t *timer) {
    if (!ui_mgr) return;
    data_manager_update();
    // 更新所有模块数据
    for (int i = 0; i < ui_mgr->module_count; i++) {
        if (ui_mgr->modules[i]->update) {
            ui_mgr->modules[i]->update();
        }
    }
}

void ui_manager_init(lv_color_t bg_color) {
    // 初始化UI管理器
    ui_mgr = lv_malloc(sizeof(ui_manager_t));
    if (!ui_mgr) {
        printf("UI管理器内存分配失败\n");
        return;
    }
    
    // 设置背景色
    lv_obj_set_style_bg_color(lv_screen_active(), bg_color, 0);
    
    // 创建主容器
    ui_mgr->main_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui_mgr->main_cont, 240, 135);
    lv_obj_set_pos(ui_mgr->main_cont, 0, 0);
    lv_obj_set_style_bg_color(ui_mgr->main_cont, bg_color, 0);
    lv_obj_set_style_border_width(ui_mgr->main_cont, 0, 0);
    lv_obj_set_style_pad_all(ui_mgr->main_cont, 10, 0);
    lv_obj_set_style_radius(ui_mgr->main_cont, 0, 0);
    
    // 初始化模块数组
    ui_mgr->modules = lv_malloc(sizeof(ui_module_t*) * MAX_MODULES);
    ui_mgr->module_count = 0;
    ui_mgr->current_module = 0;
    
    // 创建定时器
    ui_mgr->page_timer = lv_timer_create(_page_switch_cb, PAGE_SWITCH_INTERVAL, NULL);
    ui_mgr->update_timer = lv_timer_create(_update_data_cb, UPDATE_INTERVAL, NULL);
}

void ui_manager_register_module(ui_module_t *module) {
    if (!ui_mgr || !module || ui_mgr->module_count >= MAX_MODULES) return;
    
    // 添加模块到数组
    ui_mgr->modules[ui_mgr->module_count] = module;
    ui_mgr->module_count++;
    
    // 创建模块UI
    if (module->create) {
        module->create(ui_mgr->main_cont);
    }
    
    // 初始隐藏模块
    if (module->hide) {
        module->hide();
    }
}

void ui_manager_show_module(uint8_t index) {
    if (!ui_mgr || index >= ui_mgr->module_count) return;
    
    // 隐藏当前模块
    if (ui_mgr->modules[ui_mgr->current_module]->hide) {
        ui_mgr->modules[ui_mgr->current_module]->hide();
    }
    
    // 显示选定模块
    ui_mgr->current_module = index;
    if (ui_mgr->modules[index]->show) {
        ui_mgr->modules[index]->show();
    }
}

void ui_manager_deinit(void) {
    if (!ui_mgr) return;
    
    // 停止定时器
    if (ui_mgr->page_timer) {
        lv_timer_delete(ui_mgr->page_timer);
    }
    
    if (ui_mgr->update_timer) {
        lv_timer_delete(ui_mgr->update_timer);
    }
    
    // 删除所有模块
    for (int i = 0; i < ui_mgr->module_count; i++) {
        if (ui_mgr->modules[i]->delete) {
            ui_mgr->modules[i]->delete();
        }
    }
    
    // 释放模块数组
    if (ui_mgr->modules) {
        lv_free(ui_mgr->modules);
    }
    
    // 删除主容器
    if (ui_mgr->main_cont) {
        lv_obj_delete(ui_mgr->main_cont);
    }
    
    // 释放管理器
    lv_free(ui_mgr);
    ui_mgr = NULL;
}

lv_obj_t* ui_manager_get_container(void) {
    return ui_mgr ? ui_mgr->main_cont : NULL;
}
