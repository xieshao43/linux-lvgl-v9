#include "wifi_notification.h"

// Example WiFi manager that would handle the WiFi status
void wifi_status_changed_callback(wifi_state_t new_state, const char *ssid) {
    // Show notification with the new state
    wifi_notification_show(new_state, ssid);
}

// Example function to simulate WiFi connection state changes
void wifi_manager_simulate_state_change(void) {
    static wifi_state_t current_state = WIFI_STATE_DISCONNECTED;
    static const char *test_ssid = "MyHomeNetwork";
    
    switch (current_state) {
        case WIFI_STATE_DISCONNECTED:
            current_state = WIFI_STATE_CONNECTING;
            wifi_status_changed_callback(current_state, NULL);
            break;
            
        case WIFI_STATE_CONNECTING:
            current_state = WIFI_STATE_CONNECTED;
            wifi_status_changed_callback(current_state, test_ssid);
            break;
            
        case WIFI_STATE_CONNECTED:
            current_state = WIFI_STATE_DISCONNECTED;
            wifi_status_changed_callback(current_state, NULL);
            break;
    }
}

void wifi_manager_init(void) {
    // Initialize the WiFi notification system
    wifi_notification_init();
}
