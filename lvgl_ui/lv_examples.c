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

// 定义页面数量和切换间隔
#define PAGE_COUNT 2  
#define PAGE_SWITCH_INTERVAL 15000  // 15秒
#define UPDATE_INTERVAL 500   // 更新间隔，毫秒

// 新增CPU核心相关变量
#define CPU_CORES 4  // 定义核心数量
static float cpu_usage = 0.0;      // 整体CPU使用率
static float cpu_core_usage[CPU_CORES] = {0}; // 每个核心的使用率
static uint8_t cpu_temp = 0;       // CPU温度
static uint8_t cpu_core_temp[CPU_CORES] = {0}; // 每个核心的温度


// 定义颜色
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

    //页面管理相关
    uint8_t current_page;      // 当前显示的页面
    lv_timer_t *page_timer;    // 页面切换定时器

    // 内存监控相关成员
    lv_obj_t *memory_arc;      // 内存半圆弧进度条
    lv_obj_t *memory_percent_label; // 内存百分比标签
    lv_obj_t *memory_info_label;    // 内存详细信息标签

    //CPU监控相关成员
    lv_obj_t *cpu_panel;          // CPU面板
    lv_obj_t *cpu_arc;            // 总体CPU进度条
    lv_obj_t *cpu_percent_label;  // 总体CPU百分比标签
    lv_obj_t *cpu_info_label;     // CPU温度信息标签

    // 四核心相关UI元素
    lv_obj_t *cpu_core_arc[CPU_CORES];         // 每个核心的进度条
    lv_obj_t *cpu_core_percent_label[CPU_CORES]; // 每个核心的百分比标签



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
static void create_cpu_4cores_page(void);
static void show_current_page(void);
static void read_cpu_data(void);
static void page_switch_timer_cb(lv_timer_t *timer);

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
    
    // 创建所有UI元素
    create_ui();                         // 创建第一页 - 存储/内存UI
    create_cpu_4cores_page();            // 创建第二页 - CPU四核UI
    
    // 读取数据
    read_storage_data();
    read_cpu_data();
    
    // 更新UI显示
    update_ui_values();
    
    // 创建定时器定期更新
    storage_ui->update_timer = lv_timer_create(update_timer_cb, UPDATE_INTERVAL, NULL);
    
    // 初始显示第一页
    show_current_page();
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
    
    // 需要添加以下代码:
    if(storage_ui->page_timer) {
        lv_timer_delete(storage_ui->page_timer);
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

static void create_cpu_4cores_page() {
    const lv_font_t *font_small = &lv_font_montserrat_12;
    const lv_font_t *font_mid = &lv_font_montserrat_14;
    
    uint32_t color_bg = 0x121212;         // 深色背景
    uint32_t color_panel = 0x1E1E1E;      // 面板背景
    uint32_t color_core_colors[CPU_CORES] = {
        0x3498DB,  // 蓝色 - 核心0
        0xE74C3C,  // 红色 - 核心1
        0xF39C12,  // 橙色 - 核心2
        0x9B59B6   // 紫色 - 核心3
    };
    uint32_t color_text = 0xFFFFFF;       // 文本颜色
    uint32_t color_text_secondary = 0xB0B0B0; // 次要文本
    
    // 创建CPU面板
    lv_obj_t *cpu_panel = lv_obj_create(storage_ui->main_cont);
    lv_obj_set_size(cpu_panel, 240, 135);
    lv_obj_align(cpu_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cpu_panel, lv_color_hex(color_panel), 0);
    lv_obj_set_style_border_width(cpu_panel, 0, 0);
    lv_obj_set_style_radius(cpu_panel, 10, 0);
    lv_obj_set_style_shadow_width(cpu_panel, 10, 0);
    lv_obj_set_style_shadow_color(cpu_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(cpu_panel, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(cpu_panel, 5, 0);
    storage_ui->cpu_panel = cpu_panel;
    
    // CPU标题
    lv_obj_t *cpu_title = lv_label_create(cpu_panel);
    lv_obj_set_style_text_font(cpu_title, font_small, 0);
    lv_obj_set_style_text_color(cpu_title, lv_color_hex(color_text), 0);
    lv_label_set_text(cpu_title, "CPU MONITOR");
    lv_obj_align(cpu_title, LV_ALIGN_TOP_MID, 0, 3);
    
    // 四个水平进度条，每个核心一个
    int bar_height = 5;       // 进度条高度
    int bar_width = 155;       // 进度条宽度
    int bar_spacing = 25;      // 进度条之间的垂直间距
    int start_y = 25;          // 第一个进度条的垂直位置
    
    for(int i = 0; i < CPU_CORES; i++) {
        // 核心标签
        lv_obj_t *core_label = lv_label_create(cpu_panel);
        lv_obj_set_style_text_font(core_label, font_small, 0);
        lv_obj_set_style_text_color(core_label, lv_color_hex(color_core_colors[i]), 0);
        lv_label_set_text_fmt(core_label, "C%d", i);
        lv_obj_align(core_label, LV_ALIGN_TOP_LEFT, 5, start_y + i * bar_spacing);
        
        // 使用进度条
        lv_obj_t *bar = lv_bar_create(cpu_panel);
        lv_obj_set_size(bar, bar_width, bar_height);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 25, start_y + i * bar_spacing);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(color_core_colors[i]), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 3, 0);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        
        // 保存进度条引用
        storage_ui->cpu_core_arc[i] = bar;
        
        // 核心百分比标签
        storage_ui->cpu_core_percent_label[i] = lv_label_create(cpu_panel);
        lv_obj_set_style_text_font(storage_ui->cpu_core_percent_label[i], font_small, 0);
        lv_obj_set_style_text_color(storage_ui->cpu_core_percent_label[i], lv_color_hex(color_text), 0);
        lv_label_set_text(storage_ui->cpu_core_percent_label[i], "0%");
        lv_obj_align_to(storage_ui->cpu_core_percent_label[i], bar, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    }
    
    // CPU温度显示
    storage_ui->cpu_info_label = lv_label_create(cpu_panel);
    lv_obj_set_style_text_font(storage_ui->cpu_info_label, font_small, 0);
    lv_obj_set_style_text_color(storage_ui->cpu_info_label, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(storage_ui->cpu_info_label, "Temperature: 0°C");
    lv_obj_align(storage_ui->cpu_info_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // 初始时隐藏CPU面板
    lv_obj_add_flag(cpu_panel, LV_OBJ_FLAG_HIDDEN);
}

/**
 * 创建UI元素
 */
static void create_ui(void) {
    // 字体定义
    const lv_font_t *font_big = &lv_font_montserrat_22;
    const lv_font_t *font_small = &lv_font_montserrat_12;
    
    // 优雅的配色方案
    uint32_t color_bg = 0x2D3436;        // 深板岩背景色
    uint32_t color_panel = 0x34495E;     // 深蓝色面板
    uint32_t color_accent1 = 0x9B59B6;   // 优雅的紫色
    uint32_t color_accent2 = 0x3498DB;   // 精致的蓝色
    uint32_t color_text = 0xECF0F1;      // 柔和的白色
    uint32_t color_text_secondary = 0xBDC3C7; // 银灰色
    
    // 设置主背景
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(color_bg), 0);
    
    // 主容器 - 全屏
    storage_ui->main_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(storage_ui->main_cont, 240, 135);
    lv_obj_set_pos(storage_ui->main_cont, 0, 0);
    lv_obj_set_style_bg_color(storage_ui->main_cont, lv_color_hex(color_bg), 0);
    lv_obj_set_style_border_width(storage_ui->main_cont, 0, 0);
    lv_obj_set_style_pad_all(storage_ui->main_cont, 10, 0);
    lv_obj_set_style_radius(storage_ui->main_cont, 0, 0);
    
    // 统一面板
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
    
    // 存储弧形进度条 - 左侧
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
    
    // 内存弧形进度条 - 右侧
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
    
    // 存储标题
    lv_obj_t *storage_title = lv_label_create(main_panel);
    lv_obj_set_style_text_font(storage_title, font_small, 0);
    lv_obj_set_style_text_color(storage_title, lv_color_hex(color_text_secondary), 0);
    lv_label_set_text(storage_title, "STORAGE");
    lv_obj_align_to(storage_title, storage_ui->storage_arc, LV_ALIGN_OUT_TOP_MID, 0, -5);

    // 存储百分比
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
    lv_obj_align_to(storage_ui->info_label, storage_ui->storage_arc, LV_ALIGN_OUT_BOTTOM_MID, -25, 0);
    
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
    lv_obj_align_to(storage_ui->memory_info_label, storage_ui->memory_arc, LV_ALIGN_OUT_BOTTOM_MID, -30, 0);
    
    // 初始化页面管理
    storage_ui->current_page = 0;  // 默认显示第一页
    storage_ui->page_timer = lv_timer_create(page_switch_timer_cb, PAGE_SWITCH_INTERVAL, NULL);
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
    // 更新CPU核心使用率
    for(int i = 0; i < CPU_CORES; i++) {
        lv_bar_set_value(storage_ui->cpu_core_arc[i], (uint8_t)cpu_core_usage[i], LV_ANIM_OFF);
        lv_label_set_text_fmt(storage_ui->cpu_core_percent_label[i], "%d%%", (uint8_t)cpu_core_usage[i]);
    }
    
    // 更新CPU温度
    lv_label_set_text_fmt(storage_ui->cpu_info_label, "Temperature: %d°C", cpu_temp);

}

/**
 * 定时器回调函数，更新存储数据
 */

static void update_timer_cb(lv_timer_t *timer) {
    // 简化逻辑，总是读取所有数据
    read_storage_data();
    read_cpu_data();
    update_ui_values();
}

/**
 * 页面切换回调函数
 */
static void page_switch_timer_cb(lv_timer_t *timer) {
    if(storage_ui == NULL || !storage_ui->storage_arc || !storage_ui->cpu_panel) return;
    LV_UNUSED(timer);
    
    // 切换页面
    storage_ui->current_page = (storage_ui->current_page + 1) % PAGE_COUNT;
    
    // 显示当前页面
    show_current_page();
}

static void show_current_page() {
    if(storage_ui == NULL) return;
    
    // 显示/隐藏第一页元素
    if(storage_ui->storage_arc && storage_ui->memory_arc) {
        if(storage_ui->current_page == 0) {
            lv_obj_clear_flag(storage_ui->storage_arc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(storage_ui->memory_arc, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(storage_ui->storage_arc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(storage_ui->memory_arc, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // 显示/隐藏第二页元素
    if(storage_ui->cpu_panel) {
        if(storage_ui->current_page == 1) {
            lv_obj_clear_flag(storage_ui->cpu_panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(storage_ui->cpu_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * 读取CPU数据 - 读取四核心各自的使用率
 */
static void read_cpu_data(void) {
    FILE *proc_stat;
    char buffer[256];
    static uint64_t prev_idle[CPU_CORES] = {0};
    static uint64_t prev_total[CPU_CORES] = {0};
    
    // 打开/proc/stat文件读取CPU使用率
    proc_stat = fopen("/proc/stat", "r");
    if(proc_stat) {
        // 跳过第一行(总体CPU使用率)
        if(fgets(buffer, sizeof(buffer), proc_stat)) {
            // 解析总体CPU使用率，格式：cpu user nice system idle iowait irq softirq steal guest guest_nice
            unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
            if(sscanf(buffer, "cpu %lu %lu %lu %lu %lu %lu %lu %lu", 
                     &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
                
                uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
                uint64_t idle_time = idle + iowait;
                
                // 计算使用率
                if(prev_total[0] != 0) {
                    uint64_t total_diff = total - prev_total[0];
                    uint64_t idle_diff = idle_time - prev_idle[0];
                    
                    if(total_diff > 0) {
                        cpu_usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
                    }
                }
                
                prev_idle[0] = idle_time;
                prev_total[0] = total;
            }
        }
        
        // 读取各个CPU核心
        for(int i = 0; i < CPU_CORES; i++) {
            if(fgets(buffer, sizeof(buffer), proc_stat)) {
                // 格式：cpuN user nice system idle iowait irq softirq steal guest guest_nice
                unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
                if(sscanf(buffer, "cpu%*d %lu %lu %lu %lu %lu %lu %lu %lu", 
                         &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
                    
                    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
                    uint64_t idle_time = idle + iowait;
                    
                    // 计算各核心使用率
                    if(prev_total[i+1] != 0) {
                        uint64_t total_diff = total - prev_total[i+1];
                        uint64_t idle_diff = idle_time - prev_idle[i+1];
                        
                        if(total_diff > 0) {
                            cpu_core_usage[i] = 100.0 * (1.0 - (double)idle_diff / total_diff);
                        }
                    }
                    
                    prev_idle[i+1] = idle_time;
                    prev_total[i+1] = total;
                }
            }
        }
        fclose(proc_stat);
    } else {
        // 模拟数据
        cpu_usage = (rand() % 100);
        for(int i = 0; i < CPU_CORES; i++) {
            cpu_core_usage[i] = (rand() % 100);
        }
    }
    
    // 读取CPU温度 (如果系统支持)
    FILE *cmd_output = popen("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null", "r");
    if(cmd_output) {
        if(fgets(buffer, sizeof(buffer), cmd_output)) {
            int temp = atoi(buffer);
            cpu_temp = temp / 1000; // 通常温度以毫摄氏度为单位
        }
        pclose(cmd_output);
    } else {
        // 模拟数据
        cpu_temp = 35 + (rand() % 20);
    }
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
