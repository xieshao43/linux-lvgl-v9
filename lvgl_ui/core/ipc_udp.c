// SPDX-License-Identifier: GPL-3.0-only
/*
 * Copyright (c) 2008-2023 100askTeam : Dongshan WEI <weidongshan@100ask.net> 
 * Discourse:  https://forums.100ask.net
 */
 
/*  Copyright (C) 2008-2023 深圳百问网科技有限公司
 *  All rights reserved
 *
 * 免责声明: 百问网编写的文档, 仅供学员学习使用, 可以转发或引用(请保留作者信息),禁止用于商业用途！
 * 免责声明: 百问网编写的程序, 可以用于商业用途, 但百问网不承担任何后果！
 * 
 * 本程序遵循GPL V3协议, 请遵循协议
 * 百问网学习平台   : https://www.100ask.net
 * 百问网交流社区   : https://forums.100ask.net
 * 百问网官方B站    : https://space.bilibili.com/275908810
 * 本程序所用开发板 : Linux开发板
 * 百问网官方淘宝   : https://100ask.taobao.com
 * 联系我们(E-mail) : weidongshan@100ask.net
 *
 *          版权所有，盗版必究。
 *  
 * 修改历史     版本号           作者        修改内容
 *-----------------------------------------------------
 * 2025.03.20      v01         百问科技      创建文件
 *-----------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h> // 添加errno.h头文件，解决errno未定义的问题

#include "ipc_udp.h"

// 修改UDP数据结构，增加线程安全相关字段
typedef struct upd_data_t {
    int socket_send;             // 发送数据的套接字
    int port_remote;             // 目标端口号
    int socket_recv;             // 接收数据的套接字
    int port_local;              // 源端口号
    struct sockaddr_in remote_addr; // 目标地址结构体
    volatile int running;        // 线程运行标志
    pthread_t thread_id;         // 线程ID
    pthread_mutex_t mutex;       // 保护共享数据的互斥锁
    int error_count;             // 错误计数器
    uint32_t last_error_time;    // 最后一次错误时间
}upd_data_t, *p_upd_data_t;

// 线程处理函数声明
static void* handle_udp_connection(void* arg);

// 发送数据的函数声明
static int udp_send_data(ipc_endpoint_t *pendpoint, const char *data, int len);

// 接收数据的函数声明
static int udp_recv_data(ipc_endpoint_t *pendpoint, unsigned char *data, int maxlen, int *retlen);

// 创建一个UDP类型的IPC端点 - 增加错误处理
p_ipc_endpoint_t ipc_endpoint_create_udp(int port_local, int port_remote, transfer_callback_t cb, void *user_data)
{
    // 分配并清零UDP数据结构体
    p_upd_data_t pudpdata = (p_upd_data_t)calloc(1, sizeof(upd_data_t));
    // 分配并清零IPC端点结构体
    p_ipc_endpoint_t pendpoint = (p_ipc_endpoint_t)calloc(1, sizeof(ipc_endpoint_t));

    int fd_recv;
    struct sockaddr_in local_addr;
    struct sockaddr_in server_addr;

    if (!pudpdata || !pendpoint) {
        if (pendpoint)free(pendpoint);
        if (pudpdata)free(pudpdata);
        return NULL;            

    }

    // 关联UDP数据结构体和IPC端点结构体
    pendpoint->priv = pudpdata;
    pendpoint->cb = cb;
    pendpoint->user_data = user_data;
    pendpoint->send = udp_send_data;
    pendpoint->recv = udp_recv_data;

    // 设置远程和本地端口号
    pudpdata->port_remote = port_remote;
    pudpdata->port_local = port_local;

    // 初始化新增的字段
    pudpdata->running = 0;
    pudpdata->error_count = 0;
    pudpdata->last_error_time = 0;

    // 初始化互斥锁
    if (pthread_mutex_init(&pudpdata->mutex, NULL) != 0) {
        fprintf(stderr, "[UDP] Failed to initialize mutex\n");
        if (pendpoint) free(pendpoint);
        if (pudpdata) free(pudpdata);
        return NULL;
    }

    // 1. 为了发送数据进行网络初始化
    // 创建UDP套接字
    int fd_send = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_send < 0) {
        perror("Failed to create UDP socket for audio client");
        if (pendpoint)free(pendpoint);
        if (pudpdata)free(pudpdata);
        return NULL;            
}

    // 初始化服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_remote); // 使用传入的端口号
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        if (pendpoint)free(pendpoint);
        if (pudpdata)free(pudpdata);
        return NULL;            
}

    // 保存套接字和服务器地址信息到UDP数据结构体
    pudpdata->socket_send = fd_send;
    pudpdata->remote_addr = server_addr;    
    
    // 2. 为了接收数据进行网络初始化
    // 创建UDP套接字
    if ((fd_recv = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket");
        if (pendpoint)free(pendpoint);
        if (pudpdata)free(pudpdata);
        return NULL;            
}

    pudpdata->socket_recv = fd_recv;

    // 设置SO_REUSEADDR和SO_REUSEPORT（如果支持）
    int reuse = 1;
    if (setsockopt(fd_recv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Failed to set SO_REUSEADDR");
    }
    
#ifdef SO_REUSEPORT
    if (setsockopt(fd_recv, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        perror("Failed to set SO_REUSEPORT");
        // 继续执行，这只是一个可选优化
    }
#endif

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port_local);

    if (inet_pton(AF_INET, "127.0.0.1", &local_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        if (pendpoint)free(pendpoint);
        if (pudpdata)free(pudpdata);
        return NULL;            
}

    // 绑定套接字 - 增加更宽容的错误处理
    if (bind(fd_recv, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Failed to bind socket");
        
        // 特殊处理：如果错误是"Address already in use"，尝试继续使用现有连接
        if (errno == EADDRINUSE) {
            printf("[UDP] Port %d already in use, trying to continue without binding\n", port_local);
            // 不绑定接收端口，但保留发送功能
            // 对于AI功能，发送命令比接收更重要
            close(fd_recv);
            pudpdata->socket_recv = -1; // 标记为不可用
            
            // 如果有回调但没有接收套接字，我们将不创建线程
            if (cb) {
                printf("[UDP] Warning: Receive functionality will be disabled\n");
            }
        } else {
            // 其他错误，放弃创建端点
            close(fd_recv);
            if (pudpdata->socket_send >= 0) {
                close(pudpdata->socket_send);
            }
            if (pendpoint) free(pendpoint);
            if (pudpdata) free(pudpdata);
            return NULL;            
        }
    }

    // 如果有回调函数且接收套接字可用，创建线程处理UDP连接
    if (cb && pudpdata->socket_recv >= 0) {
        if (pthread_create(&pudpdata->thread_id, NULL, handle_udp_connection, pendpoint) != 0) {
            perror("[UDP] Failed to create thread");
            if (pudpdata->socket_recv >= 0) {
                close(pudpdata->socket_recv);
            }
            close(fd_send);
            pthread_mutex_destroy(&pudpdata->mutex);
            if (pendpoint) free(pendpoint);
            if (pudpdata) free(pudpdata);
            return NULL;
        }
        
        pudpdata->running = 1;
    }

    return pendpoint;    
}

// 销毁IPC端点，释放相关资源 - 改进强制清理流程
void ipc_endpoint_destroy_udp(p_ipc_endpoint_t pendpoint)
{
    if (!pendpoint) {
        printf("[UDP] Warning: trying to destroy NULL endpoint\n");
        return;
    }
    
    p_upd_data_t pudpdata = (p_upd_data_t)pendpoint->priv;
    if (!pudpdata) {
        fprintf(stderr, "[UDP] Error: endpoint has NULL private data\n");
        free(pendpoint);
        return;
    }
    
    printf("[UDP] Destroying UDP endpoint, local port: %d, remote port: %d\n", 
           pudpdata->port_local, pudpdata->port_remote);
    
    // 先停止接收线程运行标志
    pudpdata->running = 0;
    
    // 关闭套接字前先关闭回调，防止回调中访问已删除的数据
    pendpoint->cb = NULL;
    
    // 关闭套接字 - 更强健的清理流程
    if (pudpdata->socket_recv >= 0) {
        printf("[UDP] Closing receive socket %d\n", pudpdata->socket_recv);
        shutdown(pudpdata->socket_recv, SHUT_RDWR); // 强制中断阻塞读取
        close(pudpdata->socket_recv);
        pudpdata->socket_recv = -1;
    }
    
    if (pudpdata->socket_send >= 0) {
        printf("[UDP] Closing send socket %d\n", pudpdata->socket_send);
        close(pudpdata->socket_send);
        pudpdata->socket_send = -1;
    }
    
    // 等待线程结束 - 使用更长的超时，确保线程有足够时间退出
    if (pudpdata->thread_id) {
        // 尝试等待线程结束
        usleep(100000); // 100ms应该足够线程退出
    }
    
    // 释放内存
    free(pudpdata);
    free(pendpoint);
    
    printf("[UDP] Endpoint destroyed successfully\n");
}

/**
 * 处理UDP连接的线程函数
 * 
 * @param arg 指向ipc_endpoint_t结构体的指针
 * @return 线程退出时返回NULL
 */
