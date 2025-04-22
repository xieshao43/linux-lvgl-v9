#ifndef WIFI_NOTIFICATION_H
#define WIFI_NOTIFICATION_H

#include "../lvgl.h"
#include "../core/ui_manager.h"

/**
 * WiFi connection states
 */
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
} wifi_state_t;

/**
 * Initialize the WiFi notification bubble
 */
void wifi_notification_init(void);

/**
 * Show the WiFi notification bubble with current state
 * @param state The WiFi connection state to display
 * @param ssid The SSID to display when connected (can be NULL for disconnected state)
 */
void wifi_notification_show(wifi_state_t state, const char *ssid);

/**
 * Hide the WiFi notification bubble
 */
void wifi_notification_hide(void);

/**
 * Check if the WiFi notification is currently visible
 * @return true if visible, false otherwise
 */
bool wifi_notification_is_visible(void);

/**
 * Update WiFi status for a specific interface
 * @param state The WiFi connection state
 * @param ssid The SSID (can be NULL)
 * @param interface The interface name (e.g. "wlan0", "wlan1")
 */
void wifi_notification_update_interface(wifi_state_t state, const char *ssid, const char *interface);

/**
 * Release WiFi notification resources
 * Hides the notification if visible and frees all allocated resources
 */
void wifi_notification_deinit(void);

#endif // WIFI_NOTIFICATION_H
