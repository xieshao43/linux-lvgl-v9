#include "ui_rounded.h"
/*
* Apple-style rounded progress bar component
*/

// Progress bar configuration structure
typedef struct {
    int current_value;     // Current progress (0-100)
    int target_value;      // Target progress (0-100)
    int step_size;         // Animation step size
    uint32_t main_color;   // Main color as 32-bit value
    uint32_t track_color;  // Track color as 32-bit value
    uint32_t glow_color;   // Glow effect color
    bool indeterminate;    // Whether in indeterminate mode
    int indeterminate_pos; // Position for indeterminate animation
    lv_style_t shadow_style; // Shadow style
} ui_rounded_progress_t;

/**
 * Update the rounded progress bar
 * @param canvas Canvas object
 * @param config Progress configuration
 */
static void _ui_rounded_progress_update(lv_obj_t* canvas, ui_rounded_progress_t* config) 
{
    // Track parameters
    int track_width = lv_obj_get_height(canvas) / 7;  // Track width proportional to canvas height
    int arc_radius = track_width * 2;                // Arc radius proportional to track width
    int canvas_width = lv_obj_get_width(canvas);     // Canvas width
    int canvas_height = lv_obj_get_height(canvas);   // Canvas height

    // Get canvas buffer and descriptor
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    
    // Clear canvas
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
    
    // Create drawing context
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // Arc positions
    int left = arc_radius;
    int right = canvas_width - arc_radius;
    int top = arc_radius;
    int bottom = canvas_height - arc_radius;
    
    // Draw background track first (Apple UI typically has subtle background tracks)
    lv_draw_rect_dsc_t track_rect_dsc;
    lv_draw_rect_dsc_init(&track_rect_dsc);
    track_rect_dsc.bg_color = lv_color_hex(config->track_color);
    track_rect_dsc.bg_opa = LV_OPA_40;  // Subtle opacity for background track
    track_rect_dsc.border_width = 0;
    track_rect_dsc.radius = track_width/2;  // Slightly rounded edges

    // Draw background track as a rounded rectangle
    lv_area_t track_area = {
        track_width/2, 
        track_width/2, 
        canvas_width - track_width/2, 
        canvas_height - track_width/2
    };
    lv_draw_rect(&layer, &track_rect_dsc, &track_area);

    // If we're in indeterminate mode, draw differently
    if (config->indeterminate) {
        // Draw an animated segment (Apple-style indeterminate progress)
        int segment_length = (canvas_width + canvas_height) / 3; // Segment length
        int total_path = 2 * (right - left) + 2 * (bottom - top) + 4 * arc_radius * 3.14159f / 2;
        int start_pos = config->indeterminate_pos % total_path;
        
        // Draw segment style
        lv_draw_rect_dsc_t seg_rect_dsc;
        lv_draw_rect_dsc_init(&seg_rect_dsc);
        seg_rect_dsc.bg_color = lv_color_hex(config->main_color);
        seg_rect_dsc.radius = track_width/2;
        
        // Draw arc style
        lv_draw_arc_dsc_t seg_arc_dsc;
        lv_draw_arc_dsc_init(&seg_arc_dsc);
        seg_arc_dsc.width = track_width;
        seg_arc_dsc.color = lv_color_hex(config->main_color);
        seg_arc_dsc.rounded = 1;
        
        // Calculate position and draw segment
        // [Simplified - would need complete path calculation based on start_pos]
        
        // Add subtle glow effect (Apple UI often has subtle glows)
        // [Glow effect implementation would go here]
    }
    else {
        // Calculate progress position
        int progress = config->current_value;
        
        // Total path length calculation
        float arc_length = arc_radius * 3.14159f / 2; // Quarter circle
        int total_length = (right - left) * 2 + (bottom - top) * 2 + 4 * arc_length;
        int draw_length = (progress * total_length) / 100; // Length to draw based on progress
        
        // Filled progress style
        lv_draw_rect_dsc_t fill_rect_dsc;
        lv_draw_rect_dsc_init(&fill_rect_dsc);
        fill_rect_dsc.bg_color = lv_color_hex(config->main_color);
        fill_rect_dsc.border_width = 0;
        fill_rect_dsc.radius = 0;
        
        // Arc style
        lv_draw_arc_dsc_t fill_arc_dsc;
        lv_draw_arc_dsc_init(&fill_arc_dsc);
        fill_arc_dsc.width = track_width;
        fill_arc_dsc.color = lv_color_hex(config->main_color);
        fill_arc_dsc.rounded = 1;
        
        // 1. Top horizontal segment
        if (draw_length > 0) {
            int seg_length = right - left;
            int to_draw = LV_MIN(draw_length, seg_length);
            lv_area_t rect_area = {left, top - track_width/2, left + to_draw, top + track_width/2};
            lv_draw_rect(&layer, &fill_rect_dsc, &rect_area);
            draw_length -= to_draw;
        }
        
        // 2. Right upper arc
        if (draw_length > 0) {
            int to_draw = LV_MIN(draw_length, (int)arc_length);
            int angle_end = 270 + (to_draw * 90 / arc_length);
            
            fill_arc_dsc.center.x = right;
            fill_arc_dsc.center.y = top;
            fill_arc_dsc.radius = arc_radius;
            fill_arc_dsc.start_angle = 270;
            fill_arc_dsc.end_angle = angle_end;
            
            lv_draw_arc(&layer, &fill_arc_dsc);
            draw_length -= to_draw;
        }
        
        // 3. Right vertical segment
        if (draw_length > 0) {
            int seg_length = bottom - top;
            int to_draw = LV_MIN(draw_length, seg_length);
            lv_area_t rect_area = {right - track_width/2, top, right + track_width/2, top + to_draw};
            lv_draw_rect(&layer, &fill_rect_dsc, &rect_area);
            draw_length -= to_draw;
        }
        
        // 4-8. Continue drawing remaining segments
        // [Similar pattern for remaining segments]
        
        // Add subtle glow effect at the progress end
        if (progress > 0 && progress < 100) {
            // Calculate end position and add a subtle glow
            // [Would implement a gradient or glow at progress end]
        }
    }
    
    // Finish drawing
    lv_canvas_finish_layer(canvas, &layer);
}

