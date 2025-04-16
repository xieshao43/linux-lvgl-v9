#include "ui_utils.h"
#include <stdio.h>

// 添加正确的函数声明
static void _ui_utils_hide_arcs_show_bars_cb(lv_timer_t *timer);
static void _ui_utils_progress_bars_transition_to_cpu_cb(lv_timer_t *timer);  // 修改函数名
static void _ui_utils_fade_out_animation_cb(lv_timer_t *timer);
static void _ui_utils_cleanup_animation_cb(lv_timer_t *timer);
static void _ui_utils_hide_bars_show_arcs_cb(lv_timer_t *timer);
static void _ui_utils_show_arcs_fill_cb(lv_timer_t *timer);
static void _ui_anim_opacity_cb(void *obj, int32_t value);
static void _ui_anim_width_cb(void *obj, int32_t value);
static void _ui_anim_delete_on_complete(lv_anim_t *a);
// 添加新的回调函数声明
static void _ui_utils_arcs_shrink_to_actual_cb(lv_timer_t *timer);

// 添加获取CPU占用率的外部声明
extern void data_manager_get_cpu_core(uint8_t core_index, float *usage);
extern void data_manager_get_storage(uint64_t *used, uint64_t *total, uint8_t *percent);
extern void data_manager_get_memory(uint64_t *used, uint64_t *total, uint8_t *percent);

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
    // 苹果春动算法调整 - 精确模拟弹簧物理模型
    int32_t t = lv_map(a->act_time, 0, a->duration, 0, 1024);
    
    // 优化算法使过冲更加自然，更接近iOS的感觉
    if(t < 512) { 
        return lv_bezier3(t * 2, 0, 80, 1000, 1024); // 降低初始加速度
    }
    else { 
        t = t - 512;
        // 调整回弹振幅和衰减，更加自然
        int32_t step = (((t * 350) >> 10) + 180) % 360;
        int32_t value = 1024 + lv_trigo_sin(step) * 40 / LV_TRIGO_SIN_MAX;
        return value;
    }
}

/**
 * 创建从圆弧进度条到水平进度条的过渡动画
 * @note 此动画将创建临时对象并在动画结束后删除
 */
