#include "AI_ui.h"
#include "menu_ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 定义其他在common.h中没有的颜色
#define COLOR_BACKGROUND    lv_color_hex(0x121212)
#define COLOR_SURFACE       lv_color_hex(0x1E1E1E)
#define COLOR_TEXT_PRIMARY  lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_SECONDARY lv_color_hex(0xDDDDDD)
#define COLOR_AI_PRIMARY    lv_color_hex(0x3498DB) // 添加AI主题蓝色

// 动画时间常量
#define ANIM_TIME_DEFAULT   300
#define ANIM_TIME_LONG      500

// 简化的AI UI数据结构
typedef struct {
    lv_obj_t *screen;            // 主屏幕
    lv_obj_t *ai_avatar;         // AI头像容器
    lv_obj_t *ai_lottie;         // Lottie动画对象
    lv_obj_t *title;             // 标题
    lv_obj_t *status;            // 状态文本
    lv_obj_t *dialog_container;  // 对话容器
    lv_obj_t *dialog_text;       // 对话文本
    lv_timer_t *button_timer;    // 按钮检测定时器
    bool is_active;              // 是否活动状态
} ai_ui_data_t;

// AI信息
typedef struct {
    const char *title;
    const char *status;
    const char *dialog;
} ai_info_t;

// 全局UI数据
static ai_ui_data_t ui_data = {0};

// AI示例数据
static ai_info_t current_ai = {
    .title = "Clifford",
    .status = "ACTIVE",
    .dialog = "Welcome to AI Assistant!\n\n> How can I help you today?\n\n> I can process text and image queries\n\n> I can answer questions and provide information\n\n> Double-click to return to main menu\n\n> This interface demonstrates AI capabilities\n\n> Try different voice commands\n\n> Ask me about weather, news, or facts"
};

// 函数前向声明
static void return_to_menu(void);
static void button_event_timer_cb(lv_timer_t *timer);

