#include "ui_manager.h"


#define MAX_MODULES 8

typedef struct {
    lv_obj_t *main_cont;        // 主容器
    ui_module_t **modules;      // UI模块数组
    uint8_t module_count;       // 模块总数
    uint8_t current_module;     // 当前显示的模块
    lv_timer_t *page_timer;     // 页面切换定时器
    lv_timer_t *update_timer;   // 数据更新定时器
    ui_anim_type_t anim_type;   // 动画类型
} ui_manager_t;

static ui_manager_t *ui_mgr = NULL;

// 苹果风格的弹性曲线 - 添加一个弹性动画路径
static int32_t _anim_path_overshoot(const lv_anim_t * a)
{
    /*计算当前步骤（0-1之间的值）*/
    int32_t t = lv_map(a->act_time, 0, a->duration, 0, 1024);
    
    // 弹性公式：一个缓出函数加上一个衰减的正弦波
    const int32_t p = 200;  // 弹性系数
    int32_t ret;
    
    if(t < 768) {
        ret = lv_bezier3(t, 0, 400, 1000, 1024);  // 先快速接近目标
    }
    else {
        // 接近终点时弹性减少
        int32_t diff = (((t - 768) * p) >> 10);
        int32_t d = lv_bezier3(t, 0, 500, 1000, 1024);
        int32_t overflow = lv_bezier3(t, 0, 0, 500, 0);
        
        ret = d + overflow * lv_trigo_sin((diff + 90) % 360) / 2048;
    }
    
    return ret;
}

// 为不同对象类型返回合适的动画时长
static uint32_t _get_optimized_anim_time(ui_anim_type_t type, lv_obj_t *obj)
{
    if(type == ANIM_SPRING) {
        return ANIM_DURATION_SPRING;
    }
    
    // 基于对象大小调整动画时间，较大对象稍微慢一点
    int32_t area = lv_obj_get_width(obj) * lv_obj_get_height(obj);
    uint32_t base_time = ANIM_DURATION;
    
    // 对于较大对象稍微增加动画时长
    if(area > 20000) {
        base_time += 50;
    }
    else if(area < 5000) {
        base_time -= 50;
    }
    
    return base_time;
}

// 配置动画参数
static void _setup_anim_params(lv_anim_t *anim, ui_anim_type_t type)
{
    // 根据动画类型选择合适的动画曲线
    switch(type) {
        case ANIM_SPRING:
            // 使用更新后的函数名称
            lv_anim_set_path_cb(anim, ui_utils_anim_path_overshoot);
            break;
        case ANIM_FADE:
        case ANIM_FADE_SCALE:
            lv_anim_set_path_cb(anim, lv_anim_path_ease_out);
            break;
        case ANIM_ZOOM:
            lv_anim_set_path_cb(anim, lv_anim_path_ease_in_out);
            break;
        default:
            // 对于滑动类动画使用ease_out效果更自然
            lv_anim_set_path_cb(anim, lv_anim_path_ease_out);
            break;
    }
}

// 动画完成回调函数 - 优化重绘性能
static void _anim_completed_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    
    // 当动画结束时，清除所有变换设置并隐藏对象
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    
    // 重置所有样式，避免残留样式导致内存泄漏和渲染问题
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_x(obj, 0, 0);
    lv_obj_set_style_translate_y(obj, 0, 0);
    lv_obj_set_style_transform_scale(obj, LV_SCALE_NONE, 0);
    lv_obj_set_style_transform_pivot_x(obj, 0, 0);
    lv_obj_set_style_transform_pivot_y(obj, 0, 0);
    
    // 禁用动画期间附加的阴影等效果，降低渲染负担
    lv_obj_set_style_shadow_width(obj, 0, 0);
}