void ui_utils_create_transition_animation(void)
{
    // 使用更高效的方式隐藏光标（比system调用高效）
    static bool cursor_hidden = false;
    if (!cursor_hidden) {
        int fd = open("/dev/tty1", O_WRONLY);
        if (fd >= 0) {
            write(fd, "\033[?25l", 6);
            close(fd);
            cursor_hidden = true;
           }
       }

    // 获取当前活动屏幕
    lv_obj_t *parent = lv_scr_act();
    
    // 创建主面板 - 与其他面板保持一致
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 235, 130);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1E293B), 0); // 深靛蓝面板色
    lv_obj_set_style_bg_grad_color(panel, lv_color_hex(0x7C3AED), 0); // 深蓝黑色渐变终点
    lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    
    // 创建圆弧进度条 - 存储 (位置交换到左侧)
    lv_obj_t *arc_storage = lv_arc_create(panel);
    lv_obj_set_size(arc_storage, 75, 75);
    lv_obj_align(arc_storage, LV_ALIGN_LEFT_MID, 10, 0);
    lv_arc_set_rotation(arc_storage, 270);
    lv_arc_set_bg_angles(arc_storage, 0, 360);
    lv_arc_set_range(arc_storage, 0, 100);
    lv_arc_set_value(arc_storage, 100);  // 初始化为满状态
    lv_obj_remove_style(arc_storage, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc_storage, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_storage, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_storage, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_storage, lv_color_hex(0x10B981), LV_PART_INDICATOR); // 翡翠绿指示器
    lv_obj_set_style_arc_rounded(arc_storage, true, LV_PART_INDICATOR);
    
    // 创建圆弧进度条 - 内存 (位置交换到右侧)
    lv_obj_t *arc_mem = lv_arc_create(panel);
    lv_obj_set_size(arc_mem, 75, 75);
    lv_obj_align(arc_mem, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_arc_set_rotation(arc_mem, 270);
    lv_arc_set_bg_angles(arc_mem, 0, 360);
    lv_arc_set_range(arc_mem, 0, 100);
    lv_arc_set_value(arc_mem, 100);  // 初始化为满状态
    lv_obj_remove_style(arc_mem, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc_mem, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_mem, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_mem, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_mem, lv_color_hex(0x8B5CF6), LV_PART_INDICATOR); // 紫色指示器
    lv_obj_set_style_arc_rounded(arc_mem, true, LV_PART_INDICATOR);

    // 使用与CPU UI相同的进度条设置
    lv_obj_t *bars[4];
    int bar_height = 6;
    int bar_width = 150;
    int vertical_spacing = 22;
    int start_y = 20;
    int left_margin = 30;
    
    // 核心颜色数组 - 使用更新后的单色系渐变
    uint32_t color_core_colors[4] = {
        0x0284C7,  // 蓝色基础色 - Core 0
        0x8B5CF6,  // 紫色基础色 - Core 1
        0x10B981,  // 绿色基础色 - Core 2
        0xF59E0B   // 橙色基础色 - Core 3
    };
    
    for(int i = 0; i < 4; i++) {
        bars[i] = lv_bar_create(panel);
        lv_obj_set_size(bars[i], bar_width, bar_height);
        lv_obj_align(bars[i], LV_ALIGN_TOP_LEFT, left_margin, start_y + i * vertical_spacing);
        lv_bar_set_range(bars[i], 0, 100);
        lv_bar_set_value(bars[i], 0, LV_ANIM_OFF);
        lv_obj_add_flag(bars[i], LV_OBJ_FLAG_HIDDEN);
        
        // 设置样式
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(0x374151), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bars[i], LV_OPA_40, LV_PART_MAIN);
        
        // 使用同色系渐变效果 - 修复颜色处理方式
        lv_color_t light_color = lv_color_lighten(lv_color_hex(color_core_colors[i]), 40);
        lv_color_t dark_color = lv_color_darken(lv_color_hex(color_core_colors[i]), 20);
        
        lv_obj_set_style_bg_color(bars[i], light_color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(bars[i], dark_color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(bars[i], LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
        
        lv_obj_set_style_radius(bars[i], bar_height / 2, 0);
    }
    
    // 创建CPU MONITOR标题 - 初始隐藏
    lv_obj_t *title_label = lv_label_create(panel);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);  // 使用中等大小字体
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xF9FAFB), 0); // 近白色文本
    lv_label_set_text(title_label, "CPU MONITOR");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, -5);
    lv_obj_set_style_opa(title_label, LV_OPA_TRANSP, 0);  // 初始设为完全透明
    
    // 为存储对象指针创建数组
    static lv_obj_t *objects[8]; // 增加一个位置存储标题
    objects[0] = panel;
    objects[1] = arc_mem;
    objects[2] = arc_storage;
    for(int i = 0; i < 4; i++) {
        objects[i+3] = bars[i];
    }
    objects[7] = title_label;  // 存储标题标签
    
    // 动画1: 圆弧进度条从满状态回退动画
    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, arc_mem);
    lv_anim_set_values(&a1, 100, 0);  // 从100%回退到0%
    lv_anim_set_exec_cb(&a1, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_duration(&a1, 1200);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_in_out);  // 使用缓入缓出效果
    lv_anim_start(&a1);
    
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, arc_storage);
    lv_anim_set_values(&a2, 100, 0);  // 从100%回退到0%
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_duration(&a2, 1200);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_in_out);  // 使用缓入缓出效果
    lv_anim_start(&a2);
    
    // 设置延时定时器，在圆弧动画完成后转换到进度条
    // 稍微提前开始过渡，让动画有更好的重叠效果
    lv_timer_t *transition_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_hide_arcs_show_bars_cb, 
        1100, 
        objects
    );
    lv_timer_set_repeat_count(transition_timer, 1);
}

