/**
 * @file lv_examples.c
 * 系统监控UI实现
 */

/*********************
 *      包含
 *********************/
#include "lv_examples.h"
#include <stdio.h>

/*********************
 *      定义
 *********************/
#define UPDATE_INTERVAL 1000  // 更新间隔，毫秒

/**********************
 *      类型定义
 **********************/
typedef struct {
    lv_obj_t *main_panel;       // 主面板
    lv_obj_t *title_label;      // 标题标签
    lv_obj_t *cpu_arc;          // CPU进度弧
    lv_obj_t *cpu_label;        // CPU标签
    lv_obj_t *cpu_value_label;  // CPU值标签
    lv_obj_t *mem_bar;          // 内存条
    lv_obj_t *mem_label;        // 内存标签
    lv_obj_t *mem_value_label;  // 内存值标签
    lv_obj_t *time_label;       // 时间标签
    lv_timer_t *update_timer;   // 更新定时器
} ui_monitor_t;

/**********************
 *  静态变量
 **********************/
static ui_monitor_t * ui_monitor;  // UI控件指针
static uint32_t mem_total = 1024 * 1024;  // 默认1GB
static uint32_t mem_used = 0;
static uint8_t cpu_usage = 0;
static bool auto_update = false;

/**********************
 *  静态函数原型
 **********************/
static void create_ui(void);
static void update_ui_values(void);
static void update_timer_cb(lv_timer_t * timer);
static void read_system_data(void);

/**********************
 *   全局函数
 **********************/

/**
 * 初始化并显示系统监控UI
 */
void ui_example_init(void) {
    // 分配内存
    ui_monitor = lv_malloc(sizeof(ui_monitor_t));
    if(ui_monitor == NULL) return;
    
    lv_memzero(ui_monitor, sizeof(ui_monitor_t));
    
    // 设置背景颜色
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x343247), 0);
    
    // 创建UI元素
    create_ui();
    
    // 读取系统数据
    read_system_data();
    
    // 更新UI显示
    update_ui_values();
    
    // 创建定时器自动更新UI
    ui_monitor->update_timer = lv_timer_create(update_timer_cb, UPDATE_INTERVAL, NULL);
    auto_update = true;
}

/**
 * 关闭系统监控UI并清理资源
 */
void ui_example_close(void) {
    if(ui_monitor == NULL) return;
    
    // 删除定时器
    if(ui_monitor->update_timer) {
        lv_timer_delete(ui_monitor->update_timer);
        ui_monitor->update_timer = NULL;
    }
    
    // 删除UI元素
    if(ui_monitor->main_panel) {
        lv_obj_delete(ui_monitor->main_panel);
        ui_monitor->main_panel = NULL;
    }
    
    // 释放内存
    lv_free(ui_monitor);
    ui_monitor = NULL;
    auto_update = false;
}

/**
 * 手动更新系统数据
 */
void ui_example_update_data(uint32_t _mem_total, uint32_t _mem_used, uint8_t _cpu_usage) {
    // 更新数据
    mem_total = _mem_total;
    mem_used = _mem_used;
    cpu_usage = _cpu_usage > 100 ? 100 : _cpu_usage;
    
    // 如果UI已初始化，则更新显示
    if(ui_monitor) {
        update_ui_values();
    }
}

/**********************
 *   静态函数
 **********************/

/**
 * 创建UI元素
 */
