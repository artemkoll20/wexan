/*
 * wifi_scan.c
 * Background Wi-Fi scanner with thread-safe cache and update events.
 */
#include "wifi_internal.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_scan";

ESP_EVENT_DEFINE_BASE(WIFI_SCAN_EVENT);

/**
 * @brief WiFi stack initialization flag.
 *
 * Set to true after successful wifi_init() call.
 * Prevents re-initialization of WiFi stack.
 */
static bool s_wifi_initialized = false;

/**
 * @brief Scan task running flag.
 *
 * Controls the periodic scan task loop.
 * Set to true to start scanning, false to stop.
 *
 * @note Volatile for thread-safe access from multiple contexts.
 */
static volatile bool s_scan_task_running = false;

/**
 * @brief Current scan interval in milliseconds.
 *
 * Configurable via wifi_scan_start_periodic().
 * Default value is WIFI_DEFAULT_SCAN_INTERVAL_MS.
 */
static volatile uint32_t s_scan_interval_ms = WIFI_DEFAULT_SCAN_INTERVAL_MS;

/**
 * @brief Handle to the WiFi scan FreeRTOS task.
 *
 * Used for task creation and deletion.
 */
static TaskHandle_t s_scan_task_handle = NULL;

/**
 * @brief Mutex for thread-safe access to network cache.
 *
 * Protects s_network_cache during read/write operations.
 */
static SemaphoreHandle_t s_cache_lock = NULL;

/**
 * @brief Cached list of WiFi networks.
 *
 * Contains last successful scan results with version tracking
 * and last error state.
 */
static wifi_network_list_t s_network_cache = { 0 };

static void wifi_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void wifi_event_loop_init(void)
{
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
}

static void wifi_netif_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
}

static void wifi_copy_ssid(char *dst, size_t dst_size, const uint8_t *src)
{
    size_t len = 0;

    if (!dst || dst_size == 0 || !src) {
        return;
    }

    while ((len + 1U) < dst_size && src[len] != '\0') {
        dst[len] = (char)src[len];
        ++len;
    }

    dst[len] = '\0';
    if (len == 0) {
        strncpy(dst, "<hidden>", dst_size - 1U);
        dst[dst_size - 1U] = '\0';
    }
}

static esp_err_t wifi_scan_once(wifi_network_list_t *out)
{
    wifi_ap_record_t records[WIFI_MAX_NETWORKS];
    wifi_scan_config_t scan_cfg = { 0 };
    uint16_t count = WIFI_MAX_NETWORKS;
    uint16_t total_count = 0;
    uint16_t i;
    esp_err_t err;

    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_scan_get_ap_records(&count, records);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_scan_get_ap_num(&total_count);
    if (err != ESP_OK) {
        return err;
    }

    out->count = count;
    out->total_found = total_count;
    out->last_error = ESP_OK;

    for (i = 0; i < count; ++i) {
        wifi_copy_ssid(out->items[i].ssid, sizeof(out->items[i].ssid), records[i].ssid);
        out->items[i].rssi = records[i].rssi;
        out->items[i].channel = records[i].primary;
        out->items[i].authmode = (uint8_t)records[i].authmode;
    }

    return ESP_OK;
}