// 修改回调函数以适应新的对象结构
static void _ui_utils_hide_arcs_show_bars_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    lv_obj_t *title_label = objs[7];  // 获取标题标签
    
    // 为圆弧添加淡出动画
    for (int i = 1; i <= 2; i++) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, objs[i]);
        lv_anim_set_values(&a, lv_obj_get_style_opa(objs[i], 0), LV_OPA_TRANSP);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_ui_anim_opacity_cb);
        lv_anim_set_time(&a, 300);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }
    
    // 标题标签淡入动画 - 苹果风格的流畅出现
    lv_anim_t a_title;
    lv_anim_init(&a_title);
    lv_anim_set_var(&a_title, title_label);
    lv_anim_set_values(&a_title, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a_title, (lv_anim_exec_xcb_t)_ui_anim_opacity_cb);
    lv_anim_set_time(&a_title, 600);  // 稍微慢一点的淡入效果更优雅
    lv_anim_set_delay(&a_title, 150); // 在进度条显示前稍微提前出现
    lv_anim_set_path_cb(&a_title, lv_anim_path_ease_out);
    lv_anim_start(&a_title);
    
    // 依次显示进度条并添加动画效果
    for(int i = 0; i < 4; i++) {
        // 设置初始状态
        lv_obj_clear_flag(objs[i+3], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(objs[i+3], LV_OPA_TRANSP, 0);  // 初始透明
        lv_obj_set_width(objs[i+3], 10);  // 初始宽度较小
        
        // 透明度动画
        lv_anim_t a_opa;
        lv_anim_init(&a_opa);
        lv_anim_set_var(&a_opa, objs[i+3]);
        lv_anim_set_values(&a_opa, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a_opa, (lv_anim_exec_xcb_t)_ui_anim_opacity_cb);
        lv_anim_set_time(&a_opa, 400);
        lv_anim_set_delay(&a_opa, i * 80);  // 交错延迟
        lv_anim_set_path_cb(&a_opa, lv_anim_path_ease_out);
        lv_anim_start(&a_opa);
        
        // 宽度动画 - 从小到正常大小
        lv_anim_t a_width;
        lv_anim_init(&a_width);
        lv_anim_set_var(&a_width, objs[i+3]);
        lv_anim_set_values(&a_width, 10, 150);  // 从10到150的宽度
        lv_anim_set_exec_cb(&a_width, (lv_anim_exec_xcb_t)_ui_anim_width_cb);
        lv_anim_set_time(&a_width, 500);
        lv_anim_set_delay(&a_width, i * 80);
        lv_anim_set_path_cb(&a_width, lv_anim_path_ease_out);
        lv_anim_start(&a_width);
        
        // 进度条值动画
        lv_anim_t a_value;
        lv_anim_init(&a_value);
        lv_anim_set_var(&a_value, objs[i+3]);
        lv_anim_set_values(&a_value, 0, 25 * (i+1));  // 25%, 50%, 75%, 100%
        lv_anim_set_exec_cb(&a_value, (lv_anim_exec_xcb_t)lv_bar_set_value);
        lv_anim_set_time(&a_value, 800);
        lv_anim_set_delay(&a_value, 200 + i * 80);  // 延迟开始，等宽度和透明度动画先开始
        lv_anim_set_path_cb(&a_value, lv_anim_path_overshoot);
        lv_anim_start(&a_value);
    }
    
    // 设置延时定时器，在进度条动画完成后淡出所有元素
    lv_timer_t *fade_out_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_progress_bars_transition_to_cpu_cb,  // 修改回调函数
        1600,  // 给进度条足够时间显示
        objs
    );
    lv_timer_set_repeat_count(fade_out_timer, 1);
}

// 添加新的回调函数，实现水平进度条的渐出效果
static void _ui_utils_progress_bars_transition_to_cpu_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    lv_obj_t *title_label = objs[7];  // 获取标题标签
    
    // 获取实际的CPU核心占用率
    float core_usage[4];
    for (int i = 0; i < 4; i++) {
        data_manager_get_cpu_core(i, &core_usage[i]);
        
        // 确保最小显示值，避免进度条看起来为空
        if (core_usage[i] < 5.0f) {
            core_usage[i] = 5.0f;
        }
    }
    
    // 为每个进度条创建从当前值(100%,75%等)平滑过渡到实际值的动画
    for(int i = 0; i < 4; i++) {
        int32_t current_value = 25 * (i+1); // 当前值是25%, 50%, 75%, 100%
        int32_t target_value = (int32_t)core_usage[i]; // 目标值是实际CPU占用率
        
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, objs[i+3]);
        lv_anim_set_values(&a, current_value, target_value);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
        lv_anim_set_time(&a, 600);  // 使用稍短的时间更自然
        lv_anim_set_delay(&a, i * 50);  // 依次过渡，创造连续感
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); // 使用缓入缓出效果更自然
        lv_anim_start(&a);
        
        // 更新进度条标签到实际值
        lv_obj_t *bar_label = lv_obj_get_child(objs[0], -1-(4-i)); // 获取对应的标签(如果存在)
        if (bar_label && lv_obj_check_type(bar_label, &lv_label_class)) {
            lv_label_set_text_fmt(bar_label, "%d%%", target_value);
        }
    }
    
    // 清理定时器 - 给足够时间显示实际值后再删除面板
    lv_timer_t *cleanup_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_fade_out_animation_cb, 
        1200,  // 给过渡动画足够时间
        objs
    );
    lv_timer_set_repeat_count(cleanup_timer, 1);
}

