

#include "cpu_ui.h"
#include "../core/data_manager.h"
#include "../utils/ui_utils.h"
#include <stdio.h>

// 私有数据结构
typedef struct {
    lv_obj_t *panel;         // 主面板
    lv_obj_t *core_bars[CPU_CORES];  // 核心进度条
    lv_obj_t *core_labels[CPU_CORES];  // 核心百分比标签
    lv_obj_t *temp_label;   // 温度标签
} cpu_ui_data_t;

static cpu_ui_data_t ui_data;
static ui_module_t cpu_module;

// 私有函数原型
static void _create_ui(lv_obj_t *parent);
static void _update_ui(void);
static void _show_ui(void);
static void _hide_ui(void);
static void _delete_ui(void);

// 模块接口实现
static void cpu_ui_create(lv_obj_t *parent) {
    _create_ui(parent);
}

static void cpu_ui_delete(void) {
    _delete_ui();
}

static void cpu_ui_show(void) {
    _show_ui();
}

static void cpu_ui_hide(void) {
    _hide_ui();
}

static void cpu_ui_update(void) {
    _update_ui();
}

// 获取模块接口
ui_module_t* cpu_ui_get_module(void) {
    cpu_module.create = cpu_ui_create;
    cpu_module.delete = cpu_ui_delete;
    cpu_module.show = cpu_ui_show;
    cpu_module.hide = cpu_ui_hide;
    cpu_module.update = cpu_ui_update;

    return &cpu_module;
}

// 创建CPU监控UI - 基于原来的create_cpu_4cores_page()
static void _create_ui(lv_obj_t *parent) {
    const lv_font_t *font_small = &lv_font_montserrat_12;
    const lv_font_t *font_mid = &lv_font_montserrat_16;

    // 优雅高级配色方案
    uint32_t color_bg = 0x1A1A2E;         // 深蓝黑背景
    uint32_t color_panel = 0x16213E;      // 深蓝面板
    uint32_t color_accent = 0x0F3460;     // 深红紫色强调
    uint32_t color_core_colors[CPU_CORES] = {
        0xD4AF37,  // Rich Gold - Core 0
        0x9D2933,  // Crimson Red - Core 1
        0x2E8B57,  // Emerald Green - Core 2
        0x9932CC   // Royal Purple - Core 3
    };
    uint32_t color_text = 0xE6E6E6;       // 柔和白色
    uint32_t color_text_secondary = 0xB8B8D0; // 淡紫灰

    // 创建CPU面板 - 使用align而非绝对位置
    ui_data.panel = lv_obj_create(parent);
    lv_obj_set_size(ui_data.panel, 240, 135);
    lv_obj_align(ui_data.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_data.panel, lv_color_hex(color_panel), 0);
    lv_obj_set_style_border_width(ui_data.panel, 0, 0);
    lv_obj_set_style_radius(ui_data.panel, 0, 0);
    lv_obj_set_style_pad_all(ui_data.panel, 15, 0);

    // CPU标题
    lv_obj_t *cpu_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(cpu_title, font_mid, 0);
    lv_obj_set_style_text_color(cpu_title, lv_color_hex(color_text), 0);
    lv_label_set_text(cpu_title, "CPU MONITOR");
    lv_obj_align(cpu_title, LV_ALIGN_TOP_MID, 0, -10);

    // 分隔线 - 使用矩形替代线条
    lv_obj_t *separator = lv_obj_create(ui_data.panel);
    lv_obj_set_size(separator, 210, 2);
    lv_obj_align_to(separator, cpu_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    lv_obj_set_style_bg_color(separator, lv_color_hex(color_accent), 0);
    lv_obj_set_style_border_width(separator, 0, 0);
    lv_obj_set_style_radius(separator, 0, 0);

    // 四个水平进度条，每个核心一个
    int bar_height = 10;       // 进度条高度
    int bar_width = 150;       // 进度条宽度
    int vertical_spacing = 20; // 垂直间距
    int start_y = 20;          // 起始位置
    int left_margin = 30;      // 左侧边距

    for(int i = 0; i < CPU_CORES; i++) {
        // 核心标签
        lv_obj_t *core_label = lv_label_create(ui_data.panel);
        lv_obj_set_style_text_font(core_label, font_small, 0);
        lv_obj_set_style_text_color(core_label, lv_color_hex(color_core_colors[i]), 0);
        lv_label_set_text_fmt(core_label, "Core%d", i);

        // 创建进度条
        lv_obj_t *bar = lv_bar_create(ui_data.panel);
        lv_obj_set_size(bar, bar_width, bar_height);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, left_margin, start_y + i * vertical_spacing);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x2A2A3E), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(color_core_colors[i]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(bar, lv_color_hex(0x1A1A2E), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 3, 0);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        // 使用相对于进度条的中心点对齐标签
        lv_obj_align_to(core_label, bar, LV_ALIGN_OUT_LEFT_MID, -5, 0);

        // 保存进度条引用
        ui_data.core_bars[i] = bar;

        // 核心百分比标签
        ui_data.core_labels[i] = lv_label_create(ui_data.panel);
        lv_obj_set_style_text_font(ui_data.core_labels[i], font_small, 0);
        lv_obj_set_style_text_color(ui_data.core_labels[i], lv_color_hex(color_text), 0);
        lv_label_set_text(ui_data.core_labels[i], "0%");
        lv_obj_align_to(ui_data.core_labels[i], bar, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    }

    // 温度显示容器
    lv_obj_t *temp_container = lv_obj_create(ui_data.panel);
    lv_obj_set_size(temp_container, 210, 25);
    lv_obj_align(temp_container, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_set_style_bg_color(temp_container, lv_color_hex(color_accent), 0);
    lv_obj_set_style_bg_opa(temp_container, LV_OPA_20, 0);
    lv_obj_set_style_radius(temp_container, 6, 0);
    lv_obj_set_style_border_width(temp_container, 0, 0);

    // 温度标签
    ui_data.temp_label = lv_label_create(temp_container);
    lv_obj_set_style_text_font(ui_data.temp_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.temp_label, lv_color_hex(color_text), 0);
    lv_label_set_text(ui_data.temp_label, "Temperature: 0°C");
    lv_obj_center(ui_data.temp_label);

    // 初始时隐藏面板
    lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
}

// 更新UI数据
static void _update_ui(void) {
    float cpu_usage;
    uint8_t cpu_temp;

    // 获取CPU整体数据
    data_manager_get_cpu(&cpu_usage, &cpu_temp);

    // 更新温度显示
    lv_label_set_text_fmt(ui_data.temp_label, "Temperature: %d°C", cpu_temp);

    // 更新每个核心数据
    for (int i = 0; i < CPU_CORES; i++) {
        float core_usage;
        data_manager_get_cpu_core(i, &core_usage);

        // 更新进度条(带动画)
        ui_utils_animate_bar(ui_data.core_bars[i],
                           lv_bar_get_value(ui_data.core_bars[i]),
                           (int32_t)core_usage, 500);

        // 更新百分比标签
        lv_label_set_text_fmt(ui_data.core_labels[i], "%d%%", (int)core_usage);
    }
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
