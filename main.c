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
     //lv_demo_music();


    /*Handle LVGL tasks*/
    while(1) {
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
