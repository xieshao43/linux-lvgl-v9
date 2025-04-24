#include "storage_ui.h"
#include "menu_ui.h"
#include "../core/key355.h"
#include "../core/data_manager.h"
#include <stdio.h>

// Private data structure
typedef struct {
    lv_obj_t *screen;          // Main screen (replaces the previous panel)
    lv_obj_t *storage_arc;     // Storage arc
    lv_obj_t *percent_label;   // Storage percentage label
    lv_obj_t *info_label;      // Storage details label
    
    lv_obj_t *memory_arc;      // Memory arc
    lv_obj_t *memory_percent_label; // Memory percentage label
    lv_obj_t *memory_info_label;    // Memory details label

    lv_timer_t *button_timer;  // Button detection timer
    lv_timer_t *update_timer;  // Data update timer
    bool is_active;     
} storage_ui_data_t;

// Storage and memory data types
typedef struct {
    uint64_t used;
    uint64_t total;
    uint8_t percent;
} storage_data_t;

typedef struct {
    uint64_t used;
    uint64_t total;
    uint8_t percent;
} memory_data_t;

static storage_ui_data_t ui_data;

// Data cache to avoid frequent memory allocation
static struct {
    char used_str[20];
    char total_str[20];
    uint8_t storage_percent;
    uint8_t memory_percent;
    uint64_t storage_used;
    uint64_t storage_total;
    uint64_t memory_used;
    uint64_t memory_total;
    bool first_update;
} ui_cache = {
    .first_update = true
};

// Animation control variables
static lv_timer_t *arc_animation_timer = NULL;
static int32_t current_effect_angle = 0;
static bool animation_active = false;

// Function declarations
static void _batch_fetch_all_data(storage_data_t *storage_data, memory_data_t *memory_data);
static void _update_ui_data(lv_timer_t *timer);
static void _button_handler_cb(lv_timer_t *timer);

// Arc animation callback
static void _arc_animation_cb(lv_timer_t *timer) {
    if (!ui_data.screen || !ui_data.storage_arc || !ui_data.memory_arc) {
        return;
    }
    
    // Calculate dynamic effect angle
    current_effect_angle = (current_effect_angle + 3) % 360;
    
    // Get actual progress values
    int32_t storage_val = ui_cache.storage_percent;
    int32_t memory_val = ui_cache.memory_percent;
    
    // Storage arc animation - Flow effect based on actual value
    int32_t storage_angle = (storage_val * 360) / 100;
    int32_t storage_start = current_effect_angle;
    int32_t storage_end = (storage_start + storage_angle) % 360;
    
    lv_arc_set_angles(ui_data.storage_arc, storage_start, storage_end);
    
    // Memory arc animation - Offset by 180 degrees from storage
    int32_t memory_angle = (memory_val * 360) / 100;
    int32_t memory_start = (current_effect_angle + 180) % 360;
    int32_t memory_end = (memory_start + memory_angle) % 360;
    
    lv_arc_set_angles(ui_data.memory_arc, memory_start, memory_end);
}

// Start arc animation effect
static void _start_arc_animation(void) {
    if (!animation_active) {
        // Set arc to reverse mode for angle control
        lv_arc_set_mode(ui_data.storage_arc, LV_ARC_MODE_REVERSE);
        lv_arc_set_mode(ui_data.memory_arc, LV_ARC_MODE_REVERSE);
        
        // Ensure background arc is fully displayed
        lv_arc_set_bg_angles(ui_data.storage_arc, 0, 360);
        lv_arc_set_bg_angles(ui_data.memory_arc, 0, 360);
        
        // Create animation timer
        if (arc_animation_timer == NULL) {
            arc_animation_timer = lv_timer_create(_arc_animation_cb, 16, NULL); // Approx. 60FPS
        } else {
            lv_timer_resume(arc_animation_timer);
        }
        
        animation_active = true;
    }
}

// Stop arc animation effect
static void _stop_arc_animation(void) {
    if (animation_active) {
        if (arc_animation_timer) {
            lv_timer_pause(arc_animation_timer);
        }
        
        // Restore normal mode and display actual values
        lv_arc_set_mode(ui_data.storage_arc, LV_ARC_MODE_NORMAL);
        lv_arc_set_mode(ui_data.memory_arc, LV_ARC_MODE_NORMAL);
        
        lv_arc_set_value(ui_data.storage_arc, ui_cache.storage_percent);
        lv_arc_set_value(ui_data.memory_arc, ui_cache.memory_percent);
        
        animation_active = false;
    }
}

