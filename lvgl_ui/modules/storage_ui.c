#include "storage_ui.h"
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

// 添加这些缺失的类型定义
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
    bool first_update;        // 添加缺失的成员
} ui_cache = {
    .first_update = true      // 初始化为true
};

// 新增的静态动画控制变量
static lv_timer_t *spin_timer = NULL;
static lv_timer_t *active_arc_timer = NULL;  // 新增：激活状态进度条动画定时器
static int32_t current_spin_angle = 0;
static bool spin_animation_active = false;
static bool active_progress_mode = false;  // 新增：激活状态进度条模式标志

// 私有函数原型
static void _create_ui(lv_obj_t *parent);
static void _update_ui(void);
static void _show_ui(void);
static void _hide_ui(void);
static void _delete_ui(void);

// 添加这些函数的前向声明，解决隐式声明问题
static bool _needs_update(void);
static void _batch_fetch_all_data(storage_data_t *storage_data, memory_data_t *memory_data);

// 新增：spinner旋转动画回调
static void storage_spinner_animation_cb(lv_timer_t *timer) {
    // 获取当前值和目标值
    int32_t storage_current = lv_arc_get_value(ui_data.storage_arc);
    int32_t memory_current = lv_arc_get_value(ui_data.memory_arc);
    
    // 旋转角度增量 - 创造平滑的spinner效果
    current_spin_angle = (current_spin_angle + 3) % 360;
    
    // 存储圆弧从当前角度开始，扫过一定角度
    int32_t end_angle = current_spin_angle + 90; // 扫过90度的弧
    if (end_angle > 360) end_angle -= 360;
    
    // 设置存储圆弧
    lv_arc_set_start_angle(ui_data.storage_arc, current_spin_angle);
    lv_arc_set_end_angle(ui_data.storage_arc, end_angle);
    
    // 内存圆弧与存储圆弧相位差180度
    int32_t mem_start = (current_spin_angle + 180) % 360;
    int32_t mem_end = (mem_start + 90) % 360;
    if (mem_end < mem_start) mem_end += 360;
    
    // 设置内存圆弧
    lv_arc_set_start_angle(ui_data.memory_arc, mem_start);
    lv_arc_set_end_angle(ui_data.memory_arc, mem_end);
}

// 新增：激活状态进度条的动画回调 - 完全重写为流动效果
static void active_arc_animation_cb(lv_timer_t *timer) {
    // 获取当前真实进度值
    int32_t storage_value = ui_cache.storage_percent;
    int32_t memory_value = ui_cache.memory_percent;
    
    // 计算用于动画的角度
    current_spin_angle = (current_spin_angle + 3) % 360;
    
    // 计算存储和内存圆弧的角度
    int32_t storage_angle = (storage_value * 360) / 100;
    int32_t memory_angle = (memory_value * 360) / 100;
    
    // 创建围绕圆弧流动的效果 - 利用基准角度旋转整个进度条
    int32_t base_angle = current_spin_angle;
    
    // 存储进度条 - 起始角度随着基准角度旋转，但保持相同的弧长
    int32_t storage_start_angle = base_angle;
    int32_t storage_end_angle = (base_angle + storage_angle) % 360;
    
    lv_arc_set_angles(ui_data.storage_arc, storage_start_angle, storage_end_angle);
    
    // 内存进度条 - 相对存储进度条有180度偏移，制造交错效果
    int32_t memory_start_angle = (base_angle + 180) % 360;
    int32_t memory_end_angle = (memory_start_angle + memory_angle) % 360;
    
    lv_arc_set_angles(ui_data.memory_arc, memory_start_angle, memory_end_angle);
}

// 启动苹果风格spinner动画
static void start_spinner_animation() {
    if (!spin_animation_active) {
        // 设置圆弧的初始和结束角度
        lv_arc_set_bg_angles(ui_data.storage_arc, 0, 360);
        lv_arc_set_bg_angles(ui_data.memory_arc, 0, 360);
        
        // 重要：设置为旋转模式，让spinner正确显示
        lv_arc_set_mode(ui_data.storage_arc, LV_ARC_MODE_REVERSE);
        lv_arc_set_mode(ui_data.memory_arc, LV_ARC_MODE_REVERSE);
        
        // 初始化并启动动画定时器
        if (spin_timer == NULL) {
            spin_timer = lv_timer_create(storage_spinner_animation_cb, 16, NULL); // 约60FPS
        } else {
            lv_timer_resume(spin_timer);
        }
        
        spin_animation_active = true;
    }
}

