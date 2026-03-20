/*
 * runtime_events.c
 * ESP event adapters that map input, Wi-Fi and BLE events into runtime commands.
 * Enhanced with delayed screen transitions for confirm button.
 */
#include "runtime_state.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "net/bluetooth.h"
#include "net/wifi.h"
#include "ui/buttons.h"

/**
 * @brief Delay before returning to menu from scan screen (in milliseconds).
 * 
 * This delay prevents accidental menu returns and provides time for
 * the user to confirm their action. It also helps debounce the transition.
 */
#define SCREEN_TRANSITION_DELAY_MS 300

/**
 * @brief Timer handle for delayed menu return from scan screens.
 * 
 * Used when confirm button is pressed in scan mode to provide
 * a deliberate delay before returning to the menu.
 */
static esp_timer_handle_t s_confirm_back_timer = NULL;

/**
 * @brief Flag indicating if a transition is currently in progress.
 * 
 * Prevents multiple simultaneous transitions and provides
 * visual feedback to the user.
 */
static bool s_transition_in_progress = false;

/**
 * @brief Timer callback for delayed menu return.
 * 
 * This callback is called after SCREEN_TRANSITION_DELAY_MS when
 * the user presses confirm in a scan screen. It ensures the
 * transition is intentional and provides debouncing.
 */
static void on_confirm_back_timer_callback(void *arg)
{
    (void)arg;
    
    s_transition_in_progress = false;
    
    // Post the menu return command
    app_runtime_post_cmd(APP_RUNTIME_CMD_LEFT_HOLD_3S);
    
    ESP_LOGI("app_runtime", "Delayed menu return completed");
}

/**
 * @brief Start delayed transition to menu from scan screen.
 * 
 * Creates a timer that will trigger the menu return after a delay.
 * This prevents accidental transitions and provides debouncing.
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t app_start_delayed_menu_return(void)
{
    esp_err_t err;
    
    if (s_transition_in_progress) {
        ESP_LOGW("app_runtime", "Transition already in progress, ignoring");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Mark transition as in progress
    s_transition_in_progress = true;
    
    // Create timer if not already created
    if (s_confirm_back_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = on_confirm_back_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "confirm_back_timer",
        };
        
        err = esp_timer_create(&timer_args, &s_confirm_back_timer);
        if (err != ESP_OK) {
            s_transition_in_progress = false;
            ESP_LOGE("app_runtime", "Failed to create confirm back timer: %s", esp_err_to_name(err));
            return err;
        }
    }
    
    // Start the timer
    err = esp_timer_start_once(s_confirm_back_timer, SCREEN_TRANSITION_DELAY_MS * 1000ULL);
    if (err != ESP_OK) {
        s_transition_in_progress = false;
        ESP_LOGE("app_runtime", "Failed to start confirm back timer: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI("app_runtime", "Starting delayed menu return (%d ms)", SCREEN_TRANSITION_DELAY_MS);
    return ESP_OK;
}

/**
 * @brief Cancel any pending delayed transition.
 * 
 * Called when the user performs another action that should
 * cancel the pending transition.
 */
static void app_cancel_delayed_menu_return(void)
{
    if (s_transition_in_progress && s_confirm_back_timer != NULL) {
        esp_timer_stop(s_confirm_back_timer);
        s_transition_in_progress = false;
        ESP_LOGD("app_runtime", "Delayed menu return cancelled");
    }
}

static void on_ui_button_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != UI_BUTTON_EVENT) {
        return;
    }

    // Cancel any pending delayed transition on new button press
    // This prevents race conditions
    app_cancel_delayed_menu_return();

    switch (event_id) {
    case UI_BUTTON_EVENT_LEFT_CLICK:
        app_runtime_post_cmd(APP_RUNTIME_CMD_LEFT_CLICK);
        break;
    case UI_BUTTON_EVENT_RIGHT_CLICK:
        app_runtime_post_cmd(APP_RUNTIME_CMD_RIGHT_CLICK);
        break;
    case UI_BUTTON_EVENT_LEFT_HOLD_3S:
        app_runtime_post_cmd(APP_RUNTIME_CMD_LEFT_HOLD_3S);
        break;
    case UI_BUTTON_EVENT_LEFT_HOLD_4S:
        app_runtime_post_cmd(APP_RUNTIME_CMD_LEFT_HOLD_4S);
        break;
    case UI_BUTTON_EVENT_LEFT_HOLD_5S:
        app_runtime_post_cmd(APP_RUNTIME_CMD_LEFT_HOLD_5S);
        break;
    case UI_BUTTON_EVENT_CONFIRM:
        if (s_ui_state.screen == APP_SCREEN_MENU) {
            // In menu: confirm selects current item immediately
            app_runtime_post_cmd(APP_RUNTIME_CMD_MENU_CONFIRM);
        } else if (s_ui_state.screen == APP_SCREEN_PAGE) {
            // In scan screen: confirm starts delayed return to menu
            // This prevents accidental returns and provides debouncing
            esp_err_t err = app_start_delayed_menu_return();
            if (err != ESP_OK) {
                // Fallback to immediate return if timer fails
                app_runtime_post_cmd(APP_RUNTIME_CMD_LEFT_HOLD_3S);
            }
        }
        break;
    default:
        break;
    }
}

static void on_wifi_scan_updated_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_SCAN_EVENT && event_id == WIFI_SCAN_EVENT_UPDATED) {
        app_runtime_post_cmd(APP_RUNTIME_CMD_WIFI_UPDATED);
    }
}

static void on_bluetooth_scan_updated_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == BLUETOOTH_SCAN_EVENT && event_id == BLUETOOTH_SCAN_EVENT_UPDATED) {
        app_runtime_post_cmd(APP_RUNTIME_CMD_BLUETOOTH_UPDATED);
    }
}

void app_runtime_register_event_handlers(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(
        UI_BUTTON_EVENT,
        ESP_EVENT_ANY_ID,
        on_ui_button_event,
        NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_SCAN_EVENT,
        WIFI_SCAN_EVENT_UPDATED,
        on_wifi_scan_updated_event,
        NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        BLUETOOTH_SCAN_EVENT,
        BLUETOOTH_SCAN_EVENT_UPDATED,
        on_bluetooth_scan_updated_event,
        NULL));
}