// Screen switch animation callback
static void _switch_screen_anim_cb(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)var, v, 0);
}

// 滑动动画回调函数 - 统一动画效果
static void _slide_anim_cb(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t*)var, v);
}

// 修改动画完成回调函数
static void _return_to_menu_anim_complete(lv_anim_t *a) {
    // 删除当前屏幕
    lv_obj_t *screen = (lv_obj_t*)lv_anim_get_user_data(a);
    if (screen) {
        lv_obj_del(screen);
    }
    
    // 在动画完成且屏幕删除后激活菜单
    menu_ui_set_active();
    
    #if UI_DEBUG_ENABLED
    printf("[STORAGE] Returned to menu, animation complete\n");
    #endif
}

// Function to return to menu screen - 使用水平滑动效果
static void _return_to_menu(void) {
    // 停止所有定时器
    if (ui_data.button_timer) {
        lv_timer_delete(ui_data.button_timer);
        ui_data.button_timer = NULL;
    }
    
    if (ui_data.update_timer) {
        lv_timer_delete(ui_data.update_timer);
        ui_data.update_timer = NULL;
    }
    
    // 停止圆弧动画
    _stop_arc_animation();
    if (arc_animation_timer) {
        lv_timer_delete(arc_animation_timer);
        arc_animation_timer = NULL;
    }
    
    // 标记为非活动状态
    ui_data.is_active = false;
    
    // 清除实例引用，防止回调函数使用已删除的对象
    lv_obj_t *current_screen = ui_data.screen;
    ui_data.screen = NULL;
    
    // 创建菜单屏幕（但还不显示）
    menu_ui_create_screen();
    
    // 使用LVGL内置的屏幕切换动画 - 从左向右滑动效果（返回感觉）
    lv_scr_load_anim(lv_scr_act(), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, true); // 最后参数true表示自动删除旧屏幕
    
    // 激活菜单
    menu_ui_set_active();
}

// Button event handler callback
static void _button_handler_cb(lv_timer_t *timer) {
    if (!ui_data.is_active || !ui_data.screen) return;
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    // Clear event flag
    key355_clear_event();
    
    // Handle button events
    switch (event) {
        case BUTTON_EVENT_CLICK:
            // Toggle animation mode on single click
            if (animation_active) {
                _stop_arc_animation();
            } else {
                _start_arc_animation();
            }
            break;
            
        case BUTTON_EVENT_DOUBLE_CLICK:
            // Return to menu on double click
            _return_to_menu();
            break;
            
        default:
            break;
    }
}

// Batch fetch all data
static void _batch_fetch_all_data(storage_data_t *storage_data, memory_data_t *memory_data) {
    data_manager_get_storage(&storage_data->used, &storage_data->total, &storage_data->percent);
    data_manager_get_memory(&memory_data->used, &memory_data->total, &memory_data->percent);
}

