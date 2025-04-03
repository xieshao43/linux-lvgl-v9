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
    
    // 创建面板
    ui_data.panel = lv_obj_create(parent);
    lv_obj_set_size(ui_data.panel, 235, 130);
    lv_obj_align(ui_data.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_data.panel, lv_color_hex(color_panel), 0);
    lv_obj_set_style_border_width(ui_data.panel, 0, 0);
    lv_obj_set_style_radius(ui_data.panel, 12, 0);
    lv_obj_set_style_pad_all(ui_data.panel, 10, 0);
    lv_obj_set_style_shadow_width(ui_data.panel, 15, 0);
    lv_obj_set_style_shadow_color(ui_data.panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ui_data.panel, LV_OPA_30, 0);
    
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

    // Storage details
    ui_data.info_label = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(ui_data.info_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.info_label, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(ui_data.info_label, "0 / 0");
    lv_obj_align_to(ui_data.info_label, ui_data.storage_arc, LV_ALIGN_OUT_BOTTOM_MID, -25, 0);
    
    // Memory title
    lv_obj_t *memory_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(memory_title, font_small, 0);
    lv_obj_set_style_text_color(memory_title, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(memory_title, "MEMORY");
    lv_obj_align_to(memory_title, ui_data.memory_arc, LV_ALIGN_OUT_TOP_MID, 0, -5);

    // Memory percentage
    ui_data.memory_percent_label = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(ui_data.memory_percent_label, font_big, 0);
    lv_obj_set_style_text_color(ui_data.memory_percent_label, lv_color_hex(color_text), 0);
    lv_label_set_text(ui_data.memory_percent_label, "0%");
    lv_obj_align_to(ui_data.memory_percent_label, ui_data.memory_arc, LV_ALIGN_CENTER, 0, 0);
    
    // Memory details
    ui_data.memory_info_label = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(ui_data.memory_info_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.memory_info_label, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(ui_data.memory_info_label, "0 / 0");
    lv_obj_align_to(ui_data.memory_info_label, ui_data.memory_arc, LV_ALIGN_OUT_BOTTOM_MID, -30, 0);
}

// 更新UI数据
static void _update_ui(void) {
    uint64_t storage_used, storage_total;
    uint64_t memory_used, memory_total;
    uint8_t storage_percent, memory_percent;
    char used_str[20], total_str[20];
    
    // 获取存储数据
    data_manager_get_storage(&storage_used, &storage_total, &storage_percent);
    
    // 转换为可读格式
    ui_utils_size_to_str(storage_used, used_str, sizeof(used_str));
    ui_utils_size_to_str(storage_total, total_str, sizeof(total_str));
    
    // 更新存储UI
    ui_utils_animate_arc(ui_data.storage_arc, 
                         lv_arc_get_value(ui_data.storage_arc), 
                         storage_percent, 800);
    lv_label_set_text_fmt(ui_data.percent_label, "%d%%", storage_percent);
    lv_label_set_text_fmt(ui_data.info_label, "%s / %s", used_str, total_str);
    
    // 获取内存数据
    data_manager_get_memory(&memory_used, &memory_total, &memory_percent);
    
    // 转换为可读格式
    ui_utils_size_to_str(memory_used, used_str, sizeof(used_str));
    ui_utils_size_to_str(memory_total, total_str, sizeof(total_str));
    
    // 更新内存UI
    ui_utils_animate_arc(ui_data.memory_arc, 
                         lv_arc_get_value(ui_data.memory_arc), 
                         memory_percent, 800);
    lv_label_set_text_fmt(ui_data.memory_percent_label, "%d%%", memory_percent);
    lv_label_set_text_fmt(ui_data.memory_info_label, "%s / %s", used_str, total_str);
}

// 显示UI
static void _show_ui(void) {
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
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
