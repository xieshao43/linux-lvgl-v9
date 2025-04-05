/**
 * @file ui_perf_mgr.c
 * @brief UI性能管理器实现 - 苹果风格高效率优化
 */

#include "ui_perf_mgr.h"
#include "lvgl.h"
#include <string.h>

// 内部数据结构
typedef struct {
    bool batch_mode;            // 批处理模式标志
    bool animations_paused;     // 动画暂停标志
    uint32_t throttle_ms;       // 节流阀值(ms)
    uint32_t last_update_time;  // 上次更新时间
    uint32_t frame_count;       // 帧计数
    uint32_t fps_calc_time;     // FPS计算时间点
    float current_fps;          // 当前FPS
} ui_perf_data_t;

static ui_perf_data_t perf_data;

// 初始化UI性能管理器
void ui_perf_mgr_init(void) {
    memset(&perf_data, 0, sizeof(ui_perf_data_t));
    perf_data.throttle_ms = 16;  // 默认约60fps
    perf_data.current_fps = 60.0f;
}

// 开始UI批处理操作
void ui_perf_mgr_batch_begin(void) {
    if (!perf_data.batch_mode) {
        perf_data.batch_mode = true;
        lv_disp_t *disp = lv_disp_get_default();
        if (disp) {
            // 开始批处理时暂缓绘制
            lv_disp_set_render_mode(disp, LV_DISP_RENDER_MODE_PARTIAL);
        }
    }
}

// 结束UI批处理操作并执行更新
void ui_perf_mgr_batch_end(void) {
    if (perf_data.batch_mode) {
        perf_data.batch_mode = false;
        lv_disp_t *disp = lv_disp_get_default();
        if (disp) {
            // 结束批处理时恢复正常绘制
            lv_disp_set_render_mode(disp, LV_DISP_RENDER_MODE_DIRECT);
            lv_refr_now(disp);
        }
    }
}

// 暂停复杂动画效果 - 用于低性能设备
void ui_perf_mgr_pause_animations(bool pause) {
    perf_data.animations_paused = pause;
    
    if (pause) {
        // 暂停所有动画
        lv_anim_t *a = lv_anim_get_base();
        while (a) {
            lv_anim_del(a->var, a->exec_cb);
            a = a->next;
        }
    }
}

// 设置UI更新节流阀值
void ui_perf_mgr_set_throttle(uint32_t ms) {
    perf_data.throttle_ms = ms;
}

// 判断是否应该更新UI - 节流实现
bool ui_perf_mgr_should_update(void) {
    uint32_t now = lv_tick_get();
    
    // 检查是否达到更新间隔
    if (now - perf_data.last_update_time < perf_data.throttle_ms) {
        return false;
    }
    
    perf_data.last_update_time = now;
    return true;
}

// 记录UI更新开始
void ui_perf_mgr_update_begin(void) {
    // 更新FPS计算
    uint32_t now = lv_tick_get();
    if (now - perf_data.fps_calc_time >= 1000) {
        // 每秒计算一次FPS
        perf_data.current_fps = (float)perf_data.frame_count * 1000.0f / (now - perf_data.fps_calc_time);
        perf_data.frame_count = 0;
        perf_data.fps_calc_time = now;
    }
}

// 记录UI更新结束
void ui_perf_mgr_update_end(void) {
    perf_data.frame_count++;
}

// 获取UI每秒更新次数
float ui_perf_mgr_get_fps(void) {
    return perf_data.current_fps;
}
