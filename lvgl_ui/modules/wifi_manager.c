#include "wifi_notification.h"
#include "../common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

// 函数声明
void wifi_status_changed_callback(wifi_state_t new_state, const char *ssid);

// WiFi状态检测相关变量
static wifi_state_t current_wifi_state = WIFI_STATE_DISCONNECTED;
static char current_ssid[64] = {0};
static time_t last_check_time = 0;
static const int CHECK_INTERVAL = 5; // 检查间隔(秒)
static bool wifi_initialized = false;

// Allwinner H3相关配置
// 修改为支持多个WiFi接口
#define MAX_WIFI_INTERFACES 2
static const char *WIFI_DEVICES[MAX_WIFI_INTERFACES] = {"wlan0", "wlan1"};
#define PROC_NET_WIRELESS "/proc/net/wireless"
#define PROC_NET_DEV "/proc/net/dev"

// 添加接口状态跟踪
typedef struct {
    const char *interface;
    bool is_connected;
    char ssid[64];
} wifi_if_status_t;

static wifi_if_status_t wifi_interfaces[MAX_WIFI_INTERFACES];

/**
 * 初始化WiFi接口状态数组
 */
static void init_wifi_interfaces(void) {
    for (int i = 0; i < MAX_WIFI_INTERFACES; i++) {
        wifi_interfaces[i].interface = WIFI_DEVICES[i];
        wifi_interfaces[i].is_connected = false;
        memset(wifi_interfaces[i].ssid, 0, sizeof(wifi_interfaces[i].ssid));
    }
}

/**
 * 使用iwgetid命令获取当前连接的SSID
 * @param interface 网络接口名称
 * @param ssid 输出参数，存储获取到的SSID
 * @param max_len ssid缓冲区的最大长度
 * @return 如果成功获取SSID返回true，否则返回false
 */
static bool get_current_ssid(const char *interface, char *ssid, size_t max_len) {
    FILE *fp;
    char cmd[128];
    char buffer[256];
    bool result = false;
    
    // 清空输出缓冲区
    memset(ssid, 0, max_len);
    
    // 构建命令 - 使用iwgetid获取SSID（大多数Linux发行版都有）
    snprintf(cmd, sizeof(cmd), "iwgetid %s -r 2>/dev/null", interface);
    
    // 执行命令
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer) - 1, fp)) {
            // 移除末尾的换行符
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') {
                buffer[len-1] = '\0';
            }
            
            // 检查SSID是否为空
            if (strlen(buffer) > 0) {
                strncpy(ssid, buffer, max_len - 1);
                result = true;
            }
        }
        pclose(fp);
    }
    
    // H3特定优化：如果iwgetid失败，尝试通过读取特定文件获取SSID
    if (!result && access("/tmp/wifi_status", F_OK) != -1) {
        fp = fopen("/tmp/wifi_status", "r");
        if (fp) {
            if (fgets(buffer, sizeof(buffer) - 1, fp)) {
                if (strstr(buffer, "SSID=")) {
                    char *ssid_start = strstr(buffer, "SSID=") + 5;
                    char *ssid_end = strchr(ssid_start, ',');
                    if (ssid_end) *ssid_end = '\0';
                    
                    if (strlen(ssid_start) > 0) {
                        strncpy(ssid, ssid_start, max_len - 1);
                        result = true;
                    }
                }
            }
            fclose(fp);
        }
    }
    
    return result;
}

/**
 * 检查WiFi接口是否已启用
 * @param interface 网络接口名称
 * @return 如果WiFi接口已启用返回true，否则返回false
 */
