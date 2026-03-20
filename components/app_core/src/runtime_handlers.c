/*
 * runtime_handlers.c
 * Runtime command dispatcher with enhanced screen transition handling.
 */
#include "runtime_state.h"
#include "esp_log.h"

static const char *TAG = "app_runtime";

/**
 * @brief Flag to prevent concurrent screen transitions.
 * 
 * This prevents issues with button bounce or rapid presses
 * causing multiple transitions at once.
 */
static bool s_transition_busy = false;

/**
 * @brief Check if we can process a screen transition command.
 * 
 * @return true if transition is allowed, false if already transitioning
 */
static bool app_can_transition(void)
{
    if (s_transition_busy) {
        ESP_LOGW(TAG, "Transition blocked: already transitioning");
        return false;
    }
    return true;
}

/**
 * @brief Mark transition as started.
 * 
 * Called before starting a screen transition to prevent
 * concurrent transitions.
 */
static void app_transition_start(void)
{
    s_transition_busy = true;
    ESP_LOGD(TAG, "Transition started");
}

/**
 * @brief Mark transition as completed.
 * 
 * Called after screen transition is complete to allow
 * new transitions.
 */
static void app_transition_end(void)
{
    s_transition_busy = false;
    ESP_LOGD(TAG, "Transition completed");
}

/**
 * @brief Stop the currently active scan based on active page index
 * 
 * Determines scan type from active_page_index and stops the appropriate scanner.
 * Uses non-blocking stop - sets flag and lets scan tasks terminate themselves.
 */
static void app_stop_current_scan(void)
{
    if (s_ui_state.active_page_index == 0) {
        /* WiFi scan mode - stop WiFi scan (non-blocking) */
        ESP_LOGI(TAG, "Stopping WiFi scan");
        wifi_scan_stop();
    } else if (s_ui_state.active_page_index == 1) {
        /* Bluetooth scan mode - stop Bluetooth scan (non-blocking) */
        ESP_LOGI(TAG, "Stopping Bluetooth scan");
        bluetooth_scan_stop();
    }
}

void app_runtime_process_cmd(app_runtime_cmd_t cmd)
{
    switch (cmd) {
    case APP_RUNTIME_CMD_BOOT:
        s_ui_state.selected_page_index = 0;
        s_ui_state.active_page_index = 0;
        app_transition_start();
        app_enter_menu_screen();
        app_transition_end();
        break;
    case APP_RUNTIME_CMD_MENU_TICK:
        app_handle_menu_tick();
        break;
    case APP_RUNTIME_CMD_PAGE_TICK:
        app_handle_active_page_tick();
        break;
    case APP_RUNTIME_CMD_LEFT_CLICK:
        if (!app_can_transition()) {
            break;
        }
        if (s_ui_state.screen == APP_SCREEN_MENU) {
            app_transition_start();
            app_menu_select_prev();
            app_menu_reset_countdown();
            app_transition_end();
        } else if (s_ui_state.screen == APP_SCREEN_PAGE) {
            app_handle_active_page_left_click();
        }
        break;
    case APP_RUNTIME_CMD_RIGHT_CLICK:
        if (!app_can_transition()) {
            break;
        }
        if (s_ui_state.screen == APP_SCREEN_MENU) {
            app_transition_start();
            app_menu_select_next();
            app_menu_reset_countdown();
            app_transition_end();
        } else if (s_ui_state.screen == APP_SCREEN_PAGE) {
            app_handle_active_page_right_click();
        }
        break;
    case APP_RUNTIME_CMD_LEFT_HOLD_3S:
        if (!app_can_transition()) {
            break;
        }
        if (s_ui_state.screen == APP_SCREEN_PAGE) {
            app_transition_start();
            ESP_LOGI(TAG, "Returning to menu (LEFT_HOLD_3S)");
            app_enter_menu_screen();
            app_transition_end();
        }
        break;
    case APP_RUNTIME_CMD_LEFT_HOLD_4S:
        if (!app_can_transition()) {
            break;
        }
        if (s_ui_state.screen == APP_SCREEN_PAGE) {
            /* Stop current scan and return to menu */
            app_transition_start();
            ESP_LOGI(TAG, "Returning to menu (LEFT_HOLD_4S with scan stop)");
            app_stop_current_scan();
            app_enter_menu_screen();
            app_transition_end();
        }
        break;
    case APP_RUNTIME_CMD_LEFT_HOLD_5S:
        if (!app_can_transition()) {
            break;
        }
        if (s_ui_state.screen == APP_SCREEN_PAGE) {
            app_transition_start();
            ESP_LOGI(TAG, "Returning to menu (LEFT_HOLD_5S)");
            app_enter_menu_screen();
            app_transition_end();
        }
        break;
    case APP_RUNTIME_CMD_WIFI_UPDATED:
        app_handle_active_page_wifi_updated();
        break;
    case APP_RUNTIME_CMD_BLUETOOTH_UPDATED:
        app_handle_active_page_bluetooth_updated();
        break;
    case APP_RUNTIME_CMD_MENU_CONFIRM:
        if (!app_can_transition()) {
            break;
        }
        app_transition_start();
        app_handle_menu_confirm();
        app_transition_end();
        break;
    default:
        break;
    }
}
