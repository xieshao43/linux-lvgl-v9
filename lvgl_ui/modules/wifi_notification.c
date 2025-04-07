#include "wifi_notification.h"
#include <stdlib.h>

// Notification dimensions
#define NOTIF_MIN_WIDTH 120
#define NOTIF_MAX_WIDTH 240
#define NOTIF_MIN_HEIGHT 40
#define NOTIF_MAX_HEIGHT 80
#define NOTIF_CORNER_RADIUS 20
#define NOTIF_ANIM_TIME 300

// Static variables
static lv_obj_t *notif_bubble = NULL;
static lv_obj_t *notif_icon = NULL;
static lv_obj_t *notif_label = NULL;
static lv_obj_t *notif_status = NULL;
static lv_anim_t anim_x, anim_y, anim_w, anim_h;
static bool is_expanded = false;
static bool is_visible = false;
static lv_timer_t *auto_hide_timer = NULL;
static wifi_state_t current_wifi_state = WIFI_STATE_DISCONNECTED;

// Forward declarations of static functions
static void bubble_expand_anim(void);
static void bubble_collapse_anim(void);
static void create_wifi_notification(void);
static void hide_notification_timer_cb(lv_timer_t *timer);
static void notification_click_handler(lv_event_t *e);
static void update_notification_content(wifi_state_t state, const char *ssid);

// Style variables
static lv_style_t style_bubble;
static lv_style_t style_text;
static lv_style_t style_status_connected;
static lv_style_t style_status_disconnected;

// Animation callback functions
static void anim_width_cb(void *var, int32_t val) {
    lv_obj_set_width(var, val);
}

static void anim_height_cb(void *var, int32_t val) {
    lv_obj_set_height(var, val);
}

static void anim_y_cb(void *var, int32_t val) {
    lv_obj_set_y(var, val);
}

