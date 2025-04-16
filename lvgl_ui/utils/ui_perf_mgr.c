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
    
    // 新增: 自适应性能控制
    uint8_t cpu_load;          // CPU负载百分比
    uint8_t memory_pressure;   // 内存压力指数 - 修正类型错误
    bool low_power_mode;       // 低功耗模式
    uint32_t load_check_time;  // 上次负载检查时间
    uint16_t missed_frames;    // 错过帧数统计
    
    // Allwinner H3特定优化
    uint8_t cpu_temp;          // CPU温度监控
    uint8_t gpu_load;          // Mali400 GPU负载
} ui_perf_data_t;

static ui_perf_data_t perf_data;

// 初始化UI性能管理器 - 增强版
void ui_perf_mgr_init(void) {
    memset(&perf_data, 0, sizeof(ui_perf_data_t));
    
    // 针对H3处理器优化的默认设置
    perf_data.throttle_ms = 15;  // 适合H3的默认帧率约60fps 240x135
    perf_data.current_fps = 60.0f;
    perf_data.cpu_load = 0;
    perf_data.memory_pressure = 0;
    perf_data.low_power_mode = false;
    perf_data.load_check_time = 0;
    perf_data.missed_frames = 0;
    perf_data.cpu_temp = 0;
    perf_data.gpu_load = 0;
}

// 开始UI批处理操作
void ui_perf_mgr_batch_begin(void) {
    if (!perf_data.batch_mode) {
        perf_data.batch_mode = true;
        lv_disp_t *disp = lv_display_get_default();  // 改用新的API名称
        if (disp) {
            // 开始批处理时暂缓绘制
            // 适配当前LVGL版本，不调用不兼容的API
            #if LVGL_VERSION_MAJOR >= 8
            lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);  // 更新为新的API
            #else
            // 对于旧版本LVGL，使用可用的替代功能或忽略
            lv_obj_invalidate(lv_scr_act()); // 替代方案，刷新屏幕
            #endif
        }
    }
}

// 结束UI批处理操作并执行更新
void ui_perf_mgr_batch_end(void) {
    if (perf_data.batch_mode) {
        perf_data.batch_mode = false;
        lv_disp_t *disp = lv_display_get_default();  // 改用新的API名称
        if (disp) {
            // 结束批处理时恢复正常绘制
            #if LVGL_VERSION_MAJOR >= 8
            lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_DIRECT);  // 更新为新的API
            #endif
            lv_refr_now(disp);
        }
    }
}

// 暂停复杂动画效果 - 用于低性能设备
void ui_perf_mgr_pause_animations(bool pause) {
    perf_data.animations_paused = pause;
    
    if (pause) {
        // 暂停所有正在运行的动画
        #if LVGL_VERSION_MAJOR >= 8 && defined(lv_anim_get_base)
        // 在支持lv_anim_get_base的LVGL 8+版本上使用
        lv_anim_t *a = lv_anim_get_base();
        while (a) {
            lv_anim_del(a->var, a->exec_cb);
            a = a->next;
        }
        #else
        // 在旧版本或不支持的版本上，使用通用方法清除所有动画
        lv_anim_del_all();
        #endif
    }
}

// 设置UI更新节流阀值
void ui_perf_mgr_set_throttle(uint32_t ms) {
    perf_data.throttle_ms = ms;
}

// 判断是否应该更新UI - 节流实现 (增强版)
bool ui_perf_mgr_should_update(void) {
    uint32_t now = lv_tick_get();
    
    // 自适应节流: 根据系统负载动态调整节流阈值
    if (perf_data.cpu_load > 80 || perf_data.memory_pressure > 75) {
        // 系统负载高时，降低更新频率
        uint32_t adaptive_ms = perf_data.throttle_ms;
        if (perf_data.cpu_load > 90) {
            adaptive_ms = perf_data.throttle_ms * 2;  // 极端降频
        } else if (perf_data.cpu_temp > 70) {  // H3芯片温度过高
            adaptive_ms = perf_data.throttle_ms * 1.5;  // 中度降频
        }
        
        if (now - perf_data.last_update_time < adaptive_ms) {
            return false;
        }
    } else {
        // 正常负载时使用标准节流
        if (now - perf_data.last_update_time < perf_data.throttle_ms) {
            return false;
        }
    }
    
    perf_data.last_update_time = now;
    return true;
}

// 记录UI更新开始 - 优化版
void ui_perf_mgr_update_begin(void) {
    // 更新FPS计算
    uint32_t now = lv_tick_get();
    
    // 减少FPS计算频率，降低CPU开销
    if (now - perf_data.fps_calc_time >= 2000) {  // 2秒计算一次，减少开销
        // 计算FPS和检测丢帧
        perf_data.current_fps = (float)perf_data.frame_count * 1000.0f / (now - perf_data.fps_calc_time);
        
        // 检测系统健康状况
        float expected_frames = (now - perf_data.fps_calc_time) / perf_data.throttle_ms;
        if (perf_data.frame_count < expected_frames * 0.8f) {
            // 检测到明显丢帧，记录并可能触发性能模式调整
            perf_data.missed_frames += (expected_frames - perf_data.frame_count);
            
            // 如果持续丢帧超过阈值，进入低功耗模式
            if (perf_data.missed_frames > 20) {
                perf_data.low_power_mode = true;
                perf_data.missed_frames = 0; // 重置计数器
            }
        } else {
            // 性能良好，逐渐减少计数
            if (perf_data.missed_frames > 0) {
                perf_data.missed_frames--;
            }
            // 如果性能持续良好，退出低功耗模式
            if (perf_data.missed_frames == 0 && perf_data.low_power_mode) {
                perf_data.low_power_mode = false;
            }
        }
        
        perf_data.frame_count = 0;
        perf_data.fps_calc_time = now;
    }
    
    // 定期检查系统负载 (每5秒)
    if (now - perf_data.load_check_time >= 5000) {
        perf_data.load_check_time = now;
        
        // 这里可以添加读取系统负载的代码
        // 或从其他模块获取CPU/内存负载数据
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

// 新增: 设置系统负载信息，供自适应优化使用
void ui_perf_mgr_set_system_load(uint8_t cpu_load, uint8_t memory_pressure, uint8_t cpu_temp) {
    perf_data.cpu_load = cpu_load;
    perf_data.memory_pressure = memory_pressure;
    perf_data.cpu_temp = cpu_temp;
}

// 新增: 检查是否处于低功耗模式
bool ui_perf_mgr_is_low_power_mode(void) {
    return perf_data.low_power_mode;
}