// AI界面创建函数
void AI_ui_create_screen(void) {
    // 创建基本屏幕
    ui_data.screen = lv_obj_create(NULL);
    
    // 应用与其他模块一致的渐变背景
#if LV_USE_DRAW_SW_COMPLEX_GRADIENTS
    // 定义渐变色 - 使用蓝色调渐变，暗示AI/科技感
    static const lv_color_t grad_colors[2] = {
        LV_COLOR_MAKE(0x18, 0x42, 0x9B), // 蓝色调
        LV_COLOR_MAKE(0x00, 0x00, 0x00), // 纯黑色
    };
    
    static lv_grad_dsc_t grad;
    lv_grad_init_stops(&grad, grad_colors, NULL, NULL, sizeof(grad_colors) / sizeof(lv_color_t));
    lv_grad_radial_init(&grad, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_BOTTOM, LV_GRAD_EXTEND_PAD);
    
    // 应用渐变背景
    lv_obj_set_style_bg_grad(ui_data.screen, &grad, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#else
    // 纯色备选方案
    lv_obj_set_style_bg_color(ui_data.screen, COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#endif
    
    lv_obj_clear_flag(ui_data.screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建标题 - 顶部居中
    ui_data.title = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_data.title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_width(ui_data.title, 220);
    lv_obj_set_style_text_align(ui_data.title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ui_data.title, current_ai.title);
    lv_obj_align(ui_data.title, LV_ALIGN_TOP_MID, 0, 5);

    // 创建AI头像容器 - 左侧偏下 - 圆角正方形
    ui_data.ai_avatar = lv_obj_create(ui_data.screen);
    lv_obj_set_size(ui_data.ai_avatar, 85, 85);
    lv_obj_set_style_bg_color(ui_data.ai_avatar, lv_color_hex(0x1A1A1A), 0); // 深色背景
    lv_obj_set_style_bg_opa(ui_data.ai_avatar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_data.ai_avatar, 15, 0); // 圆角半径
    lv_obj_set_style_border_width(ui_data.ai_avatar, 2, 0);
    lv_obj_set_style_border_color(ui_data.ai_avatar, lv_color_hex(0x3498DB), 0); // 蓝色边框
    lv_obj_set_style_border_opa(ui_data.ai_avatar, LV_OPA_30, 0);
    lv_obj_align(ui_data.ai_avatar, LV_ALIGN_LEFT_MID, 10, 5); // 调整位置作为布局基准

    // 使用Lottie动画作为AI头像
    // 创建Lottie动画对象
    ui_data.ai_lottie = lv_lottie_create(ui_data.ai_avatar);

    // 禁用Lottie对象的滚动条和滚动功能
    lv_obj_clear_flag(ui_data.ai_lottie, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_data.ai_lottie, LV_SCROLLBAR_MODE_OFF);

    // 禁用AI头像容器的滚动条和滚动功能  
    lv_obj_clear_flag(ui_data.ai_avatar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_data.ai_avatar, LV_SCROLLBAR_MODE_OFF);

    // 设置动画文件路径
    lv_lottie_set_src_file(ui_data.ai_lottie, "/Quark-N_lvgl_9.2/lvgl_ui/lotties/Ai.json");

    // 设置缓冲区
    static uint8_t lottie_buf[85 * 85 * 4]; // 适配头像大小的缓冲区 
    lv_lottie_set_buffer(ui_data.ai_lottie, 85, 85, lottie_buf);

    // 获取动画对象并设置循环
    lv_anim_t *anim = lv_lottie_get_anim(ui_data.ai_lottie);
    if (anim) {
        lv_anim_set_repeat_count(anim, LV_ANIM_REPEAT_INFINITE); // 无限循环
    }

    // 设置大小并居中
    lv_obj_set_size(ui_data.ai_lottie, 85, 85);
    lv_obj_center(ui_data.ai_lottie);
    
    // 创建状态显示 - 与AI头像水平对齐
    ui_data.status = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_data.status, COLOR_AI_PRIMARY, 0); // 使用AI蓝色
    lv_label_set_text(ui_data.status, current_ai.status);
    // 将状态标签与AI头像居中对齐，并在头像下方显示
    lv_obj_align_to(ui_data.status, ui_data.ai_avatar, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    
    // 创建对话区域背景 - 右侧，与AI头像同高
    lv_obj_t *dialog_bg = lv_obj_create(ui_data.screen);
    lv_obj_set_size(dialog_bg, 120, 85); // 与AI头像相同高度
    lv_obj_set_style_bg_color(dialog_bg, COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(dialog_bg, LV_OPA_40, 0);
    lv_obj_set_style_border_width(dialog_bg, 1, 0);
    lv_obj_set_style_border_color(dialog_bg, lv_color_hex(0x3498DB), 0); // 蓝色边框
    lv_obj_set_style_radius(dialog_bg, 5, 0);
    // 与AI头像保持垂直对齐
    lv_obj_align(dialog_bg, LV_ALIGN_RIGHT_MID, -20, 5); // 水平对称布局

    // 创建对话容器和滚动区域
    ui_data.dialog_container = lv_obj_create(dialog_bg);
    lv_obj_set_size(ui_data.dialog_container, 100, 65);
    lv_obj_set_style_bg_opa(ui_data.dialog_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(ui_data.dialog_container, 0, 0);
    lv_obj_set_style_pad_all(ui_data.dialog_container, 0, 0);
    lv_obj_align(ui_data.dialog_container, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_scroll_dir(ui_data.dialog_container, LV_DIR_VER);
    
    // 创建对话文本
    ui_data.dialog_text = lv_label_create(ui_data.dialog_container);
    lv_obj_set_style_text_font(ui_data.dialog_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_data.dialog_text, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_width(ui_data.dialog_text, 95);
    lv_label_set_text(ui_data.dialog_text, current_ai.dialog);
    lv_obj_align(ui_data.dialog_text, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 创建按钮处理定时器
    ui_data.button_timer = lv_timer_create(button_event_timer_cb, 50, NULL);
    
    // 设置为活动状态
    ui_data.is_active = true;
    
    // 使用过渡动画加载屏幕
    lv_scr_load_anim(ui_data.screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_TIME_DEFAULT, 0, false);
}

// 按钮事件处理回调
static void button_event_timer_cb(lv_timer_t *timer) {
    if (!ui_data.is_active) return;
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    // 清除事件标志
    key355_clear_event();
    
    // 处理双击返回主菜单
    if (event == BUTTON_EVENT_DOUBLE_CLICK) {
        return_to_menu();
    }
}

// 从AI界面返回菜单界面
static void return_to_menu(void) {
    // 停止按钮定时器
    if (ui_data.button_timer) {
        lv_timer_del(ui_data.button_timer);
        ui_data.button_timer = NULL;
    }
    
    // 设置为非活动状态
    ui_data.is_active = false;
    
    // 清除实例引用
    ui_data.screen = NULL;
    
    // 创建菜单屏幕
    menu_ui_create_screen();
    
    // 使用动画切换屏幕
    lv_scr_load_anim(lv_scr_act(), LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, true);
    
    // 激活菜单
    menu_ui_set_active();
}

// 设置AI助手为活动状态
void AI_ui_set_active(void) {
    ui_data.is_active = true;
    
    // 恢复按钮定时器
    if (!ui_data.button_timer) {
        ui_data.button_timer = lv_timer_create(button_event_timer_cb, 50, NULL);
    }
}

// 释放资源
void AI_ui_deinit(void) {
    if (ui_data.button_timer) {
        lv_timer_del(ui_data.button_timer);
        ui_data.button_timer = NULL;
    }
    
    ui_data.is_active = false;
    
    if (ui_data.screen) {
        lv_obj_del(ui_data.screen);
        ui_data.screen = NULL;
        ui_data.ai_lottie = NULL; // 确保指针置空
        ui_data.ai_avatar = NULL;
    }
}

// 交互功能函数
void AI_ui_toggle_interaction(void) {
    // 这个版本不实现实际交互功能
}
