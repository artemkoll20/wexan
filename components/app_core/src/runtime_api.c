/*
 * runtime_api.c
 * Public entrypoint for runtime startup.
 */
#include "app/runtime.h"
#include "runtime_state.h"
#include "ui/buttons.h"
#include "ui/monitor.h"
#include "esp_log.h"

#define UI_BUTTON_LEFT_GPIO GPIO_NUM_3
#define UI_BUTTON_RIGHT_GPIO GPIO_NUM_2
#define UI_BUTTON_CONFIRM_GPIO GPIO_NUM_4
static const char *TAG = "app_runtime";

esp_err_t app_services_init(void)
{
    ESP_LOGI(TAG, "Initializing application services...");
    
    /* Monitor (display) initialization - must be first as other services may depend on it */
    monitor_init();
    
    ESP_LOGI(TAG, "Application services initialized successfully");
    return ESP_OK;
}

void app_runtime_start(void)
{
    ESP_LOGI(TAG, "Starting app runtime...");
    ui_buttons_init(UI_BUTTON_LEFT_GPIO, UI_BUTTON_RIGHT_GPIO, UI_BUTTON_CONFIRM_GPIO);
    app_runtime_worker_init();
    app_runtime_register_event_handlers();
    app_runtime_post_cmd(APP_RUNTIME_CMD_BOOT);
    ESP_LOGI(TAG, "App runtime started");
}
