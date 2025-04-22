#include "menu_ui.h"


// 私有数据结构
typedef struct {
    lv_obj_t *panel;           // 主面板
    lv_obj_t *menu_list;       // 菜单列表
    lv_obj_t **menu_items;     // 菜单项数组
    uint8_t item_count;        // 菜单项数量
    uint8_t current_index;     // 当前选中项
    lv_timer_t *button_timer;  // 按钮检测定时器
    bool is_active;            // 菜单是否激活
} menu_ui_data_t;

static menu_ui_data_t ui_data;
static ui_module_t menu_module;

// 菜单项定义
#define MENU_ITEM_COUNT 4  // 从3改为4，增加Intelligence选项
static const char* menu_items[MENU_ITEM_COUNT] = {
    "Storage",
    "CPU",
    "Intelligence",
    "Music"  
};

// 相应菜单项的模块索引
static const uint8_t menu_item_modules[MENU_ITEM_COUNT] = {1, 2, 0, 3};  // 假设Intelligence模块索引为3

// 使用LVGL内置符号
// 避免使用Unicode F017, F11B等无法显示的字符
static const char* menu_icons[MENU_ITEM_COUNT] = {
    LV_SYMBOL_SAVE,      // 存储图标
    LV_SYMBOL_SETTINGS,  // CPU图标
    LV_SYMBOL_HOME,      // 音乐图标
    LV_SYMBOL_AUDIO      // Intelligence图标（使用音频符号表示语音/AI助手）
};

// 突出显示当前选中的菜单项并确保其可见（添加自动滚动功能）
static void _highlight_selected_item(void) {
    for (int i = 0; i < ui_data.item_count; i++) {
        // 获取菜单项对象
        lv_obj_t *item = ui_data.menu_items[i];
        if (!item) continue; // 安全检查
        
        // 获取内容容器（第一个子对象）
        lv_obj_t *content = lv_obj_get_child(item, 0);
        if (!content) continue; // 安全检查
        
        // 获取图标和标签对象
        lv_obj_t *icon = lv_obj_get_child(content, 0);
        lv_obj_t *label = lv_obj_get_child(content, 1);
        
        if (i == ui_data.current_index) {
            // 选中项样式 - 苹果风格的柔和高亮效果
            
            // 1. iOS 13+风格的点亮效果
            lv_obj_set_style_bg_color(item, lv_color_hex(0x6366F1), 0); // iOS紫色
            lv_obj_set_style_bg_grad_color(item, lv_color_hex(0x4F46E5), 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_90, 0);
            
            // 2. 精致的内发光效果
            lv_obj_set_style_shadow_width(item, 15, 0);
            lv_obj_set_style_shadow_ofs_x(item, 0, 0);
            lv_obj_set_style_shadow_ofs_y(item, 0, 0);
            lv_obj_set_style_shadow_spread(item, 0, 0);
            lv_obj_set_style_shadow_color(item, lv_color_hex(0x6366F1), 0);
            lv_obj_set_style_shadow_opa(item, LV_OPA_30, 0);
            
            // 3. 明确设置文本颜色（纯白色）- 高对比度
            if (icon) {
                lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_opa(icon, LV_OPA_COVER, 0); // 完全不透明
            }
            
            if (label) {
                lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
                // 微小字间距增强，增强选中感
                lv_obj_set_style_text_letter_space(label, 1, 0);
            }
            
            // 4. 确保当前选中项可见 - 加入平滑滚动效果
            lv_obj_scroll_to_view(item, LV_ANIM_ON);
            
        } else {
            // 非选中项样式 - 低调暗色调
            
            // 1. 暗色透明玻璃效果
            lv_obj_set_style_bg_color(item, lv_color_hex(0x1a1a2e), 0);
            lv_obj_set_style_bg_grad_color(item, lv_color_hex(0x16213e), 0);
            lv_obj_set_style_bg_grad_dir(item, LV_GRAD_DIR_HOR, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_60, 0);
            
            // 2. 轻微的阴影
            lv_obj_set_style_shadow_width(item, 8, 0);
            lv_obj_set_style_shadow_ofs_y(item, 2, 0);
            lv_obj_set_style_shadow_color(item, lv_color_hex(0x000000), 0);
            lv_obj_set_style_shadow_opa(item, LV_OPA_10, 0);
            
            // 3. 稍暗的文本颜色
            if (icon) {
                lv_obj_set_style_text_color(icon, lv_color_hex(0xE2E8F0), 0);
                lv_obj_set_style_opa(icon, LV_OPA_80, 0); // 轻微半透明
            }
            
            if (label) {
                lv_obj_set_style_text_color(label, lv_color_hex(0xE2E8F0), 0);
                lv_obj_set_style_text_letter_space(label, 0, 0); // 正常字间距
            }
        }
    }
    
    // 强制更新显示
    lv_display_t *disp = lv_display_get_default();
    if (disp) lv_refr_now(disp);
}

