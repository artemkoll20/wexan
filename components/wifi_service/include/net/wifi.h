/*
 * wifi.h
 * Periodic Wi-Fi scan API and immutable snapshot model for UI consumers.
 */
#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"

#define WIFI_MAX_NETWORKS 20
#define WIFI_DEFAULT_SCAN_INTERVAL_MS 5000
#define WIFI_MIN_SCAN_INTERVAL_MS 1500

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t authmode;
} wifi_network_t;

typedef struct {
    wifi_network_t items[WIFI_MAX_NETWORKS];
    size_t count;
    size_t total_found;
    uint32_t version;
    esp_err_t last_error;
} wifi_network_list_t;

ESP_EVENT_DECLARE_BASE(WIFI_SCAN_EVENT);

typedef enum {
    WIFI_SCAN_EVENT_UPDATED = 1,
} wifi_scan_event_id_t;

/* Starts (or reconfigures) periodic background scanning. */
void wifi_scan_start_periodic(uint32_t interval_ms);

/* Stops periodic background scanning task. */
void wifi_scan_stop(void);

/* Copies the latest scan snapshot into out. Returns false if unavailable. */
bool wifi_get_latest_networks(wifi_network_list_t *out);

/* Returns number of UI pages for a given network item count. */
size_t wifi_page_count(size_t total_items);

/* Builds a paged text representation of the provided Wi-Fi scan snapshot. */
void wifi_build_page_text(const wifi_network_list_t *list, size_t page_index, char *out, size_t out_size);

/* Register WiFi service handlers with app_event_handlers abstraction layer. */
void wifi_service_register_handlers(void);

#endif /* APP_WIFI_H */
