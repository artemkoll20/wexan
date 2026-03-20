#ifndef BLUETOOTH_CACHE_H
#define BLUETOOTH_CACHE_H

#include <stdbool.h>
#include "bluetooth_scan_runtime.h"

void bluetooth_cache_reset(bluetooth_runtime_t *runtime, esp_err_t initial_error);
void bluetooth_cache_update_success(bluetooth_runtime_t *runtime, const bluetooth_device_list_t *data);
void bluetooth_cache_update_error(bluetooth_runtime_t *runtime, esp_err_t err);
bool bluetooth_cache_get_latest(bluetooth_runtime_t *runtime, bluetooth_device_list_t *out);
void bluetooth_cache_post_updated_event(void);

#endif /* BLUETOOTH_CACHE_H */
