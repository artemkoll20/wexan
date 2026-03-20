/*
 * bluetooth.h
 * Periodic BLE scan API and immutable snapshot model for UI consumers.
 */
#ifndef APP_BLUETOOTH_H
#define APP_BLUETOOTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"

#define BLUETOOTH_MAX_DEVICES 20
#define BLUETOOTH_DEFAULT_SCAN_INTERVAL_MS 5000
#define BLUETOOTH_MIN_SCAN_INTERVAL_MS 1500

typedef struct {
    char name[33];
    int8_t rssi;
    uint8_t addr[6];
    uint8_t addr_type;
} bluetooth_device_t;

typedef struct {
    bluetooth_device_t items[BLUETOOTH_MAX_DEVICES];
    size_t count;
    size_t total_found;
    uint32_t version;
    esp_err_t last_error;
} bluetooth_device_list_t;

ESP_EVENT_DECLARE_BASE(BLUETOOTH_SCAN_EVENT);

typedef enum {
    BLUETOOTH_SCAN_EVENT_UPDATED = 1,
} bluetooth_scan_event_id_t;

/* Starts (or reconfigures) periodic background BLE scanning. */
void bluetooth_scan_start_periodic(uint32_t interval_ms);

/* Stops periodic background BLE scanning task. */
void bluetooth_scan_stop(void);

/* Copies the latest scan snapshot into out. Returns false if unavailable. */
bool bluetooth_get_latest_devices(bluetooth_device_list_t *out);

/* Returns number of UI pages for a given scan item count. */
size_t bluetooth_page_count(size_t total_items);

/* Builds a paged text representation of the provided BLE scan snapshot. */
void bluetooth_build_page_text(const bluetooth_device_list_t *list, size_t page_index, char *out, size_t out_size);

#if CONFIG_BT_ENABLED && CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLUEDROID_ENABLED

/* Register Bluetooth service handlers with app_event_handlers abstraction layer. */
void bluetooth_service_register_handlers(void);

#endif /* CONFIG_BT_ENABLED && CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLUEDROID_ENABLED */

#endif /* APP_BLUETOOTH_H */
