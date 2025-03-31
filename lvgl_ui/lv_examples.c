/**
 * @file lv_storage_monitor.c
 * 系统存储监控UI实现
 */

/*********************
 *      包含
 *********************/
#include "lv_examples.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*********************
 *      定义
 *********************/
#define UPDATE_INTERVAL 500   // 更新间隔，毫秒
#define COLOR_PRIMARY   0x3D8361  // 墨绿色
#define COLOR_BG        0x1E2A2A  // 深灰绿色
#define COLOR_SECONDARY 0xD8B08C  // 高级金棕色

/**********************
 *      类型定义
 **********************/
typedef struct {
    lv_obj_t *main_cont;       // 主容器
    lv_obj_t *title_label;     // 标题
    lv_obj_t *storage_arc;     // 存储环形进度条
    lv_obj_t *percent_label;   // 百分比标签
    lv_obj_t *info_label;      // 详细信息标签
    lv_obj_t *path_label;      // 路径标签
    lv_timer_t *update_timer;  // 更新定时器

    // 新增内存监控相关成员
    lv_obj_t *memory_arc;      // 内存半圆弧进度条
    lv_obj_t *memory_percent_label; // 内存百分比标签
    lv_obj_t *memory_info_label;    // 内存详细信息标签
} storage_ui_t;

/**********************
 *  静态变量
 **********************/
static storage_ui_t *storage_ui;
static uint64_t total_space = 0;     // 总容量 (KB)
static uint64_t used_space = 0;      // 已用容量 (KB)
static char storage_path[64] = "/";  // 监测路径

// 新增内存相关的静态变量
static uint64_t total_memory = 0;    // 总内存 (KB)
static uint64_t used_memory = 0;     // 已用内存 (KB)

/**********************
 *  静态函数原型
 **********************/
static void create_ui(void);
static void update_ui_values(void);
static void update_timer_cb(lv_timer_t *timer);
static void read_storage_data(void);
static const char *get_size_str(uint64_t size_kb, char *buf, int buf_size);

/**********************
 *   全局函数
 **********************/

/**
 * 初始化并显示存储监控UI
 */
void storage_monitor_init(const char *path) {
    // 存储监测路径
    if(path != NULL) {
        strncpy(storage_path, path, sizeof(storage_path) - 1);
        storage_path[sizeof(storage_path) - 1] = '\0';
    }
    
    // 分配UI结构体内存
    storage_ui = lv_malloc(sizeof(storage_ui_t));
    if(storage_ui == NULL) {
        printf("存储监控UI内存分配失败\n");
        return;
    }
    
    memset(storage_ui, 0, sizeof(storage_ui_t));
    
    // 设置背景颜色
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(COLOR_BG), 0);
    
    // 创建UI元素
    create_ui();
    
    // 读取存储数据
    read_storage_data();
    
    // 更新UI显示
    update_ui_values();
    
    // 创建定时器定期更新
    storage_ui->update_timer = lv_timer_create(update_timer_cb, UPDATE_INTERVAL, NULL);
}

/**
 * 关闭存储监控UI
 */
void storage_monitor_close(void) {
    if(storage_ui == NULL) return;
    
    // 删除定时器
    if(storage_ui->update_timer) {
        lv_timer_delete(storage_ui->update_timer);
    }
    
    // 删除UI元素
    if(storage_ui->main_cont) {
        lv_obj_delete(storage_ui->main_cont);
    }
    
    // 释放内存
    lv_free(storage_ui);
    storage_ui = NULL;
}

/**********************
 *   静态函数
 **********************/

/**
 * 创建UI元素
 */