// 添加辅助动画回调函数 - 透明度
static void _ui_anim_opacity_cb(void *obj, int32_t value) {
    lv_obj_set_style_opa(obj, value, 0);
}

// 添加辅助动画回调函数 - 宽度
static void _ui_anim_width_cb(void *obj, int32_t value) {
    lv_obj_set_width(obj, value);
}

// 更新清理函数以删除所有对象
static void _ui_utils_cleanup_animation_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    // 只需删除面板，它会自动删除所有子对象
    lv_obj_delete(objs[0]);
}

/**
 * 创建从水平进度条到圆弧进度条的反向过渡动画
 * @note 此动画与create_transition_animation相反
 */
void ui_utils_create_reverse_transition_animation(void)
{
    
    // 获取当前活动屏幕
    lv_obj_t *parent = lv_scr_act();
    
    // 创建主面板 - 统一渐变效果
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 235, 130);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1E293B), 0); // 深靛蓝面板色
    lv_obj_set_style_bg_grad_color(panel, lv_color_hex(0x7C3AED), 0); // 深蓝黑色渐变终点
    lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    
    // 创建圆弧进度条 - 存储 
    lv_obj_t *arc_storage = lv_arc_create(panel);
    lv_obj_set_size(arc_storage, 75, 75);
    lv_obj_align(arc_storage, LV_ALIGN_LEFT_MID, 10, 0);
    lv_arc_set_rotation(arc_storage, 270);
    lv_arc_set_bg_angles(arc_storage, 0, 360);
    lv_arc_set_range(arc_storage, 0, 100);
    lv_arc_set_value(arc_storage, 0);  // 初始为0
    lv_obj_remove_style(arc_storage, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc_storage, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_storage, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_storage, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_storage, lv_color_hex(0x10B981), LV_PART_INDICATOR); // 翡翠绿指示器
    lv_obj_set_style_arc_rounded(arc_storage, true, LV_PART_INDICATOR);
    lv_obj_set_style_opa(arc_storage, LV_OPA_TRANSP, 0); // 初始透明
    
    // 创建圆弧进度条 - 内存 
    lv_obj_t *arc_mem = lv_arc_create(panel);
    lv_obj_set_size(arc_mem, 75, 75);
    lv_obj_align(arc_mem, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_arc_set_rotation(arc_mem, 270);
    lv_arc_set_bg_angles(arc_mem, 0, 360);
    lv_arc_set_range(arc_mem, 0, 100);
    lv_arc_set_value(arc_mem, 0);  // 初始为0
    lv_obj_remove_style(arc_mem, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc_mem, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_mem, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_mem, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_mem, lv_color_hex(0x8B5CF6), LV_PART_INDICATOR); // 紫色指示器
    lv_obj_set_style_arc_rounded(arc_mem, true, LV_PART_INDICATOR);
    lv_obj_set_style_opa(arc_mem, LV_OPA_TRANSP, 0); // 初始透明

    // 创建水平进度条
    lv_obj_t *bars[4];
    int bar_height = 6;
    int bar_width = 150;
    int vertical_spacing = 22;
    int start_y = 20;
    int left_margin = 30;
    
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
        lv_bar_set_value(bars[i], 0, LV_ANIM_OFF); // 初始值为0
        
        // 设置样式
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(0x374151), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bars[i], LV_OPA_40, LV_PART_MAIN);
        
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(color_core_colors[i]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(bars[i], lv_color_lighten(lv_color_hex(color_core_colors[i]), 30), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(bars[i], LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
        
        lv_obj_set_style_radius(bars[i], bar_height / 2, 0);
    }
    
    // 为存储对象指针创建数组
    static lv_obj_t *objects[7];
    objects[0] = panel;
    objects[1] = arc_mem;
    objects[2] = arc_storage;
    for(int i = 0; i < 4; i++) {
        objects[i+3] = bars[i];
    }
    
    // 第一阶段：进度条从0到100%的动画
    for(int i = 0; i < 4; i++) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bars[i]);
        lv_anim_set_values(&a, 0, 100); // 从0到100%
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
        lv_anim_set_time(&a, 800);  // 修改为800ms，与正向动画一致
        lv_anim_set_delay(&a, i * 80); // 修改为80ms间隔，与正向动画一致
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }
    
    // 设置第二阶段：进度条到0并淡出，圆弧淡入并填充
    lv_timer_t *phase2_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_hide_bars_show_arcs_cb, 
        1100, // 给够足够时间
        objects
    );
    lv_timer_set_repeat_count(phase2_timer, 1);
}

