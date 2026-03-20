#ifndef BLUETOOTH_SERVICE_INTERNAL_H
#define BLUETOOTH_SERVICE_INTERNAL_H

#include "net/bluetooth.h"
#include "app_core/app_event_handlers.h"

#define BLUETOOTH_SCAN_TASK_STACK_SIZE 6144
#define BLUETOOTH_SCAN_TASK_PRIORITY 4
#define BLUETOOTH_CACHE_LOCK_TIMEOUT_MS 100
#define BLUETOOTH_SCAN_PARAM_TIMEOUT_MS 1500
#define BLUETOOTH_SCAN_RESULT_TIMEOUT_MS 9000
#define BLUETOOTH_SCAN_DURATION_SECONDS 4
#define BLUETOOTH_PAGE_SIZE 4
#define BLUETOOTH_PAGE_LINE_MAX_CHARS 20

/**
 * @brief Register Bluetooth handlers with app_event_handlers.
 *
 * This function registers Bluetooth service callbacks with the app_core
 * event handler abstraction layer, enabling decoupled communication.
 */
void bluetooth_service_register_handlers(void);

#endif /* BLUETOOTH_SERVICE_INTERNAL_H */
