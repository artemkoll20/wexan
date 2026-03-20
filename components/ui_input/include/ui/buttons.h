/*
 * buttons.h
 * Two-button input module that posts click/hold events to the ESP event loop.
 */
#ifndef UI_BUTTONS_H
#define UI_BUTTONS_H

#include "driver/gpio.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(UI_BUTTON_EVENT);

typedef enum {
    UI_BUTTON_EVENT_LEFT_CLICK = 1,
    UI_BUTTON_EVENT_RIGHT_CLICK,
    UI_BUTTON_EVENT_LEFT_HOLD_3S,
    UI_BUTTON_EVENT_LEFT_HOLD_4S,
    UI_BUTTON_EVENT_LEFT_HOLD_5S,
    UI_BUTTON_EVENT_CONFIRM,
} ui_button_event_id_t;

/* Initializes button GPIO inputs and starts internal polling task once. */
void ui_buttons_init(gpio_num_t left_gpio, gpio_num_t right_gpio, gpio_num_t confirm_gpio);

#endif /* UI_BUTTONS_H */
