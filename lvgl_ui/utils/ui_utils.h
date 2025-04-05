#ifndef UI_UTILS_H
#define UI_UTILS_H

#include "../common.h"

/**
 * 将KB大小转换为可读字符串
 * @param size_kb 大小(KB)
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void ui_utils_size_to_str(uint64_t size_kb, char *buf, int buf_size);

/**
 * 创建带过渡动画的环形图
 * @param arc 环形图对象
 * @param start_value 起始值
 * @param end_value 结束值
 * @param time_ms 动画时间(ms)
 */
void ui_utils_animate_arc(lv_obj_t *arc, int32_t start_value, int32_t end_value, uint16_t time_ms);

/**
 * 创建带过渡动画的进度条
 * @param bar 进度条对象
 * @param start_value 起始值
 * @param end_value 结束值
 * @param time_ms 动画时间(ms)
 */
void ui_utils_animate_bar(lv_obj_t *bar, int32_t start_value, int32_t end_value, uint16_t time_ms);

// 添加自定义弹性动画路径函数声明
int32_t ui_utils_anim_path_overshoot(const lv_anim_t * a);

/**
 * 创建从圆弧进度条到水平进度条的过渡动画
 * 动画结束后会自动清理临时对象
 */
void ui_utils_create_transition_animation(void);

void ui_utils_create_reverse_transition_animation(void);


#endif // UI_UTILS_H
