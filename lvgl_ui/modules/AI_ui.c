#include "AI_ui.h"
#include "menu_ui.h"
#include "../core/key355.h"
#include "../core/ipc_udp.h" // 修改为新的IPC UDP头文件
#include "../core/cJSON.h" // 添加cJSON头文件
#include "../core/ai_comm_manager.h" // 修改引入头文件 - 添加AI通信管理器
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 定义其他在common.h中没有的颜色
#define COLOR_BACKGROUND    lv_color_hex(0x121212)
#define COLOR_SURFACE       lv_color_hex(0x1E1E1E)
#define COLOR_TEXT_PRIMARY  lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_SECONDARY lv_color_hex(0xDDDDDD)
#define COLOR_AI_PRIMARY    lv_color_hex(0x3498DB) // 添加AI主题蓝色
#define COLOR_AI_TITLE      lv_color_hex(0x00FFFF) // 保留用户喜欢的青色标题
#define COLOR_AI_TEXT       lv_color_hex(0xB0D0E0) // 柔和的银蓝色文本更优雅的配色

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
    lv_timer_t *scroll_timer;    // 滚动定时器
    lv_timer_t *udp_timer;       // UDP消息处理定时器 (新增)
    bool is_active;              // 是否活动状态
    bool message_received;       // 是否收到新消息 (新增)
} ai_ui_data_t;

// 定义UDP端点和端口号 (修改为标准端口)
#define UDP_PORT_RECV 5679  /* control_center向GUI的这个端口下发UI信息 */
#define UDP_PORT_SEND 5678  /* GUI向control_center的这个端口上传UI信息 */

// AI信息
typedef struct {
    const char *title;
    const char *status;
    const char *dialog;
} ai_info_t;

// 全局UI数据
static ai_ui_data_t ui_data = {0};

// 消息缓冲区 (新增)
#define MAX_DIALOG_LENGTH 1024
static char dialog_buffer[MAX_DIALOG_LENGTH] = {0};

// AI状态定义 (新增)
typedef enum {
    AI_STATE_IDLE = 0,
    AI_STATE_LISTENING = 1,
    AI_STATE_THINKING = 5,
    AI_STATE_SPEAKING = 6
} ai_state_t;

static ai_state_t current_state = AI_STATE_IDLE;

// AI示例数据
static ai_info_t current_ai = {
    .title = "Clifford",
    .dialog = "Welcome to AI Assistant!\n\n> How can I help you today?\n\n> I can process text and image queries\n\n> I can answer questions and provide information\n\n> Double-click to return to main menu\n\n> This interface demonstrates AI capabilities\n\n> Try different voice commands\n\n> Ask me about weather, news, or facts"
};

// 函数前向声明
static void return_to_menu(void);
static void button_event_timer_cb(lv_timer_t *timer);
static void start_text_autoscroll(void);
static void scroll_text_timer_cb(lv_timer_t *timer);
static void process_udp_messages(lv_timer_t *timer); // 新增UDP消息处理函数
static void update_dialog_text(const char *new_text); // 新增对话文本更新函数
static void init_ai_communication(void); // 添加init_ai_communication的前向声明

// 添加一个简单的消息缓冲区
static struct {
    bool has_new_message;
    char text[MAX_DIALOG_LENGTH];
    int state;
} message_buffer = {0};

// AI消息回调函数
static void ai_message_callback(ai_message_type_t type, const void* data, void* user_data) {
    if (!ui_data.is_active) return;
    
    switch (type) {
        case AI_MSG_TEXT: {
            const char* text = (const char*)data;
            // 安全复制消息文本
            strncpy(message_buffer.text, text, MAX_DIALOG_LENGTH - 1);
            message_buffer.text[MAX_DIALOG_LENGTH - 1] = '\0';
            message_buffer.has_new_message = true;
            break;
        }
        case AI_MSG_STATE: {
            const int* state = (const int*)data;
            message_buffer.state = *state;
            break;
        }
        default:
            break;
    }
}

