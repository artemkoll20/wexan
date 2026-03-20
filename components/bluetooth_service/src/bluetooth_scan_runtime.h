#ifndef BLUETOOTH_SCAN_RUNTIME_H
#define BLUETOOTH_SCAN_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "net/bluetooth.h"

typedef struct {
    bool initialized;
    volatile bool scan_task_running;
    volatile uint32_t scan_interval_ms;
    TaskHandle_t scan_task_handle;
    SemaphoreHandle_t cache_lock;
    SemaphoreHandle_t scan_gate_lock;
    SemaphoreHandle_t scan_param_sem;
    SemaphoreHandle_t scan_done_sem;
    bluetooth_device_list_t device_cache;
    bluetooth_device_list_t scan_snapshot;
    volatile esp_err_t scan_error;
    volatile bool scan_active;
} bluetooth_runtime_t;

#endif /* BLUETOOTH_SCAN_RUNTIME_H */