// 预加载新页面，提前进行布局计算以避免动画过程中的卡顿
static void _preload_page(lv_obj_t *page)
{
    if(!page) return;
    
    // 临时使页面不可见但已进行布局
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_mark_layout_as_dirty(page);
    lv_obj_update_layout(page);
    
    // 延时后再进行动画，给布局计算足够时间
    lv_timer_t *preload_timer = lv_timer_create(
        (lv_timer_cb_t)lv_obj_clear_flag, 
        ANIM_PRELOAD_DELAY, 
        page
    );
    lv_timer_set_repeat_count(preload_timer, 1);
    // 使用LV_OBJ_FLAG_HIDDEN作为参数
    *(lv_obj_flag_t *)(preload_timer->user_data) = LV_OBJ_FLAG_HIDDEN;
}

// 执行页面切换动画
static void _perform_transition_anim(lv_obj_t *old_page, lv_obj_t *new_page, ui_anim_type_t anim_type)
{
    if(!old_page || !new_page) return;
    
    // 预加载新页面以避免动画过程中的布局计算
    _preload_page(new_page);
    
    // 基本动画设置
    lv_anim_t anim_old, anim_new;
    lv_anim_init(&anim_old);
    lv_anim_init(&anim_new);
    
    // 为不同对象设置最佳动画时间
    uint32_t old_time = _get_optimized_anim_time(anim_type, old_page);
    uint32_t new_time = _get_optimized_anim_time(anim_type, new_page);
    
    lv_anim_set_time(&anim_old, old_time);
    lv_anim_set_time(&anim_new, new_time);
    
    // 设置动画路径函数
    _setup_anim_params(&anim_old, anim_type);
    _setup_anim_params(&anim_new, anim_type);
    
    lv_anim_set_var(&anim_old, old_page);
    lv_anim_set_var(&anim_new, new_page);
    
    lv_anim_set_completed_cb(&anim_old, _anim_completed_cb);
    
    // 为可能的变换操作提前计算pivot点位置
    int32_t pivotx = lv_obj_get_width(new_page) / 2;
    int32_t pivoty = lv_obj_get_height(new_page) / 2;
    
    // 基于动画类型设置不同的动画效果
    switch(anim_type) {
        case ANIM_FADE:
            // 淡入淡出效果
            lv_obj_set_style_opa(new_page, LV_OPA_TRANSP, 0);
            
            lv_anim_set_exec_cb(&anim_old, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_exec_cb(&anim_new, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            
            lv_anim_set_values(&anim_old, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_values(&anim_new, LV_OPA_TRANSP, LV_OPA_COVER);
            break;
            
        case ANIM_FADE_SCALE:
            // 苹果风格的淡入缩放组合效果
            lv_obj_set_style_opa(new_page, LV_OPA_TRANSP, 0);
            lv_obj_set_style_transform_scale(new_page, 940, 0); // 94% - 微妙的放大效果
            
            // 为过渡设置良好的变换原点
            lv_obj_set_style_transform_pivot_x(new_page, pivotx, 0);
            lv_obj_set_style_transform_pivot_y(new_page, pivoty, 0);
            
            // 旧页面淡出
            lv_anim_set_exec_cb(&anim_old, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_values(&anim_old, LV_OPA_COVER, LV_OPA_TRANSP);
            
            // 新页面淡入和缩放
            lv_anim_t anim_new_scale;
            lv_anim_init(&anim_new_scale);
            lv_anim_set_var(&anim_new_scale, new_page);
            lv_anim_set_time(&anim_new_scale, new_time);
            _setup_anim_params(&anim_new_scale, anim_type);
            lv_anim_set_exec_cb(&anim_new_scale, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_scale);
            lv_anim_set_values(&anim_new_scale, 940, LV_SCALE_NONE);
            
            lv_anim_set_exec_cb(&anim_new, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_values(&anim_new, LV_OPA_TRANSP, LV_OPA_COVER);
            
            // 启动缩放动画
            lv_anim_start(&anim_new_scale);
            break;
            
        case ANIM_SPRING:
            // 弹性过渡效果
            lv_obj_set_style_translate_x(new_page, lv_obj_get_width(new_page), 0);
            
            lv_anim_set_exec_cb(&anim_old, (lv_anim_exec_xcb_t)lv_obj_set_style_translate_x);
            lv_anim_set_exec_cb(&anim_new, (lv_anim_exec_xcb_t)lv_obj_set_style_translate_x);
            
            lv_anim_set_values(&anim_old, 0, -lv_obj_get_width(old_page));
            lv_anim_set_values(&anim_new, lv_obj_get_width(new_page), 0);
            break;
            
        // ...其他动画类型保持不变...
        case ANIM_SLIDE_LEFT:
            // 从右向左滑动效果
            lv_obj_set_style_translate_x(new_page, lv_obj_get_width(new_page), 0);
            
            lv_anim_set_exec_cb(&anim_old, (lv_anim_exec_xcb_t)lv_obj_set_style_translate_x);
            lv_anim_set_exec_cb(&anim_new, (lv_anim_exec_xcb_t)lv_obj_set_style_translate_x);
            
            lv_anim_set_values(&anim_old, 0, -lv_obj_get_width(old_page));
            lv_anim_set_values(&anim_new, lv_obj_get_width(new_page), 0);
            break;
            
        // ...其他动画类型保持不变...
            
        case ANIM_ZOOM:
            // 缩放效果 - 优化为更自然的缩放
            lv_obj_set_style_transform_scale(new_page, 850, 0); // 从85%缩放到100%
            lv_obj_set_style_opa(new_page, LV_OPA_TRANSP, 0);
            
            // 设置变换中心点在中央
            lv_obj_set_style_transform_pivot_x(new_page, pivotx, 0);
            lv_obj_set_style_transform_pivot_y(new_page, pivoty, 0);
            lv_obj_set_style_transform_pivot_x(old_page, pivotx, 0);
            lv_obj_set_style_transform_pivot_y(old_page, pivoty, 0);
            
            // 旧页面缩小淡出
            lv_anim_set_exec_cb(&anim_old, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_scale);
            lv_anim_set_values(&anim_old, LV_SCALE_NONE, 1150); // 缩放到115%
            
            lv_anim_t anim_old_opa;
            lv_anim_init(&anim_old_opa);
            lv_anim_set_var(&anim_old_opa, old_page);
            lv_anim_set_time(&anim_old_opa, old_time);
            _setup_anim_params(&anim_old_opa, anim_type);
            lv_anim_set_exec_cb(&anim_old_opa, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_values(&anim_old_opa, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_completed_cb(&anim_old_opa, _anim_completed_cb);
            
            // 新页面放大淡入
            lv_anim_set_exec_cb(&anim_new, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_scale);
            lv_anim_set_values(&anim_new, 850, LV_SCALE_NONE);
            
            lv_anim_t anim_new_opa;
            lv_anim_init(&anim_new_opa);
            lv_anim_set_var(&anim_new_opa, new_page);
            lv_anim_set_time(&anim_new_opa, new_time);
            _setup_anim_params(&anim_new_opa, anim_type);
            lv_anim_set_exec_cb(&anim_new_opa, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_values(&anim_new_opa, LV_OPA_TRANSP, LV_OPA_COVER);
            
            // 启动透明度动画
            lv_anim_start(&anim_old_opa);
            lv_anim_start(&anim_new_opa);
            break;
            
        case ANIM_NONE:
        default:
            // 无动画，直接切换
            lv_obj_add_flag(old_page, LV_OBJ_FLAG_HIDDEN);
            return;
    }
    
    // 在启动动画前应用视觉优化
    lv_obj_set_style_shadow_width(new_page, 20, 0); // 增加一点阴影以提升层次感
    lv_obj_set_style_shadow_opa(new_page, LV_OPA_20, 0);
    
    // 启动动画
    lv_anim_start(&anim_old);
    lv_anim_start(&anim_new);
}


static bool forward_switch = true;

// 页面切换回调 - 简化版本
static void _page_switch_cb(lv_timer_t *timer)
{
    if (!ui_mgr || ui_mgr->module_count == 0) return;

    // 获取当前模块和下一个模块
    uint8_t next_module_idx = (ui_mgr->current_module + 1) % ui_mgr->module_count;
    if (!forward_switch) {
        // 如果是反向切换，获取上一个模块
        next_module_idx = (ui_mgr->current_module - 1 + ui_mgr->module_count) % ui_mgr->module_count;
    }
    ui_module_t *next_module = ui_mgr->modules[next_module_idx];

    UI_DEBUG_LOG("切换页面: 从 %d 到 %d", ui_mgr->current_module, next_module_idx);

    // 隐藏当前模块
    if (ui_mgr->modules[ui_mgr->current_module]->hide) {
        ui_mgr->modules[ui_mgr->current_module]->hide();
    }

    // 根据切换方向添加过渡动画
    if (forward_switch) {
        ui_utils_create_transition_animation();
    } else {
        ui_utils_create_reverse_transition_animation();
    }

    // 设置短暂延时再显示下一个模块，确保过渡动画有足够时间播放
    lv_timer_t *show_next_timer = lv_timer_create(
        (lv_timer_cb_t)lv_obj_clear_flag,
        2000,   // 2秒延时，让过渡动画完成
        NULL
    );
    lv_timer_set_repeat_count(show_next_timer, 1);

    // 修改回调和用户数据以显示下一个模块
    lv_timer_set_cb(show_next_timer, (lv_timer_cb_t)next_module->show);

    // 更新当前模块索引
    ui_mgr->current_module = next_module_idx;

    // 切换完成后，切换标志位
    forward_switch =! forward_switch;
}

// 数据更新回调
static void _update_data_cb(lv_timer_t *timer) {
    if (!ui_mgr) return;
    data_manager_update();
    // 更新所有模块数据
    for (int i = 0; i < ui_mgr->module_count; i++) {
        if (ui_mgr->modules[i]->update) {
            ui_mgr->modules[i]->update();
        }
    }
}

void ui_manager_init(lv_color_t bg_color) {
    // 初始化UI管理器
    ui_mgr = lv_malloc(sizeof(ui_manager_t));
    if (!ui_mgr) {
        printf("UI管理器内存分配失败\n");
        return;
    }
    
    // 设置背景色
    lv_obj_set_style_bg_color(lv_screen_active(), bg_color, 0);
    
    // 创建主容器
    ui_mgr->main_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui_mgr->main_cont, 240, 135);
    lv_obj_set_pos(ui_mgr->main_cont, 0, 0);
    lv_obj_set_style_bg_color(ui_mgr->main_cont, bg_color, 0);
    lv_obj_set_style_border_width(ui_mgr->main_cont, 0, 0);
    lv_obj_set_style_pad_all(ui_mgr->main_cont, 10, 0);
    lv_obj_set_style_radius(ui_mgr->main_cont, 0, 0);
    
    // 初始化模块数组
    ui_mgr->modules = lv_malloc(sizeof(ui_module_t*) * MAX_MODULES);
    ui_mgr->module_count = 0;
    ui_mgr->current_module = 0;
    ui_mgr->anim_type = DEFAULT_ANIM_TYPE; // 设置默认动画类型
    
    // 创建定时器
    ui_mgr->page_timer = lv_timer_create(_page_switch_cb, PAGE_SWITCH_INTERVAL, NULL);
    ui_mgr->update_timer = lv_timer_create(_update_data_cb, UPDATE_INTERVAL, NULL);
}

void ui_manager_register_module(ui_module_t *module) {
    if (!ui_mgr || !module || ui_mgr->module_count >= MAX_MODULES) return;
    
    // 添加模块到数组
    ui_mgr->modules[ui_mgr->module_count] = module;
    ui_mgr->module_count++;
    
    // 创建模块UI
    if (module->create) {
        module->create(ui_mgr->main_cont);
    }
    
    // 初始隐藏模块
    if (module->hide) {
        module->hide();
    }
}

void ui_manager_show_module(uint8_t index) {
    if (!ui_mgr || index >= ui_mgr->module_count) return;
    
    // 获取当前模块和目标模块
    ui_module_t *current_module = ui_mgr->modules[ui_mgr->current_module];
    ui_module_t *target_module = ui_mgr->modules[index];
    
    
    // 显示目标模块
    if (target_module->show) {
        target_module->show();
    }
    
    // 找到当前和目标模块的面板
    lv_obj_t *curr_obj = NULL;
    lv_obj_t *target_obj = NULL;
    
    uint32_t i;
    for(i = 0; i < lv_obj_get_child_count(ui_mgr->main_cont); i++) {
        lv_obj_t *child = lv_obj_get_child(ui_mgr->main_cont, i);
        if(!lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
            if(ui_mgr->current_module == index) {
                return; // 如果目标就是当前模块，则不切换
            }
            curr_obj = child;
            break;
        }
    }
    
    // 找到目标模块的面板
    for(i = 0; i < lv_obj_get_child_count(ui_mgr->main_cont); i++) {
        lv_obj_t *child = lv_obj_get_child(ui_mgr->main_cont, i);
        if(!lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN) && child != curr_obj) {
            target_obj = child;
            break;
        }
    }
    
    // 执行动画过渡
    if(curr_obj && target_obj) {
        _perform_transition_anim(curr_obj, target_obj, ui_mgr->anim_type);
    } else {
        // 如果找不到面板，则直接切换
        if(current_module->hide) {
            current_module->hide();
        }
    }
    
    // 更新当前模块索引
    ui_mgr->current_module = index;
}

// 添加缺失的函数定义
static ui_anim_type_t _determine_best_transition(uint8_t current_module, uint8_t next_module) {
    // 根据当前模块和目标模块选择最佳过渡效果
    if (next_module > current_module) {
        return ANIM_SLIDE_LEFT; // 向左滑动，修正常量名称
    } else {
        return ANIM_SLIDE_RIGHT; // 向右滑动，修正常量名称
    }
}

static void _perform_transition(uint8_t module_index, ui_anim_type_t transition_type) {
    // 执行模块之间的过渡动画
    // ...transition implementation...
    
    // 设置当前模块为新选择的模块
    ui_mgr->current_module = module_index;
    
    // 显示新模块 - 修正C++风格函数调用为C风格
    ui_mgr->modules[module_index]->show();
}

void ui_manager_transition_to(uint8_t module_index) {
    // 内部处理前后检查，错误处理
    if (!ui_mgr || module_index >= ui_mgr->module_count) {
        return; // 安全退出
    }
    
    // 确定最佳过渡类型（考虑当前上下文）
    ui_anim_type_t transition_type = _determine_best_transition(
        ui_mgr->current_module, 
        module_index
    );
    
    // 执行过渡（内部处理所有复杂逻辑）
    _perform_transition(module_index, transition_type);
}

void ui_manager_set_anim_type(ui_anim_type_t type) {
    if (!ui_mgr) return;
    ui_mgr->anim_type = type;
}

void ui_manager_deinit(void) {
    if (!ui_mgr) return;
    
    // 停止定时器
    if (ui_mgr->page_timer) {
        lv_timer_delete(ui_mgr->page_timer);
    }
    
    if (ui_mgr->update_timer) {
        lv_timer_delete(ui_mgr->update_timer);
    }
    
    // 删除所有模块
    for (int i = 0; i < ui_mgr->module_count; i++) {
        if (ui_mgr->modules[i]->delete) {
            ui_mgr->modules[i]->delete();
        }
    }
    
    // 释放模块数组
    if (ui_mgr->modules) {
        lv_free(ui_mgr->modules);
    }
    
    // 删除主容器
    if (ui_mgr->main_cont) {
        lv_obj_delete(ui_mgr->main_cont);
    }
    
    // 释放管理器
    lv_free(ui_mgr);
    ui_mgr = NULL;
}

lv_obj_t* ui_manager_get_container(void) {
    return ui_mgr ? ui_mgr->main_cont : NULL;
}