// 处理收到的UDP消息 - 从缓冲区安全地更新UI
static void process_udp_messages(lv_timer_t *timer) {
    // 只在活动状态且有新消息时处理
    if (!ui_data.is_active || !message_buffer.has_new_message) return;
    
    // 安全检查UI元素，这里使用双重检查防止段错误
    if (!ui_data.dialog_text || !ui_data.status || 
        !lv_obj_is_valid(ui_data.dialog_text) || !lv_obj_is_valid(ui_data.status)) {
        message_buffer.has_new_message = false;
        return;
    }
    
    // 更新对话文本
    lv_label_set_text(ui_data.dialog_text, message_buffer.text);
    
    // 更新状态文本
    switch(message_buffer.state) {
        case AI_STATE_LISTENING:
            lv_label_set_text(ui_data.status, "Listening...");
            break;
        case AI_STATE_THINKING:
            lv_label_set_text(ui_data.status, "Thinking...");
            break;
        case AI_STATE_SPEAKING:
            lv_label_set_text(ui_data.status, "Speaking...");
            break;
        default:
            // 只在状态有值时更新
            if (message_buffer.state != 0) {
                lv_label_set_text(ui_data.status, "Idle");
            }
            break;
    }
    
    // 重新启动滚动效果
    start_text_autoscroll();
    
    // 清除新消息标志
    message_buffer.has_new_message = false;
    
    printf("[AI] Updated UI with message: %.30s...\n", message_buffer.text);
}

