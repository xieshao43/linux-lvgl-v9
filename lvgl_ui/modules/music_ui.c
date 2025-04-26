#include "music_ui.h"
#include "menu_ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 定义其他在common.h中没有的颜色
#define COLOR_BACKGROUND    lv_color_hex(0x121212)
#define COLOR_SURFACE       lv_color_hex(0x1E1E1E)
#define COLOR_TEXT_PRIMARY  lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_SECONDARY lv_color_hex(0xDDDDDD)
// 添加新颜色定义 - 更优雅的配色
#define COLOR_MUSIC_TITLE   lv_color_hex(0xFFC0CB) // 柔和的粉红色标题
#define COLOR_MUSIC_TEXT    lv_color_hex(0xE6E6FA) // 淡紫色/薰衣草色文本

// 动画时间常量
#define ANIM_TIME_DEFAULT   300
#define ANIM_TIME_LONG      500

// 简化的音乐UI数据结构 - 移除画布和进度条相关元素
typedef struct {
    lv_obj_t *screen;           // 主屏幕
    lv_obj_t *album_cover;      // 专辑封面容器
    lv_obj_t *album_lottie;     // Lottie动画对象
    lv_obj_t *song_title;       // 歌曲标题
    lv_obj_t *time_elapsed;     // 已播放时间
    lv_obj_t *time_total;       // 总时长
    lv_obj_t *lyrics_container; // 歌词容器
    lv_obj_t *lyrics_text;      // 歌词文本
    lv_timer_t *button_timer;   // 按钮检测定时器
    lv_timer_t *scroll_timer;   // 添加滚动定时器
    bool is_active;             // 是否活动状态
} music_ui_data_t;;

// 音乐信息
typedef struct {
    const char *title;
    uint32_t duration;          // 单位：秒
    const char *lyrics;         // 歌词文本
} music_track_t;

// 全局UI数据
static music_ui_data_t ui_data = {0};

// 示例音乐数据
static music_track_t current_track = {
    .title = "Now Playing Music",
    .duration = 180,
    .lyrics = "Now playing music...\n\n[00:00] Welcome to music player\n[00:10] Enjoy your favorite songs\n[00:20] With clear lyrics display\n[00:30] Double-click to return\n[00:40] To the main menu\n[00:50] The lyrics area supports\n[01:00] Auto-scrolling feature\n[01:10] For better reading\n[01:20] Experience while listening\n[01:30] To your favorite music"
};

// 函数前向声明
static void return_to_menu(void);
static void button_event_timer_cb(lv_timer_t *timer);
static void start_lyrics_autoscroll(void);
static void scroll_lyrics_timer_cb(lv_timer_t *timer);

