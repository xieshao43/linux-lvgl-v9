#include "ui_utils.h"
#include <stdio.h>

// 添加这些函数声明到文件顶部
static void _ui_utils_hide_arcs_show_bars_cb(lv_timer_t *timer);
static void _ui_utils_cleanup_animation_cb(lv_timer_t *timer);

// 将KB大小转换为可读字符串 - 改编自原有get_size_str函数
void ui_utils_size_to_str(uint64_t size_kb, char *buf, int buf_size) {
    const char *units[] = {"KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = (double)size_kb;
    
    while (size >= 1024 && unit_idx < 3) {
        size /= 1024;
        unit_idx++;
    }
    
    if (size < 10) {
        snprintf(buf, buf_size, "%.1f %s", size, units[unit_idx]);
    } else {
        snprintf(buf, buf_size, "%.0f %s", size, units[unit_idx]);
    }
}

// 创建带过渡动画的环形图 - 改进为苹果流畅风格
void ui_utils_animate_arc(lv_obj_t *arc, int32_t start_value, int32_t end_value, uint16_t time_ms) {
    if (!arc) return;
    
    // 计算动画时长 - 根据变化量调整
    uint16_t anim_time = time_ms;
    if (LV_ABS(end_value - start_value) < 10) {
        // 变化较小时，使用更短的动画时间
        anim_time = time_ms * 0.6;
    }
    else if (LV_ABS(end_value - start_value) > 50) {
        // 变化较大时，稍微延长动画时间
        anim_time = time_ms * 1.2;
    }
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_values(&a, start_value, end_value);
    lv_anim_set_time(&a, anim_time);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_arc_set_value);
    
    // 使用苹果风格的缓动函数 - 缓入缓出更自然
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    
    lv_anim_start(&a);
}

// 简化的进度条动画函数
void ui_utils_animate_bar(lv_obj_t *bar, int32_t start_value, int32_t end_value, uint16_t time_ms) {
    if (!bar) return;
    
    // 如果起始值为0，避免动画，直接设置
    if (start_value == 0) {
        lv_bar_set_value(bar, end_value, LV_ANIM_OFF);
        return;
    }
    
    // 如果变化太小，也直接设置
    if (LV_ABS(end_value - start_value) < 3) {
        lv_bar_set_value(bar, end_value, LV_ANIM_OFF);
        return;
    }
    
    // 标准动画，使用简单的动画方法
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, start_value, end_value);
    lv_anim_set_time(&a, time_ms);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// 添加新的缓动函数 - 苹果风格的弹性回弹
int32_t ui_utils_anim_path_overshoot(const lv_anim_t * a)
{
    /*计算当前步骤（0-1之间的值）*/
    int32_t t = lv_map(a->act_time, 0, a->duration, 0, 1024);
    
    // 弹性公式：先超出目标然后回弹
    if(t < 512) { // 前半段动画快速接近并超过目标
        return lv_bezier3(t * 2, 0, 100, 1100, 1024);
    }
    else { // 后半段回弹到目标
        t = t - 512;
        int32_t step = (((t * 400) >> 10) + 180) % 360;
        int32_t value = 1024 + lv_trigo_sin(step) * 50 / LV_TRIGO_SIN_MAX;
        return value;
    }
}

/**
 * 创建从圆弧进度条到水平进度条的过渡动画
 * @note 此动画将创建临时对象并在动画结束后删除
 */
