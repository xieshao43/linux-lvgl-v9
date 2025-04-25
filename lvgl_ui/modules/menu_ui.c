#include "menu_ui.h"
#include "storage_ui.h"
#include "cpu_ui.h" 
#include "music_ui.h"
#include "AI_ui.h" // 添加AI UI头文件引用

// 私有数据结构
typedef struct {
    lv_obj_t *screen;          // 主屏幕（替代之前的panel）
    lv_obj_t *menu_list;       // 菜单列表
    lv_obj_t **menu_items;     // 菜单项数组
    uint8_t item_count;        // 菜单项数量
    uint8_t current_index;     // 当前选中项
    lv_timer_t *button_timer;  // 按钮检测定时器
    bool is_active;            // 菜单是否激活
} menu_ui_data_t;

static menu_ui_data_t ui_data;

// 菜单项定义
#define MENU_ITEM_COUNT 4  // 从3改为4，增加Intelligence选项
static const char* menu_items[MENU_ITEM_COUNT] = {
    "Storage",
    "CPU",
    "Clifford",
    "Music"  
};

// 使用LVGL内置符号 - 优化版本
static const char* menu_icons[MENU_ITEM_COUNT] = {
    LV_SYMBOL_DRIVE,      // 存储图标 - 使用驱动器图标更准确表示存储
    LV_SYMBOL_CHARGE,     // CPU图标 - 使用充能图标表示性能和处理能力
    LV_SYMBOL_PLAY,       // Intelligence图标 - 使用网络连接图标表示智能互联功能
    LV_SYMBOL_AUDIO        // 音乐图标 - 使用播放图标更直观表示音乐功能
};

// 菜单项屏幕创建函数，替换之前的模块索引
static void (*create_screen_functions[MENU_ITEM_COUNT])(void) = {
    storage_ui_create_screen,   // 存储屏幕创建函数
    cpu_ui_create_screen,       // CPU屏幕创建函数
    AI_ui_create_screen,        // Intelligence屏幕创建函数（已实现）
    music_ui_create_screen      // 音乐屏幕创建函数
};

// 增加非线性动画辅助函数，实现贝塞尔曲线过渡效果
static int32_t _bezier_ease_out(int32_t t, int32_t max) {
    // 转换为0-1024范围进行计算
    int32_t normalized = (t * 1024) / max;
    // 缓动公式：贝塞尔曲线实现的缓出效果
    int32_t result = normalized - ((1024 * normalized * (1024 - normalized)) / (1024 * 1024));
    // 转换回原始范围
    return (result * max) / 1024;
}

// 优化：菜单滚动事件处理函数 - 实现更平滑的圆形滚动效果
static void _menu_scroll_event_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target(e);
    
    // 获取容器中心点坐标
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    int32_t cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;
    
    // 计算半径 - 适应小屏幕尺寸调整
    int32_t r = lv_obj_get_height(cont) * 6 / 10;  // 减小半径比例以适应小屏幕
    
    // 遍历所有子项
    uint32_t child_cnt = lv_obj_get_child_count(cont);
    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(item, &child_a);
        
        // 计算子项与容器中心的垂直距离
        int32_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;
        int32_t diff_y = child_y_center - cont_y_center;
        int32_t abs_diff_y = LV_ABS(diff_y);
        
        // X位置计算 - 中心项居中，非中心项右移
        int32_t x;
        if(abs_diff_y < 5) {
            // 中心项居中显示
            x = 0;
        } else {
            // 非中心项右移，距离越远偏移越大
            x = 35 + (abs_diff_y * 25) / r; // 基础偏移35像素，最大可达约60像素
        }
        
        // 应用X轴平移效果
        lv_obj_set_style_translate_x(item, x, 0);
        
        // 获取内容容器和文本组件
        lv_obj_t *content = lv_obj_get_child(item, 0);
        if(!content) continue;
        
        lv_obj_t *icon = lv_obj_get_child(content, 0);
        lv_obj_t *label = lv_obj_get_child(content, 1);
        
        // 应用透明度效果 - 中心项最清晰，其他项半透明
        lv_opa_t base_opa;
        if(abs_diff_y < 5) {
            // 中心项完全不透明
            base_opa = LV_OPA_COVER;
        } else {
            // 其他项半透明 - 但不要太透明
            base_opa = LV_OPA_COVER - (LV_OPA_COVER * abs_diff_y / r) / 3;  // 减少透明度变化幅度
            base_opa = LV_MAX(base_opa, LV_OPA_70); // 修复：使用正确的变量名base_opa而不是base_pa
        }
        
        // 设置项目背景透明度
        lv_obj_set_style_opa(item, base_opa, 0);
        
        // 修复：确保所有文本标签都显示，无论是否为中心项
        if(icon) {
            // 图标文本总是可见
            lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        }
        
        if(label) {
            // 标签文本总是可见
            lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        }
        
        // 应用缩放效果 - 中心项最大，其他项稍小
        int32_t scale_factor;
        if(abs_diff_y < 5) {
            scale_factor = 256; // 中心项100%大小
        } else {
            scale_factor = 256 - ((abs_diff_y * 50) / r); // 其他项缩小
            scale_factor = LV_MAX(scale_factor, 220); // 最小尺寸约85%
        }
        lv_obj_set_style_transform_zoom(item, scale_factor, 0);
        
        // 添加微小的旋转效果 - 增强视觉区分度
        int8_t rotation = 0;
        if(diff_y > 5) {
            rotation = 3; // 下方项顺时针微旋转
        } else if(diff_y < -5) {
            rotation = -3; // 上方项逆时针微旋转
        }
        lv_obj_set_style_transform_angle(item, rotation * 10, 0); // *10 因为LVGL中角度单位是0.1度
    }
}