static void wifi_cache_update_success(const wifi_network_list_t *data)
{
    uint32_t next_version;

    if (!s_cache_lock || !data) {
        return;
    }

    if (xSemaphoreTake(s_cache_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    next_version = s_network_cache.version + 1U;
    s_network_cache = *data;
    s_network_cache.version = next_version;
    s_network_cache.last_error = ESP_OK;

    xSemaphoreGive(s_cache_lock);
}

static void wifi_cache_update_error(esp_err_t err)
{
    if (!s_cache_lock) {
        return;
    }

    if (xSemaphoreTake(s_cache_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    s_network_cache.version += 1U;
    s_network_cache.last_error = err;
    xSemaphoreGive(s_cache_lock);
}

static void wifi_post_updated_event(void)
{
    esp_err_t err = esp_event_post(WIFI_SCAN_EVENT, WIFI_SCAN_EVENT_UPDATED, NULL, 0, 0);
    if (err != ESP_OK) {
        /* Ignore event queue pressure; next scan update will post again. */
    }
}

static void wifi_scan_task(void *arg)
{
    (void)arg;

    while (s_scan_task_running) {
        wifi_network_list_t snapshot;
        esp_err_t err = wifi_scan_once(&snapshot);

        if (err == ESP_OK) {
            wifi_cache_update_success(&snapshot);
        } else {
            wifi_cache_update_error(err);
        }
        wifi_post_updated_event();

        vTaskDelay(pdMS_TO_TICKS(s_scan_interval_ms));
    }

    vTaskDelete(NULL);
    s_scan_task_handle = NULL;
}

static void wifi_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_netif_t *wifi_sta_netif;

    if (s_wifi_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    if (!s_cache_lock) {
        s_cache_lock = xSemaphoreCreateMutex();
        ESP_ERROR_CHECK(s_cache_lock ? ESP_OK : ESP_ERR_NO_MEM);
    }

    wifi_nvs_init();
    wifi_netif_init();
    wifi_event_loop_init();
    wifi_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(wifi_sta_netif ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    memset(&s_network_cache, 0, sizeof(s_network_cache));
    s_network_cache.last_error = ESP_OK;
    s_wifi_initialized = true;
    ESP_LOGI(TAG, "Wi-Fi initialized");
}

void wifi_scan_start_periodic(uint32_t interval_ms)
{
    wifi_init();

    if (interval_ms < WIFI_MIN_SCAN_INTERVAL_MS) {
        interval_ms = WIFI_MIN_SCAN_INTERVAL_MS;
    }
    s_scan_interval_ms = interval_ms;

    if (s_scan_task_running) {
        ESP_LOGW(TAG, "Wi-Fi scan already running");
        return;
    }

    ESP_LOGI(TAG, "Starting periodic Wi-Fi scan, interval=%ums", interval_ms);
    s_scan_task_running = true;
    if (xTaskCreate(wifi_scan_task, "wifi_scan", WIFI_SCAN_TASK_STACK_SIZE, NULL, WIFI_SCAN_TASK_PRIORITY, &s_scan_task_handle) != pdPASS) {
        s_scan_task_running = false;
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}

void wifi_scan_stop(void)
{
    if (!s_scan_task_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping Wi-Fi scan");
    // Non-blocking stop: just set the flag and let the task terminate itself
    s_scan_task_running = false;
    // Don't wait here - let the task exit on its own
    ESP_LOGI(TAG, "Wi-Fi scan stop initiated");
}

bool wifi_get_latest_networks(wifi_network_list_t *out)
{
    if (!out || !s_cache_lock || !s_wifi_initialized) {
        return false;
    }

    if (xSemaphoreTake(s_cache_lock, pdMS_TO_TICKS(WIFI_CACHE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    *out = s_network_cache;
    xSemaphoreGive(s_cache_lock);
    return true;
}

/**
 * @brief WiFi handler implementations for app_event_handlers abstraction.
 */
static bool wifi_service_get_networks_impl(void *buffer, size_t buffer_size)
{
    if (buffer_size < sizeof(wifi_network_list_t)) {
        return false;
    }
    return wifi_get_latest_networks((wifi_network_list_t *)buffer);
}

static void wifi_service_start_scan_impl(uint32_t interval_ms)
{
    wifi_scan_start_periodic(interval_ms);
}

static void wifi_service_stop_scan_impl(void)
{
    wifi_scan_stop();
}

/**
 * @brief Static handlers structure for WiFi service.
 */
static const app_wifi_handlers_t s_wifi_handlers = {
    .get_networks = wifi_service_get_networks_impl,
    .start_scan = wifi_service_start_scan_impl,
    .stop_scan = wifi_service_stop_scan_impl,
    .scan_interval_ms = WIFI_DEFAULT_SCAN_INTERVAL_MS,
};

void wifi_service_register_handlers(void)
{
    app_event_handlers_register_wifi(&s_wifi_handlers);
}
