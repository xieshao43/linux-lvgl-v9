#include "ui_utils.h"
#include <stdio.h>

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