// 高亮项切换动画回调函数
static void _highlight_anim_cb(void *var, int32_t value) {
    lv_obj_t *item = (lv_obj_t *)var;
    if(!item) return;
    
    // 获取内容容器
    lv_obj_t *content = lv_obj_get_child(item, 0);
    if(!content) return;
    
    // 获取图标和标签对象
    lv_obj_t *icon = lv_obj_get_child(content, 0);
    lv_obj_t *label = lv_obj_get_child(content, 1);
    
    // 计算过渡值 (0-255)
    uint32_t trans = value;
    
    // 1. 左不透明右透明的渐变背景 - 动态过渡
    // 明亮的左侧起点颜色
    lv_obj_set_style_bg_color(item, lv_color_hex(0x29B6F6), 0);
    
    // 右侧终点颜色
    lv_obj_set_style_bg_grad_color(item, lv_color_hex(0x29B6F6), 0);
    
    // 使用动画值调整不透明度
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_opa(item, (trans * LV_OPA_80) / 255, 0);
    lv_obj_set_style_bg_grad_opa(item, (trans * LV_OPA_0) / 255, 0);
    
    // 调整渐变分布
    lv_obj_set_style_bg_grad_dir(item, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_main_stop(item, 0, 0);
    lv_obj_set_style_bg_grad_stop(item, 200, 0);
    
    // 3. 精致的边框效果
    lv_obj_set_style_border_color(item, lv_color_hex(0xADD8E6), 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_opa(item, (trans * LV_OPA_70) / 255, 0);
    lv_obj_set_style_border_side(item, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_TOP, 0);
    
    // 4. 微妙的阴影和光晕效果
    lv_obj_set_style_shadow_width(item, 10, 0);
    lv_obj_set_style_shadow_ofs_x(item, 0, 0);
    lv_obj_set_style_shadow_ofs_y(item, 0, 0);
    lv_obj_set_style_shadow_spread(item, 0, 0);
    lv_obj_set_style_shadow_color(item, lv_color_hex(0x29B6F6), 0);
    lv_obj_set_style_shadow_opa(item, (trans * LV_OPA_40) / 255, 0);
    
    // 5. 高对比度文本 - 平滑过渡 - 确保文字始终可见
    if (icon) {
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0); // 始终完全不透明
    }
    
    if (label) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0); // 始终完全不透明
        
        // 动态调整文字大小
        uint8_t font_size = 14 + ((trans * 2) / 255); // 从14到16
        if (trans > 200) {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        } else {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        }
        
        lv_obj_set_style_text_letter_space(label, (trans * 1) / 255, 0);
    }
}

