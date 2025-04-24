#ifndef LV_UI_COMMON_H
#define LV_UI_COMMON_H

#include "../lvgl/lvgl.h"

/* 页面管理定义 */
#define PAGE_COUNT 2
#define PAGE_SWITCH_INTERVAL 15000  // 15秒，更符合人的注意力周期
#define UPDATE_INTERVAL 500        // 更新间隔，毫秒

/* 动画过渡类型定义 - LVGL 9.2屏幕切换适用 */
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

/* 颜色方案定义 - 苹果高级感设计 (更新版) */
#define COLOR_BG           0x1A1F35  // 深蓝靛背景，更深沉优雅
#define COLOR_PANEL_DARK   0x1E293B  // 深靛蓝面板基色
#define COLOR_PANEL_LIGHT  0x334155  // 中灰蓝面板
#define COLOR_PRIMARY      0x0EA5E9  // 天蓝色主色
#define COLOR_SECONDARY    0x8B5CF6  // 优雅紫色次要色
#define COLOR_ACCENT       0x10B981  // 翡翠绿强调色

/* 进度条单色渐变色方案 */
// 蓝色系列 - Core 0
#define COLOR_CORE_0        0x0284C7  // 蓝色基础色
#define COLOR_CORE_0_LIGHT  0x38BDF8  // 蓝色亮色
#define COLOR_CORE_0_DARK   0x0369A1  // 蓝色暗色

// 紫色系列 - Core 1
#define COLOR_CORE_1        0x8B5CF6  // 紫色基础色
#define COLOR_CORE_1_LIGHT  0xA78BFA  // 紫色亮色
#define COLOR_CORE_1_DARK   0x7C3AED  // 紫色暗色

// 绿色系列 - Core 2
#define COLOR_CORE_2        0x10B981  // 绿色基础色
#define COLOR_CORE_2_LIGHT  0x34D399  // 绿色亮色
#define COLOR_CORE_2_DARK   0x059669  // 绿色暗色

// 橙色系列 - Core 3
#define COLOR_CORE_3        0xF59E0B  // 橙色基础色
#define COLOR_CORE_3_LIGHT  0xFBBF24  // 橙色亮色
#define COLOR_CORE_3_DARK   0xD97706  // 橙色暗色

/* 字体定义 */
#define FONT_SMALL  &lv_font_montserrat_12
#define FONT_NORMAL &lv_font_montserrat_16
#define FONT_LARGE  &lv_font_montserrat_22

/* 模块化UI接口 (保留用于兼容旧代码) */
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

// Allwinner H3特定优化
#define H3_OPTIMIZE_IO         1     // 针对H3的IO优化
#define H3_LOW_MEMORY_MODE     1     // 针对512MB内存的优化
#define H3_CPU_TEMP_CONTROL    1     // H3温度控制优化
#define H3_GPIO_OPT_PATH       1     // 优化GPIO路径访问
#define H3_DISPLAY_OPTIMIZE    1     // ST7789vw显示优化

// 低功耗模式配置
#define LOW_POWER_UPDATE_MS    1000  // 低功耗模式下更新间隔
#define NORMAL_UPDATE_MS       250   // 正常模式下更新间隔
#define TEMP_WARNING_THRESHOLD 70    // 温度警告阈值(℃)
#define MEM_PRESSURE_THRESHOLD 80    // 内存压力阈值(%)

#endif // LV_UI_COMMON_H
