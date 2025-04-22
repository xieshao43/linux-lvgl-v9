#include "wifi_notification.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
// 添加网络操作所需的头文件
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include <fcntl.h> 

// Notification dimensions
#define NOTIF_MIN_WIDTH 125
#define NOTIF_MAX_WIDTH 200
#define NOTIF_MIN_HEIGHT 40
#define NOTIF_MAX_HEIGHT 60
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
static char current_ssid[64] = {0};
static char current_ip[32] = {0};
static char current_interface[16] = {0}; // 添加变量存储当前连接的网卡名称

// 静态预分配缓冲区，减少内存分配
static char display_text_buffer[256];

// 添加结构体存储多个WiFi接口的信息
typedef struct {
    char interface[16];  // 网卡名称
    char ssid[64];       // 连接的SSID
    char ip[32];         // IP地址
    bool is_connected;   // 是否连接
} wifi_interface_info_t;

// 静态变量存储两个WiFi接口的状态
static wifi_interface_info_t wifi_interfaces[2] = {
    {"wlan0", "", "", false},
    {"wlan1", "", "", false}
};

// Forward declarations of static functions
static void bubble_expand_anim(void);
static void bubble_collapse_anim(void);
static void create_wifi_notification(void);
static void hide_notification_timer_cb(lv_timer_t *timer);
static void notification_click_handler(lv_event_t *e);
static void update_notification_content(wifi_state_t state, const char *ssid);
static bool get_ip_address(const char *interface, char *ip_buffer, size_t buffer_size);
static bool get_ip_address_from_interfaces(char *ip_buffer, size_t buffer_size, char *interface_buffer, size_t if_buffer_size);
static void check_all_wifi_interfaces(void);
static void update_interface_ssid(const char *ssid, const char *interface);

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

static void anim_opa_cb(void *var, int32_t val) {
    lv_obj_set_style_text_opa(var, val, 0);
}

static void hide_console_cursor(void) {
    static bool cursor_hidden = false;
    if (!cursor_hidden) {
        int fd = open("/dev/tty1", O_WRONLY);
        if (fd >= 0) {
            write(fd, "\033[?25l", 6);
            close(fd);
            cursor_hidden = true;
        }
    }
}


// 使用socket API获取IP地址，替代shell命令
static bool get_ip_address(const char *interface, char *ip_buffer, size_t buffer_size) {
    if (!interface || !ip_buffer || buffer_size < 8) return false;
    
    struct ifreq ifr;
    int sockfd;
    bool result = false;
    
    // 清空输出缓冲区
    memset(ip_buffer, 0, buffer_size);
    
    // 创建socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return false;
    }
    
    // 设置接口名称
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    
    // 获取IP地址
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) >= 0) {
        struct sockaddr_in *addr = (struct sockaddr_in*)&ifr.ifr_addr;
        // 转换为点分十进制并存储到缓冲区
        const char *ip_str = inet_ntoa(addr->sin_addr);
        if (ip_str) {
            strncpy(ip_buffer, ip_str, buffer_size - 1);
            result = true;
        }
    }
    
    close(sockfd);
    return result;
}

// 检查接口是否启用
static bool is_interface_up(const char *interface) {
    struct ifreq ifr;
    int sockfd;
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return false;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        close(sockfd);
        return false;
    }
    
    close(sockfd);
    return (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
}

// 优化后的多接口IP地址获取函数
static bool get_ip_address_from_interfaces(char *ip_buffer, size_t buffer_size, char *interface_buffer, size_t if_buffer_size) {
    if (!ip_buffer || buffer_size < 8) return false;
    
    // 检查接口列表（静态定义避免每次分配）
    static const char *interfaces[] = {"wlan0", "wlan1"};
    static const int num_interfaces = sizeof(interfaces) / sizeof(interfaces[0]);
    
    for (int i = 0; i < num_interfaces; i++) {
        // 先检查接口是否启用，避免不必要的操作
        if (is_interface_up(interfaces[i]) && 
            get_ip_address(interfaces[i], ip_buffer, buffer_size)) {
            // 如果提供了接口缓冲区，记录当前使用的接口名称
            if (interface_buffer && if_buffer_size > 0) {
                strncpy(interface_buffer, interfaces[i], if_buffer_size - 1);
                interface_buffer[if_buffer_size - 1] = '\0';
            }
            return true;
        }
    }
    
    return false;
}