// 修改：启动激活状态进度条模式
static void start_active_progress_animation() {
    if (!active_progress_mode) {
        // 设置圆弧为反向模式，让我们可以完全控制角度
        lv_arc_set_mode(ui_data.storage_arc, LV_ARC_MODE_REVERSE);
        lv_arc_set_mode(ui_data.memory_arc, LV_ARC_MODE_REVERSE);
        
        // 确保背景弧完整显示
        lv_arc_set_bg_angles(ui_data.storage_arc, 0, 360);
        lv_arc_set_bg_angles(ui_data.memory_arc, 0, 360);
        
        // 初始化并启动流动动画定时器，增加帧率使动画更流畅
        if (active_arc_timer == NULL) {
            active_arc_timer = lv_timer_create(active_arc_animation_cb, 16, NULL); // 约60FPS，更流畅
        } else {
            lv_timer_resume(active_arc_timer);
        }
        
        active_progress_mode = true;
        spin_animation_active = false;
    }
}

// 修改：停止spinner动画，转换到激活状态进度条模式
static void stop_spinner_animation() {
    if (spin_animation_active) {
        // 暂停spinner动画定时器
        if (spin_timer) {
            lv_timer_pause(spin_timer);
        }
        
        // 存储当前进度信息到缓存
        ui_cache.storage_percent = ui_cache.storage_percent;
        ui_cache.memory_percent = ui_cache.memory_percent;
        
        // 切换到激活状态进度条模式
        start_active_progress_animation();
    }
}