// Update UI data
static void _update_ui_data(lv_timer_t *timer) {
    if (!ui_data.is_active || !ui_data.screen) return;
    
    // Get latest data
    storage_data_t storage_data;
    memory_data_t memory_data;
    _batch_fetch_all_data(&storage_data, &memory_data);
    
    // Update UI only if data changes or on first update
    bool data_changed = (storage_data.percent != ui_cache.storage_percent || 
                        memory_data.percent != ui_cache.memory_percent);
                        
    if (data_changed || ui_cache.first_update) {
        // Update cache data
        ui_cache.storage_percent = storage_data.percent;
        ui_cache.memory_percent = memory_data.percent;
        ui_cache.storage_used = storage_data.used;
        ui_cache.storage_total = storage_data.total;
        ui_cache.memory_used = memory_data.used;
        ui_cache.memory_total = memory_data.total;
        
        // Update arc values if no animation effect
        if (!animation_active) {
            lv_arc_set_value(ui_data.storage_arc, storage_data.percent);
            lv_arc_set_value(ui_data.memory_arc, memory_data.percent);
        }
        
        // Update labels
        lv_label_set_text_fmt(ui_data.percent_label, "%d%%", storage_data.percent);
        ui_utils_size_to_str(storage_data.used, ui_cache.used_str, sizeof(ui_cache.used_str));
        ui_utils_size_to_str(storage_data.total, ui_cache.total_str, sizeof(ui_cache.total_str));
        lv_label_set_text_fmt(ui_data.info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
        
        lv_label_set_text_fmt(ui_data.memory_percent_label, "%d%%", memory_data.percent);
        ui_utils_size_to_str(memory_data.used, ui_cache.used_str, sizeof(ui_cache.used_str));
        ui_utils_size_to_str(memory_data.total, ui_cache.total_str, sizeof(ui_cache.total_str));
        lv_label_set_text_fmt(ui_data.memory_info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
        
        ui_cache.first_update = false;
    }
}

// Create storage UI screen
void storage_ui_create_screen(void) {
    // Create screen (use lv_obj_create instead of non-existent lv_screen_create)
    ui_data.screen = lv_obj_create(NULL);
    
#if LV_USE_DRAW_SW_COMPLEX_GRADIENTS
    // 定义渐变色 - 紫色到黑色
    static const lv_color_t grad_colors[2] = {
        LV_COLOR_MAKE(0x9B, 0x18, 0x42), // 紫红色
        LV_COLOR_MAKE(0x00, 0x00, 0x00), // 纯黑色
    };
    
    // 获取屏幕尺寸
    int32_t width = lv_display_get_horizontal_resolution(NULL);
    int32_t height = lv_display_get_vertical_resolution(NULL);
    
    // 初始化渐变描述符
    static lv_grad_dsc_t grad;
    lv_grad_init_stops(&grad, grad_colors, NULL, NULL, sizeof(grad_colors) / sizeof(lv_color_t));
    
    // 设置径向渐变 - 从中心向四周扩散
    lv_grad_radial_init(&grad, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_BOTTOM, LV_GRAD_EXTEND_PAD);
    
    // 应用渐变背景 - 修复：移除错误的 & 符号
    lv_obj_set_style_bg_grad(ui_data.screen, &grad, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#else
    // Set background style
    lv_obj_set_style_bg_color(ui_data.screen, lv_color_hex(0x1E293B), 0); // Dark indigo background
    lv_obj_set_style_bg_grad_color(ui_data.screen, lv_color_hex(0x0F172A), 0); // Gradient dark blue-black
    lv_obj_set_style_bg_grad_dir(ui_data.screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#endif
    
    // Clear scrollable flags to prevent layout issues
    lv_obj_clear_flag(ui_data.screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Font definitions
    const lv_font_t *font_big = &lv_font_montserrat_22;
    const lv_font_t *font_small = &lv_font_montserrat_12;
    
    // Storage arc progress bar - Left side - 调整位置更靠左
    ui_data.storage_arc = lv_arc_create(ui_data.screen);
    lv_obj_set_size(ui_data.storage_arc, 75, 75);
    lv_obj_align(ui_data.storage_arc, LV_ALIGN_LEFT_MID, 30, 0); // 从40改为30，更靠近左边
    lv_arc_set_rotation(ui_data.storage_arc, 270);
    lv_arc_set_bg_angles(ui_data.storage_arc, 0, 360);
    lv_arc_set_range(ui_data.storage_arc, 0, 100);
    lv_arc_set_value(ui_data.storage_arc, 0);
    lv_arc_set_mode(ui_data.storage_arc, LV_ARC_MODE_NORMAL);
    lv_obj_remove_style(ui_data.storage_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(ui_data.storage_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_data.storage_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.storage_arc, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_data.storage_arc, lv_color_hex(0x10B981), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ui_data.storage_arc, true, LV_PART_INDICATOR);
    
    // Memory arc progress bar - Right side - 调整位置更靠右
    ui_data.memory_arc = lv_arc_create(ui_data.screen);
    lv_obj_set_size(ui_data.memory_arc, 75, 75);
    lv_obj_align(ui_data.memory_arc, LV_ALIGN_RIGHT_MID, -30, 0); // 从-40改为-30，更靠近右边
    lv_arc_set_rotation(ui_data.memory_arc, 270);
    lv_arc_set_bg_angles(ui_data.memory_arc, 0, 360);
    lv_arc_set_range(ui_data.memory_arc, 0, 100);
    lv_arc_set_value(ui_data.memory_arc, 0);
    lv_arc_set_mode(ui_data.memory_arc, LV_ARC_MODE_NORMAL);
    lv_obj_remove_style(ui_data.memory_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(ui_data.memory_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_data.memory_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.memory_arc, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_data.memory_arc, lv_color_hex(0x8B5CF6), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ui_data.memory_arc, true, LV_PART_INDICATOR);
    
    // Storage title - 确保标题正确对齐
    lv_obj_t *storage_title = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(storage_title, font_small, 0);
    lv_obj_set_style_text_color(storage_title, lv_color_hex(0xF1F5F9), 0);
    lv_label_set_text(storage_title, "STORAGE");
    lv_obj_align_to(storage_title, ui_data.storage_arc, LV_ALIGN_OUT_TOP_MID, 0, -8); // 调整垂直边距

    // Storage percentage - 完全居中对齐
    ui_data.percent_label = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.percent_label, font_big, 0);
    lv_obj_set_style_text_color(ui_data.percent_label, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_width(ui_data.percent_label, 75); // 设置固定宽度，与圆弧宽度相同
    lv_obj_set_style_text_align(ui_data.percent_label, LV_TEXT_ALIGN_CENTER, 0); // 文本居中对齐
    lv_label_set_text(ui_data.percent_label, "0%");
    lv_obj_align_to(ui_data.percent_label, ui_data.storage_arc, LV_ALIGN_CENTER, 0, 0);

    // Storage details - 完全居中对齐
    ui_data.info_label = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.info_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.info_label, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_width(ui_data.info_label, 100); // 设置固定宽度，确保文本居中效果
    lv_obj_set_style_text_align(ui_data.info_label, LV_TEXT_ALIGN_CENTER, 0); // 设置文本中心对齐
    lv_label_set_text(ui_data.info_label, "0 / 0");
    lv_obj_align_to(ui_data.info_label, ui_data.storage_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    
    // Memory title - 确保标题正确对齐
    lv_obj_t *memory_title = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(memory_title, font_small, 0);
    lv_obj_set_style_text_color(memory_title, lv_color_hex(0xF1F5F9), 0);
    lv_label_set_text(memory_title, "MEMORY");
    lv_obj_align_to(memory_title, ui_data.memory_arc, LV_ALIGN_OUT_TOP_MID, 0, -8); // 调整垂直边距

    // Memory percentage - 完全居中对齐
    ui_data.memory_percent_label = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.memory_percent_label, font_big, 0);
    lv_obj_set_style_text_color(ui_data.memory_percent_label, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_width(ui_data.memory_percent_label, 75); // 设置固定宽度，与圆弧宽度相同
    lv_obj_set_style_text_align(ui_data.memory_percent_label, LV_TEXT_ALIGN_CENTER, 0); // 文本居中对齐
    lv_label_set_text(ui_data.memory_percent_label, "0%");
    lv_obj_align_to(ui_data.memory_percent_label, ui_data.memory_arc, LV_ALIGN_CENTER, 0, 0);
    
    // Memory details - 完全居中对齐
    ui_data.memory_info_label = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.memory_info_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.memory_info_label, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_width(ui_data.memory_info_label, 100); // 设置固定宽度，确保文本居中效果
    lv_obj_set_style_text_align(ui_data.memory_info_label, LV_TEXT_ALIGN_CENTER, 0); // 设置文本中心对齐
    lv_label_set_text(ui_data.memory_info_label, "0 / 0");
    lv_obj_align_to(ui_data.memory_info_label, ui_data.memory_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    
    // 设置屏幕进入动画 - 简化为只有水平滑动
    lv_obj_set_x(ui_data.screen, 240); // 初始位置在屏幕右侧外
    
    // 动画时长优化
    uint32_t anim_time = 260;
    
    // X轴滑动动画
    lv_anim_t a_x;
    lv_anim_init(&a_x);
    lv_anim_set_var(&a_x, ui_data.screen);
    lv_anim_set_values(&a_x, 240, 0);
    lv_anim_set_time(&a_x, anim_time);
    lv_anim_set_exec_cb(&a_x, _slide_anim_cb);
    lv_anim_set_path_cb(&a_x, lv_anim_path_ease_out);
    lv_anim_start(&a_x);
    
    // Create button handler timer
    ui_data.button_timer = lv_timer_create(_button_handler_cb, 50, NULL);
    
    // Create data update timer (update every 500ms)
    ui_data.update_timer = lv_timer_create(_update_ui_data, 500, NULL);
    
    // Set as active
    ui_data.is_active = true;
    
    // First data update
    ui_cache.first_update = true;
    _update_ui_data(NULL);
    
    // Start arc animation effect
    _start_arc_animation();
    
    // 直接使用LVGL内置的屏幕切换动画 - 淡入效果
    lv_scr_load_anim(ui_data.screen, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
}
