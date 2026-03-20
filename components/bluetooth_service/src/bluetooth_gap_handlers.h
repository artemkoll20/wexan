#ifndef BLUETOOTH_GAP_HANDLERS_H
#define BLUETOOTH_GAP_HANDLERS_H

#include "bluetooth_scan_runtime.h"

void bluetooth_gap_handlers_bind_runtime(bluetooth_runtime_t *runtime);
esp_err_t bluetooth_gap_handlers_register_callback(void);
void bluetooth_gap_handlers_prepare_cycle(void);
void bluetooth_gap_handlers_stop_active_scan(void);

#endif /* BLUETOOTH_GAP_HANDLERS_H */