// 非高亮项过渡动画回调函数
static void _unhighlight_anim_cb(void *var, int32_t value) {
    lv_obj_t *item = (lv_obj_t *)var;
    if(!item) return;
    
    // 获取内容容器
    lv_obj_t *content = lv_obj_get_child(item, 0);
    if(!content) return;
    
    // 获取图标和标签对象
    lv_obj_t *icon = lv_obj_get_child(content, 0);
    lv_obj_t *label = lv_obj_get_child(content, 1);
    
    // 计算过渡值，从255递减到0
    uint32_t trans = value;
    
    // 还原非高亮状态 - 渐变透明效果
    if (trans < 10) {
        // 当值接近0时，重置所有高亮效果，但保持文字可见
        lv_obj_set_style_bg_opa(item, LV_OPA_40, 0);
        lv_obj_set_style_bg_main_opa(item, LV_OPA_40, 0);
        lv_obj_set_style_bg_grad_opa(item, LV_OPA_40, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_shadow_opa(item, LV_OPA_0, 0);
        
        // 恢复普通文本样式 - 但确保文本仍然可见
        if (label) {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_letter_space(label, 0, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0); // 确保文本完全可见
        }
        
        // 确保图标也是可见的
        if (icon) {
            lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0); // 确保图标完全可见
        }
    } else {
        // 渐变过渡 - 背景效果淡出但保持文本可见
        lv_obj_set_style_bg_main_opa(item, (trans * LV_OPA_80) / 255, 0);
        lv_obj_set_style_bg_grad_opa(item, (trans * LV_OPA_0) / 255, 0);
        lv_obj_set_style_border_opa(item, (trans * LV_OPA_70) / 255, 0);
        lv_obj_set_style_shadow_opa(item, (trans * LV_OPA_40) / 255, 0);
        
        // 确保文本和图标在过渡期间始终可见
        if (icon) {
            lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0);
        }
        if (label) {
            lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
        }
    }
}

// 使用动画平滑切换选中的菜单项
static void _highlight_selected_item(void) {
    if (!ui_data.is_active || !ui_data.menu_items) return;
    
    // 滚动到视图
    if (ui_data.current_index < ui_data.item_count) {
        lv_obj_t *item = ui_data.menu_items[ui_data.current_index];
        if (item) {
            // 使用动画将选中项滚动到视图中央
            lv_obj_scroll_to_view(item, LV_ANIM_ON);
        }
    }
    
    static uint8_t previous_index = 0;
    
    // 如果索引没有变化，不需要动画过渡
    if (previous_index == ui_data.current_index) {
        return;
    }
    
    // 从前一个高亮项过渡移除高亮效果
    if (previous_index < ui_data.item_count) {
        lv_obj_t *prev_item = ui_data.menu_items[previous_index];
        if (prev_item) {
            // 创建平滑过渡动画，移除高亮效果
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, prev_item);
            lv_anim_set_values(&a, 255, 0);
            lv_anim_set_time(&a, 500);
            lv_anim_set_exec_cb(&a, _unhighlight_anim_cb);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        }
    }
    
    // 为新的高亮项添加高亮效果动画
    if (ui_data.current_index < ui_data.item_count) {
        lv_obj_t *current_item = ui_data.menu_items[ui_data.current_index];
        if (current_item) {
            // 创建平滑过渡动画，添加高亮效果
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, current_item);
            lv_anim_set_values(&a, 0, 255);
            lv_anim_set_time(&a, 500);
            lv_anim_set_exec_cb(&a, _highlight_anim_cb);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        }
    }
    
    // 更新前一个索引
    previous_index = ui_data.current_index;
    
    // 触发滚动事件更新圆形布局
    lv_obj_send_event(ui_data.menu_list, LV_EVENT_SCROLL, NULL);
}

// 滑动动画回调函数
static void _slide_anim_cb(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t*)var, v);
}

