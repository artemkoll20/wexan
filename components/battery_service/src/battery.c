/*
 * battery.c
 * Periodic battery voltage measurements for Heltec WiFi Kit 32 V3.
 */
#include "power/battery.h"
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define BATTERY_ADC_CTRL_GPIO GPIO_NUM_37
#define BATTERY_ADC_UNIT ADC_UNIT_1
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_0
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_0
#define BATTERY_ADC_BITWIDTH ADC_BITWIDTH_DEFAULT
#define BATTERY_SAMPLE_COUNT 8
#define BATTERY_SAMPLE_TRIMMED_COUNT (BATTERY_SAMPLE_COUNT - 2)
#define BATTERY_ENABLE_SETTLE_MS 5
#define BATTERY_DEFAULT_INTERVAL_MS 15000
#define BATTERY_MIN_INTERVAL_MS 1000
#define BATTERY_TASK_STACK_SIZE 4096
#define BATTERY_TASK_PRIORITY 4
#define BATTERY_CACHE_LOCK_TIMEOUT_MS 50
#define BATTERY_VALID_MIN_MV 2500
#define BATTERY_VALID_MAX_MV 4600
#define BATTERY_EMPTY_MV 3300
#define BATTERY_FULL_MV 4200
#define BATTERY_FILTER_NUMERATOR 3U
#define BATTERY_FILTER_DENOMINATOR 4U
#define BATTERY_VOLTAGE_SNAP_MV 10U
#define BATTERY_STATE_EQUAL_DELTA_MV 15U
#define BATTERY_LOW_ENTER_MV 3450
#define BATTERY_LOW_EXIT_MV 3600
#define BATTERY_LOW_ENTER_PERCENT 15
#define BATTERY_LOW_EXIT_PERCENT 18

static const char *TAG = "battery_service";

ESP_EVENT_DEFINE_BASE(BATTERY_EVENT);

static bool s_initialized = false;
static volatile bool s_task_running = false;
static volatile uint32_t s_interval_ms = BATTERY_DEFAULT_INTERVAL_MS;
static TaskHandle_t s_task_handle = NULL;
static SemaphoreHandle_t s_cache_lock = NULL;
static battery_state_t s_state_cache = { 0 };
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static esp_err_t s_cali_status = ESP_ERR_INVALID_STATE;
static bool s_low_warning_logged = false;
static bool s_voltage_filter_ready = false;
static uint32_t s_filtered_voltage_mv = 0;

typedef struct {
    uint16_t voltage_mv;
    uint8_t percent;
} battery_curve_point_t;

static const battery_curve_point_t s_battery_curve[] = {
    { 4200U, 100U },
    { 4160U,  95U },
    { 4110U,  90U },
    { 4080U,  85U },
    { 4040U,  80U },
    { 4000U,  75U },
    { 3960U,  70U },
    { 3930U,  65U },
    { 3900U,  60U },
    { 3870U,  55U },
    { 3850U,  50U },
    { 3830U,  45U },
    { 3810U,  40U },
    { 3790U,  35U },
    { 3770U,  30U },
    { 3750U,  25U },
    { 3730U,  20U },
    { 3710U,  15U },
    { 3690U,  10U },
    { 3600U,   5U },
    { 3300U,   0U },
};

static void battery_event_loop_init(void)
{
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(err));
    }
}

static void battery_gpio_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BATTERY_ADC_CTRL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure battery control GPIO: %s", esp_err_to_name(err));
        return;
    }

    err = gpio_set_level(BATTERY_ADC_CTRL_GPIO, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable battery divider: %s", esp_err_to_name(err));
    }
}

static void battery_adc_init(void)
{
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };
    esp_err_t err;

    err = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC oneshot unit: %s", esp_err_to_name(err));
        s_adc_handle = NULL;
        return;
    }

    err = adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(err));
        (void)adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
    }
}

