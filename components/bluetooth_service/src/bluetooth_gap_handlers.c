/*
 * bluetooth_gap_handlers.c
 * GAP callback implementation and advertisement result projection.
 */
#include "bluetooth_gap_handlers.h"
#include "bluetooth_internal.h"
#include <stdio.h>
#include <string.h>

#if CONFIG_BT_ENABLED && CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLUEDROID_ENABLED

#include "esp_gap_ble_api.h"

#if defined(CONFIG_BT_BLE_50_FEATURES_SUPPORTED)
#define BLUETOOTH_BLE5_SCAN_ENABLED 1
#else
#define BLUETOOTH_BLE5_SCAN_ENABLED 0
#endif

static bluetooth_runtime_t *s_runtime = NULL;

static esp_err_t bluetooth_status_to_err(esp_bt_status_t status)
{
    if (status == ESP_BT_STATUS_SUCCESS) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

static void bluetooth_copy_name(char *dst, size_t dst_size, const uint8_t *src, uint8_t src_len)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src || src_len == 0U) {
        snprintf(dst, dst_size, "<noname>");
        return;
    }

    snprintf(dst, dst_size, "%.*s", (int)src_len, (const char *)src);
}

static int bluetooth_find_device_index(const uint8_t *addr)
{
    size_t i;

    if (!s_runtime || !addr) {
        return -1;
    }

    for (i = 0; i < s_runtime->scan_snapshot.count; ++i) {
        if (memcmp(s_runtime->scan_snapshot.items[i].addr, addr, sizeof(s_runtime->scan_snapshot.items[i].addr)) == 0) {
            return (int)i;
        }
    }

    return -1;
}

/*
 * Common function to update device with advertisement data (BLE5.x extended version).
 * Handles name extraction from complete name or short name AD types.
 */
static void bluetooth_update_device_with_adv_data_ext(bluetooth_device_t *item, const uint8_t *adv_data, uint16_t adv_data_len)
{
    uint8_t name_len = 0;
    const uint8_t *name_data;

    if (!item) {
        return;
    }

    /* Try to get complete name first */
    name_data = esp_ble_resolve_adv_data_by_type(
        (uint8_t *)adv_data,
        adv_data_len,
        ESP_BLE_AD_TYPE_NAME_CMPL,
        &name_len);

    /* If not found, try short name */
    if (!name_data || name_len == 0U) {
        name_data = esp_ble_resolve_adv_data_by_type(
            (uint8_t *)adv_data,
            adv_data_len,
            ESP_BLE_AD_TYPE_NAME_SHORT,
            &name_len);
    }

    /* If still no name, use default or keep existing */
    if (!name_data || name_len == 0U) {
        if (item->name[0] == '\0') {
            bluetooth_copy_name(item->name, sizeof(item->name), NULL, 0);
        }
        return;
    }

    bluetooth_copy_name(item->name, sizeof(item->name), name_data, name_len);
}

/*
 * Common function to update device with advertisement data (BLE4.x legacy version).
 * Uses legacy esp_ble_resolve_adv_data() API.
 */
static void bluetooth_update_device_with_adv_data_legacy(bluetooth_device_t *item, const uint8_t *adv_data)
{
    uint8_t name_len = 0;
    const uint8_t *name_data;

    if (!item) {
        return;
    }

    /* Try to get complete name first */
    name_data = esp_ble_resolve_adv_data(
        (uint8_t *)adv_data,
        ESP_BLE_AD_TYPE_NAME_CMPL,
        &name_len);

    /* If not found, try short name */
    if (!name_data || name_len == 0U) {
        name_data = esp_ble_resolve_adv_data(
            (uint8_t *)adv_data,
            ESP_BLE_AD_TYPE_NAME_SHORT,
            &name_len);
    }

    /* If still no name, use default or keep existing */
    if (!name_data || name_len == 0U) {
        if (item->name[0] == '\0') {
            bluetooth_copy_name(item->name, sizeof(item->name), NULL, 0);
        }
        return;
    }

    bluetooth_copy_name(item->name, sizeof(item->name), name_data, name_len);
}

#if !BLUETOOTH_BLE5_SCAN_ENABLED
static void bluetooth_add_scan_result(const esp_ble_gap_cb_param_t *param)
{
    bluetooth_device_t *item;
    int index;
    bool is_new_entry = false;

    if (!s_runtime || !param) {
        return;
    }

    index = bluetooth_find_device_index(param->scan_rst.bda);
    if (index < 0) {
        is_new_entry = true;
        s_runtime->scan_snapshot.total_found += 1U;
        if (s_runtime->scan_snapshot.count >= BLUETOOTH_MAX_DEVICES) {
            return;
        }
        index = (int)s_runtime->scan_snapshot.count;
        s_runtime->scan_snapshot.count += 1U;
    }

    item = &s_runtime->scan_snapshot.items[index];
    memcpy(item->addr, param->scan_rst.bda, sizeof(item->addr));
    item->addr_type = param->scan_rst.ble_addr_type;
    item->rssi = param->scan_rst.rssi;

    bluetooth_update_device_with_adv_data_legacy(item, param->scan_rst.ble_adv);
}
#endif

