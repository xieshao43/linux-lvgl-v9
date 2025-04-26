#include "key355.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "../core/data_manager.h"

#define GPIO_PATH "/sys/class/gpio"
#define GPIO_PIN 355
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_UNEXPORT_PATH "/sys/class/gpio/unexport"
#define GPIO_DIR_PATH "/sys/class/gpio/gpio355/direction"
#define GPIO_VALUE_PATH "/sys/class/gpio/gpio355/value"

#define DEBOUNCE_TIME 20       
#define LONG_PRESS_TIME 800   
#define DOUBLE_CLICK_TIME 400 

static volatile bool thread_running = false;
static volatile button_event_t current_event = BUTTON_EVENT_NONE;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool in_submenu = false;
static pthread_t button_thread;
static int gpio_value_fd = -1;

// 添加全局变量用于存储当前按钮状态
static volatile bool is_button_pressed = false;

static bool write_to_file(const char *filename, const char *content) {
    int fd = open(filename, O_WRONLY);
    if (fd < 0) {
        perror("open failed");
        return false;
    }

    ssize_t written = write(fd, content, strlen(content));
    close(fd);
    return written == strlen(content);
}

static int read_from_file(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", filename, strerror(errno));
        return -1;
    }

    char buf[2] = {0};
    ssize_t read_bytes = read(fd, buf, 1);
    close(fd);
    
    if (read_bytes != 1) {
        fprintf(stderr, "read %s failed: %s\n", filename, strerror(errno));
        return -1;
    }
    
    return buf[0] == '1' ? 1 : 0;
}

static void set_button_event(button_event_t event) {
    pthread_mutex_lock(&event_mutex);
    current_event = event;
    pthread_mutex_unlock(&event_mutex);
}

static uint32_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int read_gpio_value(void) {
    if (gpio_value_fd < 0) return -1;

    char buf[2] = {0};
    off_t pos = lseek(gpio_value_fd, 0, SEEK_SET);
    if (pos == -1) {
        fprintf(stderr, "lseek failed: %s\n", strerror(errno));
        return -1;
    }

    ssize_t read_bytes = read(gpio_value_fd, buf, 1);
    if (read_bytes != 1) {
        fprintf(stderr, "read gpio value failed: %s\n", strerror(errno));
        return -1;
    }

    return buf[0] == '1' ? 1 : 0;
}

static void* button_thread_func(void *arg) {
    int prev_state = -1;
    int current_state = -1;
    uint32_t press_time = 0;
    uint32_t release_time = 0;
    uint32_t last_click_time = 0;
    int click_count = 0;
    uint32_t poll_interval = 10000;

    while (thread_running) {
        current_state = read_gpio_value();
        if (current_state < 0) {
            usleep(10000);
            continue;
        }

        current_state = !current_state;
        
        // 更新当前按钮状态
        is_button_pressed = (current_state == 1);

        if (current_state != prev_state) {
            uint32_t current_time = get_time_ms();

            if (current_state == 1) {
                press_time = current_time;
            } else if (prev_state == 1) {
                release_time = current_time;
                uint32_t duration = release_time - press_time;

                if (duration >= LONG_PRESS_TIME) {
                    set_button_event(BUTTON_EVENT_LONG_PRESS);
                    click_count = 0;
                } else if (duration >= DEBOUNCE_TIME) {
                    if (click_count > 0 && (current_time - last_click_time) < DOUBLE_CLICK_TIME) {
                        set_button_event(BUTTON_EVENT_DOUBLE_CLICK);
                        click_count = 0;
                    } else {
                        click_count = 1;
                        last_click_time = current_time;
                        set_button_event(BUTTON_EVENT_CLICK);
                    }
                }
            }

            if ((current_time - last_click_time) > DOUBLE_CLICK_TIME && click_count == 1) {
                click_count = 0;
            }

            prev_state = current_state;
        }

        usleep(poll_interval);
    }

    return NULL;
}

bool key355_init(void) {
    if (access(GPIO_DIR_PATH, F_OK) != 0) {
        char export_buf[16];
        snprintf(export_buf, sizeof(export_buf), "%d", GPIO_PIN);
        if (!write_to_file(GPIO_EXPORT_PATH, export_buf)) {
            fprintf(stderr, "export gpio failed\n");
            return false;
        }
        usleep(200000); 
    }

    if (!write_to_file(GPIO_DIR_PATH, "in")) {
        fprintf(stderr, "set gpio direction failed\n");
        return false;
    }

    gpio_value_fd = open(GPIO_VALUE_PATH, O_RDONLY);
    if (gpio_value_fd < 0) {
        fprintf(stderr, "open gpio value failed: %s\n", strerror(errno));
        return false;
    }

    pthread_mutex_lock(&thread_mutex);
    thread_running = true;
    pthread_mutex_unlock(&thread_mutex);
    
    if (pthread_create(&button_thread, NULL, button_thread_func, NULL) != 0) {
        perror("pthread_create failed");
        key355_deinit();
        return false;
    }

    return true;
}

button_event_t key355_get_event(void) {
    button_event_t event;
    pthread_mutex_lock(&event_mutex);
    event = current_event;
    current_event = BUTTON_EVENT_NONE;
    pthread_mutex_unlock(&event_mutex);
    return event;
}

void key355_clear_event(void) {
    pthread_mutex_lock(&event_mutex);
    current_event = BUTTON_EVENT_NONE;
    pthread_mutex_unlock(&event_mutex);
}

void key355_deinit(void) {
    pthread_mutex_lock(&thread_mutex);
    thread_running = false;
    pthread_mutex_unlock(&thread_mutex);
    
    pthread_join(button_thread, NULL);
    
    if (gpio_value_fd >= 0) {
        close(gpio_value_fd);
        gpio_value_fd = -1;
    }

    char unexport_buf[16];
    snprintf(unexport_buf, sizeof(unexport_buf), "%d", GPIO_PIN);
    write_to_file(GPIO_UNEXPORT_PATH, unexport_buf);

    pthread_mutex_destroy(&event_mutex);
    pthread_mutex_destroy(&thread_mutex);
}

/**
 * 获取按钮当前物理状态
 * 无需互斥锁保护，读取单个布尔值是原子操作
 */
bool key355_is_pressed(void) {
    return is_button_pressed;
}