static void battery_calibration_init(void)
{
    adc_cali_scheme_ver_t scheme_mask = 0;
    esp_err_t err;

    s_cali_handle = NULL;
    err = adc_cali_check_scheme(&scheme_mask);
    if (err != ESP_OK) {
        s_cali_status = err;
        ESP_LOGW(TAG, "ADC calibration unavailable: %s", esp_err_to_name(err));
        return;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if ((scheme_mask & ADC_CALI_SCHEME_VER_CURVE_FITTING) != 0) {
        const adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = BATTERY_ADC_UNIT,
            .chan = BATTERY_ADC_CHANNEL,
            .atten = BATTERY_ADC_ATTEN,
            .bitwidth = BATTERY_ADC_BITWIDTH,
        };

        err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
        if (err == ESP_OK) {
            s_cali_status = ESP_OK;
            ESP_LOGI(TAG, "ADC calibration enabled: curve fitting");
            return;
        }

        ESP_LOGW(TAG, "Curve-fitting calibration unavailable: %s", esp_err_to_name(err));
        s_cali_handle = NULL;
        s_cali_status = err;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if ((scheme_mask & ADC_CALI_SCHEME_VER_LINE_FITTING) != 0) {
        const adc_cali_line_fitting_config_t cali_cfg = {
            .unit_id = BATTERY_ADC_UNIT,
            .atten = BATTERY_ADC_ATTEN,
            .bitwidth = BATTERY_ADC_BITWIDTH,
        };

        err = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle);
        if (err == ESP_OK) {
            s_cali_status = ESP_OK;
            ESP_LOGI(TAG, "ADC calibration enabled: line fitting");
            return;
        }

        ESP_LOGW(TAG, "Line-fitting calibration unavailable: %s", esp_err_to_name(err));
        s_cali_handle = NULL;
        s_cali_status = err;
    }
#endif

    if (s_cali_status == ESP_OK) {
        s_cali_status = ESP_ERR_NOT_SUPPORTED;
    }
}

static uint8_t battery_percent_from_voltage(uint32_t voltage_mv)
{
    size_t i;

    if (voltage_mv <= BATTERY_EMPTY_MV) {
        return 0U;
    }
    if (voltage_mv >= BATTERY_FULL_MV) {
        return 100U;
    }

    for (i = 0; i + 1U < (sizeof(s_battery_curve) / sizeof(s_battery_curve[0])); ++i) {
        const battery_curve_point_t upper = s_battery_curve[i];
        const battery_curve_point_t lower = s_battery_curve[i + 1U];

        if (voltage_mv <= upper.voltage_mv && voltage_mv >= lower.voltage_mv) {
            const uint32_t range_mv = upper.voltage_mv - lower.voltage_mv;
            const uint32_t offset_mv = voltage_mv - lower.voltage_mv;
            const uint32_t range_percent = upper.percent - lower.percent;
            return (uint8_t)(lower.percent + ((offset_mv * range_percent + (range_mv / 2U)) / range_mv));
        }
    }

    return 0U;
}

static bool battery_low_state_for_reading(uint32_t voltage_mv, uint8_t percent, bool was_low)
{
    if (was_low) {
        return !(voltage_mv > BATTERY_LOW_EXIT_MV && percent > BATTERY_LOW_EXIT_PERCENT);
    }

    return (voltage_mv <= BATTERY_LOW_ENTER_MV) || (percent <= BATTERY_LOW_ENTER_PERCENT);
}

static bool battery_state_equal(const battery_state_t *lhs, const battery_state_t *rhs)
{
    long voltage_delta;

    if (!lhs || !rhs) {
        return false;
    }

    voltage_delta = (long)lhs->voltage_mv - (long)rhs->voltage_mv;

    return labs(voltage_delta) < (long)BATTERY_STATE_EQUAL_DELTA_MV
        && lhs->percent == rhs->percent
        && lhs->available == rhs->available
        && lhs->low == rhs->low
        && lhs->last_error == rhs->last_error;
}

static battery_state_t battery_get_cached_state_copy(void)
{
    battery_state_t snapshot = { 0 };

    if (!s_cache_lock) {
        return snapshot;
    }

    if (xSemaphoreTake(s_cache_lock, portMAX_DELAY) != pdTRUE) {
        return snapshot;
    }

    snapshot = s_state_cache;
    xSemaphoreGive(s_cache_lock);
    return snapshot;
}

