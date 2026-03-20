/*
 * bluetooth_init.c
 * BLE stack bootstrap and synchronization primitive initialization.
 */
#include "bluetooth_init.h"
#include "bluetooth_cache.h"
#include "bluetooth_gap_handlers.h"
#include "bluetooth_internal.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"

#if CONFIG_BT_ENABLED && CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLUEDROID_ENABLED

#include "esp_bt.h"
#include "esp_bt_main.h"

static void bluetooth_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void bluetooth_event_loop_init(void)
{
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
}

static void bluetooth_create_sync_primitives(bluetooth_runtime_t *runtime)
{
    if (!runtime->cache_lock) {
        runtime->cache_lock = xSemaphoreCreateMutex();
        ESP_ERROR_CHECK(runtime->cache_lock ? ESP_OK : ESP_ERR_NO_MEM);
    }
    if (!runtime->scan_gate_lock) {
        runtime->scan_gate_lock = xSemaphoreCreateMutex();
        ESP_ERROR_CHECK(runtime->scan_gate_lock ? ESP_OK : ESP_ERR_NO_MEM);
    }
    if (!runtime->scan_param_sem) {
        runtime->scan_param_sem = xSemaphoreCreateBinary();
        ESP_ERROR_CHECK(runtime->scan_param_sem ? ESP_OK : ESP_ERR_NO_MEM);
    }
    if (!runtime->scan_done_sem) {
        runtime->scan_done_sem = xSemaphoreCreateBinary();
        ESP_ERROR_CHECK(runtime->scan_done_sem ? ESP_OK : ESP_ERR_NO_MEM);
    }
}

void bluetooth_stack_init(bluetooth_runtime_t *runtime)
{
    esp_err_t err;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if (!runtime || runtime->initialized) {
        return;
    }

    bluetooth_create_sync_primitives(runtime);
    bluetooth_nvs_init();
    bluetooth_event_loop_init();

    err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    err = esp_bluedroid_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    err = esp_bluedroid_enable();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    bluetooth_gap_handlers_bind_runtime(runtime);
    ESP_ERROR_CHECK(bluetooth_gap_handlers_register_callback());

    bluetooth_cache_reset(runtime, ESP_OK);
    runtime->scan_task_running = false;
    runtime->scan_task_handle = NULL;
    runtime->scan_active = false;
    runtime->scan_error = ESP_OK;
    if (runtime->scan_interval_ms < BLUETOOTH_MIN_SCAN_INTERVAL_MS) {
        runtime->scan_interval_ms = BLUETOOTH_DEFAULT_SCAN_INTERVAL_MS;
    }
    runtime->initialized = true;
}

#else

void bluetooth_stack_init(bluetooth_runtime_t *runtime)
{
    (void)runtime;
}

#endif
