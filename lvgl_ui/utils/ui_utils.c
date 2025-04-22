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

// 大部分圆弧过渡动画已经移除，只保留必要的基本动画函数
// 之前的 create_transition_animation, create_reverse_transition_animation 等函数已移除


