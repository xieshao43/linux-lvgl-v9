#ifndef LV_UI_COMMON_H
#define LV_UI_COMMON_H

#include "../lvgl/lvgl.h"

/* 页面管理定义 */
#define PAGE_COUNT 2
#define PAGE_SWITCH_INTERVAL 6000  // 9秒，更符合人的注意力周期
#define UPDATE_INTERVAL 300        // 更新间隔，毫秒

/* 动画过渡类型定义 */
typedef enum {
    ANIM_NONE = 0,        // 无动画
    ANIM_FADE,            // 淡入淡出
    ANIM_SLIDE_LEFT,      // 从右向左滑动
    ANIM_SLIDE_RIGHT,     // 从左向右滑动
    ANIM_SLIDE_TOP,       // 从下向上滑动
    ANIM_SLIDE_BOTTOM,    // 从上向下滑动
    ANIM_ZOOM,            // 缩放效果
    ANIM_SPRING,          // 弹性动画（苹果风格）
    ANIM_FADE_SCALE       // 苹果风格淡入缩放组合
} ui_anim_type_t;

// 默认过渡动画类型
#define DEFAULT_ANIM_TYPE ANIM_FADE_SCALE
// 动画持续时间（毫秒），苹果动画通常在250-350ms之间
#define ANIM_DURATION  300
// 弹性动画时间
#define ANIM_DURATION_SPRING 500
// 预加载延迟
#define ANIM_PRELOAD_DELAY 20
// 视觉元素动画交错延迟
#define ANIM_STAGGER_DELAY 80

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

// 关闭调试日志以提高性能
#define UI_DEBUG_ENABLED 0

// 条件编译的调试日志
#if UI_DEBUG_ENABLED
#define UI_DEBUG_LOG(fmt, ...) printf("[UI DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define UI_DEBUG_LOG(fmt, ...) ((void)0)  // 不执行任何操作
#endif

// 性能优化选项
#define UI_OPTIMIZE_ANIMATIONS 1     // 优化动画效果
#define UI_THROTTLE_UPDATES    1     // 节流更新频率
#define UI_USE_BATCH_UPDATES   1     // 使用批量更新
#define UI_ADAPTIVE_QUALITY    1     // 自适应质量控制

#endif // LV_UI_COMMON_H