// 优化后的多接口IP地址获取函数 - 修改为检查所有接口并记录它们的状态
static void check_all_wifi_interfaces(void) {
    hide_console_cursor();
    bool status_changed = false;
    bool any_connected = false;
    
    for (int i = 0; i < 2; i++) {
        bool was_connected = wifi_interfaces[i].is_connected;
        wifi_interfaces[i].is_connected = is_interface_up(wifi_interfaces[i].interface);
        
        // 检测状态变化
        if (was_connected != wifi_interfaces[i].is_connected) {
            status_changed = true;
        }
        
        // 更新连接状态
        if (wifi_interfaces[i].is_connected) {
            any_connected = true;
            
            // 获取IP地址
            get_ip_address(wifi_interfaces[i].interface, 
                          wifi_interfaces[i].ip, 
                          sizeof(wifi_interfaces[i].ip));
                          
            // 如果当前无SSID记录但接口已连接，至少标记为WiFi
            if (wifi_interfaces[i].ssid[0] == '\0') {
                strncpy(wifi_interfaces[i].ssid, "WiFi", sizeof(wifi_interfaces[i].ssid) - 1);
            }
        }
    }
    
    // 如果任何接口的连接状态发生变化，更新全局状态
    if (status_changed) {
        current_wifi_state = any_connected ? WIFI_STATE_CONNECTED : WIFI_STATE_DISCONNECTED;
    }
}

// 更新当前WiFi连接的SSID（来自外部通知时使用）
static void update_interface_ssid(const char *ssid, const char *interface) {
    if (!interface || !ssid) return;
    
    for (int i = 0; i < 2; i++) {
        if (strcmp(wifi_interfaces[i].interface, interface) == 0) {
            strncpy(wifi_interfaces[i].ssid, ssid, sizeof(wifi_interfaces[i].ssid) - 1);
            wifi_interfaces[i].ssid[sizeof(wifi_interfaces[i].ssid) - 1] = '\0';
            return;
        }
    }
}