// 优化水平进度条到圆弧进度条的过渡动画
static void _ui_utils_hide_bars_show_arcs_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    
    // 进度条从100%回落到0%并淡出 - 优化为更加平滑的过渡
    for(int i = 0; i < 4; i++) {
        // 值动画 - 从100%到0%
        lv_anim_t a_value;
        lv_anim_init(&a_value);
        lv_anim_set_var(&a_value, objs[i+3]);
        lv_anim_set_values(&a_value, 100, 0);
        lv_anim_set_exec_cb(&a_value, (lv_anim_exec_xcb_t)lv_bar_set_value);
        lv_anim_set_time(&a_value, 700); // 稍微缩短时间，加快过渡
        lv_anim_set_delay(&a_value, i * 60); // 缩短间隔，让连续感更强
        lv_anim_set_path_cb(&a_value, lv_anim_path_ease_in_out);
        lv_anim_start(&a_value);
        
        // 透明度动画 - 淡出 - 调整时间与值变化保持更好的同步
        lv_anim_t a_opa;
        lv_anim_init(&a_opa);
        lv_anim_set_var(&a_opa, objs[i+3]);
        lv_anim_set_values(&a_opa, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_exec_cb(&a_opa, (lv_anim_exec_xcb_t)_ui_anim_opacity_cb);
        lv_anim_set_time(&a_opa, 350); // 略微延长透明度过渡
        lv_anim_set_delay(&a_opa, 150 + i * 60); // 调整延迟，让值和透明度变化更协调
        lv_anim_set_path_cb(&a_opa, lv_anim_path_ease_out);
        lv_anim_start(&a_opa);
    }
    
    // 延迟显示并填充圆弧 - 提前开始以便与进度条淡出重叠
    lv_timer_t *arcs_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_show_arcs_fill_cb, 
        400, // 缩短等待时间，让圆弧更早出现，增加过渡的连续性
        objs
    );
    lv_timer_set_repeat_count(arcs_timer, 1);
}

// 显示并填充圆弧的回调 - 改进动画效果
static void _ui_utils_show_arcs_fill_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    
    // 圆弧淡入 - 更加渐进
    for (int i = 1; i <= 2; i++) {
        // 透明度动画 - 淡入
        lv_anim_t a_opa;
        lv_anim_init(&a_opa);
        lv_anim_set_var(&a_opa, objs[i]);
        lv_anim_set_values(&a_opa, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a_opa, (lv_anim_exec_xcb_t)_ui_anim_opacity_cb);
        lv_anim_set_time(&a_opa, 900); // 缩短淡入时间，加快圆弧出现
        lv_anim_set_delay(&a_opa, (i-1) * 60); // 缩短延迟间隔
        lv_anim_set_path_cb(&a_opa, lv_anim_path_ease_out);
        lv_anim_start(&a_opa);
        
        // 圆弧值动画 - 从0到100%
        lv_anim_t a_value;
        lv_anim_init(&a_value);
        lv_anim_set_var(&a_value, objs[i]);
        lv_anim_set_values(&a_value, 0, 100);
        lv_anim_set_exec_cb(&a_value, (lv_anim_exec_xcb_t)lv_arc_set_value);
        lv_anim_set_time(&a_value, 900); // 缩短填充动画时间
        lv_anim_set_delay(&a_value, 100 + (i-1) * 60); // 调整延迟时间
        lv_anim_set_path_cb(&a_value, lv_anim_path_overshoot); 
        lv_anim_start(&a_value);
        
        // 修复：确保透明度在动画结束时为完全可见
        lv_obj_set_style_opa(objs[i], LV_OPA_COVER, 0);
    }
    
    // 设置计时器，在圆弧填满后收缩到实际值
    lv_timer_t *shrink_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_arcs_shrink_to_actual_cb, 
        1000,  // 缩短等待时间，让收缩动画更早开始
        objs
    );
    lv_timer_set_repeat_count(shrink_timer, 1);
}