static bool is_wifi_interface_up(const char *interface) {
    FILE *fp;
    char buffer[512];
    bool result = false;
    
    // 尝试打开/proc/net/dev文件
    fp = fopen(PROC_NET_DEV, "r");
    if (fp) {
        // 跳过前两行（标题行）
        fgets(buffer, sizeof(buffer), fp);
        fgets(buffer, sizeof(buffer), fp);
        
        // 逐行检查网络接口
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, interface)) {
                result = true;
                break;
            }
        }
        fclose(fp);
    }
    
    // 如果/proc/net/dev没有找到接口，尝试使用ifconfig
    if (!result) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "ifconfig %s 2>/dev/null | grep UP", interface);
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                result = true;
            }
            pclose(fp);
        }
    }
    
    return result;
}

/**
 * 检查WiFi是否已连接到AP
 * @param interface 网络接口名称
 * @return 如果WiFi已连接返回true，否则返回false
 */
static bool is_wifi_connected(const char *interface) {
    FILE *fp;
    char buffer[512];
    bool result = false;
    
    // 方法1：检查/proc/net/wireless文件
    fp = fopen(PROC_NET_WIRELESS, "r");
    if (fp) {
        // 跳过前两行（标题行）
        fgets(buffer, sizeof(buffer), fp);
        fgets(buffer, sizeof(buffer), fp);
        
        // 查找WiFi设备行
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, interface)) {
                // 连接状态通常显示为非零的链接质量
                result = (strstr(buffer, "0.") == NULL); // 简单判断链接质量不为0
                break;
            }
        }
        fclose(fp);
    }
    
    // 方法2：如果方法1失败，尝试使用iw命令
    if (!result) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "iw dev %s link 2>/dev/null | grep 'Connected to'", interface);
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                result = true;
            }
            pclose(fp);
        }
    }
    
    return result;
}

/**
 * 检测并更新WiFi状态
 */
static void update_wifi_status(void) {
    bool any_changes = false;  // 标记是否有任何接口状态发生变化
    bool need_notify = false;  // 是否需要发送通知
    
    // 检查每个WiFi接口的状态
    for (int i = 0; i < MAX_WIFI_INTERFACES; i++) {
        const char *interface = wifi_interfaces[i].interface;
        bool was_connected = wifi_interfaces[i].is_connected;
        bool is_up = is_wifi_interface_up(interface);
        bool is_connected = is_up && is_wifi_connected(interface);
        char ssid[64] = {0};
        
        // 获取SSID（如果已连接）
        if (is_connected) {
            get_current_ssid(interface, ssid, sizeof(ssid));
            if (ssid[0] == '\0') {
                // 如果无法获取SSID，使用默认名称
                snprintf(ssid, sizeof(ssid), "WiFi (%s)", interface);
            }
        }
        
        // 检测状态变化
        if (was_connected != is_connected || 
           (is_connected && strcmp(wifi_interfaces[i].ssid, ssid) != 0)) {
            
            // 更新接口状态
            wifi_interfaces[i].is_connected = is_connected;
            if (is_connected && ssid[0]) {
                strncpy(wifi_interfaces[i].ssid, ssid, sizeof(wifi_interfaces[i].ssid) - 1);
                wifi_interfaces[i].ssid[sizeof(wifi_interfaces[i].ssid) - 1] = '\0';
            } else {
                wifi_interfaces[i].ssid[0] = '\0';
            }
            
            any_changes = true;
            
            // 连接状态变化或连接到新网络时才需要通知
            if (was_connected != is_connected) {
                need_notify = true;
            }
        }
    }
    
    // 如果有任何接口状态变化，一次性更新UI
    if (any_changes) {
        // 确定全局WiFi状态和主要SSID
        bool any_connected = false;
        const char *primary_ssid = NULL;
        
        for (int i = 0; i < MAX_WIFI_INTERFACES; i++) {
            if (wifi_interfaces[i].is_connected) {
                any_connected = true;
                if (!primary_ssid) {
                    primary_ssid = wifi_interfaces[i].ssid;
                }
            }
        }
        
        // 更新全局WiFi状态
        wifi_state_t old_state = current_wifi_state;
        current_wifi_state = any_connected ? WIFI_STATE_CONNECTED : WIFI_STATE_DISCONNECTED;
        
        // 状态确实发生变化才通知
        if (need_notify || old_state != current_wifi_state) {
            // 使用单一通知显示所有接口状态
            for (int i = 0; i < MAX_WIFI_INTERFACES; i++) {
                if (wifi_interfaces[i].is_connected) {
                    // 发送第一个连接的接口信息
                    wifi_notification_update_interface(
                        WIFI_STATE_CONNECTED,
                        wifi_interfaces[i].ssid,
                        wifi_interfaces[i].interface
                    );
                    return; // 只需通知一次
                }
            }
            
            // 如果没有任何接口连接，发送断开连接状态
            wifi_notification_update_interface(
                WIFI_STATE_DISCONNECTED,
                NULL,
                WIFI_DEVICES[0] // 使用第一个接口作为默认
            );
        }
    }
}

