/*
 * battery.h
 * Periodic battery voltage sampling and cached battery state snapshots.
 */
#ifndef POWER_BATTERY_H
#define POWER_BATTERY_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"

typedef struct {
    uint16_t voltage_mv;
    uint8_t percent;
    bool available;
    bool low;
    uint32_t version;
    esp_err_t last_error;
} battery_state_t;

ESP_EVENT_DECLARE_BASE(BATTERY_EVENT);

typedef enum {
    BATTERY_EVENT_UPDATED = 1,
} battery_event_id_t;

/* Initializes battery ADC sampling resources once. */
void battery_service_init(void);

/* Starts (or reconfigures) periodic battery measurements. */
void battery_service_start_periodic(uint32_t interval_ms);

/* Stops periodic battery measurements. */
void battery_service_stop(void);

/* Copies the latest battery state into out. Returns false if unavailable. */
bool battery_get_latest_state(battery_state_t *out);

#endif /* POWER_BATTERY_H */