static void create_ui(void) {
    lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_title, lv_color_white());
    
    lv_style_t style_text;
    lv_style_init(&style_text);
    lv_style_set_text_font(&style_text, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_text, lv_color_white());
    
    // 主面板
    ui_monitor->main_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui_monitor->main_panel, LV_PCT(90), LV_PCT(90));
    lv_obj_center(ui_monitor->main_panel);
    lv_obj_set_style_bg_color(ui_monitor->main_panel, lv_color_hex(0x504b6c), 0);
    lv_obj_set_style_radius(ui_monitor->main_panel, 10, 0);
    lv_obj_set_style_pad_all(ui_monitor->main_panel, 20, 0);
    
    // 标题
    ui_monitor->title_label = lv_label_create(ui_monitor->main_panel);
    lv_obj_add_style(ui_monitor->title_label, &style_title, 0);
    lv_label_set_text(ui_monitor->title_label, "System Monitor");
    lv_obj_align(ui_monitor->title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // CPU使用率弧形进度条
    ui_monitor->cpu_arc = lv_arc_create(ui_monitor->main_panel);
    lv_obj_set_size(ui_monitor->cpu_arc, 150, 150);
    lv_obj_align(ui_monitor->cpu_arc, LV_ALIGN_TOP_MID, 0, 60);
    lv_arc_set_rotation(ui_monitor->cpu_arc, 135);
    lv_arc_set_bg_angles(ui_monitor->cpu_arc, 0, 270);
    lv_arc_set_value(ui_monitor->cpu_arc, 0);
    lv_obj_set_style_arc_color(ui_monitor->cpu_arc, lv_color_hex(0x6495ED), LV_PART_INDICATOR);
    lv_obj_remove_style(ui_monitor->cpu_arc, NULL, LV_PART_KNOB);
    
    // CPU标签
    ui_monitor->cpu_label = lv_label_create(ui_monitor->main_panel);
    lv_obj_add_style(ui_monitor->cpu_label, &style_text, 0);
    lv_label_set_text(ui_monitor->cpu_label, "CPU Usage");
    lv_obj_align(ui_monitor->cpu_label, LV_ALIGN_TOP_MID, 0, 220);
    
    // CPU值
    ui_monitor->cpu_value_label = lv_label_create(ui_monitor->main_panel);
    lv_obj_add_style(ui_monitor->cpu_value_label, &style_title, 0);
    lv_label_set_text(ui_monitor->cpu_value_label, "0%");
    lv_obj_align_to(ui_monitor->cpu_value_label, ui_monitor->cpu_arc, LV_ALIGN_CENTER, 0, 0);
    
    // 内存条
    ui_monitor->mem_label = lv_label_create(ui_monitor->main_panel);
    lv_obj_add_style(ui_monitor->mem_label, &style_text, 0);
    lv_label_set_text(ui_monitor->mem_label, "Mem Usd");
    lv_obj_align(ui_monitor->mem_label, LV_ALIGN_TOP_MID, 0, 260);
    
    ui_monitor->mem_bar = lv_bar_create(ui_monitor->main_panel);
    lv_obj_set_size(ui_monitor->mem_bar, 200, 20);
    lv_obj_align(ui_monitor->mem_bar, LV_ALIGN_TOP_MID, 0, 290);
    lv_bar_set_range(ui_monitor->mem_bar, 0, 100);
    lv_bar_set_value(ui_monitor->mem_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_monitor->mem_bar, lv_color_hex(0x2a2738), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_monitor->mem_bar, lv_color_hex(0x7DE0F8), LV_PART_INDICATOR);
    
    // 内存值
    ui_monitor->mem_value_label = lv_label_create(ui_monitor->main_panel);
    lv_obj_add_style(ui_monitor->mem_value_label, &style_text, 0);
    lv_label_set_text(ui_monitor->mem_value_label, "0 KB / 0 KB (0%)");
    lv_obj_align(ui_monitor->mem_value_label, LV_ALIGN_TOP_MID, 0, 320);
    
    // 时间标签
    ui_monitor->time_label = lv_label_create(ui_monitor->main_panel);
    lv_obj_add_style(ui_monitor->time_label, &style_text, 0);
    lv_label_set_text(ui_monitor->time_label, "Last updae: --:--:--");
    lv_obj_align(ui_monitor->time_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/**
 * 更新UI显示的数据
 */
static void update_ui_values(void) {
    if(ui_monitor == NULL) return;
    
    // 更新CPU弧形进度
    lv_arc_set_value(ui_monitor->cpu_arc, cpu_usage);
    
    // 更新CPU值标签
    lv_label_set_text_fmt(ui_monitor->cpu_value_label, "%d%%", cpu_usage);
    
    // 计算内存百分比
    int mem_percent = 0;
    if(mem_total > 0) {
        mem_percent = (mem_used * 100) / mem_total;
    }
    
    // 更新内存条
    lv_bar_set_value(ui_monitor->mem_bar, mem_percent, LV_ANIM_ON);
    
    // 更新内存值标签
    lv_label_set_text_fmt(ui_monitor->mem_value_label, "%d MB / %d MB (%d%%)", 
                          mem_used / 1024, mem_total / 1024, mem_percent);
    
    // 更新时间标签
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    lv_label_set_text_fmt(ui_monitor->time_label, "Last update: %02d:%02d:%02d",
                         timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

/**
 * 定时器回调函数，用于自动更新UI
 */
static void update_timer_cb(lv_timer_t * timer) {
    LV_UNUSED(timer);
    
    if(auto_update) {
        // 读取系统数据
        read_system_data();
        
        // 更新UI
        update_ui_values();
    }
}

/**
 * 从系统读取内存和CPU使用情况
 */
static void read_system_data(void) {
    // 读取内存信息
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if(meminfo) {
        char line[256];
        uint32_t mem_free = 0;
        
        while(fgets(line, sizeof(line), meminfo)) {
            // 获取总内存
            if(strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line, "MemTotal: %u", &mem_total);
            }
            // 获取可用内存
            else if(strncmp(line, "MemAvailable:", 13) == 0 || 
                    strncmp(line, "MemFree:", 8) == 0) {
                sscanf(line + 8, "%u", &mem_free);
            }
        }
        fclose(meminfo);
        
        // 计算已用内存
        if(mem_free <= mem_total) {
            mem_used = mem_total - mem_free;
        }
    }
    
    // 读取CPU使用率
    FILE *stat_file = fopen("/proc/stat", "r");
    if(stat_file) {
        static unsigned long long prev_idle = 0, prev_total = 0;
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        unsigned long long total;
        
        if(fscanf(stat_file, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                 &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
            // 计算总时间和空闲时间
            unsigned long long total_idle = idle + iowait;
            total = user + nice + system + idle + iowait + irq + softirq + steal;
            
            // 计算差值
            if(prev_total > 0 && total > prev_total) {
                unsigned long long diff_idle = total_idle - prev_idle;
                unsigned long long diff_total = total - prev_total;
                cpu_usage = 100 - (diff_idle * 100 / diff_total);
            }
            
            // 保存当前值，供下次计算
            prev_idle = total_idle;
            prev_total = total;
        }
        fclose(stat_file);
    }
}