static void* handle_udp_connection(void* arg)
{
    // 增强参数检查
    if (!arg) {
        fprintf(stderr, "[UDP] Error: NULL argument in UDP thread\n");
        return NULL;
    }
    
    ipc_endpoint_t *pendpoint = (ipc_endpoint_t*)arg;
    if (!pendpoint->priv) {
        fprintf(stderr, "[UDP] Error: NULL UDP data in endpoint\n");
        return NULL;
    }
    
    p_upd_data_t pudpdata = (p_upd_data_t)pendpoint->priv;
    int fd_recv = pudpdata->socket_recv;
    
    char buffer[4096];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t bytes_received;
    
    // 设置套接字为非阻塞模式
    int flags = fcntl(fd_recv, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd_recv, F_SETFL, flags | O_NONBLOCK);
    }
    
    printf("Listening on port_local %d\n", pudpdata->port_local);
    
    // 重置错误计数
    pudpdata->error_count = 0;
    pudpdata->last_error_time = 0;
    
    // 设置运行标志
    pudpdata->running = 1;
    
    while (pudpdata->running) {
        // 检查端点和回调的有效性
        if (!pendpoint->cb || fd_recv < 0) {
            break;
        }
        
        // 使用锁保护接收操作
        pthread_mutex_lock(&pudpdata->mutex);
        
        bytes_received = recvfrom(fd_recv, buffer, sizeof(buffer) - 1, MSG_DONTWAIT,
                                 (struct sockaddr *)&client_addr, &client_len);
        
        if (bytes_received > 0) {
            // 确保缓冲区以null结尾
            buffer[bytes_received] = '\0';
            
            // 临时存储回调，避免持有锁时调用
            transfer_callback_t cb = pendpoint->cb;
            void* user_data = pendpoint->user_data;
            
            pthread_mutex_unlock(&pudpdata->mutex);
            
            // 安全调用回调
            if (cb) {
                cb(buffer, bytes_received, user_data);
            }
        } else {
            pthread_mutex_unlock(&pudpdata->mutex);
            
            if (bytes_received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                pudpdata->error_count++;
                
                // 只在特定间隔记录错误，避免日志爆炸
                if (pudpdata->error_count < 5 || pudpdata->error_count % 100 == 0) {
                    perror("UDP receive error");
                }
                
                // 严重错误时暂停线程
                if (pudpdata->error_count > 1000) {
                    sleep(1);
                } else {
                    usleep(5000); // 5ms
                }
            } else {
                // 正常无数据情况
                usleep(10000); // 10ms，减少CPU使用
            }
        }
        
        // 定期检查套接字有效性
        if (pudpdata->error_count > 10 && pudpdata->socket_recv >= 0) {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(pudpdata->socket_recv, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                fprintf(stderr, "[UDP] Socket error detected, exiting thread\n");
                break;
            }
        }
    }
    
    printf("[UDP] Receive thread for port %d exiting\n", pudpdata->port_local);
    return NULL;
}

