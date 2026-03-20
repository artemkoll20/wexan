/*
 * deep_sleep.h
 * Sleep control interface using button events and EXT0 wakeup configuration.
 */
#ifndef DEEP_SLEEP_H
#define DEEP_SLEEP_H

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(DEEP_SLEEP_EVENT);

typedef enum {
    DEEP_SLEEP_EVENT_BUTTON_CLICKED = 1,
} deep_sleep_event_id_t;

/* Initializes button handling and wakeup behavior metadata. */
void deep_sleep_init(gpio_num_t button_gpio);

/* Arms EXT0 wakeup and enters deep sleep immediately. */
void deep_sleep_enter_now(void);

#endif /* DEEP_SLEEP_H */
