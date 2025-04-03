#ifndef LV_EXAMPLES_H
#define LV_EXAMPLES_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      包含
 *********************/
#include "../lvgl/lvgl.h"

/*********************
 *      函数声明
 *********************/
// 存储监控初始化函数
void storage_monitor_init(const char *path);

// 关闭存储监控
void storage_monitor_close(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_EXAMPLES_H */