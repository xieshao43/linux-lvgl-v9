#include "cpu_ui.h"
#include "../core/data_manager.h"
#include "../utils/ui_utils.h"
#include <stdio.h>
#include <math.h>   // 添加math.h头文件，解决fabs函数未声明问题

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

// 增加静态缓存，避免重复计算和更新
static struct {
    float core_usage[CPU_CORES];
    uint8_t cpu_temp;
    bool first_update;
} ui_cache = {
    .first_update = true
};

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

// 创建CPU监控UI - 优化视觉效果
static void _create_ui(lv_obj_t *parent) {
    const lv_font_t *font_small = &lv_font_montserrat_12;
    const lv_font_t *font_mid = &lv_font_montserrat_16;

    // 更新为苹果风格配色方案
    uint32_t color_panel = 0x111827;      // 深蓝黑面板 - 类似macOS深色模式
    uint32_t color_accent = 0x0369A1;     // iOS蓝色强调色
    uint32_t color_core_colors[CPU_CORES] = {
        0x06B6D4,  // 青色 - Core 0
        0x8B5CF6,  // 紫色 - Core 1
        0x10B981,  // 绿色 - Core 2
        0xF59E0B   // 琥珀色 - Core 3
    };
    uint32_t color_text = 0xF9FAFB;       // 近白色文本
    uint32_t color_text_secondary = 0x9CA3AF; // 中灰色次要文本

    // 创建CPU面板 - 苹果风格圆角矩形
    ui_data.panel = lv_obj_create(parent);
    lv_obj_set_size(ui_data.panel, 240, 135);
    lv_obj_align(ui_data.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_data.panel, lv_color_hex(color_panel), 0);
    lv_obj_set_style_border_width(ui_data.panel, 0, 0);
    lv_obj_set_style_radius(ui_data.panel, 16, 0); // macOS风格圆角
    lv_obj_set_style_pad_all(ui_data.panel, 15, 0);
    //
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_SCROLLABLE);
    
    // 添加渐变
    lv_obj_set_style_bg_grad_color(ui_data.panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_grad_dir(ui_data.panel, LV_GRAD_DIR_VER, 0);
    
    // 添加阴影效果
    lv_obj_set_style_shadow_width(ui_data.panel, 25, 0);
    lv_obj_set_style_shadow_spread(ui_data.panel, 4, 0);
    lv_obj_set_style_shadow_ofs_x(ui_data.panel, 0, 0);
    lv_obj_set_style_shadow_ofs_y(ui_data.panel, 6, 0);
    lv_obj_set_style_shadow_color(ui_data.panel, lv_color_hex(0x101010), 0);
    lv_obj_set_style_shadow_opa(ui_data.panel, LV_OPA_10, 0);

    // CPU标题
    lv_obj_t *cpu_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(cpu_title, font_mid, 0);
    lv_obj_set_style_text_color(cpu_title, lv_color_hex(color_text), 0);
    lv_label_set_text(cpu_title, "CPU MONITOR");
    lv_obj_align(cpu_title, LV_ALIGN_TOP_MID, 0, -5);

    // 分隔线 - 更细更优雅
    lv_obj_t *separator = lv_obj_create(ui_data.panel);
    lv_obj_set_size(separator, 210, 1);
    lv_obj_align_to(separator, cpu_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_bg_color(separator, lv_color_hex(color_accent), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_40, 0); 
    lv_obj_set_style_border_width(separator, 0, 0);
    lv_obj_set_style_radius(separator, 0, 0);

    // 进度条设置
    int bar_height = 6;        // 更细的进度条
    int bar_width = 150;       // 进度条宽度
    int vertical_spacing = 22; // 垂直间距
    int start_y = 20;          // 起始位置
    int left_margin = 30;      // 左侧边距

    // 计算最后一个进度条的位置，用于后续放置温度容器
    int last_bar_bottom = start_y + (CPU_CORES - 1) * vertical_spacing + bar_height;

    for(int i = 0; i < CPU_CORES; i++) {
        // 核心标签
        lv_obj_t *core_label = lv_label_create(ui_data.panel);
        lv_obj_set_style_text_font(core_label, font_small, 0);
        lv_obj_set_style_text_color(core_label, lv_color_hex(color_core_colors[i]), 0);
        lv_label_set_text_fmt(core_label, "Core%d", i);

        // 创建进度条 - 现代风格
        lv_obj_t *bar = lv_bar_create(ui_data.panel);
        lv_obj_set_size(bar, bar_width, bar_height);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, left_margin, start_y + i * vertical_spacing);
        
        // 背景设置为半透明
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x374151), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_40, LV_PART_MAIN);
        
        // 指示器使用渐变效果
        lv_obj_set_style_bg_color(bar, lv_color_hex(color_core_colors[i]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(bar, lv_color_lighten(lv_color_hex(color_core_colors[i]), 30), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
        
        // 圆角进度条
        lv_obj_set_style_radius(bar, bar_height / 2, 0);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        // 对齐标签
        lv_obj_align_to(core_label, bar, LV_ALIGN_OUT_LEFT_MID, -5, 0);

        // 保存进度条引用
        ui_data.core_bars[i] = bar;

        // 核心百分比标签 - 初始设为透明
        ui_data.core_labels[i] = lv_label_create(ui_data.panel);
        lv_obj_set_style_text_font(ui_data.core_labels[i], font_small, 0);
        lv_obj_set_style_text_color(ui_data.core_labels[i], lv_color_hex(color_text), 0);
        lv_obj_set_style_opa(ui_data.core_labels[i], LV_OPA_TRANSP, 0);  // 初始透明，将通过动画显示
        lv_label_set_text(ui_data.core_labels[i], "0%");
        lv_obj_align_to(ui_data.core_labels[i], bar, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    }

    // 温度显示容器 - 确保在最后一个进度条下方
    lv_obj_t *temp_container = lv_obj_create(ui_data.panel);
    lv_obj_set_size(temp_container, 210, 20); // 合理的高度
    lv_obj_clear_flag(temp_container, LV_OBJ_FLAG_SCROLLABLE);// 禁用滚动
    lv_obj_set_scrollbar_mode(temp_container, LV_SCROLLBAR_MODE_OFF);    
    
    // 放置在最后一个进度条下方，预留足够空间
    lv_obj_align(temp_container, LV_ALIGN_TOP_MID, 0, last_bar_bottom + 0); 
    
    lv_obj_set_style_bg_color(temp_container, lv_color_hex(color_accent), 0);
    lv_obj_set_style_bg_opa(temp_container, LV_OPA_10, 0);
    lv_obj_set_style_radius(temp_container, 12, 0);
    lv_obj_set_style_border_width(temp_container, 0, 0);
    lv_obj_set_style_pad_all(temp_container, 5, 0); // 添加适当的内边距

    // 温度标签
    ui_data.temp_label = lv_label_create(temp_container);
    lv_obj_set_style_text_font(ui_data.temp_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.temp_label, lv_color_hex(color_text), 0);
    lv_obj_set_style_opa(ui_data.temp_label, LV_OPA_TRANSP, 0);
    lv_label_set_text(ui_data.temp_label, "Temperature: 0°C");
    lv_obj_center(ui_data.temp_label);

    // 初始时隐藏面板
    lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
}

// 更新UI数据 - 高效更新版本
static void _update_ui(void) {
    uint8_t cpu_temp;
    
    // 只获取温度数据，省略不使用的参数
    float unused_cpu_usage;
    data_manager_get_cpu(&unused_cpu_usage, &cpu_temp);

    // 仅当温度变化时更新标签
    if (cpu_temp != ui_cache.cpu_temp) {
        ui_cache.cpu_temp = cpu_temp;
        lv_label_set_text_fmt(ui_data.temp_label, "Temperature: %d°C", cpu_temp);
    }

    // 更新各核心数据 - 使用批处理和智能缓存
    for (int i = 0; i < CPU_CORES; i++) {
        float core_usage;
        data_manager_get_cpu_core(i, &core_usage);
        
        // 计算目标值，使用最低显示值
        int32_t target = (int32_t)(core_usage < 5.0f ? 5.0f : core_usage);
        
        // 检查是否需要更新
        if (ui_cache.first_update || fabs(ui_cache.core_usage[i] - core_usage) > 0.5f) {
            ui_cache.core_usage[i] = core_usage;
            
            // 首次更新或微小变化直接设置，避免动画开销
            if (ui_cache.first_update || fabs(lv_bar_get_value(ui_data.core_bars[i]) - target) <= 2) {
                lv_bar_set_value(ui_data.core_bars[i], target, LV_ANIM_OFF);
            } else {
                // 值变化大时才使用动画，减少动画资源消耗
                int32_t current = lv_bar_get_value(ui_data.core_bars[i]);
                ui_utils_animate_bar(ui_data.core_bars[i], current, target, 500);
            }
            
            // 只有值变化时才更新标签文本
            char buf[8];
            lv_snprintf(buf, sizeof(buf), "%d%%", (int)target);
            if (strcmp(lv_label_get_text(ui_data.core_labels[i]), buf) != 0) {
                lv_label_set_text(ui_data.core_labels[i], buf);
            }
        }
    }
    
    // 重置首次更新标志
    if (ui_cache.first_update) {
        ui_cache.first_update = false;
    }
}

// 显示UI - 优化版本
static void _show_ui(void) {
    // 重置缓存状态
    ui_cache.first_update = true;
    
    // 直接设置面板可见
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
    
    // 批量设置标签和控件可见
    for (int i = 0; i < CPU_CORES; i++) {
        lv_obj_set_style_opa(ui_data.core_labels[i], LV_OPA_COVER, 0);
        lv_bar_set_value(ui_data.core_bars[i], 0, LV_ANIM_OFF);
    }
    lv_obj_set_style_opa(ui_data.temp_label, LV_OPA_COVER, 0);
    
    // 立即调用一次更新以填充数据
    _update_ui();
    
    // 设置数据管理器状态
    data_manager_set_anim_state(false);
}

// 隐藏UI - 简化版
static void _hide_ui(void) {
    if (ui_data.panel) {
        lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
    }
}

// 删除UI - 增强安全性
static void _delete_ui(void) {
    if (ui_data.panel) {
        lv_obj_delete(ui_data.panel);
        ui_data.panel = NULL;
        
        // 确保清空所有指针，防止悬空引用
        for (int i = 0; i < CPU_CORES; i++) {
            ui_data.core_bars[i] = NULL;
            ui_data.core_labels[i] = NULL;
        }
        ui_data.temp_label = NULL;
    }
}