/**
 * Progress bar animation timer callback
 */
static void _ui_rounded_progress_timer_cb(lv_timer_t* timer)
{
    lv_obj_t* canvas = (lv_obj_t*)timer->user_data;
    ui_rounded_progress_t* config = (ui_rounded_progress_t*)lv_obj_get_user_data(canvas);
    
    if (!config) return;
    
    bool need_update = false;
    
    if (config->indeterminate) {
        // Update indeterminate animation
        config->indeterminate_pos += 5;
        need_update = true;
    }
    else if (config->current_value != config->target_value) {
        // Apple animations typically have ease-in-out effect
        // Calculate remaining distance
        int distance = config->target_value - config->current_value;
        int step;
        
        // Apply easing (slower at start and end, faster in middle)
        if (abs(distance) < 10) {
            // Near target, slow down (ease-out)
            step = (distance > 0) ? 1 : -1;
        } 
        else {
            // Normal speed in middle
            step = distance > 0 ? 
                  LV_MIN(config->step_size, distance / 4 + 1) : 
                  LV_MAX(-config->step_size, distance / 4 - 1);
        }
        
        config->current_value += step;
        
        // Ensure we don't overshoot
        if ((step > 0 && config->current_value > config->target_value) ||
            (step < 0 && config->current_value < config->target_value)) {
            config->current_value = config->target_value;
        }
        
        need_update = true;
    }
    
    if (need_update) {
        _ui_rounded_progress_update(canvas, config);
    }
}

// Cleanup callback function
static void _ui_rounded_cleanup_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    ui_rounded_progress_t* config = (ui_rounded_progress_t*)lv_obj_get_user_data(obj);
    const void* buf = lv_canvas_get_buf(obj);
    
    if (config) {
        lv_style_reset(&config->shadow_style);
        lv_free(config);
    }
    if (buf) lv_free((void*)buf);
}

/**
 * Create an Apple-style rounded progress bar
 */
