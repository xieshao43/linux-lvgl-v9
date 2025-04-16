#ifndef UI_ROUNDED_PROGRESS_H
#define UI_ROUNDED_PROGRESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../common.h"

/**
 * Create an Apple-style rounded progress bar
 * @param parent Parent object
 * @param width Width
 * @param height Height
 * @param color Progress color
 * @param update_time Update interval (milliseconds)
 * @return Created canvas object
 */
lv_obj_t* ui_rounded_progress_create(lv_obj_t *parent, int width, int height, 
                                    lv_color_t color, uint16_t update_time);

/**
 * Set progress bar's target value with animation
 * @param canvas Canvas object
 * @param target_value Target progress value (0-100)
 * @param step_size Animation step size (default 1)
 */
void ui_rounded_progress_set_value(lv_obj_t* canvas, int target_value, int step_size);

/**
 * Set progress value directly (no animation)
 * @param canvas Canvas object
 * @param value Progress value (0-100)
 */
void ui_rounded_progress_set_value_direct(lv_obj_t* canvas, int value);

/**
 * Set indeterminate mode (continuous animation)
 * @param canvas Canvas object
 * @param enable Whether to enable indeterminate mode
 */
void ui_rounded_progress_set_indeterminate(lv_obj_t* canvas, bool enable);

/**
 * Create example screen with progress bar
 */
void example_screen_create(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UI_ROUNDED_PROGRESS_H */