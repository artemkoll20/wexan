#ifndef WIFI_SERVICE_INTERNAL_H
#define WIFI_SERVICE_INTERNAL_H

#include "net/wifi.h"
#include "app_core/app_event_handlers.h"

#define WIFI_SCAN_TASK_STACK_SIZE 6144
#define WIFI_SCAN_TASK_PRIORITY 4
#define WIFI_CACHE_LOCK_TIMEOUT_MS 100
#define WIFI_PAGE_SIZE 4
#define WIFI_PAGE_LINE_MAX_CHARS 20

/**
 * @brief Register WiFi handlers with app_event_handlers.
 *
 * This function registers WiFi service callbacks with the app_core
 * event handler abstraction layer, enabling decoupled communication.
 */
void wifi_service_register_handlers(void);

#endif /* WIFI_SERVICE_INTERNAL_H */
