#ifndef BLUETOOTH_SCAN_ENGINE_H
#define BLUETOOTH_SCAN_ENGINE_H

#include "bluetooth_scan_runtime.h"

typedef enum {
    BLUETOOTH_SCAN_STATE_IDLE = 0,
    BLUETOOTH_SCAN_STATE_SET_PARAMS,
    BLUETOOTH_SCAN_STATE_WAIT_PARAM_ACK,
    BLUETOOTH_SCAN_STATE_START_SCAN,
    BLUETOOTH_SCAN_STATE_WAIT_SCAN_DONE,
    BLUETOOTH_SCAN_STATE_FINALIZE,
    BLUETOOTH_SCAN_STATE_ERROR,
} bluetooth_scan_state_t;

typedef struct {
    bluetooth_runtime_t *runtime;
    bluetooth_scan_state_t state;
    esp_err_t last_error;
    bluetooth_device_list_t working_snapshot;
} bluetooth_scan_engine_t;

void bluetooth_scan_engine_prepare(bluetooth_scan_engine_t *engine, bluetooth_runtime_t *runtime);
esp_err_t bluetooth_scan_engine_run_once(bluetooth_scan_engine_t *engine, bluetooth_device_list_t *out);
void bluetooth_scan_engine_stop_active(bluetooth_runtime_t *runtime);

#endif /* BLUETOOTH_SCAN_ENGINE_H */
