/*
 * deep_sleep.c
 * Button ISR handling and deep sleep transition implementation.
 */
#include "power/deep_sleep.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUTTON_DEBOUNCE_MS 20

ESP_EVENT_DEFINE_BASE(DEEP_SLEEP_EVENT);

/**
 * @brief GPIO pin number used for deep sleep wake-up button.
 *
 * Set during deep_sleep_init() from the input parameter.
 * Used by ISR and sleep enable functions.
 */
static gpio_num_t s_button_gpio = GPIO_NUM_NC;

/**
 * @brief GPIO level considered as idle (button not pressed).
 *
 * Determined at runtime based on the actual button configuration.
 */
static int s_idle_level = 0;

/**
 * @brief GPIO level considered as pressed.
 *
 * Opposite of s_idle_level.
 */
static int s_press_level = 1;

/**
 * @brief Module initialization flag.
 *
 * Prevents multiple initializations of the deep sleep subsystem.
 */
static bool s_initialized = false;

/**
 * @brief Button pressed flag for ISR context.
 *
 * Set by ISR when button is pressed, cleared after event is posted.
 *
 * @note Volatile for thread-safe access from ISR and task context.
 */
static volatile bool s_pressed = false;

/**
 * @brief Timestamp of last button edge (for debouncing).
 *
 * Used in ISR to implement software debouncing.
 */
static volatile TickType_t s_last_edge_tick = 0;

static void deep_sleep_event_loop_init(void)
{
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
}

static void deep_sleep_gpio_isr_init(void)
{
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
}

static void deep_sleep_post_button_event_from_isr(void)
{
    BaseType_t task_woken = pdFALSE;
    esp_event_isr_post(DEEP_SLEEP_EVENT, DEEP_SLEEP_EVENT_BUTTON_CLICKED, NULL, 0, &task_woken);
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void deep_sleep_gpio_event_update(int level)
{
    if (!s_pressed && level == s_press_level) {
        s_pressed = true;
        return;
    }

    if (s_pressed && level == s_idle_level) {
        s_pressed = false;
        deep_sleep_post_button_event_from_isr();
    }
}

static void deep_sleep_button_isr(void *arg)
{
    TickType_t now = xTaskGetTickCountFromISR();
    TickType_t debounce_ticks = pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS);
    int level;

    (void)arg;

    if ((now - s_last_edge_tick) < debounce_ticks) {
        return;
    }

    s_last_edge_tick = now;
    level = gpio_get_level(s_button_gpio);
    deep_sleep_gpio_event_update(level);
}

void deep_sleep_init(gpio_num_t button_gpio)
{
    if (s_initialized) {
        return;
    }

    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << button_gpio),
        .mode = GPIO_MODE_INPUT,
        /* PROGRAM/BOOT button is typically active-low with pull-up. */
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    s_button_gpio = button_gpio;
    s_pressed = false;
    s_last_edge_tick = 0;

    ESP_ERROR_CHECK(gpio_config(&btn_conf));

    s_idle_level = gpio_get_level(button_gpio);
    s_press_level = s_idle_level ? 0 : 1;
    deep_sleep_event_loop_init();
    deep_sleep_gpio_isr_init();
    ESP_ERROR_CHECK(gpio_isr_handler_add(button_gpio, deep_sleep_button_isr, NULL));
    s_initialized = true;
}

void deep_sleep_enter_now(void)
{
    if (!s_initialized) {
        return;
    }

    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(s_button_gpio, s_press_level));
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_deep_sleep_start();
}