// 处理长按菜单项事件，使用缩放动画过渡到对应屏幕
static void _screen_load_with_zoom_animation(uint8_t screen_index) {
    if (screen_index >= MENU_ITEM_COUNT || create_screen_functions[screen_index] == NULL) {
        #if UI_DEBUG_ENABLED
        printf("[MENU] Error: Invalid screen index or function: %d\n", screen_index);
        #endif
        return;
    }
    
    // 标记为非活动，避免继续处理按钮事件
    ui_data.is_active = false;
    
    // 停止按钮定时器，防止切换过程中干扰
    if (ui_data.button_timer) {
        lv_timer_pause(ui_data.button_timer);
    }
    
    // 获取当前选中的菜单项对象
    if (ui_data.current_index < ui_data.item_count && ui_data.menu_items) {
        lv_obj_t *selected_item = ui_data.menu_items[ui_data.current_index];
        if (selected_item) {
            // 使用通用工具函数实现缩放过渡动画
            ui_utils_zoom_transition(selected_item, create_screen_functions[screen_index]);         
        }
    }
}

// 按钮事件处理定时器回调
static void _button_handler_cb(lv_timer_t *timer) {
    // 安全检查
    if (!ui_data.is_active || !ui_data.menu_items || !ui_data.screen || ui_data.item_count == 0) {
        return;
    }
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    // 清除事件标志
    key355_clear_event();
    
    // 根据按钮事件执行操作
    switch (event) {
        case BUTTON_EVENT_CLICK:
            // 单击：切换选项
            if (ui_data.item_count > 0 && ui_data.is_active) {
                // 更新索引
                ui_data.current_index = (ui_data.current_index + 1) % ui_data.item_count;
                
                // 触发平滑过渡动画
                _highlight_selected_item();
                
                #if UI_DEBUG_ENABLED
                printf("[MENU] Selected item: %d\n", ui_data.current_index);
                #endif
            }
            break;
            
        case BUTTON_EVENT_LONG_PRESS:
            // 长按：选择当前项并进入相应屏幕
            if (ui_data.current_index < ui_data.item_count && ui_data.is_active) {
                uint8_t screen_idx = ui_data.current_index;
                #if UI_DEBUG_ENABLED
                printf("[MENU] Long press detected, switching to screen: %d\n", screen_idx);
                #endif
                
                if (create_screen_functions[screen_idx] != NULL) {
                    // 暂停按钮处理定时器
                    if (ui_data.button_timer) {
                        lv_timer_pause(ui_data.button_timer);
                        #if UI_DEBUG_ENABLED
                        printf("[MENU] Button timer paused\n");
                        #endif
                    }
                    
                    // 切换到目标屏幕
                    _screen_load_with_zoom_animation(screen_idx);
                }
            }
            break;
            
        default:
            break;
    }
}

