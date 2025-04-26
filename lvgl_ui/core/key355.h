#ifndef KEY355_H
#define KEY355_H

#include <stdbool.h>
#include "lvgl.h"
#include "../core/data_manager.h"
#include <errno.h>


/**
 * 按钮事件类型
 */
typedef enum {
    BUTTON_EVENT_NONE,       // 无事件
    BUTTON_EVENT_CLICK,      // 单击事件
    BUTTON_EVENT_DOUBLE_CLICK, // 双击事件
    BUTTON_EVENT_LONG_PRESS  // 长按事件
} button_event_t;

/**
 * 初始化按钮处理模块（GPIO355）
 * @return 成功返回true，失败返回false
 */
bool key355_init(void);

/**
 * 获取当前按钮事件并自动清除事件（线程安全）
 * @return 当前按钮事件类型，获取后事件自动重置为NONE
 */
button_event_t key355_get_event(void);

/**
 * 手动清除按钮事件（线程安全）
 */
void key355_clear_event(void);

/**
 * 获取按钮当前物理状态（是否被按下）
 * @return 按钮被按下返回true，否则返回false
 */
bool key355_is_pressed(void);

/**
 * 销毁按钮处理模块，释放所有资源
 */
void key355_deinit(void);

#endif // KEY355_H
