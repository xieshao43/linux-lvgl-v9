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

#endif // UI_UTILS_H