// 简化版的音乐界面创建函数
void music_ui_create_screen(void) {
    // 创建基本屏幕
    ui_data.screen = lv_obj_create(NULL);
    
    // 应用与其他模块一致的渐变背景
#if LV_USE_DRAW_SW_COMPLEX_GRADIENTS
    // 定义渐变色 - 与CPU界面一致的紫色到黑色渐变
    static const lv_color_t grad_colors[2] = {
        LV_COLOR_MAKE(0x9B, 0x18, 0x42), // 紫红色
        LV_COLOR_MAKE(0x00, 0x00, 0x00), // 纯黑色
    };
    
    static lv_grad_dsc_t grad;
    lv_grad_init_stops(&grad, grad_colors, NULL, NULL, sizeof(grad_colors) / sizeof(lv_color_t));
    lv_grad_radial_init(&grad, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_BOTTOM, LV_GRAD_EXTEND_PAD);
    
    // 应用渐变背景
    lv_obj_set_style_bg_grad(ui_data.screen, &grad, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#else
    // 纯色备选方案
    lv_obj_set_style_bg_color(ui_data.screen, COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(ui_data.screen, LV_OPA_COVER, 0);
#endif
    
    lv_obj_clear_flag(ui_data.screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建标题 - 顶部居中
    ui_data.song_title = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(ui_data.song_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_data.song_title, COLOR_MUSIC_TITLE, 0);
    lv_obj_set_width(ui_data.song_title, 220);
    lv_obj_set_style_text_align(ui_data.song_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ui_data.song_title, current_track.title);
    lv_obj_align(ui_data.song_title, LV_ALIGN_TOP_MID, 0, 5);

        // 创建专辑封面容器 - 左侧偏下 - 圆角正方形
    ui_data.album_cover = lv_obj_create(ui_data.screen);
    lv_obj_set_size(ui_data.album_cover, 85, 85);
    lv_obj_set_style_bg_color(ui_data.album_cover, lv_color_hex(0x1A1A1A), 0); // 深色背景
    lv_obj_set_style_bg_opa(ui_data.album_cover, LV_OPA_0, 0);
    lv_obj_set_style_radius(ui_data.album_cover, 15, 0); // 圆角半径
    lv_obj_set_style_border_width(ui_data.album_cover, 0, 0);
    lv_obj_set_style_border_color(ui_data.album_cover, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(ui_data.album_cover, LV_OPA_0, 0);
    lv_obj_align(ui_data.album_cover, LV_ALIGN_LEFT_MID, 10, 5); // 调整位置作为布局基准

    // 使用Lottie动画作为封面
    // 创建Lottie动画对象
    ui_data.album_lottie = lv_lottie_create(ui_data.album_cover);

    // 禁用Lottie对象的滚动条和滚动功能
    lv_obj_clear_flag(ui_data.album_lottie, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_data.album_lottie, LV_SCROLLBAR_MODE_OFF);

    // 禁用专辑封面容器的滚动条和滚动功能  
    lv_obj_clear_flag(ui_data.album_cover, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_data.album_cover, LV_SCROLLBAR_MODE_OFF);

    // 设置动画文件路径
    lv_lottie_set_src_file(ui_data.album_lottie, "/Quark-N_lvgl_9.2/lvgl_ui/lotties/Music_init.json");

    // 设置缓冲区
    static uint8_t lottie_buf[85 * 85 * 4]; // 适配封面大小的缓冲区 
    lv_lottie_set_buffer(ui_data.album_lottie, 85, 85, lottie_buf);

    // 获取动画对象并设置循环
    lv_anim_t *anim = lv_lottie_get_anim(ui_data.album_lottie);
    if (anim) {
        lv_anim_set_repeat_count(anim, LV_ANIM_REPEAT_INFINITE); // 无限循环
    }

    // 设置大小并居中
    lv_obj_set_size(ui_data.album_lottie, 85, 85); // 稍小于容器以便看到边框
    lv_obj_center(ui_data.album_lottie);
    
    // 创建时长显示 - 与专辑封面水平对齐
    lv_obj_t *time_label = lv_label_create(ui_data.screen);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(time_label, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text_fmt(time_label, "0:00 | %d:%02d", current_track.duration / 60, current_track.duration % 60);
    // 将时长标签与专辑封面居中对齐，并在专辑封面下方显示
    lv_obj_align_to(time_label, ui_data.album_cover, LV_ALIGN_OUT_BOTTOM_MID, 0, 5); // 与专辑封面居中对齐

    
    // 创建歌词容器和滚动区域 - 适应新的歌词区域
    ui_data.lyrics_container = lv_obj_create(ui_data.screen);
    lv_obj_set_size(ui_data.lyrics_container, 130, 85); 
    lv_obj_set_style_bg_opa(ui_data.lyrics_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(ui_data.lyrics_container, 0, 0);
    lv_obj_set_style_pad_all(ui_data.lyrics_container, 0, 0);
    lv_obj_align(ui_data.lyrics_container, LV_ALIGN_RIGHT_MID, -15, 5);
    lv_obj_set_scroll_dir(ui_data.lyrics_container, LV_DIR_VER);


    
    // 创建歌词文本
    ui_data.lyrics_text = lv_label_create(ui_data.lyrics_container);
    lv_obj_set_style_text_font(ui_data.lyrics_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_data.lyrics_text, COLOR_MUSIC_TEXT, 0);
    lv_obj_set_width(ui_data.lyrics_text, 130);
    lv_label_set_text(ui_data.lyrics_text, current_track.lyrics);
    lv_obj_align(ui_data.lyrics_text, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 启动歌词自动滚动
    start_lyrics_autoscroll();
    
    // 创建按钮处理定时器
    ui_data.button_timer = lv_timer_create(button_event_timer_cb, 50, NULL);
    
    // 设置为活动状态
    ui_data.is_active = true;
    
    // 使用过渡动画加载屏幕
    lv_scr_load_anim(ui_data.screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_TIME_DEFAULT, 0, false);
}

// 按钮事件处理回调
static void button_event_timer_cb(lv_timer_t *timer) {
    if (!ui_data.is_active) return;
    
    button_event_t event = key355_get_event();
    if (event == BUTTON_EVENT_NONE) return;
    
    // 清除事件标志
    key355_clear_event();
    
    // 处理双击返回主菜单
    if (event == BUTTON_EVENT_DOUBLE_CLICK) {
        return_to_menu();
    }
}

// 歌词文本自动垂直滚动定时器回调
static void scroll_lyrics_timer_cb(lv_timer_t *timer) {
    if (!ui_data.is_active || !ui_data.lyrics_container || !ui_data.lyrics_text) return;
    
    static int scroll_pos = 0;
    static int direction = 1; // 1向下滚动，-1向上滚动
    
    // 获取文本和容器高度
    lv_coord_t text_height = lv_obj_get_height(ui_data.lyrics_text);
    lv_coord_t container_height = lv_obj_get_height(ui_data.lyrics_container);
    
    // 只有文本高度大于容器时才需要滚动
    if (text_height <= container_height) return;
    
    // 设置滚动位置
    scroll_pos += (direction * 1); // 每次滚动1个像素
    
    // 检查是否到达边界
    if (scroll_pos <= 0) {
        scroll_pos = 0;
        direction = 1; // 改为向下滚动
        lv_timer_pause(timer);
        lv_timer_set_period(timer, 1000); // 在顶部停留1秒
        lv_timer_reset(timer);
        lv_timer_resume(timer);
    }
    else if (scroll_pos >= (text_height - container_height)) {
        scroll_pos = text_height - container_height;
        direction = -1; // 改为向上滚动
        lv_timer_pause(timer);
        lv_timer_set_period(timer, 1000); // 在底部停留1秒
        lv_timer_reset(timer);
        lv_timer_resume(timer);
    }
    else {
        // 正常滚动时使用标准间隔
        lv_timer_set_period(timer, 50);
    }
    
    // 应用垂直滚动
    lv_obj_set_y(ui_data.lyrics_text, -scroll_pos);
}

// 自动滚动歌词文本的函数
static void start_lyrics_autoscroll(void) {
    // 停止现有的滚动定时器
    if (ui_data.scroll_timer) {
        lv_timer_del(ui_data.scroll_timer);
        ui_data.scroll_timer = NULL;
    }
    
    // 获取歌词文本的高度
    lv_coord_t text_height = lv_obj_get_height(ui_data.lyrics_text);
    lv_coord_t container_height = lv_obj_get_height(ui_data.lyrics_container);
    
    // 只有当文本高度超过容器高度时才需要滚动
    if (text_height > container_height) {
        // 创建定时器来执行垂直滚动
        ui_data.scroll_timer = lv_timer_create(scroll_lyrics_timer_cb, 50, NULL);
    }
}

// 从音乐界面返回菜单界面
static void return_to_menu(void) {
    // 停止按钮定时器
    if (ui_data.button_timer) {
        lv_timer_del(ui_data.button_timer);
        ui_data.button_timer = NULL;
    }
    
    // 停止滚动定时器
    if (ui_data.scroll_timer) {
        lv_timer_del(ui_data.scroll_timer);
        ui_data.scroll_timer = NULL;
    }
    
    // 设置为非活动状态
    ui_data.is_active = false;
    
    // 清除实例引用
    ui_data.screen = NULL;
    
    // 创建菜单屏幕
    menu_ui_create_screen();
    
    // 使用动画切换屏幕
    lv_scr_load_anim(lv_scr_act(), LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, true);
    
    // 激活菜单
    menu_ui_set_active();
}

// 设置音乐播放器为活动状态
void music_ui_set_active(void) {
    ui_data.is_active = true;
    
    // 恢复按钮定时器
    if (!ui_data.button_timer) {
        ui_data.button_timer = lv_timer_create(button_event_timer_cb, 50, NULL);
    }
    
    // 恢复歌词滚动
    if (ui_data.lyrics_text && ui_data.lyrics_container && !ui_data.scroll_timer) {
        start_lyrics_autoscroll();
    }
}

// 释放资源
void music_ui_deinit(void) {
    if (ui_data.button_timer) {
        lv_timer_del(ui_data.button_timer);
        ui_data.button_timer = NULL;
    }
    
    // 停止滚动定时器
    if (ui_data.scroll_timer) {
        lv_timer_del(ui_data.scroll_timer);
        ui_data.scroll_timer = NULL;
    }
    
    ui_data.is_active = false;
    
    // 不需要单独删除Lottie对象，因为删除screen时会自动删除其所有子对象
    if (ui_data.screen) {
        lv_obj_del(ui_data.screen);
        ui_data.screen = NULL;
        ui_data.album_lottie = NULL; // 确保指针置空
        ui_data.album_cover = NULL;
    }
}

// 简单占位函数 - 为了保持API兼容性
void music_ui_toggle_play_pause(void) {
    // 这个版本不实现播放/暂停功能
}
