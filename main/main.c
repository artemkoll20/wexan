/**
 * main.c
 * WEXAn application entry point.
 */

#include "app/runtime.h"
#include "net/wifi.h"
#include "net/bluetooth.h"
#include "esp_log.h"
#include "ui/monitor.h"

static const char *TAG = "app_main";

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Starting ESP32 WEXAn...");

    /* Initialize all application services through unified entry point */
    err = app_services_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Application services initialization failed: %s", esp_err_to_name(err));
        return;
    }

    /* Show boot animation after display is initialized */
    monitor_show_boot_animation(3000);

    /* Register service handlers with app_core abstraction layer */
    wifi_service_register_handlers();
#if CONFIG_BT_ENABLED && CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLUEDROID_ENABLED
    bluetooth_service_register_handlers();
#endif

    /* Start app runtime - menu navigation and mode-specific UI behavior */
    app_runtime_start();
    ESP_LOGI(TAG, "Application started successfully");
}