// 从末端收缩圆弧到实际值
static void _ui_utils_arcs_shrink_to_actual_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    lv_obj_t *arc_mem = objs[1];    // 内存圆弧
    lv_obj_t *arc_storage = objs[2]; // 存储圆弧
    
    // 获取实际的存储和内存使用率
    uint64_t storage_used, storage_total;
    uint64_t memory_used, memory_total;
    uint8_t storage_percent, memory_percent;
    
    // 获取存储使用率
    data_manager_get_storage(&storage_used, &storage_total, &storage_percent);
    
    // 获取内存使用率
    data_manager_get_memory(&memory_used, &memory_total, &memory_percent);
    
    // 确保百分比数值在有意义的范围内
    if (storage_percent < 5) storage_percent = 5;
    if (memory_percent < 5) memory_percent = 5;
    
    // 设置圆弧的背景角度范围和起始位置
    lv_arc_set_bg_angles(arc_mem, 0, 360); // 设置为完整圆弧
    lv_arc_set_bg_angles(arc_storage, 0, 360); // 设置为完整圆弧
    
    // 初始设置为完整圆弧
    lv_arc_set_angles(arc_mem, 0, 360); 
    lv_arc_set_angles(arc_storage, 0, 360);
    
    // 计算未使用部分对应的角度（与之前相反）
    int32_t mem_unused_angle = 360 * (100 - memory_percent) / 100;
    int32_t storage_unused_angle = 360 * (100 - storage_percent) / 100;
    
    // 内存圆弧动画 - 通过控制起始角度实现从开始位置收缩效果
    lv_anim_t a_memory;
    lv_anim_init(&a_memory);
    lv_anim_set_var(&a_memory, arc_mem);
    lv_anim_set_values(&a_memory, 0, mem_unused_angle);  // 从0度增加到未使用的角度
    lv_anim_set_time(&a_memory, 800);
    lv_anim_set_path_cb(&a_memory, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_memory, (lv_anim_exec_xcb_t)lv_arc_set_start_angle);  // 改为控制起始角度
    lv_anim_start(&a_memory);
    
    // 存储圆弧动画 - 同样通过控制起始角度实现从开始位置收缩
    lv_anim_t a_storage;
    lv_anim_init(&a_storage);
    lv_anim_set_var(&a_storage, arc_storage);
    lv_anim_set_values(&a_storage, 0, storage_unused_angle);  // 从0度增加到未使用的角度
    lv_anim_set_time(&a_storage, 800);
    lv_anim_set_delay(&a_storage, 120); // 稍微调整延迟时间，让两个动画错开一点
    lv_anim_set_path_cb(&a_storage, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_storage, (lv_anim_exec_xcb_t)lv_arc_set_start_angle);  // 改为控制起始角度
    lv_anim_start(&a_storage);
    
    // 清理动画 - 延长等待时间，确保收缩动画完成
    lv_timer_t *cleanup_timer = lv_timer_create(
        (lv_timer_cb_t)_ui_utils_fade_out_animation_cb, 
        1400,
        objs
    );
    lv_timer_set_repeat_count(cleanup_timer, 1);
    
    // 在动画开始前禁用定时器，避免重复触发
    lv_timer_pause(timer);
}

// 渐变淡出整个面板再清理的函数
static void _ui_utils_fade_out_animation_cb(lv_timer_t *timer) {
    lv_obj_t **objs = (lv_obj_t **)timer->user_data;
    lv_obj_t *panel = objs[0];
    
    // 创建整体面板的淡出动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 500);  // 更短时间使过渡更快
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_ui_anim_opacity_cb);
    
    // 添加动画结束回调来删除对象
    lv_anim_set_completed_cb(&a, _ui_anim_delete_on_complete);
    
    lv_anim_start(&a);
}

// 动画完成时删除对象的回调
static void _ui_anim_delete_on_complete(lv_anim_t *a) {
    lv_obj_t *obj = (lv_obj_t *)a->var;

    // 修复：确保删除对象时清理所有子对象
    if (obj) {
        lv_obj_clean(obj);
        lv_obj_del(obj);
    }
}