static void bubble_expand_anim(void) {
    if (is_expanded) return;
    
    lv_obj_clear_flag(notif_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(notif_status, LV_OBJ_FLAG_HIDDEN);

    lv_anim_init(&anim_w);
    lv_anim_set_var(&anim_w, notif_bubble);
    lv_anim_set_values(&anim_w, lv_obj_get_width(notif_bubble), NOTIF_MAX_WIDTH);
    lv_anim_set_time(&anim_w, NOTIF_ANIM_TIME);
    lv_anim_set_exec_cb(&anim_w, anim_width_cb);
    lv_anim_set_path_cb(&anim_w, lv_anim_path_ease_out);
    lv_anim_start(&anim_w);

    lv_anim_init(&anim_h);
    lv_anim_set_var(&anim_h, notif_bubble);
    lv_anim_set_values(&anim_h, lv_obj_get_height(notif_bubble), NOTIF_MAX_HEIGHT);
    lv_anim_set_time(&anim_h, NOTIF_ANIM_TIME);
    lv_anim_set_exec_cb(&anim_h, anim_height_cb);
    lv_anim_set_path_cb(&anim_h, lv_anim_path_ease_out);
    lv_anim_start(&anim_h);

    lv_obj_set_style_text_font(notif_icon, &lv_font_montserrat_24, 0); // 增大图标字体
    lv_obj_align(notif_icon, LV_ALIGN_LEFT_MID, 15, 0); // 调整位置
    
    // 添加：调整文本大小
    lv_obj_set_style_text_font(notif_label, &lv_font_montserrat_16, 0); // 增大文本字体
    lv_obj_align(notif_label, LV_ALIGN_TOP_MID, 0, 15); // 调整位置


    is_expanded = true;
}

static void bubble_collapse_anim(void) {
    if (!is_expanded) return;
    
    lv_anim_init(&anim_w);
    lv_anim_set_var(&anim_w, notif_bubble);
    lv_anim_set_values(&anim_w, lv_obj_get_width(notif_bubble), NOTIF_MIN_WIDTH);
    lv_anim_set_time(&anim_w, NOTIF_ANIM_TIME);
    lv_anim_set_exec_cb(&anim_w, anim_width_cb);
    lv_anim_set_path_cb(&anim_w, lv_anim_path_ease_out);
    lv_anim_start(&anim_w);

    lv_anim_init(&anim_h);
    lv_anim_set_var(&anim_h, notif_bubble);
    lv_anim_set_values(&anim_h, lv_obj_get_height(notif_bubble), NOTIF_MIN_HEIGHT);
    lv_anim_set_time(&anim_h, NOTIF_ANIM_TIME);
    lv_anim_set_exec_cb(&anim_h, anim_height_cb);
    lv_anim_set_path_cb(&anim_h, lv_anim_path_ease_out);
    lv_anim_start(&anim_h);
    
    
    lv_obj_add_flag(notif_status, LV_OBJ_FLAG_HIDDEN);
    if (current_wifi_state == WIFI_STATE_CONNECTED) {
        lv_label_set_text(notif_label, "Connected");
    } else {
        lv_label_set_text(notif_label, "WiFi");
    }
    
    // 调整文本样式和位置
    lv_obj_set_style_text_font(notif_label, &lv_font_montserrat_12, 0); // 小字体
    lv_obj_align(notif_label, LV_ALIGN_RIGHT_MID, -10, 0); // 放在右侧
    
    lv_obj_add_flag(notif_status, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(notif_icon, &lv_font_montserrat_16, 0);
    lv_obj_align(notif_icon, LV_ALIGN_LEFT_MID, 5, 0);


    is_expanded = false;
}

static void anim_hide_complete_cb(lv_anim_t *a) {
    lv_obj_add_flag(notif_bubble, LV_OBJ_FLAG_HIDDEN);
    is_visible = false;
    if (is_expanded) {
        is_expanded = false;
        lv_obj_set_size(notif_bubble, NOTIF_MIN_WIDTH, NOTIF_MIN_HEIGHT);
        lv_obj_set_style_text_font(notif_icon, &lv_font_montserrat_16, 0);
        lv_obj_align(notif_icon, LV_ALIGN_LEFT_MID, 5, 0);
        lv_obj_add_flag(notif_status, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_wifi_notification(void) {
    // Initialize styles
    lv_style_init(&style_bubble);
// 高级渐变方案 - 苹果风格单色渐变
    lv_style_set_bg_color(&style_bubble, lv_color_hex(0x9C27B0));        // 紫色基础色
    lv_style_set_bg_grad_color(&style_bubble, lv_color_hex(0x7C3AED));   // 紫色暗色
    lv_style_set_bg_grad_dir(&style_bubble, LV_GRAD_DIR_VER);
    lv_style_set_bg_opa(&style_bubble,200);  // 略微提高透明度增加高级感

    // 添加细微边框增强玻璃感
    lv_style_set_border_color(&style_bubble, lv_color_hex(0xFBFBFB));
    lv_style_set_border_opa(&style_bubble, 90);  // 减小不透明度
    lv_style_set_border_width(&style_bubble, 1);
    
    lv_style_set_radius(&style_bubble, NOTIF_CORNER_RADIUS);
    lv_style_set_shadow_width(&style_bubble, 15);
    lv_style_set_shadow_color(&style_bubble, lv_color_black());
    lv_style_set_shadow_ofs_y(&style_bubble, 5);
    lv_style_set_pad_all(&style_bubble, 8);
    

    lv_style_init(&style_text);
    lv_style_set_text_color(&style_text, lv_color_white());
    
    lv_style_init(&style_status_connected);
    lv_style_set_bg_color(&style_status_connected, lv_color_hex(0x10B981)); // 翡翠绿状态指示器
    lv_style_set_radius(&style_status_connected, LV_RADIUS_CIRCLE);
    
    lv_style_init(&style_status_disconnected);
    lv_style_set_bg_color(&style_status_disconnected, lv_color_hex(0xEF4444)); // 红色状态指示器
    lv_style_set_radius(&style_status_disconnected, LV_RADIUS_CIRCLE);
    
    // Create the bubble container
    notif_bubble = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(notif_bubble);
    lv_obj_add_style(notif_bubble, &style_bubble, 0);
    lv_obj_set_size(notif_bubble, NOTIF_MIN_WIDTH, NOTIF_MIN_HEIGHT);
    
    // Center the bubble at the top of the screen
    int32_t screen_width = lv_display_get_horizontal_resolution(NULL);
    lv_obj_align(notif_bubble, LV_ALIGN_TOP_MID, 0, -NOTIF_MAX_HEIGHT);
    
    // WiFi icon
    LV_IMAGE_DECLARE(img_wifi_icon); // This would be your WiFi icon image
    notif_icon = lv_label_create(notif_bubble);
    lv_label_set_text(notif_icon, LV_SYMBOL_WIFI);
    lv_obj_align(notif_icon, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_style(notif_icon, &style_text, 0);
    
    // Status text
    notif_label = lv_label_create(notif_bubble);
    lv_label_set_text(notif_label, "WiFi");
    lv_obj_align(notif_label, LV_ALIGN_RIGHT_MID, -10, 0); // 修改位置到右侧
    lv_obj_add_style(notif_label, &style_text, 0);
    lv_obj_set_style_text_font(notif_label, &lv_font_montserrat_12, 0); // 设置小字体
    
    // Status indicator
    notif_status = lv_obj_create(notif_bubble);
    lv_obj_remove_style_all(notif_status);
    lv_obj_add_style(notif_status, &style_status_disconnected, 0);
    lv_obj_set_size(notif_status, 12, 12);
    lv_obj_align(notif_status, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(notif_status, LV_OBJ_FLAG_HIDDEN);
    
    // Add click event
    lv_obj_add_event_cb(notif_bubble, notification_click_handler, LV_EVENT_CLICKED, NULL);
    
    // Hide the bubble initially
    lv_obj_add_flag(notif_bubble, LV_OBJ_FLAG_HIDDEN);
}

static void hide_notification_timer_cb(lv_timer_t *timer) {
    wifi_notification_hide();
    auto_hide_timer = NULL;
}

static void notification_click_handler(lv_event_t *e) {
    if (is_expanded) {
        bubble_collapse_anim();
    } else {
        bubble_expand_anim();
        
        // Reset the auto-hide timer when expanding
        if (auto_hide_timer) {
            lv_timer_reset(auto_hide_timer);
        }
    }
}

static void update_notification_content(wifi_state_t state, const char *ssid) {
    current_wifi_state = state;
    
    switch (state) {
        case WIFI_STATE_CONNECTED:
            lv_obj_remove_style_all(notif_status);
            lv_obj_add_style(notif_status, &style_status_connected, 0);
            if (ssid) {
                lv_label_set_text_fmt(notif_label, "Connected to\n%s", ssid);
            } else {
                lv_label_set_text(notif_label, "WiFi Connected");
            }
            break;
            
        case WIFI_STATE_CONNECTING:
            lv_obj_remove_style_all(notif_status);
            lv_obj_add_style(notif_status, &style_status_disconnected, 0);
            lv_label_set_text(notif_label, "Connecting to WiFi...");
            break;
            
        case WIFI_STATE_DISCONNECTED:
        default:
            lv_obj_remove_style_all(notif_status);
            lv_obj_add_style(notif_status, &style_status_disconnected, 0);
            lv_label_set_text(notif_label, "WiFi Disconnected");
            break;
    }
}

void wifi_notification_init(void) {
    create_wifi_notification();
}

void wifi_notification_show(wifi_state_t state, const char *ssid) {
    if (!notif_bubble) {
        create_wifi_notification();
    }
    
    // Update content based on state
    update_notification_content(state, ssid);
    
    // Show the notification if hidden
    if (!is_visible) {
        lv_obj_clear_flag(notif_bubble, LV_OBJ_FLAG_HIDDEN);
        is_visible = true;
        
        // Animate from top of screen
        lv_obj_set_y(notif_bubble, -NOTIF_MAX_HEIGHT);
        
        lv_anim_init(&anim_y);
        lv_anim_set_var(&anim_y, notif_bubble);
        lv_anim_set_values(&anim_y, -NOTIF_MAX_HEIGHT, 10); // Move down to be visible
        lv_anim_set_time(&anim_y, NOTIF_ANIM_TIME);
        lv_anim_set_exec_cb(&anim_y, anim_y_cb);
        lv_anim_set_path_cb(&anim_y, lv_anim_path_ease_out);
        lv_anim_start(&anim_y);
        
        // Initially show in collapsed state
        is_expanded = false;
        
        // Auto-expand after showing
        lv_timer_t *expand_timer = lv_timer_create(
            (lv_timer_cb_t)bubble_expand_anim, 
            NOTIF_ANIM_TIME + 50, 
            NULL
        );
        lv_timer_set_repeat_count(expand_timer, 1);
    } else {
        if (!is_expanded) {
            bubble_expand_anim();
        }
    }
    
    // Set auto-hide timer
    if (auto_hide_timer) {
        lv_timer_del(auto_hide_timer);
    }
    auto_hide_timer = lv_timer_create(hide_notification_timer_cb, 5000, NULL);
    lv_timer_set_repeat_count(auto_hide_timer, 1);
}

void wifi_notification_hide(void) {
    if (!is_visible || !notif_bubble) return;
    
    // First collapse if expanded
    if (is_expanded) {
        bubble_collapse_anim();
        // Wait a bit before hiding completely
        lv_timer_t *hide_timer = lv_timer_create(
            (lv_timer_cb_t)wifi_notification_hide, 
            NOTIF_ANIM_TIME + 50, 
            NULL
        );
        lv_timer_set_repeat_count(hide_timer, 1);
        return;
    }
    
    // Animate upwards
    lv_anim_init(&anim_y);
    lv_anim_set_var(&anim_y, notif_bubble);
    lv_anim_set_values(&anim_y, lv_obj_get_y(notif_bubble), -NOTIF_MAX_HEIGHT);
    lv_anim_set_time(&anim_y, NOTIF_ANIM_TIME);
    lv_anim_set_exec_cb(&anim_y, anim_y_cb);
    lv_anim_set_path_cb(&anim_y, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&anim_y, anim_hide_complete_cb);
    lv_anim_start(&anim_y);
}

bool wifi_notification_is_visible(void) {
    return is_visible;
}
