/*
 * app_event_handlers.h
 * Abstract event handlers for decoupling app_core from WiFi/Bluetooth implementations.
 */
#ifndef APP_EVENT_HANDLERS_H
#define APP_EVENT_HANDLERS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Abstract handler for WiFi scan data retrieval.
 *
 * Implementations should copy current WiFi scan results into provided buffer.
 *
 * @param[out] buffer Pointer to buffer to receive network data
 * @param[in] buffer_size Size of provided buffer in bytes
 * @return true if data was copied successfully, false otherwise
 */
typedef bool (*app_wifi_get_networks_t)(void *buffer, size_t buffer_size);

/**
 * @brief Abstract handler for starting WiFi periodic scanning.
 *
 * @param interval_ms Scan interval in milliseconds
 */
typedef void (*app_wifi_start_scan_t)(uint32_t interval_ms);

/**
 * @brief Abstract handler for stopping WiFi scanning.
 */
typedef void (*app_wifi_stop_scan_t)(void);

/**
 * @brief Abstract handler for Bluetooth scan data retrieval.
 *
 * Implementations should copy current Bluetooth scan results into provided buffer.
 *
 * @param[out] buffer Pointer to buffer to receive device data
 * @param[in] buffer_size Size of provided buffer in bytes
 * @return true if data was copied successfully, false otherwise
 */
typedef bool (*app_bluetooth_get_devices_t)(void *buffer, size_t buffer_size);

/**
 * @brief Abstract handler for starting Bluetooth periodic scanning.
 *
 * @param interval_ms Scan interval in milliseconds
 */
typedef void (*app_bluetooth_start_scan_t)(uint32_t interval_ms);

/**
 * @brief Abstract handler for stopping Bluetooth scanning.
 */
typedef void (*app_bluetooth_stop_scan_t)(void);

/**
 * @brief Structure holding WiFi service abstraction pointers.
 */
typedef struct {
    app_wifi_get_networks_t get_networks;    /**< Get current WiFi scan results */
    app_wifi_start_scan_t start_scan;        /**< Start periodic WiFi scanning */
    app_wifi_stop_scan_t stop_scan;          /**< Stop WiFi scanning */
    uint32_t scan_interval_ms;               /**< Default scan interval */
} app_wifi_handlers_t;

/**
 * @brief Structure holding Bluetooth service abstraction pointers.
 */
typedef struct {
    app_bluetooth_get_devices_t get_devices;    /**< Get current Bluetooth scan results */
    app_bluetooth_start_scan_t start_scan;       /**< Start periodic Bluetooth scanning */
    app_bluetooth_stop_scan_t stop_scan;         /**< Stop Bluetooth scanning */
    uint32_t scan_interval_ms;                   /**< Default scan interval */
} app_bluetooth_handlers_t;

/**
 * @brief Register WiFi service handlers.
 *
 * @param handlers Pointer to WiFi handlers structure
 */
void app_event_handlers_register_wifi(const app_wifi_handlers_t *handlers);

/**
 * @brief Register Bluetooth service handlers.
 *
 * @param handlers Pointer to Bluetooth handlers structure
 */
void app_event_handlers_register_bluetooth(const app_bluetooth_handlers_t *handlers);

/**
 * @brief Check if WiFi handlers are registered.
 *
 * @return true if WiFi handlers are available
 */
bool app_event_handlers_wifi_available(void);

/**
 * @brief Check if Bluetooth handlers are registered.
 *
 * @return true if Bluetooth handlers are available
 */
bool app_event_handlers_bluetooth_available(void);

/**
 * @brief Get WiFi scan results via registered handler.
 *
 * @param[out] buffer Pointer to buffer to receive network data
 * @param[in] buffer_size Size of provided buffer
 * @return true if successful
 */
bool app_event_handlers_wifi_get_networks(void *buffer, size_t buffer_size);

/**
 * @brief Start WiFi scanning via registered handler.
 *
 * @param interval_ms Scan interval in milliseconds
 */
void app_event_handlers_wifi_start_scan(uint32_t interval_ms);

/**
 * @brief Stop WiFi scanning via registered handler.
 */
void app_event_handlers_wifi_stop_scan(void);

/**
 * @brief Get WiFi scan interval.
 *
 * @return Default scan interval in milliseconds
 */
uint32_t app_event_handlers_wifi_get_interval(void);

/**
 * @brief Get Bluetooth scan results via registered handler.
 *
 * @param[out] buffer Pointer to buffer to receive device data
 * @param[in] buffer_size Size of provided buffer
 * @return true if successful
 */
bool app_event_handlers_bluetooth_get_devices(void *buffer, size_t buffer_size);

/**
 * @brief Start Bluetooth scanning via registered handler.
 *
 * @param interval_ms Scan interval in milliseconds
 */
void app_event_handlers_bluetooth_start_scan(uint32_t interval_ms);

/**
 * @brief Stop Bluetooth scanning via registered handler.
 */
void app_event_handlers_bluetooth_stop_scan(void);

/**
 * @brief Get Bluetooth scan interval.
 *
 * @return Default scan interval in milliseconds
 */
uint32_t app_event_handlers_bluetooth_get_interval(void);

#endif // APP_EVENT_HANDLERS_H
