/*
 * bluetooth_scan_engine.c
 * Explicit BLE scan cycle state machine.
 */
#include "bluetooth_scan_engine.h"
#include "bluetooth_gap_handlers.h"
#include "bluetooth_internal.h"
#include <string.h>

#if CONFIG_BT_ENABLED && CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLUEDROID_ENABLED

#include "esp_gap_ble_api.h"

#if defined(CONFIG_BT_BLE_50_FEATURES_SUPPORTED)
#define BLUETOOTH_BLE5_SCAN_ENABLED 1
#else
#define BLUETOOTH_BLE5_SCAN_ENABLED 0
#endif

#if BLUETOOTH_BLE5_SCAN_ENABLED
static const esp_ble_ext_scan_params_t s_ble_scan_params = {
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
    .cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK,
    .uncoded_cfg = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .scan_interval = 0x50,
        .scan_window = 0x30,
    },
};
#else
static const esp_ble_scan_params_t s_ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};
#endif

static esp_err_t bluetooth_scan_set_params(void)
{
#if BLUETOOTH_BLE5_SCAN_ENABLED
    return esp_ble_gap_set_ext_scan_params(&s_ble_scan_params);
#else
    return esp_ble_gap_set_scan_params(&s_ble_scan_params);
#endif
}

static esp_err_t bluetooth_scan_start(void)
{
#if BLUETOOTH_BLE5_SCAN_ENABLED
    /* esp_ble_gap_start_ext_scan uses 10 ms duration units. */
    return esp_ble_gap_start_ext_scan((uint32_t)BLUETOOTH_SCAN_DURATION_SECONDS * 100U, 0);
#else
    return esp_ble_gap_start_scanning(BLUETOOTH_SCAN_DURATION_SECONDS);
#endif
}

void bluetooth_scan_engine_prepare(bluetooth_scan_engine_t *engine, bluetooth_runtime_t *runtime)
{
    if (!engine) {
        return;
    }

    memset(engine, 0, sizeof(*engine));
    engine->runtime = runtime;
    engine->state = BLUETOOTH_SCAN_STATE_IDLE;
    engine->last_error = ESP_OK;
}

void bluetooth_scan_engine_stop_active(bluetooth_runtime_t *runtime)
{
    if (!runtime || !runtime->scan_active) {
        return;
    }

    bluetooth_gap_handlers_stop_active_scan();
}

/* State handler: Set scan parameters. */
static void bluetooth_scan_handle_set_params(bluetooth_scan_engine_t *engine)
{
    engine->last_error = bluetooth_scan_set_params();
    engine->state = (engine->last_error == ESP_OK) ? BLUETOOTH_SCAN_STATE_WAIT_PARAM_ACK
                                                  : BLUETOOTH_SCAN_STATE_ERROR;
}

/* State handler: Wait for parameter acknowledgment. */
static void bluetooth_scan_handle_wait_param_ack(bluetooth_scan_engine_t *engine, bluetooth_runtime_t *runtime)
{
    if (xSemaphoreTake(runtime->scan_param_sem, pdMS_TO_TICKS(BLUETOOTH_SCAN_PARAM_TIMEOUT_MS)) != pdTRUE) {
        engine->last_error = ESP_ERR_TIMEOUT;
        engine->state = BLUETOOTH_SCAN_STATE_ERROR;
        return;
    }

    if (runtime->scan_error != ESP_OK) {
        engine->last_error = runtime->scan_error;
        engine->state = BLUETOOTH_SCAN_STATE_ERROR;
        return;
    }

    engine->state = BLUETOOTH_SCAN_STATE_START_SCAN;
}

/* State handler: Start BLE scan. */
static void bluetooth_scan_handle_start_scan(bluetooth_scan_engine_t *engine, bluetooth_runtime_t *runtime)
{
    engine->last_error = bluetooth_scan_start();
    if (engine->last_error != ESP_OK) {
        engine->state = BLUETOOTH_SCAN_STATE_ERROR;
        return;
    }

    runtime->scan_active = true;
    engine->state = BLUETOOTH_SCAN_STATE_WAIT_SCAN_DONE;
}

