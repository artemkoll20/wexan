#ifndef APP_CORE_RUNTIME_STATE_H
#define APP_CORE_RUNTIME_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app/runtime.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "net/bluetooth.h"
#include "net/wifi.h"

#define WIFI_SCAN_INTERVAL_MS 9000
#define BLUETOOTH_SCAN_INTERVAL_MS 9000
#define PAGE_TICK_MS APP_RUNTIME_PAGE_TICK_MS
#define MENU_ENTRY_COUNTDOWN_S 0
#define MENU_TICK_MS 250
#define MENU_COUNTDOWN_ANIMATION_STEPS 4
#define WIFI_PAGE_TEXT_SIZE 192
#define APP_RUNTIME_TASK_STACK_SIZE 6144
#define APP_RUNTIME_TASK_PRIORITY 5
#define APP_RUNTIME_QUEUE_LENGTH 16

typedef enum {
    APP_SCREEN_MENU = 0,
    APP_SCREEN_PAGE,
} app_screen_t;

typedef enum {
    APP_RUNTIME_CMD_BOOT = 1,
    APP_RUNTIME_CMD_MENU_TICK,
    APP_RUNTIME_CMD_PAGE_TICK,
    APP_RUNTIME_CMD_LEFT_CLICK,
    APP_RUNTIME_CMD_RIGHT_CLICK,
    APP_RUNTIME_CMD_WIFI_UPDATED,
    APP_RUNTIME_CMD_BLUETOOTH_UPDATED,
    APP_RUNTIME_CMD_MENU_CONFIRM,
    APP_RUNTIME_CMD_MENU_BACK,
} app_runtime_cmd_t;

typedef struct {
    wifi_network_list_t current_networks;
    wifi_network_list_t polled_networks;
    wifi_network_list_t pending_networks;
    bluetooth_device_list_t current_bluetooth_devices;
    bluetooth_device_list_t polled_bluetooth_devices;
    bluetooth_device_list_t pending_bluetooth_devices;
    char page_text[WIFI_PAGE_TEXT_SIZE];
    app_screen_t screen;
    size_t selected_page_index;
    size_t active_page_index;
    uint32_t shown_wifi_version;
    uint32_t shown_bluetooth_version;
    size_t shown_wifi_page;
    size_t shown_bluetooth_page;
    uint8_t menu_countdown_s;
    uint8_t menu_countdown_phase;
    bool has_pending_networks;
    bool has_pending_bluetooth_devices;
    esp_timer_handle_t menu_timer;
    esp_timer_handle_t page_timer;
    bool menu_timer_running;
    bool page_timer_running;
} app_ui_state_t;

/**
 * @brief Global UI state structure for application runtime.
 *
 * Contains all UI-related state including network lists, bluetooth devices,
 * current screen, page indices, timers, and countdown state.
 *
 * @note This is a module-local static variable managed by app_core component.
 *       For embedded systems, this global state pattern is acceptable.
 */
extern app_ui_state_t s_ui_state;

/**
 * @brief FreeRTOS queue for receiving UI runtime commands.
 *
 * Commands like menu ticks, page transitions, button clicks are posted here
 * and processed by the runtime worker task.
 */
extern QueueHandle_t s_runtime_cmd_queue;

/**
 * @brief Handle to the main runtime worker task.
 *
 * Used for task notifications and management.
 */
extern TaskHandle_t s_runtime_task;

void app_runtime_post_cmd(app_runtime_cmd_t cmd);
void app_runtime_worker_init(void);
void app_runtime_register_event_handlers(void);
void app_runtime_process_cmd(app_runtime_cmd_t cmd);

void app_enter_menu_screen(void);
void app_enter_selected_mode(void);
void app_menu_select_prev(void);
void app_menu_select_next(void);
void app_menu_reset_countdown(void);
void app_handle_menu_tick(void);
void app_handle_menu_confirm(void);
void app_handle_active_page_tick(void);
void app_handle_active_page_left_click(void);
void app_handle_active_page_right_click(void);
void app_handle_active_page_wifi_updated(void);
void app_handle_active_page_bluetooth_updated(void);

#endif /* APP_CORE_RUNTIME_STATE_H */
