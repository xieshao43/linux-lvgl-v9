#include "lv_examples.h"


/**
 * 初始化并显示系统监控UI
 */
void storage_monitor_init(const char *path) {

    // 初始化数据管理器
    data_manager_init(path);

    // 初始化UI管理器
    ui_manager_init(lv_color_hex(COLOR_BG));
    
    // 设置过渡动画类型
    ui_manager_set_anim_type(ANIM_SLIDE_LEFT); 

    // 注册存储模块
    ui_manager_register_module(storage_ui_get_module());

    // 注册CPU模块
    ui_manager_register_module(cpu_ui_get_module());

    // 初始化WiFi管理器（内部会初始化通知系统）
    wifi_manager_init();
    
    // 显示第一个模块
    ui_manager_show_module(0);

}

/**
 * 关闭存储监控UI
 */
void storage_monitor_close(void) {
    // 清理UI管理器
    ui_manager_deinit();
}

/**
 * 为保持兼容性添加的函数
 */
void read_storage_data(void) {
    data_manager_update();
}

/**
 * 尝试禁用终端输出，在UI初始化完成后调用
 */
void disable_terminal_output(void) {
    static bool terminal_disabled = false;
    if (terminal_disabled) return;
    
    // 重定向标准输出和错误输出
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
        close(null_fd);
    }
    
    // 尝试多个可能的TTY设备
    const char* tty_devices[] = {
        "/dev/tty",
        "/dev/tty0",
        "/dev/tty1",
        "/dev/console",
        "/dev/pts/0"
    };
    
    for (int i = 0; i < sizeof(tty_devices)/sizeof(tty_devices[0]); i++) {
        int tty_fd = open(tty_devices[i], O_WRONLY | O_NONBLOCK);
        if (tty_fd >= 0) {
            // 清屏并隐藏光标
            write(tty_fd, "\033[2J\033[H\033[?25l", 13);
            close(tty_fd);
            terminal_disabled = true;
            break;
        }
    }
    
    // 如果上面的方法都失败，尝试使用system调用
    if (!terminal_disabled) {
        system("stty -echo");  // 禁用回显
        system("clear");       // 清屏
        terminal_disabled = true;
    }
}
