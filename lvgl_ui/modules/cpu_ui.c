#include "cpu_ui.h"
#include "../core/data_manager.h"
#include "../core/ui_manager.h" // 添加UI管理器头文件
#include "../core/key355.h"     // 添加按钮处理头文件
#include "../utils/ui_utils.h"
#include "../utils/ui_rounded.h"
#include <stdio.h>
#include <math.h>   // 添加math.h头文件，解决fabs函数未声明问题


// 私有数据结构
typedef struct {
    lv_obj_t *panel;         // 主面板
    lv_obj_t *core_bars[CPU_CORES];  // 核心进度条
    lv_obj_t *core_labels[CPU_CORES];  // 核心百分比标签
    lv_obj_t *temp_label;   // 温度标签
    lv_timer_t *button_timer;  // 添加：按钮检测定时器
    bool is_active;   
} cpu_ui_data_t;

static cpu_ui_data_t ui_data;
static ui_module_t cpu_module;

// 私有函数原型
static void _create_ui(lv_obj_t *parent);
static void _update_ui(void);
static void _show_ui(void);
static void _hide_ui(void);
static void _delete_ui(void);
static void _button_handler_cb(lv_timer_t *timer); // 添加按钮处理回调的前向声明

// 增加静态缓存，避免重复计算和更新
static struct {
    float core_usage[CPU_CORES];
    uint8_t cpu_temp;
    bool first_update;
    uint32_t last_update_time;  // 添加上次更新时间戳
    uint32_t update_interval;   // 添加动态更新间隔
    int slow_update_counter;    // 添加慢速更新计数器
    int no_change_counter;      // 添加无变化计数器
} ui_cache = {
    .first_update = true,
    .update_interval = 300,     // 初始更新间隔300ms
    .slow_update_counter = 0,
    .no_change_counter = 0
};

// 模块接口实现
static void cpu_ui_create(lv_obj_t *parent) {
    _create_ui(parent);
}

static void cpu_ui_delete(void) {
    _delete_ui();
}

static void cpu_ui_show(void) {
    _show_ui();
}

static void cpu_ui_hide(void) {
    _hide_ui();
}

static void cpu_ui_update(void) {
    _update_ui();
}

// 获取模块接口
ui_module_t* cpu_ui_get_module(void) {
    cpu_module.create = cpu_ui_create;
    cpu_module.delete = cpu_ui_delete;
    cpu_module.show = cpu_ui_show;
    cpu_module.hide = cpu_ui_hide;
    cpu_module.update = cpu_ui_update;

    return &cpu_module;
}