// 声明外部类型以解决编译错误
typedef struct {
    lv_obj_t *page;       // 页面对象
    bool destroy;         // 是否销毁
    lv_anim_ready_cb_t user_cb; // 用户回调函数
    void *user_data;      // 用户数据
} ui_page_anim_data_t;

// 添加模块切换包装函数，接收定时器作为参数
static void _module_switch_wrapper(lv_timer_t *timer) {
    if(timer == NULL) return;
    
    // 从定时器的用户数据中获取模块索引
    uint8_t module_idx = (uint8_t)((uintptr_t)timer->user_data);
    
    #if UI_DEBUG_ENABLED
    printf("[MENU] Switch wrapper: switching to module %d\n", module_idx);
    #endif
    
    // 调用UI管理器的模块切换函数
    ui_manager_show_module(module_idx);
}

// 保存当前要跳转的模块索引
static uint8_t pending_module_index = 0;

// 修改函数，简化模块索引获取方式
static void _module_switch_from_anim(lv_anim_t *a) {
    // 直接使用之前保存的待切换模块索引
    uint8_t module_idx = pending_module_index;
    
    #if UI_DEBUG_ENABLED
    printf("[MENU] Switch from anim: switching to module %d\n", module_idx);
    #endif
    
    // 安全检查，确保模块索引有效
    if (module_idx < ui_manager_get_module_count()) {
        // 调用UI管理器切换到目标模块
        ui_manager_show_module(module_idx);
    } else {
        printf("[MENU] Error: Invalid module index: %d\n", module_idx);
    }
}

// 按钮事件处理定时器回调 - 修改按键逻辑
static void _button_handler_cb(lv_timer_t *timer) {
    // 更严格的安全检查
    if (!ui_data.is_active || !ui_data.menu_items || !ui_data.panel || ui_data.item_count == 0) {
        #if UI_DEBUG_ENABLED
        printf("[MENU] Button handler: safety check failed\n");
        #endif
        return;
    }
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    // 清除事件标志
    key355_clear_event();
    
    // 调试输出
    #if UI_DEBUG_ENABLED
    printf("[MENU] Button event: %d\n", event);
    #endif
    
    // 根据按钮事件执行操作
    switch (event) {
        case BUTTON_EVENT_CLICK:
            // 单击：切换选项 - 增加索引安全检查
            if (ui_data.item_count > 0 && ui_data.is_active) {
                ui_data.current_index = (ui_data.current_index + 1) % ui_data.item_count;
                _highlight_selected_item();
                #if UI_DEBUG_ENABLED
                printf("[MENU] Selected item: %d\n", ui_data.current_index);
                #endif
            }
            break;
            
        case BUTTON_EVENT_LONG_PRESS:
            // 长按：选择当前项并进入相应模块 - 增加更严格的检查
            if (ui_data.current_index < ui_data.item_count && ui_data.is_active) {
                uint8_t module_idx = menu_item_modules[ui_data.current_index];
                #if UI_DEBUG_ENABLED
                printf("[MENU] Long press detected, switching to module: %d\n", module_idx);
                #endif
                
                if (module_idx < ui_manager_get_module_count()) {
                    // 在切换前先暂停动画和定时器，防止引起内存问题
                    lv_anim_del_all();
                    
                    // 停止自身的按钮处理定时器
                    if (ui_data.button_timer) {
                        lv_timer_pause(ui_data.button_timer);
                        #if UI_DEBUG_ENABLED
                        printf("[MENU] Button timer paused\n");
                        #endif
                    }
                    
                    // 标记为非活动
                    ui_data.is_active = false;
                    
                    // 保存要切换的模块索引到全局变量
                    pending_module_index = module_idx;
                    
                    // 创建退出动画，完成后切换到目标模块
                    ui_utils_page_exit_anim(
                        ui_data.panel,
                        ANIM_FADE_SCALE,  // 使用苹果风格的淡出+缩放效果
                        ANIM_DURATION,    // 使用预定义的动画时长
                        false,            // 不销毁面板，由ui_manager负责
                        (lv_anim_ready_cb_t)_module_switch_from_anim  // 动画完成后执行模块切换
                    );
                }
            }
            break;
            
        default:
            break;
    }
}