static void create_ui(void) {
    // Font definitions
    const lv_font_t *font_big = &lv_font_montserrat_22;
    const lv_font_t *font_small = &lv_font_montserrat_12;
    
    // Elegant color scheme
    uint32_t color_bg = 0x2D3436;        // Deep slate background
    uint32_t color_panel = 0x34495E;     // Rich navy panel
    uint32_t color_accent1 = 0x9B59B6;   // Elegant purple
    uint32_t color_accent2 = 0x3498DB;   // Sophisticated blue
    uint32_t color_text = 0xECF0F1;      // Soft white
    uint32_t color_text_secondary = 0xBDC3C7; // Silver gray
    
    // Set main background
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(color_bg), 0);
    
    // Main container - full screen
    storage_ui->main_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(storage_ui->main_cont, 240, 135);
    lv_obj_set_pos(storage_ui->main_cont, 0, 0);
    lv_obj_set_style_bg_color(storage_ui->main_cont, lv_color_hex(color_bg), 0);
    lv_obj_set_style_border_width(storage_ui->main_cont, 0, 0);
    lv_obj_set_style_pad_all(storage_ui->main_cont, 10, 0);
    lv_obj_set_style_radius(storage_ui->main_cont, 0, 0);
    
    // Single unified panel
    lv_obj_t *main_panel = lv_obj_create(storage_ui->main_cont);
    lv_obj_set_size(main_panel, 235, 130);
    lv_obj_align(main_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(main_panel, lv_color_hex(color_panel), 0);
    lv_obj_set_style_border_width(main_panel, 0, 0);
    lv_obj_set_style_radius(main_panel, 12, 0);
    lv_obj_set_style_pad_all(main_panel, 10, 0);
    lv_obj_set_style_shadow_width(main_panel, 15, 0);
    lv_obj_set_style_shadow_color(main_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(main_panel, LV_OPA_30, 0);
    
    // Storage arc - left side
    storage_ui->storage_arc = lv_arc_create(main_panel);
    lv_obj_set_size(storage_ui->storage_arc, 75, 75);
    lv_obj_align(storage_ui->storage_arc, LV_ALIGN_LEFT_MID, 10, 0);
    lv_arc_set_rotation(storage_ui->storage_arc, 270);
    lv_arc_set_bg_angles(storage_ui->storage_arc, 0, 360);
    lv_arc_set_range(storage_ui->storage_arc, 0, 100);
    lv_arc_set_value(storage_ui->storage_arc, 0);
    lv_obj_remove_style(storage_ui->storage_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(storage_ui->storage_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(storage_ui->storage_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(storage_ui->storage_arc, lv_color_hex(COLOR_SECONDARY), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(storage_ui->storage_arc, lv_color_hex(0x4A5568), LV_PART_MAIN);
    
    // Memory arc - right side
    storage_ui->memory_arc = lv_arc_create(main_panel);
    lv_obj_set_size(storage_ui->memory_arc, 75, 75);
    lv_obj_align(storage_ui->memory_arc, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_arc_set_rotation(storage_ui->memory_arc, 270);
    lv_arc_set_bg_angles(storage_ui->memory_arc, 0, 360);
    lv_arc_set_range(storage_ui->memory_arc, 0, 100);
    lv_arc_set_value(storage_ui->memory_arc, 0);
    lv_obj_remove_style(storage_ui->memory_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(storage_ui->memory_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(storage_ui->memory_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(storage_ui->memory_arc, lv_color_hex(COLOR_SECONDARY), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(storage_ui->memory_arc, lv_color_hex(0x4A5568), LV_PART_MAIN);
    
    // Storage title
    lv_obj_t *storage_title = lv_label_create(main_panel);
    lv_obj_set_style_text_font(storage_title, font_small, 0);
    lv_obj_set_style_text_color(storage_title, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(storage_title, "STORAGE");
    lv_obj_align_to(storage_title, storage_ui->storage_arc, LV_ALIGN_OUT_TOP_MID, 0, -5);

    // Storage percentage
    storage_ui->percent_label = lv_label_create(main_panel);
    lv_obj_set_style_text_font(storage_ui->percent_label, font_big, 0);
    lv_obj_set_style_text_color(storage_ui->percent_label, lv_color_hex(color_text), 0);
    lv_label_set_text(storage_ui->percent_label, "0%");
    lv_obj_align_to(storage_ui->percent_label, storage_ui->storage_arc, LV_ALIGN_CENTER, 0, 0);

    // Storage details - aligned with its arc
    storage_ui->info_label = lv_label_create(main_panel);
    lv_obj_set_style_text_font(storage_ui->info_label, font_small, 0);
    lv_obj_set_style_text_color(storage_ui->info_label, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(storage_ui->info_label, "0 / 0");
    lv_obj_align_to(storage_ui->info_label, storage_ui->storage_arc, LV_ALIGN_OUT_BOTTOM_MID, -20, 0);
    
    // Memory title
    lv_obj_t *memory_title = lv_label_create(main_panel);
    lv_obj_set_style_text_font(memory_title, font_small, 0);
    lv_obj_set_style_text_color(memory_title, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(memory_title, "MEMORY");
    lv_obj_align_to(memory_title, storage_ui->memory_arc, LV_ALIGN_OUT_TOP_MID, 0, -5);

    // Memory percentage
    storage_ui->memory_percent_label = lv_label_create(main_panel);
    lv_obj_set_style_text_font(storage_ui->memory_percent_label, font_big, 0);
    lv_obj_set_style_text_color(storage_ui->memory_percent_label, lv_color_hex(color_text), 0);
    lv_label_set_text(storage_ui->memory_percent_label, "0%");
    lv_obj_align_to(storage_ui->memory_percent_label, storage_ui->memory_arc, LV_ALIGN_CENTER, 0, 0);
    
    // Memory details - aligned with its arc
    storage_ui->memory_info_label = lv_label_create(main_panel);
    lv_obj_set_style_text_font(storage_ui->memory_info_label, font_small, 0);
    lv_obj_set_style_text_color(storage_ui->memory_info_label, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(storage_ui->memory_info_label, "0 / 0");
    lv_obj_align_to(storage_ui->memory_info_label, storage_ui->memory_arc, LV_ALIGN_OUT_BOTTOM_MID, -25, 0);
}

/**
 * 更新UI显示
 */
static void update_ui_values(void) {
    if(storage_ui == NULL) return;
    
    // 计算存储使用百分比
    uint8_t storage_percent = 0;
    if(total_space > 0) {
        storage_percent = (uint8_t)((used_space * 100) / total_space);
    }
    
    // 设置存储弧形进度条
    lv_arc_set_value(storage_ui->storage_arc, storage_percent);
    
    // 更新存储百分比标签
    lv_label_set_text_fmt(storage_ui->percent_label, "%d%%", storage_percent);
    
    // 更新存储详细信息标签
    char used_buf[20], total_buf[20];
    lv_label_set_text_fmt(storage_ui->info_label, "%s / %s", 
                          get_size_str(used_space, used_buf, sizeof(used_buf)),
                          get_size_str(total_space, total_buf, sizeof(total_buf)));

    // 计算内存使用百分比
    uint8_t memory_percent = 0;
    if(total_memory > 0) {
        memory_percent = (uint8_t)((used_memory * 100) / total_memory);
    }
    
    // 设置内存半圆弧进度条
    lv_arc_set_value(storage_ui->memory_arc, memory_percent);
    
    // 更新内存百分比标签
    lv_label_set_text_fmt(storage_ui->memory_percent_label, "%d%%", memory_percent);
    
    // 更新内存详细信息标签
    lv_label_set_text_fmt(storage_ui->memory_info_label, "%s / %s", 
                          get_size_str(used_memory, used_buf, sizeof(used_buf)),
                          get_size_str(total_memory, total_buf, sizeof(total_buf)));
}

/**
 * 定时器回调函数，更新存储数据
 */
static void update_timer_cb(lv_timer_t *timer) {
    LV_UNUSED(timer);
    
    read_storage_data();
    update_ui_values();
}

/**
 * 读取系统存储数据
 */
static void read_storage_data(void) {
    // 尝试使用statvfs读取文件系统信息
    FILE *cmd_output;
    char buffer[256];
    
    // 默认值，以防无法读取
    total_space = 10 * 1024 * 1024;  // 10 GB
    used_space = 4 * 1024 * 1024;    // 4 GB
    
    // 执行df命令获取存储信息
    snprintf(buffer, sizeof(buffer), "df -k %s | tail -n 1", storage_path);
    cmd_output = popen(buffer, "r");
    
    if(cmd_output) {
        if(fgets(buffer, sizeof(buffer), cmd_output) != NULL) {
            // 解析df命令输出，格式: 文件系统 总块数 已用 可用 已用% 挂载点
            char fs[64], mount[64];
            uint64_t blocks, used, available;
            int percent;
            
            if(sscanf(buffer, "%s %llu %llu %llu %d%% %s", 
                    fs, &blocks, &used, &available, &percent, mount) >= 5) {
                // df命令的块大小通常是1024字节(1KB)
                total_space = blocks;
                used_space = used;
            }
        }
        pclose(cmd_output);
    } else {
        // 模拟数据，添加一些随机变化
        static int direction = 1;
        
        used_space += (rand() % 1024) * 100 * direction;
        
        if(used_space > total_space * 0.95) {
            direction = -1;
        } else if(used_space < total_space * 0.3) {
            direction = 1;
        }
        
        if(used_space > total_space) {
            used_space = total_space;
        }
    }

    // 读取内存信息
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (strstr(line, "MemTotal:")) {
                sscanf(line, "MemTotal: %llu kB", &total_memory);
            } else if (strstr(line, "MemAvailable:")) {
                uint64_t available_memory;
                sscanf(line, "MemAvailable: %llu kB", &available_memory);
                used_memory = total_memory - available_memory;
            }
        }
        fclose(meminfo);
    } else {
        // 模拟内存数据
        total_memory = 2048 * 1024;  // 2 GB
        used_memory = 512 * 1024;    // 512 MB
    }
}

/**
 * 将KB大小转换为可读性好的字符串(KB, MB, GB)
 */
static const char *get_size_str(uint64_t size_kb, char *buf, int buf_size) {
    const char *units[] = {"KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = (double)size_kb;
    
    while(size >= 1024 && unit_idx < 3) {
        size /= 1024;
        unit_idx++;
    }
    
    if(size < 10) {
        snprintf(buf, buf_size, "%.1f %s", size, units[unit_idx]);
    } else {
        snprintf(buf, buf_size, "%.0f %s", size, units[unit_idx]);
    }
    
    return buf;
}    
