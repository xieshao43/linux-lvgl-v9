#ifndef MUSIC_UI_H
#define MUSIC_UI_H

#include "../common.h"
#include "../utils/ui_utils.h"
#include "../core/key355.h"


/**
 * 创建音乐播放器界面
 * 显示简约的音乐播放器UI，支持基本动画效果
 */
void music_ui_create_screen(void);

/**
 * 设置音乐播放器为活动状态
 * 当从其他页面返回音乐播放器时调用
 */
void music_ui_set_active(void);

/**
 * 模拟播放/暂停状态切换
 * 仅UI展示，不包含实际音频处理
 */
void music_ui_toggle_play_pause(void);



#endif // MUSIC_UI_H
