#include "storage_ui.h"
#include "../core/data_manager.h"
#include "../utils/ui_utils.h"
#include <stdio.h>

// 私有数据结构 - 从原有storage_ui_t提取关键部分
typedef struct {
    lv_obj_t *panel;           // 主面板
    lv_obj_t *storage_arc;     // 存储环形图
    lv_obj_t *percent_label;   // 存储百分比标签
    lv_obj_t *info_label;      // 存储详情标签
    
    lv_obj_t *memory_arc;      // 内存环形图
    lv_obj_t *memory_percent_label; // 内存百分比标签
    lv_obj_t *memory_info_label;    // 内存详情标签
} storage_ui_data_t;

static storage_ui_data_t ui_data;
static ui_module_t storage_module;

// 添加静态缓存，避免频繁内存分配
static struct {
    char used_str[20];
    char total_str[20];
    uint8_t storage_percent;
    uint8_t memory_percent;
    uint64_t storage_used;
    uint64_t storage_total;
    uint64_t memory_used;
    uint64_t memory_total;
} ui_cache;

// 私有函数原型
static void _create_ui(lv_obj_t *parent);
static void _update_ui(void);
static void _show_ui(void);
static void _hide_ui(void);
static void _delete_ui(void);

// 模块接口实现
static void storage_ui_create(lv_obj_t *parent) {
    _create_ui(parent);
}

static void storage_ui_delete(void) {
    _delete_ui();
}

static void storage_ui_show(void) {
    _show_ui();
}

static void storage_ui_hide(void) {
    _hide_ui();
}

static void storage_ui_update(void) {
    _update_ui();
}

// 获取模块接口
ui_module_t* storage_ui_get_module(void) {
    storage_module.create = storage_ui_create;
    storage_module.delete = storage_ui_delete;
    storage_module.show = storage_ui_show;
    storage_module.hide = storage_ui_hide;
    storage_module.update = storage_ui_update;
    
    return &storage_module;
}