// 创建UI
static void _create_ui(lv_obj_t *parent) {
    // 创建主面板 - 撑满整个屏幕空间
    ui_data.panel = lv_obj_create(parent);
    lv_obj_set_size(ui_data.panel, 240, 135);
    
    // 更新背景为更深邃的黑色，增强高级感
    lv_obj_set_style_bg_color(ui_data.panel, lv_color_hex(0x121824), 0);
    lv_obj_set_style_bg_grad_color(ui_data.panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_grad_dir(ui_data.panel, LV_GRAD_DIR_VER, 0);
    
    // 增加模糊效果的视觉暗示 - 微妙的阴影边缘
    lv_obj_set_style_bg_opa(ui_data.panel, LV_OPA_90, 0); // 微妙的半透明效果
    lv_obj_set_style_radius(ui_data.panel, 16, 0);
    lv_obj_set_style_pad_all(ui_data.panel, 10, 0);
    lv_obj_set_style_border_width(ui_data.panel, 0, 0);
    
    // 添加苹果风格的玻璃模糊效果暗示
    lv_obj_set_style_shadow_width(ui_data.panel, 30, 0);
    lv_obj_set_style_shadow_ofs_x(ui_data.panel, 0, 0);
    lv_obj_set_style_shadow_ofs_y(ui_data.panel, 0, 0);
    lv_obj_set_style_shadow_spread(ui_data.panel, 0, 0);
    lv_obj_set_style_shadow_color(ui_data.panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ui_data.panel, LV_OPA_20, 0);
    
    lv_obj_center(ui_data.panel);
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建背景大字体"Menu"标签 - 苹果设计语言核心：精致的细节
    lv_obj_t *bg_label = lv_label_create(ui_data.panel);
    
    // 使用更大的字体，但由于系统限制，我们使用可用的最大字体
    lv_obj_set_style_text_font(bg_label, &lv_font_montserrat_16, 0);
    
    // 使用较大的字间距和字体放大来模拟更大的字体效果
    lv_obj_set_style_text_letter_space(bg_label, 5, 0);
    lv_obj_set_style_transform_zoom(bg_label, 300, 0); // 放大到300%
    
    // 设置优雅的半透明灰色 - 典型的苹果设计风格
    lv_obj_set_style_text_color(bg_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(bg_label, LV_OPA_10, 0); // 非常低的透明度，营造水印效果
    
    lv_label_set_text(bg_label, "MENU");
    
    // 居中放置，但稍稍偏向面板顶部
    lv_obj_center(bg_label);
    lv_obj_set_style_transform_pivot_x(bg_label, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(bg_label, lv_pct(50), 0);
    
    // 创建菜单列表容器 - 垂直布局，占满面板
    ui_data.menu_list = lv_obj_create(ui_data.panel);
    lv_obj_set_size(ui_data.menu_list, 220, 115);
    
    // 完全透明背景，让背景的MENU字样显示
    lv_obj_set_style_bg_opa(ui_data.menu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_data.menu_list, 0, 0);
    lv_obj_set_style_pad_all(ui_data.menu_list, 5, 0);
    lv_obj_set_flex_flow(ui_data.menu_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_data.menu_list, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(ui_data.menu_list);
    
    // 启用滚动功能，为自动滚动做准备
    lv_obj_clear_flag(ui_data.menu_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(ui_data.menu_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scrollbar_mode(ui_data.menu_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_data.menu_list, LV_DIR_VER);
    
    // 创建菜单项
    ui_data.item_count = MENU_ITEM_COUNT;
    ui_data.menu_items = lv_malloc(sizeof(lv_obj_t*) * ui_data.item_count);
    
    for (int i = 0; i < ui_data.item_count; i++) {
        // 为每个菜单项创建卡片式容器 - 苹果风格的圆角扁平卡片
        lv_obj_t *item = lv_obj_create(ui_data.menu_list);
        lv_obj_set_size(item, 210, 26);  // 稍微减小高度，更加紧凑优雅
        lv_obj_set_style_radius(item, 10, 0);
        
        // 使用玻璃态设计 - 典型的iOS暗黑模式效果
        lv_obj_set_style_bg_color(item, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_bg_grad_color(item, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_bg_grad_dir(item, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_60, 0);  // 半透明背景
        
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 2, 0);
        
        // 添加iOS式阴影 - 更加细腻
        lv_obj_set_style_shadow_width(item, 8, 0);
        lv_obj_set_style_shadow_ofs_y(item, 2, 0);
        lv_obj_set_style_shadow_ofs_x(item, 0, 0);
        lv_obj_set_style_shadow_color(item, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(item, LV_OPA_10, 0);
        
        // 创建水平布局容器
        lv_obj_t *content = lv_obj_create(item);
        lv_obj_set_size(content, 190, 22); // 略微调小以适应更紧凑的布局
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_all(content, 0, 0);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_center(content);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        
        // 创建图标 - 苹果风格的简约图标
        lv_obj_t *icon = lv_label_create(content);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xE2E8F0), 0);
        lv_obj_set_style_opa(icon, LV_OPA_80, 0);  // 轻微半透明，更优雅
        lv_label_set_text(icon, menu_icons[i]);
        
        // 创建标签 - 使用苹果的San Francisco字体风格
        lv_obj_t *label = lv_label_create(content);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        // 根据苹果设计规范，文本略微加亮以增强可读性
        lv_obj_set_style_text_color(label, lv_color_hex(0xF8FAFC), 0);
        lv_label_set_text(label, menu_items[i]);
        lv_obj_set_style_pad_left(label, 10, 0);
        
        ui_data.menu_items[i] = item;
    }
    
    // 初始化选择索引
    ui_data.current_index = 0;
    _highlight_selected_item();
    
    // 创建按钮检测定时器
    ui_data.button_timer = lv_timer_create(_button_handler_cb, 50, NULL);
    
    // 设置菜单激活状态
    ui_data.is_active = true;
}

// 删除UI - 修改以确保安全释放资源
static void _delete_ui(void) {
    #if UI_DEBUG_ENABLED
    printf("[MENU] Deleting menu UI\n");
    #endif
    
    // 先停止所有动画，防止动画回调访问已删除对象
    lv_anim_del_all();
    
    // 停止按钮检测定时器，确保不再访问UI元素
    if (ui_data.button_timer != NULL) {
        lv_timer_delete(ui_data.button_timer);
        ui_data.button_timer = NULL;
        #if UI_DEBUG_ENABLED
        printf("[MENU] Button timer deleted\n");
        #endif
    }
    
    // 先将UI标记为不活动，避免异步回调继续操作
    ui_data.is_active = false;
    
    // 清理菜单项数组之前，先保存项数，避免释放后访问
    uint8_t saved_item_count = ui_data.item_count;
    ui_data.item_count = 0;  // 先设为0，这样其他函数不会再访问这些项
    
    // 释放菜单项数组 - 增加安全检查
    if (ui_data.menu_items != NULL) {
        // 添加双重检查
        lv_obj_t **items_to_free = ui_data.menu_items;
        ui_data.menu_items = NULL;  // 先置空，防止其他地方继续使用
        
        // 安全释放内存
        lv_free(items_to_free);
    }
    
    // 删除面板前，先保存引用并置空
    lv_obj_t *panel_to_delete = ui_data.panel;
    if (panel_to_delete != NULL) {
        // 先清空引用，确保不会再被访问
        ui_data.panel = NULL;
        ui_data.menu_list = NULL;
        
        // 删除面板及其所有子对象
        lv_obj_delete(panel_to_delete);
    }
    
    // 重置其他状态变量
    ui_data.current_index = 0;
}

// 显示UI
static void _show_ui(void) {
    #if UI_DEBUG_ENABLED
    printf("[MENU] Showing menu UI\n");
    #endif
    
    if (ui_data.panel != NULL) {
        // 先确保面板可见
        lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
        
        // 确保当前索引有效
        if (ui_data.current_index >= ui_data.item_count && ui_data.item_count > 0) {
            ui_data.current_index = 0;
        }
        
        // 应用高亮效果
        _highlight_selected_item();
        
        // 处理按钮定时器
        if (ui_data.button_timer != NULL) {
            // 恢复按钮定时器，确保能响应按钮事件
            lv_timer_resume(ui_data.button_timer);
            #if UI_DEBUG_ENABLED
            printf("[MENU] Button timer resumed\n");
            #endif
        } else {
            // 如果定时器不存在，重新创建
            ui_data.button_timer = lv_timer_create(_button_handler_cb, 50, NULL);
            #if UI_DEBUG_ENABLED
            printf("[MENU] Button timer created\n");
            #endif
        }
        
        // 最后设置活动状态
        ui_data.is_active = true;
        #if UI_DEBUG_ENABLED
        printf("[MENU] Menu activated\n");
        #endif
    }
}

// 隐藏UI
static void _hide_ui(void) {
    if (ui_data.panel != NULL) {
        lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
        ui_data.is_active = false;
    }
}

// 更新UI
static void _update_ui(void) {
    // 菜单不需要频繁更新
}

// 获取模块接口
ui_module_t* menu_ui_get_module(void) {
    menu_module.create = _create_ui;
    menu_module.delete = _delete_ui;
    menu_module.show = _show_ui;
    menu_module.hide = _hide_ui;
    menu_module.update = _update_ui;
    
    return &menu_module;
}
