#ifndef AI_COMM_MANAGER_H
#define AI_COMM_MANAGER_H

#include "../core/ipc_udp.h"
#include <stddef.h>
#include <stdbool.h>

// 消息类型定义
typedef enum {
    AI_MSG_TEXT,       // 文本消息
    AI_MSG_STATE,      // 状态更新
    AI_MSG_COMMAND     // 命令消息
} ai_message_type_t;

// AI消息回调函数类型
typedef void (*ai_message_callback_t)(ai_message_type_t type, const void* data, void* user_data);

// 初始化AI通信管理器
bool ai_comm_manager_init(void);

// 清理AI通信管理器
void ai_comm_manager_deinit(void);

// 注册消息回调
void ai_comm_manager_register_callback(ai_message_callback_t callback, void* user_data);

// 取消注册消息回调
void ai_comm_manager_unregister_callback(ai_message_callback_t callback);

// 发送消息
bool ai_comm_manager_send_message(const char* message);

// 获取连接状态
bool ai_comm_manager_is_connected(void);

#endif // AI_COMM_MANAGER_H
