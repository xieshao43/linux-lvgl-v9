#ifndef LV_UI_COMMON_H
#define LV_UI_COMMON_H

#include "../lvgl/lvgl.h"

/* 页面管理定义 */
#define PAGE_COUNT 2
#define PAGE_SWITCH_INTERVAL 15000  // 15秒
#define UPDATE_INTERVAL 300         // 更新间隔，毫秒

/* CPU相关定义 */
#define CPU_CORES 4  // 定义核心数量

/* 颜色方案定义 */
#define COLOR_BG         0x1E2A2A  // 深灰绿色背景
#define COLOR_PANEL_DARK 0x16213E  // 深蓝面板
#define COLOR_PANEL_LIGHT 0x34495E // 浅蓝面板
#define COLOR_PRIMARY    0x3D8361  // 墨绿色
#define COLOR_SECONDARY  0xD8B08C  // 金棕色
#define COLOR_ACCENT     0x9B59B6  // 紫色

/* 字体定义 */
#define FONT_SMALL  &lv_font_montserrat_12
#define FONT_NORMAL &lv_font_montserrat_16
#define FONT_LARGE  &lv_font_montserrat_22

/* 每个UI模块应实现的接口 */
typedef struct {
    void (*create)(lv_obj_t *parent);  // 创建UI
    void (*delete)(void);              // 删除UI
    void (*show)(void);                // 显示UI
    void (*hide)(void);                // 隐藏UI
    void (*update)(void);              // 更新数据
} ui_module_t;

#endif // LV_UI_COMMON_H