#if BLUETOOTH_BLE5_SCAN_ENABLED
static void bluetooth_add_ext_scan_result(const esp_ble_gap_ext_adv_report_t *param)
{
    bluetooth_device_t *item;
    int index;
    bool is_new_entry = false;

    if (!s_runtime || !param) {
        return;
    }

    index = bluetooth_find_device_index(param->addr);
    if (index < 0) {
        is_new_entry = true;
        s_runtime->scan_snapshot.total_found += 1U;
        if (s_runtime->scan_snapshot.count >= BLUETOOTH_MAX_DEVICES) {
            return;
        }
        index = (int)s_runtime->scan_snapshot.count;
        s_runtime->scan_snapshot.count += 1U;
    }

    item = &s_runtime->scan_snapshot.items[index];
    memcpy(item->addr, param->addr, sizeof(item->addr));
    item->addr_type = param->addr_type;
    item->rssi = param->rssi;

    bluetooth_update_device_with_adv_data_ext(item, param->adv_data, param->adv_data_len);
}
#endif

static void bluetooth_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (!s_runtime) {
        return;
    }

    switch (event) {
#if BLUETOOTH_BLE5_SCAN_ENABLED
    case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
        if (param) {
            s_runtime->scan_error = bluetooth_status_to_err(param->set_ext_scan_params.status);
        } else {
            s_runtime->scan_error = ESP_FAIL;
        }
        if (s_runtime->scan_param_sem) {
            xSemaphoreGive(s_runtime->scan_param_sem);
        }
        break;
    case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
        if (param && param->ext_scan_start.status != ESP_BT_STATUS_SUCCESS) {
            s_runtime->scan_error = bluetooth_status_to_err(param->ext_scan_start.status);
            s_runtime->scan_active = false;
            if (s_runtime->scan_done_sem) {
                xSemaphoreGive(s_runtime->scan_done_sem);
            }
        }
        break;
    case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
        if (param) {
            bluetooth_add_ext_scan_result(&param->ext_adv_report.params);
        }
        break;
    case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
        s_runtime->scan_active = false;
        if (param && param->ext_scan_stop.status != ESP_BT_STATUS_SUCCESS) {
            s_runtime->scan_error = bluetooth_status_to_err(param->ext_scan_stop.status);
        }
        if (s_runtime->scan_done_sem) {
            xSemaphoreGive(s_runtime->scan_done_sem);
        }
        break;
#else
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        if (param) {
            s_runtime->scan_error = bluetooth_status_to_err(param->scan_param_cmpl.status);
        } else {
            s_runtime->scan_error = ESP_FAIL;
        }
        if (s_runtime->scan_param_sem) {
            xSemaphoreGive(s_runtime->scan_param_sem);
        }
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param && param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            s_runtime->scan_error = bluetooth_status_to_err(param->scan_start_cmpl.status);
            s_runtime->scan_active = false;
            if (s_runtime->scan_done_sem) {
                xSemaphoreGive(s_runtime->scan_done_sem);
            }
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        if (!param) {
            break;
        }
        if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            bluetooth_add_scan_result(param);
        } else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
            s_runtime->scan_active = false;
            if (s_runtime->scan_done_sem) {
                xSemaphoreGive(s_runtime->scan_done_sem);
            }
        }
        break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        s_runtime->scan_active = false;
        if (param && param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            s_runtime->scan_error = bluetooth_status_to_err(param->scan_stop_cmpl.status);
        }
        if (s_runtime->scan_done_sem) {
            xSemaphoreGive(s_runtime->scan_done_sem);
        }
        break;
#endif
    default:
        break;
    }
}

static void bluetooth_drain_sem(SemaphoreHandle_t sem)
{
    if (!sem) {
        return;
    }

    while (xSemaphoreTake(sem, 0) == pdTRUE) {
    }
}

void bluetooth_gap_handlers_bind_runtime(bluetooth_runtime_t *runtime)
{
    s_runtime = runtime;
}

esp_err_t bluetooth_gap_handlers_register_callback(void)
{
    if (!s_runtime) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_ble_gap_register_callback(bluetooth_gap_callback);
}

void bluetooth_gap_handlers_prepare_cycle(void)
{
    if (!s_runtime) {
        return;
    }

    memset(&s_runtime->scan_snapshot, 0, sizeof(s_runtime->scan_snapshot));
    s_runtime->scan_error = ESP_OK;
    s_runtime->scan_active = false;
    bluetooth_drain_sem(s_runtime->scan_param_sem);
    bluetooth_drain_sem(s_runtime->scan_done_sem);
}

void bluetooth_gap_handlers_stop_active_scan(void)
{
    if (!s_runtime || !s_runtime->scan_active) {
        return;
    }

#if BLUETOOTH_BLE5_SCAN_ENABLED
    (void)esp_ble_gap_stop_ext_scan();
#else
    (void)esp_ble_gap_stop_scanning();
#endif
}

#else

void bluetooth_gap_handlers_bind_runtime(bluetooth_runtime_t *runtime)
{
    (void)runtime;
}

esp_err_t bluetooth_gap_handlers_register_callback(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

void bluetooth_gap_handlers_prepare_cycle(void)
{
}

void bluetooth_gap_handlers_stop_active_scan(void)
{
}

#endif
