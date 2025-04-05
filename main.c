#include "lvgl/lvgl.h"
//#include "lvgl/demos/lv_demos.h"
#include "lvgl_ui/lv_examples.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>

int main(void)
{
    lv_init();

    /*Linux frame buffer device init*/
    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb1"); //Quark's framebuffer is on fb1

    /*Create a Demo*/
    storage_monitor_init("/");

    // 在合适的位置(如主函数)添加
    printf("[DEBUG] 注册CPU UI模块\n");
    ui_manager_register_module(cpu_ui_get_module());
    printf("[DEBUG] 注册完成，设置初始模块\n");
    ui_manager_show_module(0); // 显示第一个模块

    // lv_demo_stress();
    // lv_demo_music();
    // lv_demo_benchmark();
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();


    /*Handle LVGL tasks*/
    while(1) {
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
