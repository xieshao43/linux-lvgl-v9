#include "cpu_ui.h"
#include "../core/data_manager.h"
#include "../core/ui_manager.h" // 添加UI管理器头文件
#include "../core/key355.h"     // 添加按钮处理头文件
#include "../utils/ui_utils.h"
#include <stdio.h>
#include <math.h>   // 添加math.h头文件，解决fabs函数未声明问题
#include "menu_ui.h" // 添加菜单UI头文件

// 私有数据结构
typedef struct {
    lv_obj_t *screen;        // 主屏幕
    lv_obj_t *core_bars[CPU_CORES];  // 核心进度条
    lv_obj_t *core_labels[CPU_CORES];  // 核心百分比标签
    lv_obj_t *temp_label;   // 温度标签
    lv_timer_t *button_timer;  // 按钮检测定时器
    lv_timer_t *update_timer;  // 数据更新定时器
    bool is_active;   
} cpu_ui_data_t;

static cpu_ui_data_t ui_data;

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

// 函数前向声明
static void _update_ui_data(lv_timer_t *timer);
static void _button_handler_cb(lv_timer_t *timer);
static void _create_smooth_bar_animation(lv_obj_t *bar, int32_t start_value, int32_t end_value);

// 切换屏幕的动画回调
static void _slide_anim_cb(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t*)var, v);
}

// 简化版返回菜单函数 - 使用LVGL内置动画
static void _return_to_menu(void) {
    // 停止定时器，避免切换中的异常刷新
    if (ui_data.button_timer) {
        lv_timer_delete(ui_data.button_timer);
        ui_data.button_timer = NULL;
    }
    
    if (ui_data.update_timer) {
        lv_timer_delete(ui_data.update_timer);
        ui_data.update_timer = NULL;
    }
    
    // 标记为非活动状态
    ui_data.is_active = false;
    
    // 清除实例引用，防止回调函数使用已删除的对象
    lv_obj_t *current_screen = ui_data.screen;
    ui_data.screen = NULL;
    
    // 创建菜单屏幕（但还不显示）
    menu_ui_create_screen();
    
    // 使用LVGL内置的屏幕切换动画 - 从左向右滑动效果
    lv_scr_load_anim(lv_scr_act(), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, true); // 最后参数true表示自动删除旧屏幕
    
    // 激活菜单
    menu_ui_set_active();
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
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        } else {
            // 小幅下降使用最平缓的缓动
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        }
    } else {
        // 数值增加时，更温和的上升曲线
        if (change_magnitude > 50) {
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        } else if (change_magnitude > 20) {
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        } else {
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
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

// 更新UI数据 - 平滑版本
static void _update_ui_data(lv_timer_t *timer) {
    // 安全检查 - 确保处于活动状态
    if (!ui_data.is_active || !ui_data.screen) return;
    
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
    
    // 仅在首次更新或温度变化时更新温度显示 - 使用更简洁的苹果风格格式
    if (ui_cache.first_update || abs((int)cpu_temp - (int)ui_cache.cpu_temp) >= 1) {
        ui_cache.cpu_temp = cpu_temp;
        lv_label_set_text_fmt(ui_data.temp_label, "%d°C", cpu_temp); // 简化为直接显示温度
        
        // 确保温度标签可见
        lv_obj_set_style_opa(ui_data.temp_label, LV_OPA_COVER, 0);
        
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
    
    // 在更新过程的开始部分确保所有百分比标签都是可见的
    for (int i = 0; i < CPU_CORES; i++) {
        if (ui_data.core_labels[i]) {
            lv_obj_set_style_opa(ui_data.core_labels[i], LV_OPA_COVER, 0);
        }
    }
    
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
                        // 确保文本更新后标签始终可见
                        lv_obj_set_style_opa(ui_data.core_labels[i], LV_OPA_COVER, 0);
                    }
                }
            }
        }
    }
    
    // 只在实际有更新时请求重绘
    if (needs_invalidate) {
        // 使用区域无效化而不是整个面板，减少重绘区域
        lv_obj_invalidate(ui_data.screen);
    }
    
    // 重置首次更新标志
    ui_cache.first_update = false;
}

