#ifndef AI_UI_H
#define AI_UI_H

#include "../common.h"
#include "../utils/ui_utils.h"
#include "../core/key355.h"


/**
 * 创建AI助手界面
 * 显示简约的AI助手UI，支持基本动画效果
 */
void AI_ui_create_screen(void);

/**
 * 设置AI助手为活动状态
 * 当从其他页面返回AI助手时调用
 */
void AI_ui_set_active(void);

/**
 * 模拟AI交互状态切换
 * 仅UI展示，不包含实际AI处理
 */
void AI_ui_toggle_interaction(void);



#endif // AI_UI_H
