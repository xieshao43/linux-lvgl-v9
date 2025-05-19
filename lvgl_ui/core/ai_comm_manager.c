#include "ai_comm_manager.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define UDP_PORT_RECV 5679  /* control_center向GUI的这个端口下发UI信息 */
#define UDP_PORT_SEND 5678  /* GUI向control_center的这个端口上传UI信息 */
#define MAX_MESSAGE_LENGTH 1024

// 回调节点结构
typedef struct callback_node {
    ai_message_callback_t callback;
    void* user_data;
    struct callback_node* next;
} callback_node_t;

// 管理器状态
static struct {
    p_ipc_endpoint_t endpoint;      // UDP端点
    callback_node_t* callbacks;     // 回调链表
    bool initialized;               // 是否已初始化
    char buffer[MAX_MESSAGE_LENGTH]; // 消息缓冲区
} manager = {0};

// UDP消息回调函数
static int udp_message_handler(char *buffer, size_t length, void *user_data) {
    if (!buffer || length == 0) return 0;
    
    // 打印接收的消息，便于调试
    printf("[AI_COMM] Received message: %.*s\n", (int)length, buffer);
    
    // 使用cJSON进行解析
    cJSON *root = cJSON_Parse(buffer);
    if (root) {
        // 解析文本字段
        cJSON *text_item = cJSON_GetObjectItem(root, "text");
        if (cJSON_IsString(text_item) && text_item->valuestring) {
            // 通知所有已注册的回调
            callback_node_t* node = manager.callbacks;
            while (node) {
                node->callback(AI_MSG_TEXT, text_item->valuestring, node->user_data);
                node = node->next;
            }
        }
        
        // 解析状态字段
        cJSON *state_item = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsNumber(state_item)) {
            int state = state_item->valueint;
            // 通知所有已注册的回调
            callback_node_t* node = manager.callbacks;
            while (node) {
                node->callback(AI_MSG_STATE, &state, node->user_data);
                node = node->next;
            }
        }
        
        cJSON_Delete(root);
    }
    
    return 0;
}

// 检查端口是否可用
static bool is_port_available(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    
    return result == 0;
}

bool ai_comm_manager_init(void) {
    if (manager.initialized) {
        return true; // 已经初始化，无需再次初始化
    }
    
    // 检查端口是否已被使用
    bool port_available = is_port_available(UDP_PORT_RECV);
    if (!port_available) {
        printf("[AI_COMM] Port %d is already in use, trying to connect without binding\n", UDP_PORT_RECV);
    }
    
    // 创建UDP端点
    manager.endpoint = ipc_endpoint_create_udp(UDP_PORT_RECV, UDP_PORT_SEND, udp_message_handler, NULL);
    
    if (!manager.endpoint) {
        fprintf(stderr, "[AI_COMM] Failed to initialize UDP endpoint\n");
        return false;
    }
    
    printf("[AI_COMM] UDP communication initialized, recv port: %d, send port: %d\n", 
           UDP_PORT_RECV, UDP_PORT_SEND);
    
    // 初始化回调链表
    manager.callbacks = NULL;
    manager.initialized = true;
    
    return true;
}

void ai_comm_manager_deinit(void) {
    if (!manager.initialized) {
        return; // 未初始化，无需清理
    }
    
    // 清理回调链表
    callback_node_t* current = manager.callbacks;
    while (current) {
        callback_node_t* next = current->next;
        free(current);
        current = next;
    }
    manager.callbacks = NULL;
    
    // 清理UDP端点
    if (manager.endpoint) {
        ipc_endpoint_destroy_udp(manager.endpoint);
        manager.endpoint = NULL;
    }
    
    manager.initialized = false;
}

void ai_comm_manager_register_callback(ai_message_callback_t callback, void* user_data) {
    if (!manager.initialized || !callback) {
        return;
    }
    
    // 创建新的回调节点
    callback_node_t* node = (callback_node_t*)malloc(sizeof(callback_node_t));
    if (!node) {
        fprintf(stderr, "[AI_COMM] Failed to allocate memory for callback\n");
        return;
    }
    
    node->callback = callback;
    node->user_data = user_data;
    node->next = manager.callbacks;
    manager.callbacks = node;
}

void ai_comm_manager_unregister_callback(ai_message_callback_t callback) {
    if (!manager.initialized || !callback) {
        return;
    }
    
    callback_node_t** pp = &manager.callbacks;
    while (*pp) {
        callback_node_t* current = *pp;
        
        if (current->callback == callback) {
            *pp = current->next;
            free(current);
        } else {
            pp = &(current->next);
        }
    }
}

bool ai_comm_manager_send_message(const char* message) {
    if (!manager.initialized || !manager.endpoint || !message) {
        return false;
    }
    
    return manager.endpoint->send(manager.endpoint, message, strlen(message)) == 0;
}

bool ai_comm_manager_is_connected(void) {
    return manager.initialized && manager.endpoint != NULL;
}