// 按钮事件处理回调
static void _button_handler_cb(lv_timer_t *timer) {
    if (!ui_data.is_active || !ui_data.screen) return;
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    // 清除事件标志
    key355_clear_event();
    
    // 处理按钮事件
    switch (event) {
        case BUTTON_EVENT_DOUBLE_CLICK:
            // 双击返回菜单
            _return_to_menu();
            break;
            
        default:
            break;
    }
}

// 创建CPU监控屏幕 - 更新布局
void cpu_ui_create_screen(void) {
    const lv_font_t *font_small = &lv_font_montserrat_12;
    const lv_font_t *font_mid = &lv_font_montserrat_14;
    const lv_font_t *font_large = &lv_font_montserrat_18; // 使用更大字体

    // 创建屏幕
    ui_data.screen = lv_obj_create(NULL);
    
    // 禁用滚动功能
    lv_obj_clear_flag(ui_data.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_data.screen, LV_SCROLLBAR_MODE_OFF);
    
#if LV_USE_DRAW_SW_COMPLEX_GRADIENTS
    // 定义渐变色 - 紫色到黑色
    static const lv_color_t grad_colors[2] = {
        LV_COLOR_MAKE(0x9B, 0x18, 0x42), // 紫红色
        LV_COLOR_MAKE(0x00, 0x00, 0x00), // 纯黑色
    };
    
    // 初始化渐变描述符
    static lv_grad_dsc_t grad;
    lv_grad_init_stops(&grad, grad_colors, NULL, NULL, sizeof(grad_colors) / sizeof(lv_color_t));
    
    // 设置径向渐变 - 从中心向四周扩散
    lv_grad_radial_init(&grad, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_BOTTOM, LV_GRAD_EXTEND_PAD);
    
    // 应用渐变背景 - 修复：移除错误的 & 符号，直接使用对象指针
    lv_obj_set_style_bg_grad(ui_data.screen, &grad, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#else
    // 简化背景样式 - 纯黑背景提升对比度
    lv_obj_set_style_bg_color(ui_data.screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#endif
    
    // 核心颜色数组 - 在小屏幕上使用明亮的颜色
    uint32_t color_core_bases[CPU_CORES] = {
        0x0A84FF,  // 蓝色 - Core 0
        0xBF5AF2,  // 紫色 - Core 1
        0x30D158,  // 绿色 - Core 2
        0xFF9F0A   // 橙色 - Core 3
    };
    
    uint32_t color_text = 0xFFFFFF;  // 白色文本
    
    // 创建标题容器以增强标题醒目度
    lv_obj_t *title_container = lv_obj_create(ui_data.screen);
    lv_obj_set_size(title_container, 160, 24);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_bg_color(title_container, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(title_container, LV_OPA_50, 0);
    lv_obj_set_style_radius(title_container, 4, 0);
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 修改标题为更醒目的样式
    lv_obj_t *cpu_title = lv_label_create(title_container);
    lv_obj_set_style_text_font(cpu_title, font_large, 0);  // 更大字体
    lv_obj_set_style_text_color(cpu_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_letter_space(cpu_title, 2, 0);  // 增加字间距
    lv_label_set_text(cpu_title, "CPU MONITOR");
    lv_obj_center(cpu_title);  // 在容器中居中
    
    // 进度条布局优化
    int bar_height = 6;           // 紧凑进度条高度
    int bar_width = 160;          // 适当减小进度条宽度以适应Core文本
    int vertical_spacing = 22;    // 垂直间距
    int start_y = 36;             // 从顶部开始的位置
    int left_margin =45;         // 增加左侧留空以适应"Core X"标签

    // 创建核心进度条组
    for(int i = 0; i < CPU_CORES; i++) {
        // 核心标签 - 保持"Core X"格式
        lv_obj_t *core_label = lv_label_create(ui_data.screen);
        lv_obj_set_style_text_font(core_label, font_small, 0);
        lv_obj_set_style_text_color(core_label, lv_color_hex(color_core_bases[i]), 0);
        lv_label_set_text_fmt(core_label, "Core %d", i);
        
        // 创建进度条
        lv_obj_t *bar = lv_bar_create(ui_data.screen);
        lv_obj_set_size(bar, bar_width, bar_height);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, left_margin, start_y + i * vertical_spacing + 5);
        
        // 将Core标签垂直中心与进度条的垂直中心对齐
        lv_obj_align_to(core_label, bar, LV_ALIGN_OUT_LEFT_MID, -4, 0);
        
        // 进度条背景
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_50, LV_PART_MAIN);
        
        // 进度条指示器
        lv_obj_set_style_bg_color(bar, lv_color_hex(color_core_bases[i]), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 3, 0);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        
        // 保存进度条引用
        ui_data.core_bars[i] = bar;

        // 核心百分比标签
        ui_data.core_labels[i] = lv_label_create(ui_data.screen);
        lv_obj_set_style_text_font(ui_data.core_labels[i], font_small, 0);
        lv_obj_set_style_text_color(ui_data.core_labels[i], lv_color_hex(color_text), 0);
        lv_obj_set_width(ui_data.core_labels[i], 30);
        lv_obj_set_style_text_align(ui_data.core_labels[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(ui_data.core_labels[i], "0%");
        lv_obj_align(ui_data.core_labels[i], LV_ALIGN_TOP_LEFT, left_margin + bar_width + 2, start_y + i * vertical_spacing);
        lv_obj_set_style_opa(ui_data.core_labels[i], LV_OPA_COVER, 0);
    }

    // 温度显示容器 - 底部居中
    lv_obj_t *temp_container = lv_obj_create(ui_data.screen);
    lv_obj_set_size(temp_container, 150, 15);  // 增大容器宽度以容纳更多文字
    lv_obj_clear_flag(temp_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(temp_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // 简化温度容器
    lv_obj_set_style_bg_opa(temp_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(temp_container, 0, 0);
    lv_obj_set_style_pad_all(temp_container, 0, 0);

    // 温度标签文本
    lv_obj_t *temp_text = lv_label_create(temp_container);
    lv_obj_set_style_text_font(temp_text, font_small, 0);
    lv_obj_set_style_text_color(temp_text, lv_color_hex(color_text), 0);
    lv_label_set_text(temp_text, " Temperature:");
    lv_obj_align(temp_text, LV_ALIGN_LEFT_MID, 0, 0);

    // 温度值 - 在温度文本右侧
    ui_data.temp_label = lv_label_create(temp_container);
    lv_obj_set_style_text_font(ui_data.temp_label, font_small, 0);
    lv_obj_set_style_text_color(ui_data.temp_label, lv_color_hex(color_text), 0);
    lv_label_set_text(ui_data.temp_label, "0°C");
    lv_obj_align_to(ui_data.temp_label, temp_text, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    // 温度图标 - 在温度值之后
    lv_obj_t *temp_icon = lv_label_create(temp_container);
    lv_obj_set_style_text_font(temp_icon, font_small, 0);
    lv_obj_set_style_text_color(temp_icon, lv_color_hex(0xFF3B30), 0);
    lv_label_set_text(temp_icon, LV_SYMBOL_WARNING);
    lv_obj_align_to(temp_icon, ui_data.temp_label, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    
    // 设置屏幕进入动画 - 简化为只有水平滑动
    lv_obj_set_x(ui_data.screen, 240); // 初始位置在屏幕右侧外
    
    // 动画时长优化
    uint32_t anim_time = 260;
    
    // X轴滑动动画
    lv_anim_t a_x;
    lv_anim_init(&a_x);
    lv_anim_set_var(&a_x, ui_data.screen);
    lv_anim_set_values(&a_x, 240, 0);
    lv_anim_set_time(&a_x, anim_time);
    lv_anim_set_exec_cb(&a_x, _slide_anim_cb);
    lv_anim_set_path_cb(&a_x, lv_anim_path_ease_out);
    lv_anim_start(&a_x);
    
    // 创建按钮处理定时器
    ui_data.button_timer = lv_timer_create(_button_handler_cb, 100, NULL);
    
    // 创建数据更新定时器
    ui_data.update_timer = lv_timer_create(_update_ui_data, 300, NULL);
    
    // 设置为活动状态
    ui_data.is_active = true;
    
    // 首次更新数据
    ui_cache.first_update = true;
    _update_ui_data(NULL);
    
    // 直接使用LVGL内置的屏幕切换动画 
    lv_scr_load_anim(ui_data.screen, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, true);
}
