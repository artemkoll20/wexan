/*
 * bluetooth_scan.c
 * Public BLE scanner API facade and periodic worker lifecycle.
 */
#include "bluetooth_internal.h"
#include <string.h>
#include "bluetooth_cache.h"
#include "bluetooth_init.h"
#include "bluetooth_scan_engine.h"
#include "bluetooth_scan_runtime.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "bluetooth_scan";

ESP_EVENT_DEFINE_BASE(BLUETOOTH_SCAN_EVENT);

#if CONFIG_BT_ENABLED && CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLUEDROID_ENABLED

static bluetooth_runtime_t s_runtime = {
    .scan_interval_ms = BLUETOOTH_DEFAULT_SCAN_INTERVAL_MS,
};

static void bluetooth_scan_task(void *arg)
{
    bluetooth_runtime_t *runtime = (bluetooth_runtime_t *)arg;
    bluetooth_scan_engine_t engine;

    if (!runtime) {
        vTaskDelete(NULL);
        return;
    }

    bluetooth_scan_engine_prepare(&engine, runtime);

    while (runtime->scan_task_running) {
        bluetooth_device_list_t snapshot;
        esp_err_t err = bluetooth_scan_engine_run_once(&engine, &snapshot);
        uint32_t remaining_ms;

        if (err == ESP_OK) {
            bluetooth_cache_update_success(runtime, &snapshot);
        } else {
            bluetooth_cache_update_error(runtime, err);
        }
        bluetooth_cache_post_updated_event();

        remaining_ms = runtime->scan_interval_ms;
        while (runtime->scan_task_running && remaining_ms > 0U) {
            uint32_t delay_chunk = remaining_ms > 100U ? 100U : remaining_ms;
            vTaskDelay(pdMS_TO_TICKS(delay_chunk));
            remaining_ms -= delay_chunk;
        }
    }

    vTaskDelete(NULL);
    runtime->scan_task_handle = NULL;
}

static void bluetooth_init(void)
{
    bluetooth_stack_init(&s_runtime);
}

void bluetooth_scan_start_periodic(uint32_t interval_ms)
{
    bluetooth_init();

    if (interval_ms < BLUETOOTH_MIN_SCAN_INTERVAL_MS) {
        interval_ms = BLUETOOTH_MIN_SCAN_INTERVAL_MS;
    }
    s_runtime.scan_interval_ms = interval_ms;

    if (s_runtime.scan_task_running) {
        ESP_LOGW(TAG, "BLE scan already running");
        return;
    }

    ESP_LOGI(TAG, "Starting periodic BLE scan, interval=%ums", interval_ms);
    s_runtime.scan_task_running = true;
    if (xTaskCreate(
            bluetooth_scan_task,
            "ble_scan",
            BLUETOOTH_SCAN_TASK_STACK_SIZE,
            &s_runtime,
            BLUETOOTH_SCAN_TASK_PRIORITY,
            &s_runtime.scan_task_handle) != pdPASS) {
        s_runtime.scan_task_running = false;
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}

void bluetooth_scan_stop(void)
{
    if (!s_runtime.scan_task_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping BLE scan");
    // Non-blocking stop: just set the flag and let the task terminate itself
    s_runtime.scan_task_running = false;
    if (s_runtime.scan_active) {
        bluetooth_scan_engine_stop_active(&s_runtime);
    }
    // Don't wait here - let the task exit on its own
    ESP_LOGI(TAG, "BLE scan stop initiated");
}

bool bluetooth_get_latest_devices(bluetooth_device_list_t *out)
{
    return bluetooth_cache_get_latest(&s_runtime, out);
}

/**
 * @brief Bluetooth handler implementations for app_event_handlers abstraction.
 */
static bool bluetooth_service_get_devices_impl(void *buffer, size_t buffer_size)
{
    if (buffer_size < sizeof(bluetooth_device_list_t)) {
        return false;
    }
    return bluetooth_get_latest_devices((bluetooth_device_list_t *)buffer);
}

static void bluetooth_service_start_scan_impl(uint32_t interval_ms)
{
    bluetooth_scan_start_periodic(interval_ms);
}

static void bluetooth_service_stop_scan_impl(void)
{
    bluetooth_scan_stop();
}

/**
 * @brief Static handlers structure for Bluetooth service.
 */
static const app_bluetooth_handlers_t s_bluetooth_handlers = {
    .get_devices = bluetooth_service_get_devices_impl,
    .start_scan = bluetooth_service_start_scan_impl,
    .stop_scan = bluetooth_service_stop_scan_impl,
    .scan_interval_ms = BLUETOOTH_MIN_SCAN_INTERVAL_MS,
};

void bluetooth_service_register_handlers(void)
{
    app_event_handlers_register_bluetooth(&s_bluetooth_handlers);
}

#else

static bool s_stub_initialized = false;
static bluetooth_device_list_t s_stub_cache = { 0 };

static void bluetooth_init(void)
{
    if (s_stub_initialized) {
        return;
    }

    memset(&s_stub_cache, 0, sizeof(s_stub_cache));
    s_stub_cache.last_error = ESP_ERR_NOT_SUPPORTED;
    s_stub_initialized = true;
}

void bluetooth_scan_start_periodic(uint32_t interval_ms)
{
    esp_err_t err;

    (void)interval_ms;
    bluetooth_init();
    s_stub_cache.version += 1U;
    s_stub_cache.last_error = ESP_ERR_NOT_SUPPORTED;
    err = esp_event_post(BLUETOOTH_SCAN_EVENT, BLUETOOTH_SCAN_EVENT_UPDATED, NULL, 0, 0);
    if (err != ESP_OK) {
        /* Event loop may not be ready yet. */
    }
}

void bluetooth_scan_stop(void)
{
}

bool bluetooth_get_latest_devices(bluetooth_device_list_t *out)
{
    if (!out) {
        return false;
    }

    bluetooth_init();
    *out = s_stub_cache;
    return true;
}

void bluetooth_service_register_handlers(void)
{
    /* No-op when Bluetooth is disabled */
}

#endif
