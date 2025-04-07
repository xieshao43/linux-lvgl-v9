#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "wifi_notification.h"

/**
 * 初始化WiFi管理器
 * 会自动初始化WiFi通知系统并开始定期检查WiFi状态
 */
void wifi_manager_init(void);

/**
 * 手动触发WiFi状态检查
 */
void wifi_manager_check_now(void);

/**
 * 模拟WiFi状态变化（用于演示和测试）
 */
void wifi_manager_simulate_state_change(void);

#endif // WIFI_MANAGER_H
