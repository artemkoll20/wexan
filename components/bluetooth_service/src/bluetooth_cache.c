/*
 * bluetooth_cache.c
 * Thread-safe BLE scan cache mutation and update event publishing.
 */
#include "bluetooth_cache.h"
#include "bluetooth_internal.h"
#include <string.h>
#include "esp_event.h"

void bluetooth_cache_reset(bluetooth_runtime_t *runtime, esp_err_t initial_error)
{
    if (!runtime) {
        return;
    }

    memset(&runtime->device_cache, 0, sizeof(runtime->device_cache));
    runtime->device_cache.last_error = initial_error;
}

void bluetooth_cache_update_success(bluetooth_runtime_t *runtime, const bluetooth_device_list_t *data)
{
    uint32_t next_version;

    if (!runtime || !runtime->cache_lock || !data) {
        return;
    }

    if (xSemaphoreTake(runtime->cache_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    next_version = runtime->device_cache.version + 1U;
    runtime->device_cache = *data;
    runtime->device_cache.version = next_version;
    runtime->device_cache.last_error = ESP_OK;

    xSemaphoreGive(runtime->cache_lock);
}

void bluetooth_cache_update_error(bluetooth_runtime_t *runtime, esp_err_t err)
{
    if (!runtime || !runtime->cache_lock) {
        return;
    }

    if (xSemaphoreTake(runtime->cache_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    runtime->device_cache.version += 1U;
    runtime->device_cache.last_error = err;
    xSemaphoreGive(runtime->cache_lock);
}

bool bluetooth_cache_get_latest(bluetooth_runtime_t *runtime, bluetooth_device_list_t *out)
{
    if (!runtime || !out || !runtime->cache_lock || !runtime->initialized) {
        return false;
    }

    if (xSemaphoreTake(runtime->cache_lock, pdMS_TO_TICKS(BLUETOOTH_CACHE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    *out = runtime->device_cache;
    xSemaphoreGive(runtime->cache_lock);
    return true;
}

void bluetooth_cache_post_updated_event(void)
{
    esp_err_t err = esp_event_post(BLUETOOTH_SCAN_EVENT, BLUETOOTH_SCAN_EVENT_UPDATED, NULL, 0, 0);
    if (err != ESP_OK) {
        /* Ignore event queue pressure; next scan update will post again. */
    }
}