// 创建存储与内存UI - 基于原来的create_ram_rom_ui()
static void _create_ui(lv_obj_t *parent) {
    // 字体定义
    const lv_font_t *font_big = &lv_font_montserrat_22;
    const lv_font_t *font_small = &lv_font_montserrat_12;
    
    // 优雅的配色方案
    uint32_t color_bg = 0x2D3436;        // 深板岩背景色
    uint32_t color_panel = 0x34495E;     // 深蓝色面板
    uint32_t color_accent1 = 0x9B59B6;   // 优雅的紫色
    uint32_t color_accent2 = 0x3498DB;   // 精致的蓝色
    uint32_t color_text = 0xECF0F1;      // 柔和的白色
    uint32_t color_text_secondary = 0xBDC3C7; // 银灰色
    
    // 创建面板 - 使用圆角矩形增加现代感
    ui_data.panel = lv_obj_create(parent);
    lv_obj_set_size(ui_data.panel, 235, 130);
    lv_obj_align(ui_data.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_data.panel, lv_color_hex(color_panel), 0);
    lv_obj_set_style_border_width(ui_data.panel, 0, 0);
    lv_obj_set_style_radius(ui_data.panel, 16, 0); // 更大的圆角
    lv_obj_set_style_pad_all(ui_data.panel, 10, 0);
    
    // 添加微妙的渐变色
    lv_obj_set_style_bg_grad_color(ui_data.panel, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_grad_dir(ui_data.panel, LV_GRAD_DIR_VER, 0);
    
    // 优化阴影 - 使用苹果风格的柔和阴影
    lv_obj_set_style_shadow_width(ui_data.panel, 20, 0);
    lv_obj_set_style_shadow_spread(ui_data.panel, 4, 0); // 阴影扩散，增加柔和度
    lv_obj_set_style_shadow_ofs_x(ui_data.panel, 0, 0);  // 居中阴影
    lv_obj_set_style_shadow_ofs_y(ui_data.panel, 6, 0);  // 轻微下移
    lv_obj_set_style_shadow_color(ui_data.panel, lv_color_hex(0x101010), 0);
    lv_obj_set_style_shadow_opa(ui_data.panel, LV_OPA_10, 0); // 更柔和的阴影
    
    // 存储弧形进度条 - 左侧
    ui_data.storage_arc = lv_arc_create(ui_data.panel);
    lv_obj_set_size(ui_data.storage_arc, 75, 75);
    lv_obj_align(ui_data.storage_arc, LV_ALIGN_LEFT_MID, 10, 0);
    lv_arc_set_rotation(ui_data.storage_arc, 270);
    lv_arc_set_bg_angles(ui_data.storage_arc, 0, 360);
    lv_arc_set_range(ui_data.storage_arc, 0, 100);
    lv_arc_set_value(ui_data.storage_arc, 0);
    lv_obj_remove_style(ui_data.storage_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(ui_data.storage_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_data.storage_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.storage_arc, lv_color_hex(0x8DBF9B), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.storage_arc, lv_color_hex(0x4A5568), LV_PART_MAIN);
    
    // 内存弧形进度条 - 右侧
    ui_data.memory_arc = lv_arc_create(ui_data.panel);
    lv_obj_set_size(ui_data.memory_arc, 75, 75);
    lv_obj_align(ui_data.memory_arc, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_arc_set_rotation(ui_data.memory_arc, 270);
    lv_arc_set_bg_angles(ui_data.memory_arc, 0, 360);
    lv_arc_set_range(ui_data.memory_arc, 0, 100);
    lv_arc_set_value(ui_data.memory_arc, 0);
    lv_obj_remove_style(ui_data.memory_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(ui_data.memory_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_data.memory_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.memory_arc, lv_color_hex(COLOR_SECONDARY), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.memory_arc, lv_color_hex(0x4A5568), LV_PART_MAIN);
    
    // 存储标题
    lv_obj_t *storage_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(storage_title, font_small, 0);
    lv_obj_set_style_text_color(storage_title, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(storage_title, "STORAGE");
    lv_obj_align_to(storage_title, ui_data.storage_arc, LV_ALIGN_OUT_TOP_MID, 0, -5);

    // 存储百分比
    ui_data.percent_label = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(ui_data.percent_label, font_big, 0);
    lv_obj_set_style_text_color(ui_data.percent_label, lv_color_hex(color_text), 0);
    lv_label_set_text(ui_data.percent_label, "0%");
    lv_obj_align_to(ui_data.percent_label, ui_data.storage_arc, LV_ALIGN_CENTER, 0, 0);

    // 存储详情
    ui_data.info_label = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(ui_data.info_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.info_label, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(ui_data.info_label, "0 / 0");
    lv_obj_align_to(ui_data.info_label, ui_data.storage_arc, LV_ALIGN_OUT_BOTTOM_MID, -25, 0);
    
    // 内存标题
    lv_obj_t *memory_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(memory_title, font_small, 0);
    lv_obj_set_style_text_color(memory_title, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(memory_title, "MEMORY");
    lv_obj_align_to(memory_title, ui_data.memory_arc, LV_ALIGN_OUT_TOP_MID, 0, -5);

    // 内存百分比
    ui_data.memory_percent_label = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(ui_data.memory_percent_label, font_big, 0);
    lv_obj_set_style_text_color(ui_data.memory_percent_label, lv_color_hex(color_text), 0);
    lv_label_set_text(ui_data.memory_percent_label, "0%");
    lv_obj_align_to(ui_data.memory_percent_label, ui_data.memory_arc, LV_ALIGN_CENTER, 0, 0);
    
    // 内存详情
    ui_data.memory_info_label = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(ui_data.memory_info_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.memory_info_label, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(ui_data.memory_info_label, "0 / 0");
    lv_obj_align_to(ui_data.memory_info_label, ui_data.memory_arc, LV_ALIGN_OUT_BOTTOM_MID, -30, 0);
    
    // 优化环形图视觉效果
    lv_obj_set_style_arc_rounded(ui_data.storage_arc, true, LV_PART_INDICATOR); // 圆角进度条
    lv_obj_set_style_arc_rounded(ui_data.memory_arc, true, LV_PART_INDICATOR);
    
    // 初始时隐藏面板
    lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
}

// 更新UI数据 - 智能缓存版
static void _update_ui(void) {
    uint8_t storage_percent, memory_percent;
    uint64_t storage_used, storage_total;
    uint64_t memory_used, memory_total;
    bool storage_changed = false;
    bool memory_changed = false;
    
    // 获取存储数据
    data_manager_get_storage(&storage_used, &storage_total, &storage_percent);
    
    // 检查存储数据是否变化 - 添加首次更新检查
    static bool first_update = true;
    if (first_update || 
        storage_percent != ui_cache.storage_percent || 
        storage_used != ui_cache.storage_used ||
        storage_total != ui_cache.storage_total) {
        
        // 更新缓存
        ui_cache.storage_percent = storage_percent;
        ui_cache.storage_used = storage_used;
        ui_cache.storage_total = storage_total;
        storage_changed = true;
        
        // 转换为可读格式 - 重用缓存字符串
        ui_utils_size_to_str(storage_used, ui_cache.used_str, sizeof(ui_cache.used_str));
        ui_utils_size_to_str(storage_total, ui_cache.total_str, sizeof(ui_cache.total_str));
        
        // 输出调试信息，帮助诊断问题
        printf("存储更新: %d%%, %s/%s\n", storage_percent, ui_cache.used_str, ui_cache.total_str);
        
        // 更新存储UI - 使用优化的动画控制逻辑
        int32_t current = lv_arc_get_value(ui_data.storage_arc);
        if (abs(current - storage_percent) > 2) {
            ui_utils_animate_arc(ui_data.storage_arc, current, storage_percent, 800);
        } else {
            lv_arc_set_value(ui_data.storage_arc, storage_percent);
        }
        
        // 更新标签
        lv_label_set_text_fmt(ui_data.percent_label, "%d%%", storage_percent);
        lv_label_set_text_fmt(ui_data.info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    }
    
    // 获取内存数据
    data_manager_get_memory(&memory_used, &memory_total, &memory_percent);
    
    // 检查内存数据是否变化 - 添加首次更新检查
    if (first_update || 
        memory_percent != ui_cache.memory_percent ||
        memory_used != ui_cache.memory_used ||
        memory_total != ui_cache.memory_total) {
        
        // 更新缓存
        ui_cache.memory_percent = memory_percent;
        ui_cache.memory_used = memory_used;
        ui_cache.memory_total = memory_total;
        memory_changed = true;
        
        // 转换为可读格式 - 重用缓存字符串
        ui_utils_size_to_str(memory_used, ui_cache.used_str, sizeof(ui_cache.used_str));
        ui_utils_size_to_str(memory_total, ui_cache.total_str, sizeof(ui_cache.total_str));
        
        // 输出调试信息，帮助诊断问题
        //printf("内存更新: %d%%, %s/%s\n", memory_percent, ui_cache.used_str, ui_cache.total_str);
        
        // 更新内存UI - 使用优化的动画控制逻辑
        int32_t current = lv_arc_get_value(ui_data.memory_arc);
        if (abs(current - memory_percent) > 2) {
            ui_utils_animate_arc(ui_data.memory_arc, current, memory_percent, 800);
        } else {
            lv_arc_set_value(ui_data.memory_arc, memory_percent);
        }
        
        // 更新标签
        lv_label_set_text_fmt(ui_data.memory_percent_label, "%d%%", memory_percent);
        lv_label_set_text_fmt(ui_data.memory_info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    }
    
    // 重置首次更新标志
    first_update = false;
}

// 显示UI - 添加苹果风格的交错进入动画
static void _show_ui(void) {
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
    
    // 获取当前实际数据，而不是硬编码为100%
    uint8_t storage_percent, memory_percent;
    uint64_t storage_used, storage_total;
    uint64_t memory_used, memory_total;
    
    // 获取最新数据
    data_manager_get_storage(&storage_used, &storage_total, &storage_percent);
    data_manager_get_memory(&memory_used, &memory_total, &memory_percent);
    
    // 保存到缓存
    ui_cache.storage_percent = storage_percent;
    ui_cache.storage_used = storage_used;
    ui_cache.storage_total = storage_total;
    ui_cache.memory_percent = memory_percent;
    ui_cache.memory_used = memory_used;
    ui_cache.memory_total = memory_total;
    
    // 通知数据管理器动画开始
    data_manager_set_anim_state(true);
    
    // 创建交错进入动画效果
    lv_anim_t anim;
    
    // 存储环形图动画 - 动画到实际百分比，而不是固定的100%
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ui_data.storage_arc);
    lv_anim_set_values(&anim, 0, storage_percent); // 使用实际百分比
    lv_anim_set_time(&anim, 800);
    lv_anim_set_delay(&anim, 0);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_start(&anim);
    
    // 内存环形图动画 - 动画到实际百分比
    lv_anim_t anim2;
    lv_anim_init(&anim2);
    lv_anim_set_var(&anim2, ui_data.memory_arc);
    lv_anim_set_values(&anim2, 0, memory_percent); // 使用实际百分比
    lv_anim_set_time(&anim2, 800);
    lv_anim_set_delay(&anim2, ANIM_STAGGER_DELAY); // 交错延迟
    lv_anim_set_path_cb(&anim2, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim2, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_start(&anim2);
    
    // 更新显示标签
    lv_label_set_text_fmt(ui_data.percent_label, "%d%%", storage_percent);
    ui_utils_size_to_str(storage_used, ui_cache.used_str, sizeof(ui_cache.used_str));
    ui_utils_size_to_str(storage_total, ui_cache.total_str, sizeof(ui_cache.total_str));
    lv_label_set_text_fmt(ui_data.info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    
    lv_label_set_text_fmt(ui_data.memory_percent_label, "%d%%", memory_percent);
    ui_utils_size_to_str(memory_used, ui_cache.used_str, sizeof(ui_cache.used_str));
    ui_utils_size_to_str(memory_total, ui_cache.total_str, sizeof(ui_cache.total_str));
    lv_label_set_text_fmt(ui_data.memory_info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    
    // 创建结束动画事件的计时器
    lv_timer_t *anim_end_timer = lv_timer_create(
        (lv_timer_cb_t)data_manager_set_anim_state, 
        900, 
        (void *)false
    );
    lv_timer_set_repeat_count(anim_end_timer, 1);
    
    // 添加强制更新定时器，确保数据获取后显示正确
    lv_timer_t *force_update_timer = lv_timer_create(
        (lv_timer_cb_t)_update_ui,
        1000,  // 1秒后强制更新一次
        NULL
    );
    lv_timer_set_repeat_count(force_update_timer, 1);
}

// 隐藏UI
static void _hide_ui(void) {
    lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
}

// 删除UI
static void _delete_ui(void) {
    if (ui_data.panel) {
        lv_obj_delete(ui_data.panel);
        ui_data.panel = NULL;
    }
}