static uint32_t battery_filter_voltage(uint32_t measured_mv, bool previous_available)
{
    uint32_t filtered_mv;

    if (!s_voltage_filter_ready || !previous_available) {
        s_filtered_voltage_mv = measured_mv;
        s_voltage_filter_ready = true;
        return measured_mv;
    }

    filtered_mv = ((s_filtered_voltage_mv * BATTERY_FILTER_NUMERATOR) + measured_mv)
        / BATTERY_FILTER_DENOMINATOR;
    if (filtered_mv > measured_mv && (filtered_mv - measured_mv) < BATTERY_VOLTAGE_SNAP_MV) {
        filtered_mv = measured_mv;
    } else if (measured_mv > filtered_mv && (measured_mv - filtered_mv) < BATTERY_VOLTAGE_SNAP_MV) {
        filtered_mv = measured_mv;
    }

    s_filtered_voltage_mv = filtered_mv;
    return filtered_mv;
}

static void battery_post_updated_event(void)
{
    esp_err_t err = esp_event_post(BATTERY_EVENT, BATTERY_EVENT_UPDATED, NULL, 0, 0);
    if (err != ESP_OK) {
        /* Ignore event queue pressure; the next state change will retry. */
    }
}

static void battery_cache_update_if_changed(const battery_state_t *next_state)
{
    battery_state_t previous_state;
    bool changed;
    bool low_entered;

    if (!next_state || !s_cache_lock) {
        return;
    }

    if (xSemaphoreTake(s_cache_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    previous_state = s_state_cache;
    changed = !battery_state_equal(&previous_state, next_state);
    if (changed) {
        s_state_cache = *next_state;
        s_state_cache.version = previous_state.version + 1U;
    }

    low_entered = changed && !previous_state.low && next_state->low;
    if (!next_state->low) {
        s_low_warning_logged = false;
    } else if (low_entered && !s_low_warning_logged) {
        s_low_warning_logged = true;
    }

    xSemaphoreGive(s_cache_lock);

    if (changed) {
        if (low_entered) {
            ESP_LOGW(TAG, "Low battery detected: %u mV (%u%%)",
                (unsigned)next_state->voltage_mv,
                (unsigned)next_state->percent);
        }
        battery_post_updated_event();
    }
}

static esp_err_t battery_read_trimmed_raw(int *out_raw)
{
    uint32_t sum = 0;
    int min_raw = INT32_MAX;
    int max_raw = INT32_MIN;
    uint32_t i;
    esp_err_t err;

    if (!out_raw || !s_adc_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    for (i = 0; i < BATTERY_SAMPLE_COUNT; ++i) {
        int raw = 0;

        err = adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw);
        if (err != ESP_OK) {
            return err;
        }

        if (raw < min_raw) {
            min_raw = raw;
        }
        if (raw > max_raw) {
            max_raw = raw;
        }
        sum += (uint32_t)raw;
    }

    *out_raw = (int)((sum - (uint32_t)min_raw - (uint32_t)max_raw) / BATTERY_SAMPLE_TRIMMED_COUNT);
    return ESP_OK;
}

static esp_err_t battery_convert_raw_to_adc_mv(int raw, uint32_t *out_adc_mv)
{
    int calibrated_mv = 0;

    if (!out_adc_mv) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_cali_handle) {
        esp_err_t err = adc_cali_raw_to_voltage(s_cali_handle, raw, &calibrated_mv);
        if (err != ESP_OK) {
            return err;
        }

        *out_adc_mv = (uint32_t)calibrated_mv;
        return ESP_OK;
    }

    *out_adc_mv = ((uint32_t)raw * 1100U) / 4095U;
    return (s_cali_status == ESP_OK) ? ESP_ERR_NOT_SUPPORTED : s_cali_status;
}

static esp_err_t battery_measure_once(battery_state_t *out_state)
{
    battery_state_t measured = { 0 };
    battery_state_t previous_state;
    int raw = 0;
    uint32_t adc_mv = 0;
    uint32_t battery_mv = 0;
    esp_err_t read_err;
    esp_err_t convert_err;

    if (!out_state) {
        return ESP_ERR_INVALID_ARG;
    }

    previous_state = battery_get_cached_state_copy();

    if (!s_adc_handle) {
        measured.last_error = ESP_ERR_INVALID_STATE;
        *out_state = measured;
        return measured.last_error;
    }

    if (gpio_set_level(BATTERY_ADC_CTRL_GPIO, 0) != ESP_OK) {
        measured.last_error = ESP_FAIL;
        *out_state = measured;
        return measured.last_error;
    }

    vTaskDelay(pdMS_TO_TICKS(BATTERY_ENABLE_SETTLE_MS));
    read_err = battery_read_trimmed_raw(&raw);
    (void)gpio_set_level(BATTERY_ADC_CTRL_GPIO, 1);

    if (read_err != ESP_OK) {
        measured.last_error = read_err;
        *out_state = measured;
        return read_err;
    }

    convert_err = battery_convert_raw_to_adc_mv(raw, &adc_mv);
    battery_mv = (adc_mv * 49U + 5U) / 10U;

    measured.available = battery_mv >= BATTERY_VALID_MIN_MV && battery_mv <= BATTERY_VALID_MAX_MV;
    if (measured.available) {
        battery_mv = battery_filter_voltage(battery_mv, previous_state.available);
        measured.voltage_mv = (uint16_t)battery_mv;
    } else {
        measured.voltage_mv = 0U;
        s_voltage_filter_ready = false;
    }
    measured.percent = measured.available ? battery_percent_from_voltage(battery_mv) : 0U;
    measured.low = measured.available
        ? battery_low_state_for_reading(battery_mv, measured.percent, previous_state.low)
        : false;
    measured.last_error = convert_err;

    *out_state = measured;
    return (convert_err == ESP_OK || convert_err == ESP_ERR_NOT_SUPPORTED) ? ESP_OK : convert_err;
}

static void battery_task(void *arg)
{
    (void)arg;

    while (s_task_running) {
        battery_state_t snapshot = { 0 };

        (void)battery_measure_once(&snapshot);
        battery_cache_update_if_changed(&snapshot);
        vTaskDelay(pdMS_TO_TICKS(s_interval_ms));
    }

    s_task_handle = NULL;
    vTaskDelete(NULL);
}

void battery_service_init(void)
{
    if (s_initialized) {
        return;
    }

    battery_event_loop_init();

    if (!s_cache_lock) {
        s_cache_lock = xSemaphoreCreateMutex();
        if (!s_cache_lock) {
            ESP_LOGE(TAG, "Failed to create battery cache lock");
            return;
        }
    }

    memset(&s_state_cache, 0, sizeof(s_state_cache));
    s_state_cache.last_error = ESP_ERR_INVALID_STATE;
    s_voltage_filter_ready = false;
    s_filtered_voltage_mv = 0U;

    battery_gpio_init();
    battery_adc_init();
    if (!s_adc_handle) {
        return;
    }

    battery_calibration_init();
    s_initialized = true;
    ESP_LOGI(TAG, "Battery service initialized");
}

void battery_service_start_periodic(uint32_t interval_ms)
{
    if (!s_initialized) {
        battery_service_init();
    }
    if (!s_initialized) {
        return;
    }

    if (interval_ms < BATTERY_MIN_INTERVAL_MS) {
        interval_ms = BATTERY_MIN_INTERVAL_MS;
    }
    s_interval_ms = interval_ms;

    if (s_task_running) {
        ESP_LOGW(TAG, "Battery sampling already running");
        return;
    }

    s_task_running = true;
    if (xTaskCreate(battery_task, "battery_task", BATTERY_TASK_STACK_SIZE, NULL, BATTERY_TASK_PRIORITY, &s_task_handle) != pdPASS) {
        s_task_running = false;
        ESP_LOGE(TAG, "Failed to create battery task");
        return;
    }

    ESP_LOGI(TAG, "Battery sampling started, interval=%lu ms", (unsigned long)s_interval_ms);
}

void battery_service_stop(void)
{
    if (!s_task_running) {
        return;
    }

    s_task_running = false;
    ESP_LOGI(TAG, "Battery sampling stop requested");
}

bool battery_get_latest_state(battery_state_t *out)
{
    if (!out || !s_cache_lock || !s_initialized) {
        return false;
    }

    if (xSemaphoreTake(s_cache_lock, pdMS_TO_TICKS(BATTERY_CACHE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    *out = s_state_cache;
    xSemaphoreGive(s_cache_lock);
    return true;
}