static void bubble_expand_anim(void) {
    if (is_expanded) return;
    
    lv_obj_clear_flag(notif_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(notif_status, LV_OBJ_FLAG_HIDDEN);
    
    // 在动画开始时先将文本设为透明
    lv_obj_set_style_text_opa(notif_label, 0, 0);

    // 气泡宽度动画
    lv_anim_init(&anim_w);
    lv_anim_set_var(&anim_w, notif_bubble);
    lv_anim_set_values(&anim_w, lv_obj_get_width(notif_bubble), NOTIF_MAX_WIDTH);
    lv_anim_set_time(&anim_w, NOTIF_ANIM_TIME);
    lv_anim_set_exec_cb(&anim_w, anim_width_cb);
    lv_anim_set_path_cb(&anim_w, lv_anim_path_ease_out);
    lv_anim_start(&anim_w);

    // 气泡高度动画
    lv_anim_init(&anim_h);
    lv_anim_set_var(&anim_h, notif_bubble);
    lv_anim_set_values(&anim_h, lv_obj_get_height(notif_bubble), NOTIF_MAX_HEIGHT);
    lv_anim_set_time(&anim_h, NOTIF_ANIM_TIME);
    lv_anim_set_exec_cb(&anim_h, anim_height_cb);
    lv_anim_set_path_cb(&anim_h, lv_anim_path_ease_out);
    lv_anim_start(&anim_h);

    // 设置图标和位置 - 增加左侧边距防止文本重叠
    lv_obj_set_style_text_font(notif_icon, &lv_font_montserrat_24, 0);
    lv_obj_align(notif_icon, LV_ALIGN_LEFT_MID, 10, 0);  // 将图标靠近左侧

    // 为文本设置宽度限制和对齐方式，确保不会与图标重叠
    lv_obj_set_style_text_font(notif_label, &lv_font_montserrat_16, 0);
    // 从图标右侧开始，留出足够边距，宽度自动调整
    lv_obj_set_width(notif_label, NOTIF_MAX_WIDTH - 50);  // 给图标留出足够空间
    lv_obj_set_style_text_align(notif_label, LV_TEXT_ALIGN_RIGHT, 0);  // 文本右对齐
    lv_obj_align(notif_label, LV_ALIGN_TOP_RIGHT, -12, 2);  // 调整右边距
    
    // 更新扩展内容（SSID和IP地址）- 优化显示多网卡状态
    memset(display_text_buffer, 0, sizeof(display_text_buffer));
    
    // 更新所有接口状态
    check_all_wifi_interfaces();
    
    // 检查是否有接口已连接
    bool any_connected = false;
    for (int i = 0; i < 2; i++) {
        if (wifi_interfaces[i].is_connected) {
            any_connected = true;
            break;
        }
    }
    
    if (any_connected) {
        // 构建显示文本，只包含WiFi名称，不显示接口名称
        int offset = 0;
        int connected_count = 0;
        
        // 计算连接的WiFi数量
        for (int i = 0; i < 2; i++) {
            if (wifi_interfaces[i].is_connected) {
                connected_count++;
            }
        }
        
        // 如果连接了两个WiFi，可能需要增大高度以显示全部内容
        if (connected_count > 1) {
            lv_obj_set_height(notif_bubble, NOTIF_MAX_HEIGHT + 10);
        }
        
        // 显示所有连接的WiFi名称
        for (int i = 0; i < 2; i++) {
            if (wifi_interfaces[i].is_connected) {
                // 添加换行，但第一行不需要
                if (offset > 0) {
                    offset += snprintf(display_text_buffer + offset, 
                                      sizeof(display_text_buffer) - offset, 
                                      "\n");
                }
                
                // 只显示WiFi名称
                offset += snprintf(display_text_buffer + offset,
                                  sizeof(display_text_buffer) - offset,
                                  "%s",
                                  wifi_interfaces[i].ssid);
                
                // 如果只有一个WiFi连接，显示其IP地址
                if (connected_count == 1 && wifi_interfaces[i].ip[0] != '\0') {
                    offset += snprintf(display_text_buffer + offset,
                                      sizeof(display_text_buffer) - offset,
                                      "\n%s",
                                      wifi_interfaces[i].ip);
                }
            }
        }
    } else if (current_wifi_state == WIFI_STATE_CONNECTING) {
        snprintf(display_text_buffer, sizeof(display_text_buffer), "Connecting to\nWiFi...");
    } else {
        snprintf(display_text_buffer, sizeof(display_text_buffer), "WiFi\nDisconnected");
    }
    
    lv_label_set_text(notif_label, display_text_buffer);
    
    // 添加文本淡入动画
    lv_anim_t anim_text_opa;
    lv_anim_init(&anim_text_opa);
    lv_anim_set_var(&anim_text_opa, notif_label);
    lv_anim_set_values(&anim_text_opa, 0, 255); // 从完全透明到完全不透明
    lv_anim_set_time(&anim_text_opa, NOTIF_ANIM_TIME * 0.8); // 稍快于展开动画完成
    lv_anim_set_delay(&anim_text_opa, NOTIF_ANIM_TIME * 0.4); // 展开到一半时开始淡入
    lv_anim_set_exec_cb(&anim_text_opa, anim_opa_cb);
    lv_anim_set_path_cb(&anim_text_opa, lv_anim_path_ease_out);
    lv_anim_start(&anim_text_opa);
    
    // 状态指示器也可以添加淡入效果
    lv_obj_set_style_bg_opa(notif_status, 0, 0);
    lv_anim_t anim_status_opa;
    lv_anim_init(&anim_status_opa);
    lv_anim_set_var(&anim_status_opa, notif_status);
    lv_anim_set_values(&anim_status_opa, 0, 255);
    lv_anim_set_time(&anim_status_opa, NOTIF_ANIM_TIME * 0.7);
    lv_anim_set_delay(&anim_status_opa, NOTIF_ANIM_TIME * 0.5);
    lv_anim_set_exec_cb(&anim_status_opa, (lv_anim_exec_xcb_t)lv_obj_set_style_bg_opa);
    lv_anim_set_path_cb(&anim_status_opa, lv_anim_path_ease_out);
    lv_anim_start(&anim_status_opa);

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
    
    // 设置折叠状态下的文本内容 - 苹果风格简洁显示连接状态
    if (current_wifi_state == WIFI_STATE_CONNECTED) {
        lv_label_set_text(notif_label, "Connected");
    } else if (current_wifi_state == WIFI_STATE_CONNECTING) {
        lv_label_set_text(notif_label, "Connecting...");
    } else {
        lv_label_set_text(notif_label, "WiFi");
    }
    
    // 调整文本样式和位置 - 确保折叠状态下不会重合
    lv_obj_set_style_text_font(notif_label, &lv_font_montserrat_12, 0); // 小字体
    lv_obj_align(notif_label, LV_ALIGN_RIGHT_MID, -12, 0); // 放在右侧，增加边距
    
    lv_obj_add_flag(notif_status, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(notif_icon, &lv_font_montserrat_16, 0);
    lv_obj_align(notif_icon, LV_ALIGN_LEFT_MID, 8, 0);  // 向右移动一点，避免挤压

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
    
    // WiFi icon - 调整初始位置
    notif_icon = lv_label_create(notif_bubble);
    lv_label_set_text(notif_icon, LV_SYMBOL_WIFI);
    lv_obj_align(notif_icon, LV_ALIGN_LEFT_MID, 8, 0);  // 增加边距
    lv_obj_add_style(notif_icon, &style_text, 0);
    
    // Status text - 增加与图标的距离
    notif_label = lv_label_create(notif_bubble);
    lv_label_set_text(notif_label, "WiFi");
    lv_obj_align(notif_label, LV_ALIGN_RIGHT_MID, -12, 0); // 增加边距
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
    // 保存旧状态，用于检测变化
    wifi_state_t old_state = current_wifi_state;
    
    current_wifi_state = state;
    
    // 确定当前激活的网卡接口
    char *active_interface = NULL;
    
    // 先检查wlan0是否已连接
    if (is_interface_up("wlan0")) {
        active_interface = "wlan0";
    }
    // 再检查wlan1是否已连接
    else if (is_interface_up("wlan1")) {
        active_interface = "wlan1";
    }
    
    // 保存当前的SSID到对应接口
    if (ssid && *ssid && active_interface) {
        update_interface_ssid(ssid, active_interface);
    }
    
    // 如果已连接，尝试获取所有接口的状态
    if (state == WIFI_STATE_CONNECTED) {
        // 检查并更新所有接口状态
        check_all_wifi_interfaces();
    }
    
    switch (state) {
        case WIFI_STATE_CONNECTED:
            lv_obj_remove_style_all(notif_status);
            lv_obj_add_style(notif_status, &style_status_connected, 0);
            
            if (is_expanded) {
                // 这部分逻辑已移到bubble_expand_anim函数中
                bubble_expand_anim();  // 重新触发展开动画，更新内容
            } else {
                // 折叠状态只显示"已连接"
                lv_label_set_text(notif_label, "Connected");
            }
            break;
            
        case WIFI_STATE_CONNECTING:
            lv_obj_remove_style_all(notif_status);
            lv_obj_add_style(notif_status, &style_status_disconnected, 0);
            
            lv_label_set_text(notif_label, is_expanded ? "Connecting to\nWiFi..." : "Connecting...");
            break;
            
        case WIFI_STATE_DISCONNECTED:
        default:
            lv_obj_remove_style_all(notif_status);
            lv_obj_add_style(notif_status, &style_status_disconnected, 0);
            
            lv_label_set_text(notif_label, is_expanded ? "WiFi\nDisconnected" : "WiFi");
            break;
    }
    
    // 如果状态从未连接变为已连接，或者从已连接变为未连接，显示通知
    if ((old_state != WIFI_STATE_CONNECTED && state == WIFI_STATE_CONNECTED) ||
        (old_state == WIFI_STATE_CONNECTED && state != WIFI_STATE_CONNECTED)) {
        // 状态变更时触发通知显示
        if (!is_visible) {
            wifi_notification_show(state, ssid);
        }
    }
}

void wifi_notification_init(void) {
    create_wifi_notification();
}

void wifi_notification_show(wifi_state_t state, const char *ssid) {
    if (!notif_bubble) {
        create_wifi_notification();
    }
    
    // 检查所有网卡状态 - 确保获取最新状态
    check_all_wifi_interfaces();
    
    // Update content based on state - 仅在状态变化时更新
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

// 新增函数 - 用于外部直接触发特定网卡的WiFi状态更新
void wifi_notification_update_interface(wifi_state_t state, const char *ssid, const char *interface) {
    if (!interface) return;
    
    bool status_changed = false;
    
    // 更新指定接口的信息
    for (int i = 0; i < 2; i++) {
        if (strcmp(wifi_interfaces[i].interface, interface) == 0) {
            // 记录旧状态用于检测变化
            bool was_connected = wifi_interfaces[i].is_connected;
            
            wifi_interfaces[i].is_connected = (state == WIFI_STATE_CONNECTED);
            
            if (ssid && *ssid) {
                strncpy(wifi_interfaces[i].ssid, ssid, sizeof(wifi_interfaces[i].ssid) - 1);
                wifi_interfaces[i].ssid[sizeof(wifi_interfaces[i].ssid) - 1] = '\0';
            } else if (state != WIFI_STATE_CONNECTED) {
                // 断开连接时清空SSID
                wifi_interfaces[i].ssid[0] = '\0';
            }
            
            if (state == WIFI_STATE_CONNECTED) {
                // 获取IP地址
                get_ip_address(interface, wifi_interfaces[i].ip, sizeof(wifi_interfaces[i].ip));
            } else {
                wifi_interfaces[i].ip[0] = '\0';
            }
            
            // 检测状态变化
            if (was_connected != wifi_interfaces[i].is_connected) {
                status_changed = true;
            }
            break;
        }
    }
    
    if (status_changed) {
        // 检查是否有任何接口已连接
        bool any_connected = false;
        for (int i = 0; i < 2; i++) {
            if (wifi_interfaces[i].is_connected) {
                any_connected = true;
                break;
            }
        }
        
        // 更新全局WiFi状态
        current_wifi_state = any_connected ? WIFI_STATE_CONNECTED : WIFI_STATE_DISCONNECTED;
        
        // 显示通知
        wifi_notification_show(current_wifi_state, ssid);
    } else if (is_expanded && is_visible) {
        // 如果通知已展开且可见，但没有状态变化，仍然更新显示内容
        bubble_expand_anim();
    }
}

/**
 * 释放WiFi通知系统资源
 */
void wifi_notification_deinit(void) {
    // 先隐藏通知（如果可见）
    if (is_visible) {
        wifi_notification_hide();
    }
    
    // 删除自动隐藏定时器
    if (auto_hide_timer) {
        lv_timer_delete(auto_hide_timer);
        auto_hide_timer = NULL;
    }
    
    // 停止所有相关动画
    if (notif_bubble) {
        lv_anim_del(notif_bubble, NULL);
        if (notif_label) lv_anim_del(notif_label, NULL);
        if (notif_icon) lv_anim_del(notif_icon, NULL);
        if (notif_status) lv_anim_del(notif_status, NULL);
    }
    
    // 删除通知气泡及其所有子部件
    if (notif_bubble) {
        lv_obj_delete(notif_bubble);
        notif_bubble = NULL;
        notif_icon = NULL;
        notif_label = NULL;
        notif_status = NULL;
    }
    
    // 重置状态标志
    is_expanded = false;
    is_visible = false;
    
    #if UI_DEBUG_ENABLED
    printf("[WIFI] Notification system deinitialized\n");
    #endif
}