// 更新对话文本 (新增)
static void update_dialog_text(const char *new_text) {
    if (!ui_data.dialog_text || !new_text) return;
    
    // 更新文本
    lv_label_set_text(ui_data.dialog_text, new_text);
    
    // 重新启动滚动效果
    start_text_autoscroll();
}

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
    
    // 创建标题 - 直接创建标题标签，类似音乐界面
    ui_data.title = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.title, &lv_font_montserrat_20, 0); // 更大的字体
    lv_obj_set_style_text_color(ui_data.title, COLOR_AI_TITLE, 0); // 保持原有的青色
    lv_obj_set_style_text_letter_space(ui_data.title, 2, 0); // 保持字间距
    lv_obj_set_width(ui_data.title, 180); // 设置合适的宽度
    lv_obj_set_style_text_align(ui_data.title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ui_data.title, current_ai.title);
    lv_obj_align(ui_data.title, LV_ALIGN_TOP_MID, 0, 5);
    
    // 创建状态文本 (添加状态显示)
    ui_data.status = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_data.status, COLOR_AI_PRIMARY, 0);
    lv_label_set_text(ui_data.status, "Idle");
    lv_obj_align(ui_data.status, LV_ALIGN_TOP_MID, 0, 30); // 放在标题下方
    
    // 创建AI头像容器 - 左侧偏下 - 圆角正方形
    ui_data.ai_avatar = lv_obj_create(ui_data.screen);
    lv_obj_set_size(ui_data.ai_avatar, 85, 85);
    lv_obj_set_style_bg_color(ui_data.ai_avatar, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(ui_data.ai_avatar, LV_OPA_0, 0);
    lv_obj_set_style_radius(ui_data.ai_avatar, 15, 0);
    lv_obj_set_style_border_width(ui_data.ai_avatar, 0, 0);
    lv_obj_set_style_border_color(ui_data.ai_avatar, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_border_opa(ui_data.ai_avatar, LV_OPA_0, 0);
    lv_obj_align(ui_data.ai_avatar, LV_ALIGN_LEFT_MID, 10, 5);

    // 创建Lottie动画对象
    ui_data.ai_lottie = lv_lottie_create(ui_data.ai_avatar);

    // 禁用滚动功能
    lv_obj_clear_flag(ui_data.ai_lottie, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_data.ai_lottie, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ui_data.ai_avatar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_data.ai_avatar, LV_SCROLLBAR_MODE_OFF);

    // 设置动画文件路径和缓冲区
    lv_lottie_set_src_file(ui_data.ai_lottie, "/Quark-N_lvgl_9.2/lvgl_ui/lotties/AI_Show.json");
    
    static uint8_t lottie_buf[85 * 85 * 4];
    lv_lottie_set_buffer(ui_data.ai_lottie, 85, 85, lottie_buf);

    // 设置循环
    lv_anim_t *anim = lv_lottie_get_anim(ui_data.ai_lottie);
    if (anim) {
        lv_anim_set_repeat_count(anim, LV_ANIM_REPEAT_INFINITE);
    }

    // 设置大小和位置
    lv_obj_set_size(ui_data.ai_lottie, 85, 85);
    lv_obj_center(ui_data.ai_lottie);
    

    // 创建对话容器和滚动区域
    ui_data.dialog_container = lv_obj_create(ui_data.screen); // 改为screen而不是dialog_bg
    lv_obj_set_size(ui_data.dialog_container, 130, 100);
    lv_obj_set_style_bg_opa(ui_data.dialog_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(ui_data.dialog_container, 0, 0);
    lv_obj_set_style_pad_all(ui_data.dialog_container, 0, 0);
    // 直接定位到屏幕右侧
    lv_obj_align(ui_data.dialog_container, LV_ALIGN_RIGHT_MID, -15, 15);
    lv_obj_set_scroll_dir(ui_data.dialog_container, LV_DIR_VER);
    
    // 创建对话文本
    ui_data.dialog_text = lv_label_create(ui_data.dialog_container);
    lv_obj_set_style_text_font(ui_data.dialog_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_data.dialog_text, COLOR_AI_TEXT, 0); // 使用新定义的AI文本颜色
    lv_obj_set_width(ui_data.dialog_text, 130);
    lv_label_set_text(ui_data.dialog_text, current_ai.dialog);
    lv_obj_align(ui_data.dialog_text, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 开启垂直滚动
    start_text_autoscroll();
    
    // 创建按钮处理定时器
    ui_data.button_timer = lv_timer_create(button_event_timer_cb, 50, NULL);
    
    // 设置为活动状态
    ui_data.is_active = true;
    ui_data.message_received = false;
    
    // 初始化UDP通信
    init_ai_communication();
    
    // 使用过渡动画加载屏幕
    lv_scr_load_anim(ui_data.screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_TIME_DEFAULT, 0, false);
}

// 按钮事件处理回调 (修改)
static void button_event_timer_cb(lv_timer_t *timer) {
    if (!ui_data.is_active) return;
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    // 清除事件标志
    key355_clear_event();
    
    // 处理双击返回主菜单
    if (event == BUTTON_EVENT_DOUBLE_CLICK) {
        return_to_menu();
    } else if (event == BUTTON_EVENT_CLICK) {
        // 单击发送消息请求新的AI内容
        const char *request_message = "{\"type\":\"ai_request\",\"action\":\"get_content\"}";
        ai_comm_manager_send_message(request_message);
    }
}

// 文本自动垂直滚动定时器回调 - 优化版本
static void scroll_text_timer_cb(lv_timer_t *timer) {
    if (!ui_data.is_active || !ui_data.dialog_container || !ui_data.dialog_text) return;
    
    static int scroll_pos = 0;
    static int direction = 1; // 1向下滚动，-1向上滚动
    static uint32_t last_time = 0;
    
    // 获取当前时间
    uint32_t current_time = lv_tick_get();
    
    // 控制滚动速度，LVGL 9.2中建议使用lv_tick_get()控制时间
    if(current_time - last_time < 20) return; // 控制刷新率
    last_time = current_time;
    
    // 获取文本和容器高度
    lv_coord_t text_height = lv_obj_get_height(ui_data.dialog_text);
    lv_coord_t container_height = lv_obj_get_height(ui_data.dialog_container);
    
    // 只有文本高度大于容器时才需要滚动
    if (text_height <= container_height) return;
    
    // 设置滚动位置
    scroll_pos += (direction * 1); // 每次滚动1个像素
    
    // 检查是否到达边界
    if (scroll_pos <= 0) {
        scroll_pos = 0;
        direction = 1; // 改为向下滚动
        lv_timer_pause(timer);
        lv_timer_set_period(timer, 1000); // 在顶部停留1秒
        lv_timer_reset(timer);
        lv_timer_resume(timer);
    }
    else if (scroll_pos >= (text_height - container_height)) {
        scroll_pos = text_height - container_height;
        direction = -1; // 改为向上滚动
        lv_timer_pause(timer);
        lv_timer_set_period(timer, 1000); // 在底部停留1秒
        lv_timer_reset(timer);
        lv_timer_resume(timer);
    }
    else {
        // 正常滚动时使用标准间隔 - LVGL 9.2能够更好地支持精确周期
        lv_timer_set_period(timer, 50);
    }
    
    // 应用垂直滚动 - LVGL 9.2 API
    lv_obj_set_y(ui_data.dialog_text, -scroll_pos);
}

// 自动滚动文本的函数 - 改为垂直滚动
static void start_text_autoscroll(void) {
    // 停止现有的滚动定时器
    if (ui_data.scroll_timer) {
        lv_timer_del(ui_data.scroll_timer);
        ui_data.scroll_timer = NULL;
    }
    
    // 获取对话文本的高度
    lv_coord_t text_height = lv_obj_get_height(ui_data.dialog_text);
    lv_coord_t container_height = lv_obj_get_height(ui_data.dialog_container);
    
    // 只有当文本高度超过容器高度时才需要滚动
    if (text_height > container_height) {
        // 创建定时器来执行垂直滚动
        ui_data.scroll_timer = lv_timer_create(scroll_text_timer_cb, 50, NULL);
    }
}

// 从AI界面返回菜单界面 - 修复安全退出
static void return_to_menu(void) {
    printf("[AI] Returning to menu\n");
    
    // 设置为非活动状态 - 防止消息处理
    ui_data.is_active = false;
    
    // 停止所有定时器
    if (ui_data.button_timer) {
        lv_timer_del(ui_data.button_timer);
        ui_data.button_timer = NULL;
    }
    
    if (ui_data.scroll_timer) {
        lv_timer_del(ui_data.scroll_timer);
        ui_data.scroll_timer = NULL;
    }
    
    if (ui_data.udp_timer) {
        lv_timer_del(ui_data.udp_timer);
        ui_data.udp_timer = NULL;
    }
    
    // 取消注册AI消息回调 - 不再接收消息
    ai_comm_manager_unregister_callback(ai_message_callback);
    
    // 保存当前屏幕引用
    lv_obj_t *old_screen = ui_data.screen;
    
    // 清空所有对象引用，防止回调访问
    ui_data.screen = NULL;
    ui_data.dialog_text = NULL;
    ui_data.dialog_container = NULL;
    ui_data.ai_avatar = NULL;
    ui_data.ai_lottie = NULL;
    ui_data.title = NULL;
    ui_data.status = NULL;
    
    // 创建菜单屏幕
    menu_ui_create_screen();
    
    // 使用与其他界面一致的动画切换屏幕
    lv_scr_load_anim(lv_scr_act(), LV_SCR_LOAD_ANIM_FADE_OUT, 300, 0, true);
    
    // 激活菜单
    menu_ui_set_active();
    
    printf("[AI] Successfully returned to menu\n");
}

// 初始化AI UI的UDP通信
static void init_ai_communication(void) {
    // 重置消息缓冲区
    memset(&message_buffer, 0, sizeof(message_buffer));
    
    // 确保AI通信管理器已初始化（这会在全局初始化时处理）
    if (!ai_comm_manager_is_connected()) {
        ai_comm_manager_init();
    }
    
    // 注册消息回调
    ai_comm_manager_register_callback(ai_message_callback, NULL);
    
    // 创建UDP处理定时器 - 使用更低频率降低CPU负载
    ui_data.udp_timer = lv_timer_create(process_udp_messages, 200, NULL);
    
    // 发送初始化状态消息
    const char *init_message = "{\"type\":\"ai_status\",\"status\":\"ready\"}";
    ai_comm_manager_send_message(init_message);
}

// 设置AI助手为活动状态
void AI_ui_set_active(void) {
    ui_data.is_active = true;
    
    // 恢复按钮定时器
    if (!ui_data.button_timer) {
        ui_data.button_timer = lv_timer_create(button_event_timer_cb, 50, NULL);
    }
    
    // 恢复文本滚动
    if (ui_data.dialog_text && ui_data.dialog_container && !ui_data.scroll_timer) {
        start_text_autoscroll();
    }
    
    // 恢复UDP通信定时器
    if (!ui_data.udp_timer) {
        ui_data.udp_timer = lv_timer_create(process_udp_messages, 100, NULL);
    }
}

// 交互功能函数 - 添加发送UDP请求的功能 (修改)
void AI_ui_toggle_interaction(void) {
    if (!ui_data.is_active) return;
    
    // 发送交互请求消息
    const char *interact_message = "{\"type\":\"ai_interaction\",\"action\":\"toggle\"}";
    ai_comm_manager_send_message(interact_message);
}