// 停止所有动画效果
static void stop_all_animations() {
    // 停止spinner动画
    if (spin_timer) {
        lv_timer_pause(spin_timer);
    }
    
    // 停止激活状态进度条动画
    if (active_arc_timer) {
        lv_timer_pause(active_arc_timer);
    }
    
    // 重置模式和角度
    spin_animation_active = false;
    active_progress_mode = false;
    
    // 恢复为正常进度条模式
    lv_arc_set_mode(ui_data.storage_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_mode(ui_data.memory_arc, LV_ARC_MODE_NORMAL);
    
    // 重置线宽到默认值
    lv_obj_set_style_arc_width(ui_data.storage_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_data.memory_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_data.storage_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ui_data.memory_arc, 8, LV_PART_INDICATOR);
    
    // 设置正确的进度值
    lv_arc_set_value(ui_data.storage_arc, ui_cache.storage_percent);
    lv_arc_set_value(ui_data.memory_arc, ui_cache.memory_percent);
}

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
    // 创建可复用的样式对象
    static lv_style_t panel_style;
    static lv_style_t arc_bg_style;
    static lv_style_t arc_indic_style;
    static lv_style_t title_style;
    static lv_style_t percent_style;
    static lv_style_t info_style;
    
    static bool styles_initialized = false;
    
    if(!styles_initialized) {
        lv_style_init(&panel_style);
        lv_style_set_bg_color(&panel_style, lv_color_hex(0x1E293B));         // 深靛蓝面板色
        lv_style_set_bg_grad_color(&panel_style, lv_color_hex(0x7C3AED));    // 深蓝黑色渐变终点
        lv_style_set_bg_grad_dir(&panel_style, LV_GRAD_DIR_VER);
        lv_style_set_border_width(&panel_style, 0);
        lv_style_set_radius(&panel_style, 16);
        lv_style_set_pad_all(&panel_style, 10);
        lv_style_set_shadow_width(&panel_style, 20);
        lv_style_set_shadow_spread(&panel_style, 4);
        lv_style_set_shadow_color(&panel_style, lv_color_hex(0x101010));
        lv_style_set_shadow_opa(&panel_style, LV_OPA_10);
        
        // 初始化其他样式...
        styles_initialized = true;
    }
    
    // 应用样式
    ui_data.panel = lv_obj_create(parent);
    lv_obj_set_size(ui_data.panel, 235, 130);
    lv_obj_align(ui_data.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(ui_data.panel, &panel_style, 0);
    // 在创建面板后添加
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);

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
    
    // 存储弧形进度条 - 左侧 - 修改为苹果风格
    ui_data.storage_arc = lv_arc_create(ui_data.panel);
    lv_obj_set_size(ui_data.storage_arc, 75, 75);
    lv_obj_align(ui_data.storage_arc, LV_ALIGN_LEFT_MID, 10, 0);
    lv_arc_set_rotation(ui_data.storage_arc, 270);
    lv_arc_set_bg_angles(ui_data.storage_arc, 0, 360);
    lv_arc_set_range(ui_data.storage_arc, 0, 100);
    lv_arc_set_value(ui_data.storage_arc, 0);
    lv_arc_set_mode(ui_data.storage_arc, LV_ARC_MODE_NORMAL); // 显式设置为正常模式
    lv_obj_remove_style(ui_data.storage_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(ui_data.storage_arc, 6, LV_PART_MAIN); // 使用更细的线条更像苹果风格
    lv_obj_set_style_arc_width(ui_data.storage_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.storage_arc, lv_color_hex(0x0F172A), LV_PART_MAIN); // 更深的背景色
    lv_obj_set_style_arc_color(ui_data.storage_arc, lv_color_hex(0x10B981), LV_PART_INDICATOR); // 翡翠绿指示器
    lv_obj_set_style_arc_rounded(ui_data.storage_arc, true, LV_PART_INDICATOR); // 苹果风格的圆角指示器
    
    // 内存弧形进度条 - 右侧 - 使用单色但鲜艳的颜色
    ui_data.memory_arc = lv_arc_create(ui_data.panel);
    lv_obj_set_size(ui_data.memory_arc, 75, 75);
    lv_obj_align(ui_data.memory_arc, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_arc_set_rotation(ui_data.memory_arc, 270);
    lv_arc_set_bg_angles(ui_data.memory_arc, 0, 360);
    lv_arc_set_range(ui_data.memory_arc, 0, 100);
    lv_arc_set_value(ui_data.memory_arc, 0);
    lv_arc_set_mode(ui_data.memory_arc, LV_ARC_MODE_NORMAL); // 显式设置为正常模式
    lv_obj_remove_style(ui_data.memory_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(ui_data.memory_arc, 6, LV_PART_MAIN); // 使用更细的线条更像苹果风格
    lv_obj_set_style_arc_width(ui_data.memory_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_data.memory_arc, lv_color_hex(0x0F172A), LV_PART_MAIN); // 更深的背景色
    lv_obj_set_style_arc_color(ui_data.memory_arc, lv_color_hex(0x8B5CF6), LV_PART_INDICATOR); // 优雅紫色指示器
    lv_obj_set_style_arc_rounded(ui_data.memory_arc, true, LV_PART_INDICATOR); // 圆角效果
    
    // 存储标题
    lv_obj_t *storage_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(storage_title, font_small, 0);
    lv_obj_set_style_text_color(storage_title, lv_color_hex(color_text), 0);
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
    lv_obj_set_style_text_color(ui_data.info_label, lv_color_hex(color_text), 0);
    lv_label_set_text(ui_data.info_label, "0 / 0");
    lv_obj_align_to(ui_data.info_label, ui_data.storage_arc, LV_ALIGN_OUT_BOTTOM_MID, -25, 0);
    
    // 内存标题
    lv_obj_t *memory_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(memory_title, font_small, 0);
    lv_obj_set_style_text_color(memory_title, lv_color_hex(color_text), 0);
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
    lv_obj_set_style_text_color(ui_data.memory_info_label, lv_color_hex(color_text), 0);
    lv_label_set_text(ui_data.memory_info_label, "0 / 0");
    lv_obj_align_to(ui_data.memory_info_label, ui_data.memory_arc, LV_ALIGN_OUT_BOTTOM_MID, -30, 0);
    
    // 优化环形图视觉效果
    lv_obj_set_style_arc_rounded(ui_data.storage_arc, true, LV_PART_INDICATOR); // 圆角进度条
    lv_obj_set_style_arc_rounded(ui_data.memory_arc, true, LV_PART_INDICATOR);
    
    // 初始时隐藏面板
    lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
}

// 苹果风格的性能优化 - 主线程阻塞时间极短
static void _update_ui(void) {
    // 预先检查是否需要更新，避免不必要的UI刷新
    if (!_needs_update() && !ui_cache.first_update) {
        return; // 跳过不必要的更新
    }
    
    // 批量获取所有数据，减少函数调用开销
    storage_data_t storage_data;
    memory_data_t memory_data;
    _batch_fetch_all_data(&storage_data, &memory_data);
    
    // 更新存储进度条和标签
    lv_arc_set_value(ui_data.storage_arc, storage_data.percent);
    lv_label_set_text_fmt(ui_data.percent_label, "%d%%", storage_data.percent);
    
    // 更新存储信息标签
    ui_utils_size_to_str(storage_data.used, ui_cache.used_str, sizeof(ui_cache.used_str));
    ui_utils_size_to_str(storage_data.total, ui_cache.total_str, sizeof(ui_cache.total_str));
    lv_label_set_text_fmt(ui_data.info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    
    // 更新内存进度条和标签
    lv_arc_set_value(ui_data.memory_arc, memory_data.percent);
    lv_label_set_text_fmt(ui_data.memory_percent_label, "%d%%", memory_data.percent);
    
    // 更新内存信息标签
    ui_utils_size_to_str(memory_data.used, ui_cache.used_str, sizeof(ui_cache.used_str));
    ui_utils_size_to_str(memory_data.total, ui_cache.total_str, sizeof(ui_cache.total_str));
    lv_label_set_text_fmt(ui_data.memory_info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    
    // 更新缓存状态
    ui_cache.storage_percent = storage_data.percent;
    ui_cache.storage_used = storage_data.used;
    ui_cache.storage_total = storage_data.total;
    ui_cache.memory_percent = memory_data.percent;
    ui_cache.memory_used = memory_data.used;
    ui_cache.memory_total = memory_data.total;
    
    // 重置首次更新标志
    ui_cache.first_update = false;
}

// 检查是否需要更新UI的实现
static bool _needs_update(void) {
    // 检查是否需要更新UI
    return data_manager_has_changes();
}

// 批量获取数据的实现
static void _batch_fetch_all_data(storage_data_t *storage_data, memory_data_t *memory_data) {
    // 批量获取各种数据
    data_manager_get_storage(&storage_data->used, &storage_data->total, &storage_data->percent);
    data_manager_get_memory(&memory_data->used, &memory_data->total, &memory_data->percent);
}

// 显示UI - 添加苹果风格的交错进入动画
static void _show_ui(void) {
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
    
    // 获取当前实际数据
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
    
    // 更新显示标签
    lv_label_set_text_fmt(ui_data.percent_label, "%d%%", storage_percent);
    ui_utils_size_to_str(storage_used, ui_cache.used_str, sizeof(ui_cache.used_str));
    ui_utils_size_to_str(storage_total, ui_cache.total_str, sizeof(ui_cache.total_str));
    lv_label_set_text_fmt(ui_data.info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    
    lv_label_set_text_fmt(ui_data.memory_percent_label, "%d%%", memory_percent);
    ui_utils_size_to_str(memory_used, ui_cache.used_str, sizeof(ui_cache.used_str));
    ui_utils_size_to_str(memory_total, ui_cache.total_str, sizeof(ui_cache.total_str));
    lv_label_set_text_fmt(ui_data.memory_info_label, "%s / %s", ui_cache.used_str, ui_cache.total_str);
    
    // 先显示spinner效果0.5秒钟，更快切换到流动效果
    start_spinner_animation();
    
    // 0.5秒后停止spinner并切换到激活状态进度条模式
    lv_timer_t *spinner_to_active_progress_timer = lv_timer_create(
        (lv_timer_cb_t)stop_spinner_animation, 
        500, // 0.5秒后切换到激活状态进度条显示
        NULL
    );
    lv_timer_set_repeat_count(spinner_to_active_progress_timer, 1);
    
    // 创建结束动画事件的计时器 - 不再需要停止动画，保持激活状态
    lv_timer_t *anim_end_timer = lv_timer_create(
        (lv_timer_cb_t)data_manager_set_anim_state, 
        1000, // 1秒后结束动画状态
        (void *)false
    );
    lv_timer_set_repeat_count(anim_end_timer, 1);
}

// 隐藏UI
static void _hide_ui(void) {
    lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
}

// 修改：删除UI时清理所有动画资源
static void _delete_ui(void) {
    if (ui_data.panel) {
        // 清理spinner定时器
        if (spin_timer) {
            lv_timer_delete(spin_timer);
            spin_timer = NULL;
        }
        
        // 清理激活状态进度条定时器
        if (active_arc_timer) {
            lv_timer_delete(active_arc_timer);
            active_arc_timer = NULL;
        }
        
        lv_obj_delete(ui_data.panel);
        ui_data.panel = NULL;
    }
}