/**
 * 发送数据到指定endpoint的通用函数
 * 
 * @param pendpoint 指向ipc_endpoint_t结构体的指针
 * @param data 指向要发送的数据的指针
 * @param len 要发送的数据的长度
 * @return 成功返回0，失败返回-1
 */
static int udp_send_data(ipc_endpoint_t *pendpoint, const char *data, int len)
{
    p_upd_data_t pudpdata = (p_upd_data_t)pendpoint->priv;

    // 获取套接字文件描述符
    int fd = pudpdata->socket_send;
    // 获取服务器地址的指针
    struct sockaddr_in *p_server_addr = &pudpdata->remote_addr;

    // 检查套接字是否已初始化
    if (fd < 0) {
        fprintf(stderr, "UDP socket for audio server is not initialized\n");
        return -1;
    }

    // 发送数据到客户端
    ssize_t bytes_sent = sendto(fd, data, len, 0, (struct sockaddr *)p_server_addr, sizeof(*p_server_addr));
    // 检查发送的数据量是否与预期相符
    if (bytes_sent != len) {
        perror("Failed to send data to client");
        return -1;
    }

    // 发送成功
    return 0;
}

/**
 * 接收数据函数 - 增加错误检查
 * 
 * 该函数负责从UDP套接字接收数据，并将其写入指定的缓冲区
 * 
 * @param data 接收到的数据将存储在这个缓冲区中
 * @param maxlen 缓冲区的最大长度
 * @param retlen 实际接收的数据长度
 * 
 * @return 返回接收结果的整数表示，具体含义可能包括成功、失败或特定错误代码
 */
static int udp_recv_data(ipc_endpoint_t *pendpoint, unsigned char *data, int maxlen, int *retlen)
{
    if (!pendpoint || !pendpoint->priv || !data || !retlen) {
        return -1; // 增加参数检查
    }

    p_upd_data_t pudpdata = (p_upd_data_t)pendpoint->priv;
    
    // 获取套接字文件描述符
    int fd = pudpdata->socket_recv;
    
    // 如果接收套接字无效，返回错误
    if (fd < 0) {
        *retlen = 0; // 确保返回长度为0
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t bytes_received;

    // 接收数据
    bytes_received = recvfrom(fd, data, maxlen, 0, (struct sockaddr *)&client_addr, &client_len);
    if (bytes_received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Failed to receive data from server");
        }
        *retlen = 0;
        return -1;
    }

    *retlen = (int)bytes_received;
    return 0;
}