/* Helper: Check if scan result timeout should be accepted. */
static bool bluetooth_scan_is_timeout_acceptable(bluetooth_runtime_t *runtime)
{
    /* Some controllers report completion only after explicit stop.
     * Accept a late completion or partial snapshot before treating it as a hard timeout. */
    if (xSemaphoreTake(runtime->scan_done_sem, pdMS_TO_TICKS(1500)) == pdTRUE) {
        return (runtime->scan_error == ESP_OK);
    }
    return (runtime->scan_error == ESP_OK);
}

/* State handler: Wait for scan to complete. */
static void bluetooth_scan_handle_wait_scan_done(bluetooth_scan_engine_t *engine, bluetooth_runtime_t *runtime)
{
    bool has_results = (runtime->scan_snapshot.count > 0U || runtime->scan_snapshot.total_found > 0U);

    if (xSemaphoreTake(runtime->scan_done_sem, pdMS_TO_TICKS(BLUETOOTH_SCAN_RESULT_TIMEOUT_MS)) != pdTRUE) {
        bluetooth_scan_engine_stop_active(runtime);

        if (bluetooth_scan_is_timeout_acceptable(runtime) || has_results) {
            engine->state = BLUETOOTH_SCAN_STATE_FINALIZE;
            return;
        }

        engine->last_error = ESP_ERR_TIMEOUT;
        engine->state = BLUETOOTH_SCAN_STATE_ERROR;
        return;
    }

    if (runtime->scan_error != ESP_OK) {
        engine->last_error = runtime->scan_error;
        engine->state = BLUETOOTH_SCAN_STATE_ERROR;
        return;
    }

    engine->state = BLUETOOTH_SCAN_STATE_FINALIZE;
}

esp_err_t bluetooth_scan_engine_run_once(bluetooth_scan_engine_t *engine, bluetooth_device_list_t *out)
{
    bluetooth_runtime_t *runtime;

    if (!engine || !engine->runtime || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    runtime = engine->runtime;
    memset(out, 0, sizeof(*out));
    memset(&engine->working_snapshot, 0, sizeof(engine->working_snapshot));

    if (!runtime->scan_gate_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(runtime->scan_gate_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    engine->state = BLUETOOTH_SCAN_STATE_SET_PARAMS;
    engine->last_error = ESP_OK;
    bluetooth_gap_handlers_prepare_cycle();

    /* Process each state through dedicated handler functions. */
    while (engine->state != BLUETOOTH_SCAN_STATE_FINALIZE && engine->state != BLUETOOTH_SCAN_STATE_ERROR) {
        switch (engine->state) {
        case BLUETOOTH_SCAN_STATE_SET_PARAMS:
            bluetooth_scan_handle_set_params(engine);
            break;
        case BLUETOOTH_SCAN_STATE_WAIT_PARAM_ACK:
            bluetooth_scan_handle_wait_param_ack(engine, runtime);
            break;
        case BLUETOOTH_SCAN_STATE_START_SCAN:
            bluetooth_scan_handle_start_scan(engine, runtime);
            break;
        case BLUETOOTH_SCAN_STATE_WAIT_SCAN_DONE:
            bluetooth_scan_handle_wait_scan_done(engine, runtime);
            break;
        default:
            engine->last_error = ESP_FAIL;
            engine->state = BLUETOOTH_SCAN_STATE_ERROR;
            break;
        }
    }

    if (engine->state == BLUETOOTH_SCAN_STATE_FINALIZE) {
        engine->working_snapshot = runtime->scan_snapshot;
        engine->working_snapshot.last_error = ESP_OK;
        *out = engine->working_snapshot;
    }

    runtime->scan_active = false;
    xSemaphoreGive(runtime->scan_gate_lock);
    return engine->last_error;
}

#else

void bluetooth_scan_engine_prepare(bluetooth_scan_engine_t *engine, bluetooth_runtime_t *runtime)
{
    (void)engine;
    (void)runtime;
}

void bluetooth_scan_engine_stop_active(bluetooth_runtime_t *runtime)
{
    (void)runtime;
}

esp_err_t bluetooth_scan_engine_run_once(bluetooth_scan_engine_t *engine, bluetooth_device_list_t *out)
{
    (void)engine;
    (void)out;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