/**
 * 定时检查WiFi状态的回调函数
 */
static void wifi_check_timer_cb(lv_timer_t *timer) {
    time_t now = time(NULL);
    
    // 定期检查WiFi状态（避免过于频繁）
    if (now - last_check_time >= CHECK_INTERVAL) {
        update_wifi_status();
        last_check_time = now;
    }
}

// WiFi状态变化回调函数 - 通知UI
void wifi_status_changed_callback(wifi_state_t new_state, const char *ssid) {
    // 显示带有新状态的通知
    wifi_notification_show(new_state, ssid);
    
    #if UI_DEBUG_ENABLED
    const char *state_str = "Unknown";
    switch (new_state) {
        case WIFI_STATE_DISCONNECTED: state_str = "Disconnected"; break;
        case WIFI_STATE_CONNECTING: state_str = "Connecting"; break;
        case WIFI_STATE_CONNECTED: state_str = "Connected"; break;
    }
    printf("[WIFI] Status changed: %s, SSID: %s\n", state_str, ssid ? ssid : "None");
    #endif
}

// 初始化WiFi管理器
void wifi_manager_init(void) {
    if (wifi_initialized) return;
    
    // 初始化WiFi接口状态数组
    init_wifi_interfaces();
    
    // 初始化WiFi通知系统
    wifi_notification_init();
    
    // 立即进行初始状态检查
    update_wifi_status();
    last_check_time = time(NULL);
    
    // 创建定时器定期检查WiFi状态
    lv_timer_create(wifi_check_timer_cb, CHECK_INTERVAL * 1000, NULL);
    
    wifi_initialized = true;
    
    #if UI_DEBUG_ENABLED
    printf("[WIFI] Manager initialized\n");
    #endif
}

// 手动触发WiFi状态检查
void wifi_manager_check_now(void) {
    update_wifi_status();
    last_check_time = time(NULL);
}

// 模拟WiFi状态变化的函数 - 用于测试，在实际部署时可以删除
void wifi_manager_simulate_state_change(void) {
    static wifi_state_t sim_state = WIFI_STATE_DISCONNECTED;
    static const char *test_ssids[] = {"Quark 2.4G", "H3-DevBoard", "MyHomeNetwork"};
    static int ssid_idx = 0;
    
    switch (sim_state) {
        case WIFI_STATE_DISCONNECTED:
            sim_state = WIFI_STATE_CONNECTING;
            wifi_status_changed_callback(sim_state, NULL);
            break;
            
        case WIFI_STATE_CONNECTING:
            sim_state = WIFI_STATE_CONNECTED;
            wifi_status_changed_callback(sim_state, test_ssids[ssid_idx]);
            ssid_idx = (ssid_idx + 1) % 3; // 循环使用测试SSID
            break;
            
        case WIFI_STATE_CONNECTED:
            sim_state = WIFI_STATE_DISCONNECTED;
            wifi_status_changed_callback(sim_state, NULL);
            break;
    }
}
