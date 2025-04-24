#ifndef UI_UTILS_H
#define UI_UTILS_H

#include "../common.h"
#include "../core/data_manager.h"  // 添加对data_manager的引用

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
 * 创建页面过渡动画 - 苹果风格
 * @param old_page 当前页面对象
 * @param new_page 新页面对象
 * @param anim_type 动画类型(参见ui_anim_type_t)
 * @param duration_ms 动画持续时间(ms)
 * @param destroy_old 是否在动画结束后销毁旧页面
 * @param user_cb 动画完成后的用户回调函数(可为NULL)
 */
void ui_utils_page_transition(lv_obj_t *old_page, lv_obj_t *new_page, ui_anim_type_t anim_type, uint32_t duration_ms, bool destroy_old, lv_anim_ready_cb_t user_cb);

/**
 * 创建进入动画 - 仅用于新页面
 * @param page 页面对象
 * @param anim_type 动画类型
 * @param duration_ms 动画持续时间(ms)
 * @param user_cb 动画完成后的用户回调函数(可为NULL)
 */
void ui_utils_page_enter_anim(lv_obj_t *page, ui_anim_type_t anim_type, uint32_t duration_ms, lv_anim_ready_cb_t user_cb);

/**
 * 创建退出动画 - 仅用于旧页面
 * @param page 页面对象
 * @param anim_type 动画类型
 * @param duration_ms 动画持续时间(ms)
 * @param destroy 是否在动画结束后销毁页面
 * @param user_cb 动画完成后的用户回调函数(可为NULL)
 */
void ui_utils_page_exit_anim(lv_obj_t *page, ui_anim_type_t anim_type, uint32_t duration_ms, bool destroy, lv_anim_ready_cb_t user_cb);

/**
 * 创建iOS风格的缩放过渡动画，从菜单项扩展到全屏
 * @param selected_item 选中的菜单项对象
 * @param create_screen_func 创建新屏幕的函数指针
 */
void ui_utils_zoom_transition(lv_obj_t *selected_item, void (*create_screen_func)(void));

#endif // UI_UTILS_H