lv_obj_t* ui_rounded_progress_create(lv_obj_t *parent, int width, int height, 
                                    lv_color_t color, uint16_t update_time)
{
    // 1. Create canvas
    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_obj_set_size(canvas, width, height);
    lv_obj_center(canvas);
    
    // 2. Allocate buffer
    uint32_t buf_size = width * height * sizeof(lv_color_t);
    void* cbuf = lv_malloc(buf_size);
    if (cbuf == NULL) {
        LV_LOG_ERROR("Memory allocation failed");
        lv_obj_delete(canvas);
        return NULL;
    }
    
    lv_canvas_set_buffer(canvas, cbuf, width, height, LV_COLOR_FORMAT_NATIVE);
    
    // 3. Initialize config data
    ui_rounded_progress_t* config = (ui_rounded_progress_t*)lv_malloc(sizeof(ui_rounded_progress_t));
    if (config == NULL) {
        LV_LOG_ERROR("Memory allocation failed");
        lv_free(cbuf);
        lv_obj_delete(canvas);
        return NULL;
    }
    
    // Apple UI typically has subtle colors
    config->current_value = 0;
    config->target_value = 0;
    config->step_size = 2;  // Slightly faster animations for responsiveness
    config->main_color = lv_color_to_u32(color);
    
    // Apple UI typically has light gray tracks
    config->track_color = lv_color_to_u32(lv_palette_lighten(LV_PALETTE_GREY, 3));
    
    // Apple UI often has subtle glow effects
    lv_color_t glow_color = color;
    config->glow_color = lv_color_to_u32(glow_color);
    
    config->indeterminate = false;
    config->indeterminate_pos = 0;
    
    // Initialize shadow style
    lv_style_init(&config->shadow_style);
    lv_style_set_shadow_width(&config->shadow_style, 5);
    lv_style_set_shadow_color(&config->shadow_style, lv_color_darken(color, LV_OPA_30));
    lv_style_set_shadow_opa(&config->shadow_style, LV_OPA_40);
    
    lv_obj_add_style(canvas, &config->shadow_style, 0);
    lv_obj_set_user_data(canvas, config);
    
    // 4. Initialize progress bar
    _ui_rounded_progress_update(canvas, config);
    
    // 5. Create timer - Apple UI is typically smooth with frequent updates
    lv_timer_create(_ui_rounded_progress_timer_cb, update_time, canvas);
    
    // 6. Set cleanup callback
    lv_obj_add_event_cb(canvas, _ui_rounded_cleanup_cb, LV_EVENT_DELETE, NULL);
    
    return canvas;
}

/**
 * Set the progress bar's target value with animation
 */
void ui_rounded_progress_set_value(lv_obj_t* canvas, int target_value, int step_size)
{
    if (!lv_obj_check_type(canvas, &lv_canvas_class)) return;
    
    ui_rounded_progress_t* config = (ui_rounded_progress_t*)lv_obj_get_user_data(canvas);
    if (!config) return;
    
    // Set indeterminate mode off
    config->indeterminate = false;
    
    // Limit range
    target_value = LV_CLAMP(0, target_value, 100);
    step_size = LV_MAX(1, step_size);
    
    // Set target value and step size
    config->target_value = target_value;
    config->step_size = step_size;
    
    // If target is reached, add a subtle bounce effect (Apple style)
    if (config->current_value == target_value && target_value == 100) {
        // Could implement a small bounce animation here
    }
}

/**
 * Set progress value directly (no animation)
 */
void ui_rounded_progress_set_value_direct(lv_obj_t* canvas, int value)
{
    if (!lv_obj_check_type(canvas, &lv_canvas_class)) return;
    
    ui_rounded_progress_t* config = (ui_rounded_progress_t*)lv_obj_get_user_data(canvas);
    if (!config) return;
    
    // Set indeterminate mode off
    config->indeterminate = false;
    
    // Limit range
    value = LV_CLAMP(0, value, 100);
    
    // Set current and target values the same
    config->current_value = value;
    config->target_value = value;
    
    // Update display
    _ui_rounded_progress_update(canvas, config);
}

/**
 * Set indeterminate mode (Apple-style loading indicator)
 */
void ui_rounded_progress_set_indeterminate(lv_obj_t* canvas, bool enable)
{
    if (!lv_obj_check_type(canvas, &lv_canvas_class)) return;
    
    ui_rounded_progress_t* config = (ui_rounded_progress_t*)lv_obj_get_user_data(canvas);
    if (!config) return;
    
    config->indeterminate = enable;
    config->indeterminate_pos = 0;
    
    // Update display
    _ui_rounded_progress_update(canvas, config);
}

/**
 * Create example screen with Apple-style progress bar
 */
void example_screen_create(void)
{
    lv_obj_t* scr = lv_scr_act();
    
    // Set dark background for better contrast (Apple-style)
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), 0);
    
    // Create a progress bar with Apple blue accent color
    lv_obj_t* progress = ui_rounded_progress_create(
        scr,                       // Parent
        240,                       // Width
        135,                       // Height
        lv_color_hex(0x007AFF),    // Apple blue color
        16                         // Update frequency (60fps)
    );
    
    // Position slightly above center (Apple design often positions elements this way)
    lv_obj_align(progress, LV_ALIGN_CENTER, 0, -20);
    
    // Add label below progress bar
    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text(label, "Loading...");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 80);
    
    // Set progress animation
    ui_rounded_progress_set_value(progress, 75, 2);
}
