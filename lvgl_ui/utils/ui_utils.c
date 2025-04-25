#include "ui_utils.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

// 页面过渡动画数据结构
typedef struct {
    lv_obj_t *page;       // 页面对象
    bool destroy;         // 是否销毁
    lv_anim_ready_cb_t user_cb; // 用户回调函数
    void *user_data;      // 添加用户数据指针，用于存储模块索引等信息
} ui_page_anim_data_t;

// 基本动画辅助函数
static void _ui_anim_opacity_cb(void *obj, int32_t value) {
    lv_obj_set_style_opa(obj, value, 0);
}

static void _ui_anim_width_cb(void *obj, int32_t value) {
    lv_obj_set_width(obj, value);
}

// 动画完成回调
static void _ui_page_anim_completed(lv_anim_t *a) {
    ui_page_anim_data_t *data = (ui_page_anim_data_t *)a->user_data;
    
    if (data) {
        // 执行用户回调
        if (data->user_cb) {
            data->user_cb(a);
        }
        
        // 销毁对象（如果需要）
        if (data->destroy && data->page) {
            lv_obj_delete(data->page);
        }
        
        // 释放动画数据
        free(data);
    }
}

// 将KB大小转换为可读字符串
void ui_utils_size_to_str(uint64_t size_kb, char *buf, int buf_size) {
    const char *units[] = {"KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = (double)size_kb;
    
    while (size >= 1024 && unit_idx < 3) {
        size /= 1024;
        unit_idx++;
    }
    
    if (size < 10) {
        snprintf(buf, buf_size, "%.1f %s", size, units[unit_idx]);
    } else {
        snprintf(buf, buf_size, "%.0f %s", size, units[unit_idx]);
    }
}

// 创建带过渡动画的环形图
void ui_utils_animate_arc(lv_obj_t *arc, int32_t start_value, int32_t end_value, uint16_t time_ms) {
    if (!arc) return;
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_values(&a, start_value, end_value);
    lv_anim_set_time(&a, time_ms);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// 简化的进度条动画函数
void ui_utils_animate_bar(lv_obj_t *bar, int32_t start_value, int32_t end_value, uint16_t time_ms) {
    if (!bar) return;
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, start_value, end_value);
    lv_anim_set_time(&a, time_ms);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// 添加苹果风格的弹性回弹动画路径
int32_t ui_utils_anim_path_overshoot(const lv_anim_t * a)
{
    int32_t t = lv_map(a->act_time, 0, a->duration, 0, 1024);
    
    if(t < 512) { 
        return lv_bezier3(t * 2, 0, 80, 1000, 1024);
    }
    else { 
        t = t - 512;
        int32_t step = (((t * 350) >> 10) + 180) % 360;
        int32_t value = 1024 + lv_trigo_sin(step) * 40 / LV_TRIGO_SIN_MAX;
        return value;
    }
}

// 创建页面过渡动画
void ui_utils_page_transition(lv_obj_t *old_page, lv_obj_t *new_page, ui_anim_type_t anim_type, uint32_t duration_ms, bool destroy_old, lv_anim_ready_cb_t user_cb) {
    // 确保新页面存在
    if (new_page == NULL) return;
    
    // 获取屏幕尺寸
    lv_coord_t scr_width = lv_display_get_horizontal_resolution(NULL);
    lv_coord_t scr_height = lv_display_get_vertical_resolution(NULL);
    
    // 确保新页面可见
    lv_obj_clear_flag(new_page, LV_OBJ_FLAG_HIDDEN);
    
    // 处理旧页面的退出动画
    if (old_page != NULL) {
        ui_page_anim_data_t *exit_data = (ui_page_anim_data_t *)malloc(sizeof(ui_page_anim_data_t));
        if (!exit_data) return;
        
        exit_data->page = old_page;
        exit_data->destroy = destroy_old;
        exit_data->user_cb = NULL;
        exit_data->user_data = NULL; // 初始化用户数据为NULL
        
        lv_anim_t exit_anim;
        lv_anim_init(&exit_anim);
        lv_anim_set_var(&exit_anim, old_page);
        lv_anim_set_time(&exit_anim, duration_ms);
        lv_anim_set_user_data(&exit_anim, exit_data);
        lv_anim_set_ready_cb(&exit_anim, _ui_page_anim_completed);
        
        // 设置退出动画效果
        switch (anim_type) {
            case ANIM_FADE:
                lv_anim_set_values(&exit_anim, LV_OPA_COVER, LV_OPA_TRANSP);
                lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
                lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
                break;
                
            case ANIM_SLIDE_LEFT:
                lv_anim_set_values(&exit_anim, 0, -scr_width);
                lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
                lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
                break;
                
            case ANIM_SLIDE_RIGHT:
                lv_anim_set_values(&exit_anim, 0, scr_width);
                lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
                lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
                break;
                
            case ANIM_FADE_SCALE:
                lv_obj_set_style_transform_pivot_x(old_page, lv_pct(50), 0);
                lv_obj_set_style_transform_pivot_y(old_page, lv_pct(50), 0);
                
                lv_anim_set_values(&exit_anim, LV_ZOOM_NONE, LV_ZOOM_NONE - 30);
                lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_scale);
                lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
                
                // 透明度动画
                lv_anim_t opa_anim;
                lv_anim_init(&opa_anim);
                lv_anim_set_var(&opa_anim, old_page);
                lv_anim_set_time(&opa_anim, duration_ms);
                lv_anim_set_values(&opa_anim, LV_OPA_COVER, LV_OPA_TRANSP);
                lv_anim_set_exec_cb(&opa_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
                lv_anim_start(&opa_anim);
                break;
                
            case ANIM_NONE:
            default:
                if (destroy_old) {
                    lv_obj_delete(old_page);
                }
                free(exit_data);
                exit_data = NULL;
                break;
        }
        
        if (anim_type != ANIM_NONE) {
            lv_anim_start(&exit_anim);
        }
    }
    
    // 处理新页面的进入动画
    ui_page_anim_data_t *enter_data = (ui_page_anim_data_t *)malloc(sizeof(ui_page_anim_data_t));
    if (!enter_data) return;
    
    enter_data->page = new_page;
    enter_data->destroy = false;
    enter_data->user_cb = user_cb;
    enter_data->user_data = NULL; // 初始化用户数据为NULL
    
    // 根据动画类型设置初始状态
    switch (anim_type) {
        case ANIM_FADE:
            lv_obj_set_style_opa(new_page, LV_OPA_TRANSP, 0);
            break;
            
        case ANIM_SLIDE_LEFT:
            lv_obj_set_x(new_page, scr_width);
            break;
            
        case ANIM_SLIDE_RIGHT:
            lv_obj_set_x(new_page, -scr_width);
            break;
            
        case ANIM_FADE_SCALE:
            lv_obj_set_style_transform_pivot_x(new_page, lv_pct(50), 0);
            lv_obj_set_style_transform_pivot_y(new_page, lv_pct(50), 0);
            lv_obj_set_style_transform_scale(new_page, LV_ZOOM_NONE - 30, 0);
            lv_obj_set_style_opa(new_page, LV_OPA_TRANSP, 0);
            break;
            
        case ANIM_NONE:
        default:
            free(enter_data);
            if (user_cb) {
                lv_anim_t dummy;
                user_cb(&dummy);
            }
            return;
    }
    
    // 创建进入动画
    lv_anim_t enter_anim;
    lv_anim_init(&enter_anim);
    lv_anim_set_var(&enter_anim, new_page);
    lv_anim_set_time(&enter_anim, duration_ms);
    lv_anim_set_user_data(&enter_anim, enter_data);
    lv_anim_set_ready_cb(&enter_anim, _ui_page_anim_completed);
    
    // 设置进入动画效果
    switch (anim_type) {
        case ANIM_FADE:
            lv_anim_set_values(&enter_anim, LV_OPA_TRANSP, LV_OPA_COVER);
            lv_anim_set_exec_cb(&enter_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&enter_anim, lv_anim_path_ease_in);
            break;
            
        case ANIM_SLIDE_LEFT:
        case ANIM_SLIDE_RIGHT:
            lv_anim_set_values(&enter_anim, lv_obj_get_x(new_page), 0);
            lv_anim_set_exec_cb(&enter_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
            lv_anim_set_path_cb(&enter_anim, lv_anim_path_ease_out);
            break;
            
        case ANIM_FADE_SCALE:
            lv_anim_set_values(&enter_anim, LV_ZOOM_NONE - 30, LV_ZOOM_NONE);
            lv_anim_set_exec_cb(&enter_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_scale);
            lv_anim_set_path_cb(&enter_anim, lv_anim_path_ease_out);
            
            // 透明度动画
            lv_anim_t opa_anim;
            lv_anim_init(&opa_anim);
            lv_anim_set_var(&opa_anim, new_page);
            lv_anim_set_values(&opa_anim, LV_OPA_TRANSP, LV_OPA_COVER);
            lv_anim_set_time(&opa_anim, duration_ms);
            lv_anim_set_exec_cb(&opa_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_start(&opa_anim);
            break;
    }
    
    lv_anim_start(&enter_anim);
}

// 创建进入动画
void ui_utils_page_enter_anim(lv_obj_t *page, ui_anim_type_t anim_type, uint32_t duration_ms, lv_anim_ready_cb_t user_cb) {
    if (page == NULL) return;
    ui_utils_page_transition(NULL, page, anim_type, duration_ms, false, user_cb);
}

// 创建退出动画
void ui_utils_page_exit_anim(lv_obj_t *page, ui_anim_type_t anim_type, uint32_t duration_ms, bool destroy, lv_anim_ready_cb_t user_cb) {
    if (page == NULL) return;
    
    ui_page_anim_data_t *exit_data = (ui_page_anim_data_t *)malloc(sizeof(ui_page_anim_data_t));
    if (!exit_data) return;
    
    exit_data->page = page;
    exit_data->destroy = destroy;
    exit_data->user_cb = user_cb;
    exit_data->user_data = NULL; // 初始化用户数据为NULL
    
    lv_coord_t scr_width = lv_display_get_horizontal_resolution(NULL);
    lv_coord_t scr_height = lv_display_get_vertical_resolution(NULL);
    
    lv_anim_t exit_anim;
    lv_anim_init(&exit_anim);
    lv_anim_set_var(&exit_anim, page);
    lv_anim_set_time(&exit_anim, duration_ms);
    lv_anim_set_user_data(&exit_anim, exit_data);
    lv_anim_set_ready_cb(&exit_anim, _ui_page_anim_completed);
    
    switch (anim_type) {
        case ANIM_FADE:
            lv_anim_set_values(&exit_anim, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
            break;
            
        case ANIM_SLIDE_LEFT:
            lv_anim_set_values(&exit_anim, 0, -scr_width);
            lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
            lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
            break;
            
        case ANIM_SLIDE_RIGHT:
            lv_anim_set_values(&exit_anim, 0, scr_width);
            lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
            lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
            break;
            
        case ANIM_FADE_SCALE:
            lv_obj_set_style_transform_pivot_x(page, lv_pct(50), 0);
            lv_obj_set_style_transform_pivot_y(page, lv_pct(50), 0);
            
            lv_anim_set_values(&exit_anim, LV_ZOOM_NONE, LV_ZOOM_NONE - 30);
            lv_anim_set_exec_cb(&exit_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_scale);
            lv_anim_set_path_cb(&exit_anim, lv_anim_path_ease_out);
            
            // 透明度动画
            lv_anim_t opa_anim;
            lv_anim_init(&opa_anim);
            lv_anim_set_var(&opa_anim, page);
            lv_anim_set_values(&opa_anim, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_time(&opa_anim, duration_ms);
            lv_anim_set_exec_cb(&opa_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_start(&opa_anim);
            break;
            
        case ANIM_NONE:
        default:
            if (destroy) {
                lv_obj_delete(page);
            } else {
                lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
            }
            
            if (user_cb) {
                lv_anim_t dummy;
                user_cb(&dummy);
            }
            
            free(exit_data);
            return;
    }
    
    lv_anim_start(&exit_anim);
}

// 创建自定义页面转换动画函数示例
void custom_screen_transition(lv_obj_t *old_screen, lv_obj_t *new_screen) {
    // 步骤A：触发事件（这里是函数调用）
    
    // 步骤B：检查是否需要动画
    bool use_animation = true; // 可以根据条件判断
    
    if (use_animation) {
        // 步骤C：创建动画
        lv_anim_t a;
        lv_anim_init(&a);
        
        // 步骤E：设置动画参数
        lv_anim_set_var(&a, old_screen);
        lv_anim_set_values(&a, 0, -240);  // 从0到-240像素
        // 使用已定义的动画回调函数，而不是未声明的函数
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);  // 使用水平滑动动画
        lv_anim_set_ready_cb(&a, _ui_page_anim_completed);  // 使用已定义的完成回调
        
        // 为回调函数准备数据
        ui_page_anim_data_t *exit_data = (ui_page_anim_data_t *)malloc(sizeof(ui_page_anim_data_t));
        if (exit_data) {
            exit_data->page = old_screen;
            exit_data->destroy = true;
            exit_data->user_cb = NULL;
            exit_data->user_data = NULL;
            lv_anim_set_user_data(&a, exit_data);
        }
        
        // 步骤F：启动动画
        lv_anim_start(&a);
        
        // 步骤G-I：LVGL内部自动处理动画帧执行和完成检查
    } else {
        // 步骤D：直接更新
        lv_obj_del(old_screen);
        lv_scr_load(new_screen);
    }
}

// 添加更可靠的屏幕过渡函数
void ui_utils_screen_transition(lv_obj_t *old_screen, void (*create_new_screen)(void), ui_anim_type_t anim_type) {
    // 步骤A和B：是否使用动画
    bool use_animation = (anim_type != ANIM_NONE);
    
    // 暂停渲染
    lv_timer_t* ticker = lv_display_get_refr_timer(lv_display_get_default());
    if (ticker) lv_timer_pause(ticker);
    
    // 创建新屏幕
    create_new_screen();
    lv_obj_t *new_screen = lv_scr_act();
    
    if (use_animation) {
        // 配置旧屏幕出场动画
        lv_anim_t a_out;
        lv_anim_init(&a_out);
        lv_anim_set_var(&a_out, old_screen);
        
        // 配置新屏幕入场动画
        lv_anim_t a_in;
        lv_anim_init(&a_in);
        lv_anim_set_var(&a_in, new_screen);
        
        // 根据动画类型设置参数
        switch (anim_type) {
            case ANIM_SLIDE_LEFT:
                // 设置滑动效果参数...
                break;
            case ANIM_FADE:
                // 设置淡入淡出效果参数...
                break;
            // 其他动画类型...
        }
        
        // 恢复渲染
        if (ticker) lv_timer_resume(ticker);
        
        // 启动动画
        lv_anim_start(&a_out);
        lv_anim_start(&a_in);
    } else {
        // 直接切换
        lv_obj_del(old_screen);
        
        // 恢复渲染
        if (ticker) lv_timer_resume(ticker);
    }
}

// 颜色过渡动画回调 - 更新菜单背景色为渐变效果
static void _ui_anim_color_cb(void *var, int32_t v) {
    lv_obj_t *obj = (lv_obj_t *)var;
    
    // 精确控制颜色过渡：从蓝色(0x26a0da)到紫红色(0x9B1842)
    uint8_t r = lv_map(v, 0, 255, 0x26, 0x9B);
    uint8_t g = lv_map(v, 0, 255, 0xa0, 0x18);
    uint8_t b = lv_map(v, 0, 255, 0xda, 0x42);
    
    lv_obj_set_style_bg_color(obj, lv_color_make(r, g, b), 0);
    
    // 创建动态渐变效果 - 逐渐从无渐变过渡到有渐变
    if (v > 128) {
        // 设置渐变颜色并调整渐变强度
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
        
        // 动态调整渐变不透明度，使过渡更平滑
        lv_opa_t grad_opa = lv_map(v, 129, 255, LV_OPA_10, LV_OPA_40);
        lv_obj_set_style_bg_grad_opa(obj, grad_opa, 0);
    }
}

// 增强玻璃效果动画回调
static void _ui_anim_glass_effect_cb(void *var, int32_t v) {
    lv_obj_t *obj = (lv_obj_t *)var;
    
    // 动态调整阴影宽度，创造立体感变化
    uint8_t shadow_width = lv_map(v, 0, 255, 15, 25);
    lv_obj_set_style_shadow_width(obj, shadow_width, 0);
    
    // 动态调整模糊效果
    uint8_t blur = lv_map(v, 0, 255, 0, 5);
    lv_obj_set_style_shadow_spread(obj, blur, 0);
}

// 卡片淡出完成后的回调
static void _card_fade_ready_cb(lv_anim_t *a) {
    // 删除过渡卡片
    lv_obj_t *card = (lv_obj_t *)a->var;
    if (card) {
        lv_obj_del(card);
    }
}

// 延迟加载定时器回调 - 将lambda函数改为普通C函数
static void _delayed_load_timer_cb(lv_timer_t *t) {
    // 正确使用LVGL API来获取用户数据，而不是直接访问内部结构
    void **timer_data = (void**)lv_timer_get_user_data(t);
    if (!timer_data) {
        lv_timer_del(t);
        return;
    }
    
    void (*create_func)(void) = (void (*)(void))timer_data[0];
    lv_obj_t *transition_card = (lv_obj_t*)timer_data[1];
    
    // 如果有创建函数，则创建新屏幕
    if (create_func) {
        create_func();
    }
    
    // 获取创建的新屏幕
    lv_obj_t *new_screen = lv_scr_act();
    
    // 将新屏幕初始设为透明
    lv_obj_set_style_opa(new_screen, LV_OPA_0, 0);
    
    // 确保卡片在最前面
    if (transition_card) {
        lv_obj_move_foreground(transition_card);
        
        // 创建卡片消失动画
        lv_anim_t card_fade;
        lv_anim_init(&card_fade);
        lv_anim_set_var(&card_fade, transition_card);
        lv_anim_set_time(&card_fade, 300);  // 增加淡出时间使过渡更平滑
        lv_anim_set_values(&card_fade, LV_OPA_COVER, LV_OPA_0);
        lv_anim_set_exec_cb(&card_fade, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_path_cb(&card_fade, lv_anim_path_ease_out);
        lv_anim_set_ready_cb(&card_fade, _card_fade_ready_cb);
        lv_anim_start(&card_fade);
    }
    
    // 新屏幕的平滑淡入动画
    lv_anim_t fade_in;
    lv_anim_init(&fade_in);
    lv_anim_set_var(&fade_in, new_screen);
    lv_anim_set_delay(&fade_in, 100); // 轻微延迟，让卡片先开始淡出
    lv_anim_set_time(&fade_in, 400);  // 更长的淡入时间，使过渡更平滑
    lv_anim_set_values(&fade_in, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_exec_cb(&fade_in, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_path_cb(&fade_in, lv_anim_path_ease_out);
    lv_anim_start(&fade_in);
    
    // 删除定时器和数据
    free(timer_data);
    lv_timer_del(t);
}

// iOS风格缩放动画的完成回调 - 实现流畅过渡
static void _zoom_anim_ready_cb(lv_anim_t *a) {
    // 获取过渡卡片
    lv_obj_t *transition_card = (lv_obj_t *)a->var;
    if (!transition_card) return;
    
    // 获取动画完成回调中存储的屏幕创建函数
    void (*create_func)(void) = (void (*)(void))a->user_data;
    
    // 获取原菜单屏幕
    lv_obj_t *old_screen = lv_scr_act();
    
    // 提前开始淡出菜单项，使过渡更平滑
    if (old_screen) {
        lv_obj_clean(old_screen);
        uint32_t child_count = lv_obj_get_child_count(old_screen);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(old_screen, i);
            if (child && child != transition_card) {
                // 创建快速淡出效果
                lv_anim_t fade_anim;
                lv_anim_init(&fade_anim);
                lv_anim_set_var(&fade_anim, child);
                lv_anim_set_time(&fade_anim, 200); // 更快的淡出速度
                lv_anim_set_values(&fade_anim, LV_OPA_COVER, LV_OPA_0);
                lv_anim_set_exec_cb(&fade_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
                lv_anim_set_path_cb(&fade_anim, lv_anim_path_ease_in); // 使用ease-in让淡出更自然
                lv_anim_start(&fade_anim);
            }
        }
    }
    
    // 在卡片完全展开后、新屏幕加载前增加短暂停顿
    // 准备传递给定时器的数据
    void **timer_data = malloc(2 * sizeof(void*));
    if (timer_data) {
        timer_data[0] = (void*)create_func;
        timer_data[1] = transition_card;
        
        // 使用正常C函数而不是lambda，并正确设置用户数据
        lv_timer_t *delayed_load = lv_timer_create(_delayed_load_timer_cb, 
                                                  50, // 150ms延迟
                                                  timer_data);
                                                  
        if (!delayed_load) {
            // 创建定时器失败，释放内存并直接执行
            free(timer_data);
            if (create_func) {
                create_func();
            }
        }
    } else {
        // 内存分配失败，直接调用创建函数
        if (create_func) {
            create_func();
        }
    }
}

// 实现iOS风格的缩放过渡动画
void ui_utils_zoom_transition(lv_obj_t *selected_item, void (*create_screen_func)(void)) {
    if (!selected_item || !create_screen_func) {
        return;
    }
    
    // 获取选中项的位置和尺寸
    lv_area_t item_coords;
    lv_obj_get_coords(selected_item, &item_coords);
    
    // 获取选中项的样式以匹配颜色和外观
    lv_color_t item_color = lv_obj_get_style_bg_color(selected_item, 0);
    lv_coord_t item_radius = lv_obj_get_style_radius(selected_item, 0);
    
    // 创建一个临时对象作为过渡用的"卡片"
    lv_obj_t *transition_card = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(transition_card, item_coords.x1, item_coords.y1);
    lv_obj_set_size(transition_card, lv_area_get_width(&item_coords), lv_area_get_height(&item_coords));
    
    // 精确匹配选中项外观
    lv_obj_set_style_border_width(transition_card, 0, 0);
    lv_obj_set_style_radius(transition_card, item_radius, 0);
    lv_obj_set_style_bg_color(transition_card, item_color, 0);
    lv_obj_set_style_bg_opa(transition_card, 220, 0);
    lv_obj_set_style_shadow_width(transition_card, 15, 0);
    lv_obj_set_style_shadow_color(transition_card, lv_color_hex(0x26a0da), 0);
    lv_obj_set_style_shadow_opa(transition_card, LV_OPA_40, 0);
    
    // 添加内容复制逻辑 - 复制选中项内容的图像到卡片
    lv_obj_t *content = lv_obj_get_child(selected_item, 0);
    if (content) {
        lv_obj_t *clone_content = lv_obj_create(transition_card);
        lv_obj_set_size(clone_content, lv_obj_get_width(content), lv_obj_get_height(content));
        lv_obj_set_style_bg_opa(clone_content, LV_OPA_0, 0);
        lv_obj_set_style_border_width(clone_content, 0, 0);
        lv_obj_center(clone_content);
        
        // 复制图标和标签
        lv_obj_t *icon = lv_obj_get_child(content, 0);
        lv_obj_t *clone_icon = NULL;  // 初始化为NULL
        
        if (icon) {
            clone_icon = lv_label_create(clone_content);
            lv_obj_set_style_text_font(clone_icon, lv_obj_get_style_text_font(icon, 0), 0);
            lv_obj_set_style_text_color(clone_icon, lv_color_white(), 0);
            lv_label_set_text(clone_icon, lv_label_get_text(icon));
            lv_obj_align(clone_icon, LV_ALIGN_LEFT_MID, 0, 0);
        }
        
        lv_obj_t *label = lv_obj_get_child(content, 1);
        if (label) {
            lv_obj_t *clone_label = lv_label_create(clone_content);
            lv_obj_set_style_text_font(clone_label, lv_obj_get_style_text_font(label, 0), 0);
            lv_obj_set_style_text_color(clone_label, lv_color_white(), 0);
            lv_label_set_text(clone_label, lv_label_get_text(label));
            
            // 修复：只有当clone_icon存在时才对齐到它
            if (clone_icon) {
                lv_obj_align_to(clone_label, clone_icon, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
            } else {
                lv_obj_align(clone_label, LV_ALIGN_CENTER, 0, 0);
            }
        }
    }
    
    // 将过渡卡片置于最前方
    lv_obj_move_foreground(transition_card);
    
    // 获取屏幕尺寸
    lv_coord_t scr_width = lv_display_get_horizontal_resolution(NULL);
    lv_coord_t scr_height = lv_display_get_vertical_resolution(NULL);
    
    // 计算终点位置，完全覆盖屏幕
    lv_coord_t end_width = scr_width + 20;
    lv_coord_t end_height = scr_height + 20;
    lv_coord_t end_x = -10;
    lv_coord_t end_y = -10;
    
    // 创建卡片动画序列 - 优化时序
    lv_anim_t a_pos_x, a_pos_y, a_width, a_height, a_radius, a_color, a_glass;
    uint16_t anim_time = 400; // 增加动画时间使过渡更加平滑
    
    // 1. 位置X动画 - 使用缓动函数创造更自然的运动
    lv_anim_init(&a_pos_x);
    lv_anim_set_var(&a_pos_x, transition_card);
    lv_anim_set_values(&a_pos_x, item_coords.x1, end_x);
    lv_anim_set_exec_cb(&a_pos_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a_pos_x, anim_time);
    lv_anim_set_path_cb(&a_pos_x, lv_anim_path_ease_out);
    
    // 2. 位置Y动画
    lv_anim_init(&a_pos_y);
    lv_anim_set_var(&a_pos_y, transition_card);
    lv_anim_set_values(&a_pos_y, item_coords.y1, end_y);
    lv_anim_set_exec_cb(&a_pos_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_pos_y, anim_time);
    lv_anim_set_path_cb(&a_pos_y, lv_anim_path_ease_out);
    
    // 3. 宽度动画 - 稍快的扩展速度
    lv_anim_init(&a_width);
    lv_anim_set_var(&a_width, transition_card);
    lv_anim_set_values(&a_width, lv_area_get_width(&item_coords), end_width);
    lv_anim_set_exec_cb(&a_width, (lv_anim_exec_xcb_t)lv_obj_set_width);
    lv_anim_set_time(&a_width, anim_time - 20); // 略微提前完成
    lv_anim_set_path_cb(&a_width, lv_anim_path_ease_out);
    
    // 4. 高度动画
    lv_anim_init(&a_height);
    lv_anim_set_var(&a_height, transition_card);
    lv_anim_set_values(&a_height, lv_area_get_height(&item_coords), end_height);
    lv_anim_set_exec_cb(&a_height, (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_time(&a_height, anim_time);
    lv_anim_set_path_cb(&a_height, lv_anim_path_ease_out);
    
    // 在卡片内容变化到一定程度后才隐藏内容
    if (content) {
        lv_obj_t *clone_content = lv_obj_get_child(transition_card, 0);
        if (clone_content) {
            lv_anim_t fade_content;
            lv_anim_init(&fade_content);
            lv_anim_set_var(&fade_content, clone_content);
            lv_anim_set_values(&fade_content, LV_OPA_COVER, LV_OPA_0);
            lv_anim_set_time(&fade_content, anim_time / 2);
            lv_anim_set_exec_cb(&fade_content, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&fade_content, lv_anim_path_ease_out);
            lv_anim_start(&fade_content);
        }
    }
    
    // 5. 圆角动画 - 从圆角渐变到无圆角
    lv_anim_init(&a_radius);
    lv_anim_set_var(&a_radius, transition_card);
    lv_anim_set_values(&a_radius, item_radius, 0);
    lv_anim_set_exec_cb(&a_radius, (lv_anim_exec_xcb_t)lv_obj_set_style_radius);
    lv_anim_set_time(&a_radius, anim_time * 0.7); // 更短的时间让圆角先消失
    lv_anim_set_path_cb(&a_radius, lv_anim_path_ease_in); // 使用ease-in让变化更早发生
    
    // 6. 颜色渐变动画 - 从当前颜色到目标颜色
    lv_anim_init(&a_color);
    lv_anim_set_var(&a_color, transition_card);
    lv_anim_set_values(&a_color, 0, 255);
    lv_anim_set_exec_cb(&a_color, _ui_anim_color_cb);
    lv_anim_set_time(&a_color, anim_time + 50); // 延长颜色变化时间，使其在尺寸变化后继续
    lv_anim_set_path_cb(&a_color, lv_anim_path_linear); // 线性颜色变化效果最好
    
    // 7. 玻璃效果动画 - 增强立体质感变化
    lv_anim_init(&a_glass);
    lv_anim_set_var(&a_glass, transition_card);
    lv_anim_set_values(&a_glass, 0, 255);
    lv_anim_set_exec_cb(&a_glass, _ui_anim_glass_effect_cb);
    lv_anim_set_time(&a_glass, anim_time + 100); // 最长的动画时间
    lv_anim_set_path_cb(&a_glass, lv_anim_path_overshoot); // 使用弹性效果
    
    // 存储屏幕创建函数并设置回调
    lv_anim_set_user_data(&a_height, (void*)create_screen_func);
    lv_anim_set_ready_cb(&a_height, _zoom_anim_ready_cb);
    
    // 启动所有动画
    lv_anim_start(&a_pos_x);
    lv_anim_start(&a_pos_y);
    lv_anim_start(&a_width);
    lv_anim_start(&a_height);
    lv_anim_start(&a_radius);
    lv_anim_start(&a_color);
    lv_anim_start(&a_glass);
    
    // 添加音效或触觉反馈逻辑可以在这里插入
}