void ui_utils_create_transition_animation(void)
{
    // 获取当前活动屏幕
    lv_obj_t *parent = lv_scr_act();
    
    // 创建主面板 - 与其他面板保持一致
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 235, 130);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_PANEL_DARK), 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    
    // 创建圆弧进度条 - 内存 (与storage_ui匹配)
    lv_obj_t *arc_mem = lv_arc_create(panel);
    lv_obj_set_size(arc_mem, 75, 75);
    lv_obj_align(arc_mem, LV_ALIGN_LEFT_MID, 10, 0);
    lv_arc_set_rotation(arc_mem, 270);
    lv_arc_set_bg_angles(arc_mem, 0, 360);
    lv_arc_set_range(arc_mem, 0, 100);
    lv_arc_set_value(arc_mem, 0);
    lv_obj_remove_style(arc_mem, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc_mem, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_mem, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_mem, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_mem, lv_color_hex(0x4A5568), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_mem, true, LV_PART_INDICATOR);
    
    // 创建圆弧进度条 - 存储 (与storage_ui匹配)
    lv_obj_t *arc_storage = lv_arc_create(panel);
    lv_obj_set_size(arc_storage, 75, 75);
    lv_obj_align(arc_storage, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_arc_set_rotation(arc_storage, 270);
    lv_arc_set_bg_angles(arc_storage, 0, 360);
    lv_arc_set_range(arc_storage, 0, 100);
    lv_arc_set_value(arc_storage, 0);
    lv_obj_remove_style(arc_storage, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc_storage, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_storage, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_storage, lv_color_hex(COLOR_SECONDARY), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_storage, lv_color_hex(0x4A5568), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_storage, true, LV_PART_INDICATOR);

    // 使用与CPU UI相同的进度条设置
    lv_obj_t *bars[4];
    int bar_height = 6;         // 与CPU UI一致的高度
    int bar_width = 150;        // 与CPU UI一致的宽度
    int vertical_spacing = 22;  // 与CPU UI一致的垂直间距
    int start_y = 20;           // 与CPU UI一致的起始Y坐标
    int left_margin = 30;       // 与CPU UI一致的左侧边距
    
    // 核心颜色数组 - 从CPU UI复制
    uint32_t color_core_colors[4] = {
        0x06B6D4,  // 青色 - Core 0
        0x8B5CF6,  // 紫色 - Core 1
        0x10B981,  // 绿色 - Core 2
        0xF59E0B   // 琥珀色 - Core 3
    };
    
    for(int i = 0; i < 4; i++) {
        bars[i] = lv_bar_create(panel);
        lv_obj_set_size(bars[i], bar_width, bar_height);
        lv_obj_align(bars[i], LV_ALIGN_TOP_LEFT, left_margin, start_y + i * vertical_spacing);
        lv_bar_set_range(bars[i], 0, 100);
        lv_bar_set_value(bars[i], 0, LV_ANIM_OFF);
        lv_obj_add_flag(bars[i], LV_OBJ_FLAG_HIDDEN);
        
        // 背景设置为半透明 - 与CPU UI保持一致
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(0x374151), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bars[i], LV_OPA_40, LV_PART_MAIN);
        
        // 指示器使用渐变效果 - 与CPU UI保持一致
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(color_core_colors[i]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(bars[i], lv_color_lighten(lv_color_hex(color_core_colors[i]), 30), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(bars[i], LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
        
        // 圆角进度条 - 与CPU UI保持一致
        lv_obj_set_style_radius(bars[i], bar_height / 2, 0);
    }
    
    // 为存储这些对象的指针创建结构
    static lv_obj_t *objects[7];  // 1个面板 + 2个圆弧 + 4个进度条
    objects[0] = panel;
    objects[1] = arc_mem;
    objects[2] = arc_storage;
    for(int i = 0; i < 4; i++) {
        objects[i+3] = bars[i];
    }
    
    // 动画1: 圆弧进度条填充动画
    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, arc_mem);
    lv_anim_set_values(&a1, 0, 100);
    lv_anim_set_exec_cb(&a1, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_duration(&a1, 1200);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
    lv_anim_start(&a1);
    
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, arc_storage);
    lv_anim_set_values(&a2, 0, 100);
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_duration(&a2, 1200);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_start(&a2);
    
    // 设置延时定时器，在圆弧动画完成后转换到进度条
    lv_timer_t *transition_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_hide_arcs_show_bars_cb, 
        1300, 
        objects
    );
    lv_timer_set_repeat_count(transition_timer, 1);
}

// 修改回调函数以适应新的对象结构
static void _ui_utils_hide_arcs_show_bars_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    lv_obj_t *panel = objs[0];
    
    // 隐藏圆弧
    lv_obj_add_flag(objs[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objs[2], LV_OBJ_FLAG_HIDDEN);
    
    // 显示并动画进度条 - 保持原有动画逻辑
    for(int i = 0; i < 4; i++) {
        lv_obj_clear_flag(objs[i+3], LV_OBJ_FLAG_HIDDEN);
        
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, objs[i+3]);
        lv_anim_set_values(&a, 0, 25 * (i+1)); // 25%, 50%, 75%, 100%
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
        lv_anim_set_duration(&a, 800);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_start(&a);
    }
    
    // 最后清理动画的定时器
    lv_timer_t *cleanup_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_cleanup_animation_cb, 
        1800, 
        objs
    );
    lv_timer_set_repeat_count(cleanup_timer, 1);
}

// 更新清理函数以删除所有对象
static void _ui_utils_cleanup_animation_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    // 只需删除面板，它会自动删除所有子对象
    lv_obj_delete(objs[0]);
}
