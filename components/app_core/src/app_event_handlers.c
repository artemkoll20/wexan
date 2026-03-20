/*
 * app_event_handlers.c
 * Implementation of abstract event handlers for WiFi/Bluetooth services.
 */
#include "app_core/app_event_handlers.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/** WiFi handlers storage */
static const app_wifi_handlers_t *s_wifi_handlers = NULL;

/** Bluetooth handlers storage */
static const app_bluetooth_handlers_t *s_bluetooth_handlers = NULL;

void app_event_handlers_register_wifi(const app_wifi_handlers_t *handlers)
{
    s_wifi_handlers = handlers;
}

void app_event_handlers_register_bluetooth(const app_bluetooth_handlers_t *handlers)
{
    s_bluetooth_handlers = handlers;
}

bool app_event_handlers_wifi_available(void)
{
    return (s_wifi_handlers != NULL
            && s_wifi_handlers->get_networks != NULL
            && s_wifi_handlers->start_scan != NULL
            && s_wifi_handlers->stop_scan != NULL);
}

bool app_event_handlers_bluetooth_available(void)
{
    return (s_bluetooth_handlers != NULL
            && s_bluetooth_handlers->get_devices != NULL
            && s_bluetooth_handlers->start_scan != NULL
            && s_bluetooth_handlers->stop_scan != NULL);
}

bool app_event_handlers_wifi_get_networks(void *buffer, size_t buffer_size)
{
    if (!app_event_handlers_wifi_available() || !buffer) {
        return false;
    }
    return s_wifi_handlers->get_networks(buffer, buffer_size);
}

void app_event_handlers_wifi_start_scan(uint32_t interval_ms)
{
    if (!app_event_handlers_wifi_available()) {
        return;
    }
    s_wifi_handlers->start_scan(interval_ms);
}

void app_event_handlers_wifi_stop_scan(void)
{
    if (!app_event_handlers_wifi_available()) {
        return;
    }
    s_wifi_handlers->stop_scan();
}

uint32_t app_event_handlers_wifi_get_interval(void)
{
    if (!s_wifi_handlers) {
        return 0;
    }
    return s_wifi_handlers->scan_interval_ms;
}

bool app_event_handlers_bluetooth_get_devices(void *buffer, size_t buffer_size)
{
    if (!app_event_handlers_bluetooth_available() || !buffer) {
        return false;
    }
    return s_bluetooth_handlers->get_devices(buffer, buffer_size);
}

void app_event_handlers_bluetooth_start_scan(uint32_t interval_ms)
{
    if (!app_event_handlers_bluetooth_available()) {
        return;
    }
    s_bluetooth_handlers->start_scan(interval_ms);
}

void app_event_handlers_bluetooth_stop_scan(void)
{
    if (!app_event_handlers_bluetooth_available()) {
        return;
    }
    s_bluetooth_handlers->stop_scan();
}

uint32_t app_event_handlers_bluetooth_get_interval(void)
{
    if (!s_bluetooth_handlers) {
        return 0;
    }
    return s_bluetooth_handlers->scan_interval_ms;
}