// 创建菜单屏幕 - 苹果风格优化版
void menu_ui_create_screen(void) {
    // 创建新的屏幕对象
    ui_data.screen = lv_obj_create(NULL);
    
#if LV_USE_DRAW_SW_COMPLEX_GRADIENTS
    // 定义渐变色 - 保留原有的紫色到黑色渐变
    static const lv_color_t grad_colors[2] = {
        LV_COLOR_MAKE(0x9B, 0x18, 0x42), // 紫红色
        LV_COLOR_MAKE(0x00, 0x00, 0x00), // 纯黑色
    };
    
    // 初始化渐变描述符
    static lv_grad_dsc_t grad;
    lv_grad_init_stops(&grad, grad_colors, NULL, NULL, sizeof(grad_colors) / sizeof(lv_color_t));
    
    // 设置径向渐变 - 从中心向四周扩散
    lv_grad_radial_init(&grad, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_BOTTOM, LV_GRAD_EXTEND_PAD);
    
    // 应用渐变背景
    lv_obj_set_style_bg_grad(ui_data.screen, &grad, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#endif
    
    lv_obj_clear_flag(ui_data.screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建圆形滚动菜单容器 - 改进的iOS风格
    ui_data.menu_list = lv_obj_create(ui_data.screen);
    lv_obj_set_size(ui_data.menu_list, 220, 120); // 增加高度以占据更多空间
    lv_obj_align(ui_data.menu_list, LV_ALIGN_CENTER, 0, 0); // 完全居中
    
    // 设置菜单容器样式 - 透明背景
    lv_obj_set_style_bg_opa(ui_data.menu_list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(ui_data.menu_list, 0, 0);
    lv_obj_set_style_pad_all(ui_data.menu_list, 10, 0); // 增加内边距
    
    // 设置垂直滚动布局
    lv_obj_set_flex_flow(ui_data.menu_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_data.menu_list, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 配置滚动行为 - 苹果风格的弹性滚动效果增强
    lv_obj_set_scroll_dir(ui_data.menu_list, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui_data.menu_list, LV_SCROLL_SNAP_CENTER); // 确保项目对齐到中心
    lv_obj_set_scrollbar_mode(ui_data.menu_list, LV_SCROLLBAR_MODE_OFF); // 隐藏滚动条
    
    // 增强苹果风格的滚动体验
    lv_obj_add_flag(ui_data.menu_list, LV_OBJ_FLAG_SCROLL_CHAIN); // 保持滚动链
    lv_obj_add_flag(ui_data.menu_list, LV_OBJ_FLAG_SCROLL_ELASTIC); // 添加弹性滚动效果
    lv_obj_add_flag(ui_data.menu_list, LV_OBJ_FLAG_SCROLL_MOMENTUM); // 添加动量滚动效果
    
    // 自定义滚动动画参数，使滚动更加平滑
    lv_obj_set_scroll_snap_y(ui_data.menu_list, LV_SCROLL_SNAP_CENTER); // 强制吸附到中心
    
    // 设置自定义滚动动画参数
    lv_obj_set_style_anim_time(ui_data.menu_list, 500, 0); // 延长动画时间
    
    // 添加圆形滚动效果事件
    lv_obj_add_event_cb(ui_data.menu_list, _menu_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(ui_data.menu_list, _menu_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);
    
    // 创建菜单项
    ui_data.item_count = MENU_ITEM_COUNT;
    ui_data.menu_items = lv_malloc(sizeof(lv_obj_t*) * ui_data.item_count);
    
    // 创建iOS风格的菜单项
    for (int i = 0; i < ui_data.item_count; i++) {
        // 创建菜单项容器 - 更大的圆角
        lv_obj_t *item = lv_obj_create(ui_data.menu_list);
        lv_obj_set_size(item, 180, 40);
        lv_obj_set_style_radius(item, 12, 0); // 大圆角更符合iOS风格
        lv_obj_set_scrollbar_mode(item, LV_SCROLLBAR_MODE_OFF); 
        
        // 毛玻璃效果 - 使用更自然的渐变色调
        lv_obj_set_style_bg_color(item, lv_color_hex(0x314755), 0); // 保持深青色背景
        lv_obj_set_style_bg_opa(item, LV_OPA_60, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        
        // 添加微妙阴影效果
        lv_obj_set_style_shadow_width(item, 10, 0);
        lv_obj_set_style_shadow_ofs_y(item, 3, 0);
        lv_obj_set_style_shadow_color(item, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(item, LV_OPA_20, 0);
        
        // 改进玻璃质感效果 - 颜色过渡更加平滑
        lv_obj_set_style_bg_grad_dir(item, LV_GRAD_DIR_HOR, 0);
        
        // 左侧起点颜色 - 使用明亮的紫红色，与背景形成呼应
        lv_obj_set_style_bg_color(item, lv_color_hex(0xc94b4b), 0);
        
        // 右侧终点颜色 - 使用深蓝色作为过渡
        lv_obj_set_style_bg_grad_color(item, lv_color_hex(0x4b134f), 0);   
        
        // 调整不透明度 - 左侧较高，整体偏低
        lv_obj_set_style_bg_opa(item, LV_OPA_40, 0);                      
        
        // 调整渐变位置 - 使渐变更加自然
        lv_obj_set_style_bg_main_stop(item, 0, 0);       // 渐变从左边缘开始
        lv_obj_set_style_bg_grad_stop(item, 180, 0);     // 渐变提前结束，使右侧区域更透明
        
        // 添加精细的边框效果 - 顶部和左侧有微妙高光
        lv_obj_set_style_border_color(item, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_opa(item, LV_OPA_20, 0);  // 修改: 使用有效的LV_OPA_20替代LV_OPA_15
        lv_obj_set_style_border_side(item, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_TOP, 0); // 只在左侧和顶部显示边框
        
        // 增强玻璃光泽感 - 添加内部高光
        lv_obj_set_style_shadow_width(item, 20, 0);
        lv_obj_set_style_shadow_spread(item, 0, 0);
        lv_obj_set_style_shadow_ofs_x(item, 5, 0);       // 右移阴影创造左侧明亮效果
        lv_obj_set_style_shadow_ofs_y(item, 0, 0);
        lv_obj_set_style_shadow_color(item, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_shadow_opa(item, LV_OPA_20, 0);  // 修改: 使用有效的LV_OPA_20
        
        // 添加微妙的外阴影增强立体感
        lv_obj_set_style_shadow_width(item, 8, LV_STATE_DEFAULT | LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_x(item, 0, LV_STATE_DEFAULT | LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(item, 2, LV_STATE_DEFAULT | LV_PART_MAIN);
        lv_obj_set_style_shadow_color(item, lv_color_hex(0x000000), LV_STATE_DEFAULT | LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(item, LV_OPA_20, LV_STATE_DEFAULT | LV_PART_MAIN);
        
        // 内容容器
        lv_obj_t *content = lv_obj_create(item);
        lv_obj_set_size(content, 160, 30);
        lv_obj_set_style_bg_opa(content, LV_OPA_0, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_all(content, 0, 0);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_center(content);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        
        // 创建图标 - 使用渐变透明效果的白色图标
        lv_obj_t *icon = lv_label_create(content);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0);  // 确保图标始终可见
        lv_label_set_text(icon, menu_icons[i]);
        
        // 创建标签 - 使用清晰但柔和的白色文字
        lv_obj_t *label = lv_label_create(content);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);  // 修复：应该设置label的不透明度，而不是icon
        lv_label_set_text(label, menu_items[i]);
        lv_obj_set_style_pad_left(label, 15, 0);
        
        ui_data.menu_items[i] = item;
    }
    
    // 初始化选择索引
    ui_data.current_index = 0;
    
    // 创建按钮检测定时器
    ui_data.button_timer = lv_timer_create(_button_handler_cb, 50, NULL);
    
    // 设置菜单激活状态
    ui_data.is_active = true;
    
    // 执行初始高亮和滚动
    _highlight_selected_item();
    
    // 确保第一个菜单项在视图中央
    lv_obj_scroll_to_view(lv_obj_get_child(ui_data.menu_list, 0), LV_ANIM_OFF);
    
    // 触发一次滚动事件回调，以初始化视觉效果
    lv_obj_send_event(ui_data.menu_list, LV_EVENT_SCROLL, NULL);
    
    // 检查是否是从其他页面返回
    bool from_other_screen = (lv_scr_act() != NULL && lv_scr_act() != ui_data.screen);
    
    // 根据是否从其他页面返回选择加载方式
    if (from_other_screen) {
        // 从其他页面返回时使用
        lv_scr_load_anim(ui_data.screen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, true);
    } else {
        // 首次加载时使用淡入效果
        lv_scr_load_anim(ui_data.screen, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, true);
    }
}

// 优化当切换回菜单时调用的函数
void menu_ui_set_active(void) {
    // 确保菜单处于活跃状态
    ui_data.is_active = true;
    
    // 检查按钮定时器是否存在并正常工作
    if (ui_data.button_timer) {
        lv_timer_resume(ui_data.button_timer);
        lv_timer_reset(ui_data.button_timer); // 重置定时器，确保它立即开始工作
    } else {
        // 如果定时器不存在，创建一个新的
        ui_data.button_timer = lv_timer_create(_button_handler_cb, 50, NULL);
    }
    
    // 更新高亮显示并滚动到正确位置
    _highlight_selected_item();
    
    // 触发一次滚动事件来更新视觉效果
    if (ui_data.menu_list) {
        lv_obj_send_event(ui_data.menu_list, LV_EVENT_SCROLL, NULL);
    }
    
   
    //lv_obj_invalidate(ui_data.screen);
    //lv_refr_now(lv_display_get_default());浪费了太多性能，而且导致不流畅卡顿，暂时不需要
    
    #if UI_DEBUG_ENABLED
    printf("[MENU] Menu activated, button timer: %p\n", (void*)ui_data.button_timer);
    #endif
}
