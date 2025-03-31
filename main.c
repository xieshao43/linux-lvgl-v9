#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lvgl_ui/lv_examples.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>

int main(void)
{
    lv_init();

    /*Linux frame buffer device init*/
    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    /*Create a Demo*/
    //ui_example_init();
    //ui_example_update_data(1024 * 1024, 512 * 1024, 50);

    // lv_demo_stress();
    lv_demo_music();
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