// 创建CPU监控UI - 优化视觉效果
static void _create_ui(lv_obj_t *parent) {
    const lv_font_t *font_small = &lv_font_montserrat_12;
    const lv_font_t *font_mid = &lv_font_montserrat_16;

    // 苹果设计语言风格配色方案
    uint32_t color_panel = 0x1E293B;      // 深靛蓝面板色
    uint32_t color_accent = 0x0EA5E9;     // iOS天蓝色强调色

    // 核心颜色数组 - 单一色调的不同深浅版本
    uint32_t color_core_bases[CPU_CORES] = {
        0x0284C7,  // 蓝色基础色 - Core 0
        0x8B5CF6,  // 紫色基础色 - Core 1
        0x10B981,  // 绿色基础色 - Core 2
        0xF59E0B   // 橙色基础色 - Core 3
    };
    
    // 进度条亮色数组 - 用于渐变起点
    uint32_t color_core_lights[CPU_CORES] = {
        0x38BDF8,  // 蓝色亮色 - Core 0
        0xA78BFA,  // 紫色亮色 - Core 1
        0x34D399,  // 绿色亮色 - Core 2
        0xFBBF24   // 橙色亮色 - Core 3
    };
    
    // 进度条暗色数组 - 用于渐变终点
    uint32_t color_core_darks[CPU_CORES] = {
        0x0369A1,  // 蓝色暗色 - Core 0
        0x7C3AED,  // 紫色暗色 - Core 1
        0x059669,  // 绿色暗色 - Core 2
        0xD97706   // 橙色暗色 - Core 3
    };

    uint32_t color_text = 0xF9FAFB;       // 近白色文本
    uint32_t color_text_secondary = 0x94A3B8; // 中灰色次要文本

    // 创建CPU面板 - 苹果风格圆角矩形
    ui_data.panel = lv_obj_create(parent);
    lv_obj_set_size(ui_data.panel, 240, 135);
    lv_obj_align(ui_data.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_data.panel, lv_color_hex(0x1E293B), 0);  // 深靛蓝面板色
    lv_obj_set_style_border_width(ui_data.panel, 0, 0);
    lv_obj_set_style_radius(ui_data.panel, 16, 0); // macOS风格圆角
    lv_obj_set_style_pad_all(ui_data.panel, 15, 0);
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_SCROLLABLE);
    
    // 添加渐变 - 统一与其他面板相同的渐变效果
    lv_obj_set_style_bg_grad_color(ui_data.panel, lv_color_hex(0x334155), 0);  // 使用更浅的渐变色，减少渲染负担
    lv_obj_set_style_bg_grad_dir(ui_data.panel, LV_GRAD_DIR_VER, 0);
    
    // 简化阴影效果，减少渲染负担
    lv_obj_set_style_shadow_width(ui_data.panel, 15, 0);
    lv_obj_set_style_shadow_spread(ui_data.panel, 0, 0);
    lv_obj_set_style_shadow_ofs_y(ui_data.panel, 4, 0);
    lv_obj_set_style_shadow_color(ui_data.panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ui_data.panel, LV_OPA_10, 0);

    // CPU标题
    lv_obj_t *cpu_title = lv_label_create(ui_data.panel);
    lv_obj_set_style_text_font(cpu_title, font_mid, 0);
    lv_obj_set_style_text_color(cpu_title, lv_color_hex(color_text), 0);
    lv_label_set_text(cpu_title, "CPU MONITOR");
    lv_obj_align(cpu_title, LV_ALIGN_TOP_MID, 0, -5);

    // 进度条设置
    int bar_height = 6;        // 更细的进度条
    int bar_width = 150;       // 进度条宽度
    int vertical_spacing = 22; // 垂直间距
    int start_y = 20;          // 起始位置
    int left_margin = 30;      // 左侧边距

    // 计算最后一个进度条的位置，用于后续放置温度容器
    int last_bar_bottom = start_y + (CPU_CORES - 1) * vertical_spacing + bar_height;

    for(int i = 0; i < CPU_CORES; i++) {
        // 核心标签
        lv_obj_t *core_label = lv_label_create(ui_data.panel);
        lv_obj_set_style_text_font(core_label, font_small, 0);
        lv_obj_set_style_text_color(core_label, lv_color_hex(color_core_bases[i]), 0);
        lv_label_set_text_fmt(core_label, "Core%d", i);

        // 创建进度条 - 现代风格
        lv_obj_t *bar = lv_bar_create(ui_data.panel);
        lv_obj_set_size(bar, bar_width, bar_height);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, left_margin, start_y + i * vertical_spacing);
        
        // 背景设置为半透明
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x374151), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_40, LV_PART_MAIN);
        
        // 增强指示器效果 - 添加更细腻的渐变和光效
        lv_obj_set_style_bg_color(bar, lv_color_hex(color_core_lights[i]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(bar, lv_color_hex(color_core_darks[i]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
        
        // 添加轻微的光泽效果
        lv_obj_set_style_bg_main_stop(bar, 0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_stop(bar, 255, LV_PART_INDICATOR);
        
        // 增强过渡效果
        lv_obj_set_style_shadow_width(bar, 3, LV_PART_INDICATOR);  
        lv_obj_set_style_shadow_color(bar, lv_color_hex(color_core_lights[i]), LV_PART_INDICATOR);
        lv_obj_set_style_shadow_opa(bar, LV_OPA_20, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_spread(bar, 0, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_ofs_x(bar, 0, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_ofs_y(bar, 0, LV_PART_INDICATOR);
        
        // 圆角进度条
        lv_obj_set_style_radius(bar, bar_height / 2, 0);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        
        // 对齐标签
        lv_obj_align_to(core_label, bar, LV_ALIGN_OUT_LEFT_MID, -5, 0);

        // 保存进度条引用
        ui_data.core_bars[i] = bar;

        // 核心百分比标签 - 初始设为透明
        ui_data.core_labels[i] = lv_label_create(ui_data.panel);
        lv_obj_set_style_text_font(ui_data.core_labels[i], font_small, 0);
        lv_obj_set_style_text_color(ui_data.core_labels[i], lv_color_hex(color_text), 0);
        lv_obj_set_style_opa(ui_data.core_labels[i], LV_OPA_TRANSP, 0);  // 初始透明，将通过动画显示
        lv_label_set_text(ui_data.core_labels[i], "0%");
        lv_obj_align_to(ui_data.core_labels[i], bar, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    }

    // 温度显示容器 - 确保在最后一个进度条下方
    lv_obj_t *temp_container = lv_obj_create(ui_data.panel);
    lv_obj_set_size(temp_container, 210, 20); // 合理的高度
    lv_obj_clear_flag(temp_container, LV_OBJ_FLAG_SCROLLABLE);// 禁用滚动
    lv_obj_set_scrollbar_mode(temp_container, LV_SCROLLBAR_MODE_OFF);    
    
    // 放置在最后一个进度条下方，预留足够空间
    lv_obj_align(temp_container, LV_ALIGN_TOP_MID, 0, last_bar_bottom + 0); 
    
    lv_obj_set_style_bg_color(temp_container, lv_color_hex(color_accent), 0);
    lv_obj_set_style_bg_opa(temp_container, LV_OPA_10, 0);
    lv_obj_set_style_radius(temp_container, 12, 0);
    lv_obj_set_style_border_width(temp_container, 0, 0);
    lv_obj_set_style_pad_all(temp_container, 5, 0); // 添加适当的内边距

    // 温度标签
    ui_data.temp_label = lv_label_create(temp_container);
    lv_obj_set_style_text_font(ui_data.temp_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.temp_label, lv_color_hex(color_text), 0);
    lv_obj_set_style_opa(ui_data.temp_label, LV_OPA_TRANSP, 0);
    lv_label_set_text(ui_data.temp_label, "Temperature: 0°C");
    lv_obj_center(ui_data.temp_label);

    // 初始时隐藏面板
    lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
    ui_data.button_timer = lv_timer_create(_button_handler_cb, 50, NULL);
    ui_data.is_active = false;
}

// 增强进度条动画效果 - 超级平滑版
static void _create_smooth_bar_animation(lv_obj_t *bar, int32_t start_value, int32_t end_value) {
    // 计算变化幅度
    int32_t change_magnitude = abs(end_value - start_value);
    uint16_t duration;
    
    // 即使是极小变化也使用轻微动画，避免跳变，更加平滑
    if (change_magnitude <= 2) {
        // 非常微小的变化使用最短动画
        duration = 400;
    } 
    // 动态调整动画时间 - 大幅延长以获得更柔和的效果
    else if (change_magnitude > 60) {
        duration = 1800;      // 极大变化 - 1.8秒，增加50%
    } else if (change_magnitude > 40) {
        duration = 1500;      // 大幅度变化 - 1.5秒，增加87%
    } else if (change_magnitude > 20) {
        duration = 1200;      // 中等变化 - 1.2秒，增加100%
    } else if (change_magnitude > 10) {
        duration = 1000;      // 小变化 - 1秒，增加100%
    } else {
        duration = 800;       // 微小变化 - 800ms，增加100%
    }
    
    // 创建进度条值动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, start_value, end_value);
    lv_anim_set_time(&a, duration);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
    
    // 根据变化方向和幅度选择最温和的缓动函数
    if (start_value > end_value) {
        // 数值减少时使用更加平滑的过渡
        if (change_magnitude > 30) {
            // 使用更平缓的缓动，避免开始下降太快
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 改为平缓的ease_out
        } else {
            // 小幅下降使用最平缓的缓动
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        }
    } else {
        // 数值增加时，更温和的上升曲线
        if (change_magnitude > 50) {
            // 大幅增长移除超调，改用平滑上升
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 替换overshoot为ease_out
        } else if (change_magnitude > 20) {
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); // 更加平滑的过渡
        } else {
            // 小幅增长用最平滑的方式 - 使用ease_in_out替代不可用的sine
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); // 修正：使用ease_in_out代替sine
        }
    }
    
    // 启动动画
    lv_anim_start(&a);
    
    // 性能优化：对于标签动画，只在变化较大时进行，避免微小变化也触发动画
    if (change_magnitude > 8) {
        // 查找关联的标签并为其创建同步动画
        for (int i = 0; i < CPU_CORES; i++) {
            if (ui_data.core_bars[i] == bar && ui_data.core_labels[i] != NULL) {
                // 为标签创建更温和的透明度动画
                lv_anim_t label_anim;
                lv_anim_init(&label_anim);
                lv_anim_set_var(&label_anim, ui_data.core_labels[i]);
                
                // 降低闪动强度，从100%到90%，而不是之前的80%
                lv_anim_set_values(&label_anim, LV_OPA_COVER, LV_OPA_90);
                lv_anim_set_time(&label_anim, duration/4); // 更短的闪动时间
                lv_anim_set_exec_cb(&label_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
                lv_anim_set_path_cb(&label_anim, lv_anim_path_ease_out);
                lv_anim_start(&label_anim);
                
                // 闪动后恢复正常不透明度
                lv_anim_t label_anim2;
                lv_anim_init(&label_anim2);
                lv_anim_set_var(&label_anim2, ui_data.core_labels[i]);
                lv_anim_set_delay(&label_anim2, duration/4);
                lv_anim_set_values(&label_anim2, LV_OPA_90, LV_OPA_COVER);
                lv_anim_set_time(&label_anim2, duration*3/4); // 更长的恢复时间
                lv_anim_set_exec_cb(&label_anim2, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
                lv_anim_set_path_cb(&label_anim2, lv_anim_path_ease_in);
                lv_anim_start(&label_anim2);
                
                break;
            }
        }
    }
}

// 更新UI数据 - 极度平滑版本
static void _update_ui(void) {
    // 安全检查 - 确保面板存在且处于活动状态
    if (!ui_data.panel || !ui_data.is_active) return;
    
    // ===== 更新频率优化 =====
    uint32_t current_time = lv_tick_get();
    
    // 如果不是首次更新，则根据当前设置的更新间隔节流更新频率
    if (!ui_cache.first_update && 
        current_time - ui_cache.last_update_time < ui_cache.update_interval) {
        return; // 跳过更新，节省资源
    }
    
    // 记录本次更新时间
    ui_cache.last_update_time = current_time;
    
    // ===== 数据变化检测 =====
    static uint8_t last_cpu_temp = 0;
    static float last_core_usage_sum = 0;
    static float last_core_values[CPU_CORES] = {0};
    static bool animation_in_progress = false;
    
    // 获取温度数据 - 只获取必要信息
    uint8_t cpu_temp;
    float overall_usage;
    data_manager_get_cpu(&overall_usage, &cpu_temp);
    
    // 仅在首次更新或温度变化时更新温度显示
    if (ui_cache.first_update || abs((int)cpu_temp - (int)ui_cache.cpu_temp) >= 1) {
        ui_cache.cpu_temp = cpu_temp;
        lv_label_set_text_fmt(ui_data.temp_label, "Temperature: %d°C", cpu_temp);
        
        // 温度变化时的微妙动画效果，仅适用于大变化
        if (!ui_cache.first_update && abs((int)cpu_temp - (int)last_cpu_temp) >= 3) {
            // 温度变化较大时，添加轻微闪动引起注意，更温和
            lv_anim_t temp_anim;
            lv_anim_init(&temp_anim);
            lv_anim_set_var(&temp_anim, ui_data.temp_label);
            lv_anim_set_values(&temp_anim, LV_OPA_COVER, LV_OPA_90); // 更轻微的闪烁
            lv_anim_set_time(&temp_anim, 400); // 更长的动画时间
            lv_anim_set_exec_cb(&temp_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&temp_anim, lv_anim_path_ease_out);
            lv_anim_start(&temp_anim);
            
            // 闪动后恢复正常不透明度
            lv_anim_t temp_anim2;
            lv_anim_init(&temp_anim2);
            lv_anim_set_var(&temp_anim2, ui_data.temp_label);
            lv_anim_set_delay(&temp_anim2, 400);
            lv_anim_set_values(&temp_anim2, LV_OPA_90, LV_OPA_COVER);
            lv_anim_set_time(&temp_anim2, 500); // 更长的恢复时间
            lv_anim_set_exec_cb(&temp_anim2, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&temp_anim2, lv_anim_path_ease_in);
            lv_anim_start(&temp_anim2);
        }
    }
    
    // 快速计算总体使用率变化，用于判断是否需要详细更新
    float current_core_usage_sum = 0;
    float core_values[CPU_CORES]; // 存储获取的所有核心数据，避免重复调用
    
    // 检测是否有动画正在进行中 - 避免中断现有动画
    animation_in_progress = false;
    for (int i = 0; i < CPU_CORES; i++) {
        lv_anim_t *a = lv_anim_get(ui_data.core_bars[i], (lv_anim_exec_xcb_t)lv_bar_set_value);
        if (a != NULL) {
            animation_in_progress = true;
            break;
        }
    }
    
    // 如果动画正在进行中，几乎总是推迟更新，除非有剧烈变化
    if (animation_in_progress && !ui_cache.first_update) {
        // 检查是否有显著变化
        for (int i = 0; i < CPU_CORES; i++) {
            float core_usage;
            data_manager_get_cpu_core(i, &core_usage);
            
            // 提高阈值，只有非常大的变化才中断动画
            if (fabs(core_usage - last_core_values[i]) > 30.0f) {
                animation_in_progress = false;
                break;
            }
        }
        
        // 如果无剧烈变化且动画仍在进行，推迟更新
        if (animation_in_progress) {
            return;
        }
    }
    
    // 获取所有核心数据
    for (int i = 0; i < CPU_CORES; i++) {
        float core_usage;
        data_manager_get_cpu_core(i, &core_usage);
        core_values[i] = core_usage;
        current_core_usage_sum += core_usage;
    }
    
    // 检测是否有显著变化 - 提高阈值以降低更新频率
    bool significant_change = 
        ui_cache.first_update || 
        abs((int)cpu_temp - (int)last_cpu_temp) >= 2 ||  // 温度变化增加到2度
        fabs(current_core_usage_sum - last_core_usage_sum) >= 8.0f;  // 总体变化增加到8%
    
    // ===== 智能更新频率调整 =====
    if (!significant_change) {
        // 无显著变化时，增加计数器并降低更新频率
        ui_cache.no_change_counter++;
        
        if (ui_cache.no_change_counter > 30) {  // 降低到30次
            // 长时间无变化 - 极低频更新模式
            ui_cache.update_interval = 1800; // 提高到1.8秒更新一次
            ui_cache.slow_update_counter++;
            
            // 大多数时间完全跳过更新，每25次才实际执行一次
            if (ui_cache.slow_update_counter < 25) {
                return;
            }
            ui_cache.slow_update_counter = 0;
        }
        else if (ui_cache.no_change_counter > 15) {  // 降低到15次
            // 中等时间无变化 - 低频更新模式
            ui_cache.update_interval = 1200; // 1.2秒更新一次
            ui_cache.slow_update_counter++;
            
            // 每10次才实际执行一次
            if (ui_cache.slow_update_counter < 10) {
                return;
            }
            ui_cache.slow_update_counter = 0;
        }
        else if (ui_cache.no_change_counter > 5) {  // 降低到5次
            // 短时间无变化 - 降低更新频率
            ui_cache.update_interval = 800; // 800ms更新一次
        }
    } else {
        // 有显著变化，重置计数器并恢复正常更新频率
        ui_cache.no_change_counter = 0;
        ui_cache.slow_update_counter = 0;
        ui_cache.update_interval = 500; // 增加到500ms，让变化更平滑
    }
    
    // 更新参考值
    last_cpu_temp = cpu_temp;
    last_core_usage_sum = current_core_usage_sum;
    
    // ===== 核心数据智能更新 =====
    // 提高变化阈值，减少不必要的更新
    float change_threshold = ui_cache.no_change_counter > 15 ? 3.0f : 4.0f;
    bool needs_invalidate = false; // 跟踪是否需要重绘
    
    for (int i = 0; i < CPU_CORES; i++) {
        float core_usage = core_values[i];
        last_core_values[i] = core_usage; // 更新上次的值，用于下次比较
        
        // 计算目标值，使用最低显示值1%，避免空条
        int32_t target = (int32_t)(core_usage < 1.0f ? 1.0f : core_usage);
        
        // 检查是否需要更新 - 仅当首次更新或变化显著时
        if (ui_cache.first_update || fabs(ui_cache.core_usage[i] - core_usage) >= change_threshold) {
            ui_cache.core_usage[i] = core_usage; // 更新缓存的值
            
            // 获取当前值，计算差异
            int32_t current = lv_bar_get_value(ui_data.core_bars[i]);
            int32_t diff = abs(target - current);
            
            if (diff > 0) { // 只在实际有变化时更新
                needs_invalidate = true; // 标记需要重绘
                
                // 优化动画策略 - 几乎所有变化都使用动画，但微小变化使用更短的动画
                if (diff <= 1 && ui_cache.no_change_counter > 20) {
                    // 只有在长期稳定且差异极小时才跳过动画
                    lv_bar_set_value(ui_data.core_bars[i], target, LV_ANIM_OFF);
                } else {
                    // 使用平滑动画，几乎所有情况
                    _create_smooth_bar_animation(ui_data.core_bars[i], current, target);
                }
                
                // 仅当标签文本实际需要变化且变化幅度超过1%时才更新
                if (diff >= 1) {
                    char buf[8];
                    lv_snprintf(buf, sizeof(buf), "%d%%", (int)target);
                    if (strcmp(lv_label_get_text(ui_data.core_labels[i]), buf) != 0) {
                        lv_label_set_text(ui_data.core_labels[i], buf);
                    }
                }
            }
        }
    }
    
    // 只在实际有更新时请求重绘
    if (needs_invalidate) {
        // 使用区域无效化而不是整个面板，减少重绘区域
        lv_obj_invalidate(ui_data.panel);
    }
    
    // 重置首次更新标志
    ui_cache.first_update = false;
}

// 显示UI - 优化版本
static void _show_ui(void) {
    // 重置缓存状态
    ui_cache.first_update = true;
    
    // 直接设置面板可见
    lv_obj_clear_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
    
    // 批量设置标签和控件可见
    for (int i = 0; i < CPU_CORES; i++) {
        lv_obj_set_style_opa(ui_data.core_labels[i], LV_OPA_COVER, 0);
        lv_bar_set_value(ui_data.core_bars[i], 0, LV_ANIM_OFF);
    }
    lv_obj_set_style_opa(ui_data.temp_label, LV_OPA_COVER, 0);
    
    // 添加苹果风格的进入动画
    ui_utils_page_enter_anim(
        ui_data.panel,
        ANIM_SLIDE_LEFT, // 从右向左滑入效果
        ANIM_DURATION,
        NULL
    );
    
    // 立即调用一次更新以填充数据
    _update_ui();
    ui_data.is_active = true;
    
    // 设置数据管理器状态 - 确保使用布尔值
    data_manager_set_anim_state(false);
    
    // 重置更新频率相关参数
    ui_cache.update_interval = 300;
    ui_cache.no_change_counter = 0;
    ui_cache.slow_update_counter = 0;
    ui_cache.last_update_time = 0;  // 强制首次更新立即发生
}

// 隐藏UI - 简化版
static void _hide_ui(void) {
    if (ui_data.panel) {
        lv_obj_add_flag(ui_data.panel, LV_OBJ_FLAG_HIDDEN);
        ui_data.is_active = false; // 添加此行
    }
}

// 删除UI - 增强安全性
static void _delete_ui(void) {
    #if UI_DEBUG_ENABLED
    printf("[CPU] Deleting CPU UI resources\n");
    #endif
    
    // 先标记为非活动，防止异步访问
    ui_data.is_active = false;
    
    // 停止所有动画
    lv_anim_del_all();
    
    // 删除按钮定时器
    if (ui_data.button_timer) {
        lv_timer_delete(ui_data.button_timer);
        ui_data.button_timer = NULL;
        #if UI_DEBUG_ENABLED
        printf("[CPU] Button timer deleted\n");
        #endif
    }
    
    // 等待任务处理完成
    lv_task_handler();
    
    if (ui_data.panel) {
        // 保存临时引用并清空指针
        lv_obj_t *panel_to_delete = ui_data.panel;
        
        // 清空所有指针，防止悬空引用
        ui_data.panel = NULL;
        for (int i = 0; i < CPU_CORES; i++) {
            ui_data.core_bars[i] = NULL;
            ui_data.core_labels[i] = NULL;
        }
        ui_data.temp_label = NULL;
        
        // 安全删除面板及其子对象
        lv_obj_delete(panel_to_delete);
        #if UI_DEBUG_ENABLED
        printf("[CPU] Panel deleted\n");
        #endif
    }
    
    // 重置缓存数据
    memset(&ui_cache, 0, sizeof(ui_cache));
    ui_cache.first_update = true;
}

// 添加一个新的全局变量用于确保模块索引的安全传递
static uint8_t menu_module_index = 0; // 菜单模块索引始终为0

// 添加安全包装函数，确保正确设置动画状态
static void _set_anim_state_wrapper(lv_timer_t *timer) {
    // 安全调用，确保传递false值
    data_manager_set_anim_state(false);
}

// 专门用于跳转到菜单的包装函数
static void _switch_to_menu_wrapper(lv_timer_t *timer) {
    // 确保动画状态被重置
    data_manager_set_anim_state(false);
    
    // 确保安全跳转到菜单模块
    #if UI_DEBUG_ENABLED
    printf("[CPU] Switching to menu module: %d\n", menu_module_index);
    #endif
    
    // 安全调用UI管理器
    ui_manager_show_module(menu_module_index);
}

// 改进按钮处理函数
static void _button_handler_cb(lv_timer_t *timer) {
    // 增强安全性检查
    if (!ui_data.is_active || !ui_data.panel) return;
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    #if UI_DEBUG_ENABLED
    printf("[CPU] Button event: %d\n", event);
    #endif
    
    // 简化按钮处理逻辑
    switch (event) {
        case BUTTON_EVENT_DOUBLE_CLICK:
            #if UI_DEBUG_ENABLED
            printf("[CPU] Double click detected, returning to menu\n");
            #endif
            
            // 标记为非活动状态
            ui_data.is_active = false;
            
            // 停止所有正在进行的动画
            lv_anim_del_all();
            
            // 添加苹果风格退出动画
            ui_utils_page_exit_anim(
                ui_data.panel,
                ANIM_SLIDE_RIGHT,  // 向右滑出
                ANIM_DURATION,
                false,
                NULL
            );
            
            // 确保重置所有状态
            data_manager_set_anim_state(false);
            
            // 延迟切换到菜单界面，使用专用的包装函数而非直接传递索引
            lv_timer_t *menu_timer = lv_timer_create(
                _switch_to_menu_wrapper,  // 使用安全包装函数
                ANIM_DURATION + 50,       // 稍微延迟确保动画完成
                NULL                      // 不需要传递用户数据，使用全局变量
            );
            lv_timer_set_repeat_count(menu_timer, 1);
            break;
            
        default:
            break;
    }
}
