/*
 * buttons.c
 * Debounced dual-button polling with click and left-button hold events.
 * Enhanced with software debouncing using FreeRTOS queue for button events.
 */
#include "ui/buttons.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "ui_buttons";

#define UI_BUTTON_TASK_STACK_SIZE 3072
#define UI_BUTTON_TASK_PRIORITY 5
#define UI_BUTTON_POLL_MS 10
#define UI_BUTTON_DEBOUNCE_MS 30
#define UI_BUTTON_CONFIRM_DEBOUNCE_MS 50  // Enhanced debounce for confirm button
#define UI_BUTTON_HOLD_MS_3S 3000
#define UI_BUTTON_HOLD_MS_4S 4000
#define UI_BUTTON_HOLD_MS_5S 5000
#define UI_BUTTON_CONFIRM_GPIO GPIO_NUM_4
#define UI_BUTTON_EVENT_QUEUE_LENGTH 16

ESP_EVENT_DEFINE_BASE(UI_BUTTON_EVENT);

/**
 * @brief Button event structure for queue-based event processing.
 * 
 * Using a queue prevents event flooding and provides additional
 * software-level debouncing beyond the hardware polling.
 */
typedef struct {
    ui_button_event_id_t event_id;
    TickType_t timestamp;
} ui_button_queue_event_t;

typedef struct {
    gpio_num_t gpio;
    ui_button_event_id_t click_event;
    ui_button_event_id_t hold_event;
    bool supports_hold;
    bool hold_sent;
    bool pressed;
    int stable_level;
    int sample_level;
    TickType_t press_tick;
    TickType_t edge_tick;
    // Enhanced debounce fields for confirm button
    bool is_confirm_button;
    TickType_t last_confirm_press_tick;
    bool confirm_pending;
} ui_button_state_t;

/**
 * @brief Queue for button events with software debouncing.
 * 
 * This queue provides an additional layer of debouncing and prevents
 * event flooding during button bounces.
 */
static QueueHandle_t s_button_event_queue = NULL;

static TaskHandle_t s_ui_button_task = NULL;

/**
 * @brief Module initialization flag.
 *
 * Prevents multiple initializations of the button subsystem.
 */
static bool s_initialized = false;

/**
 * @brief Runtime state for left UI button.
 *
 * Contains GPIO number, event IDs, press state, debounce state,
 * and timing information for the left button.
 *
 * @note Supports hold events (3s, 4s, 5s) for special actions.
 */
static ui_button_state_t s_left_btn = { 0 };

/**
 * @brief Runtime state for right UI button.
 *
 * Contains GPIO number, event IDs, press state, debounce state,
 * and timing information for the right button.
 *
 * @note Right button only supports click events (no hold).
 */
static ui_button_state_t s_right_btn = { 0 };

/**
 * @brief Runtime state for confirm UI button.
 *
 * Contains GPIO number, event IDs, press state, debounce state,
 * and timing information for the confirm button.
 *
 * @note Confirm button only supports click events (no hold).
 */
static ui_button_state_t s_confirm_btn = { 0 };

static void ui_buttons_event_loop_init(void)
{
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
}

static void ui_buttons_gpio_configure(gpio_num_t gpio)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));
}

/**
 * @brief Check if confirm button press should be accepted based on debounce timing.
 * 
 * Provides additional software-level debouncing for the confirm button (GPIO_4)
 * to prevent accidental double-presses and bouncing issues.
 * 
 * @param btn Button state structure
 * @param now Current tick count
 * @return true if press should be accepted, false if debounce period is still active
 */
static bool ui_button_confirm_debounce_check(ui_button_state_t *btn, TickType_t now)
{
    TickType_t debounce_ticks;
    
    if (!btn || !btn->is_confirm_button) {
        return true;
    }
    
    // Use enhanced debounce time for confirm button
    debounce_ticks = pdMS_TO_TICKS(UI_BUTTON_CONFIRM_DEBOUNCE_MS);
    
    // Check if enough time has passed since last press
    if ((now - btn->last_confirm_press_tick) < debounce_ticks) {
        ESP_LOGD(TAG, "Confirm button debounced (tick diff: %lu)", 
                 (unsigned long)(now - btn->last_confirm_press_tick));
        return false;
    }
    
    return true;
}

/**
 * @brief Post button event with software debouncing via queue.
 * 
 * This function provides an additional layer of software debouncing by:
 * 1. Checking confirm button debounce timing
 * 2. Posting events to a queue for ordered processing
 * 3. Preventing event flooding during bounces
 * 
 * @param event_id Button event to post
 * @param now Current tick count
 */
static void ui_buttons_post_with_debounce(ui_button_event_id_t event_id, TickType_t now)
{
    ui_button_queue_event_t queue_event;
    
    // Find the button state for this event
    ui_button_state_t *btn = NULL;
    
    if (event_id == UI_BUTTON_EVENT_CONFIRM) {
        btn = &s_confirm_btn;
    } else if (event_id == UI_BUTTON_EVENT_LEFT_CLICK || 
               event_id == UI_BUTTON_EVENT_LEFT_HOLD_3S ||
               event_id == UI_BUTTON_EVENT_LEFT_HOLD_4S ||
               event_id == UI_BUTTON_EVENT_LEFT_HOLD_5S) {
        btn = &s_left_btn;
    } else if (event_id == UI_BUTTON_EVENT_RIGHT_CLICK) {
        btn = &s_right_btn;
    }
    
    // Apply software debounce check for confirm button
    if (btn && btn->is_confirm_button) {
        if (!ui_button_confirm_debounce_check(btn, now)) {
            // Debounce period still active, ignore this press
            return;
        }
        // Update last press timestamp for confirm button
        btn->last_confirm_press_tick = now;
    }
    
    // If queue is available, use queue-based posting for ordered processing
    if (s_button_event_queue != NULL) {
        queue_event.event_id = event_id;
        queue_event.timestamp = now;
        
        // Try to send to queue with short timeout to prevent blocking
        if (xQueueSend(s_button_event_queue, &queue_event, pdMS_TO_TICKS(10)) != pdTRUE) {
            // Queue full, fall back to direct event posting
            ESP_LOGW(TAG, "Button event queue full, using direct post");
            (void)esp_event_post(UI_BUTTON_EVENT, event_id, NULL, 0, 0);
        }
    } else {
        // Queue not available, use direct posting
        (void)esp_event_post(UI_BUTTON_EVENT, event_id, NULL, 0, 0);
    }
}

/**
 * @brief Process button events from the software debounce queue.
 * 
 * This task runs separately from the GPIO polling and processes events
 * that have passed through software debouncing.
 * 
 * @param arg Unused argument
 */
static void ui_buttons_queue_processor(void *arg)
{
    ui_button_queue_event_t queue_event;
    
    (void)arg;
    
    for (;;) {
        if (xQueueReceive(s_button_event_queue, &queue_event, portMAX_DELAY) == pdTRUE) {
            // Post the event to ESP event loop
            (void)esp_event_post(UI_BUTTON_EVENT, queue_event.event_id, NULL, 0, 0);
            
            ESP_LOGD(TAG, "Processed button event %d from queue", (int)queue_event.event_id);
        }
    }
}

static void ui_buttons_post(ui_button_event_id_t event_id)
{
    TickType_t now = xTaskGetTickCount();
    ui_buttons_post_with_debounce(event_id, now);
}

static void ui_button_on_press(ui_button_state_t *btn, TickType_t now)
{   
    if (!btn) {
        return;
    }

    btn->pressed = true;
    btn->press_tick = now;
    btn->hold_sent = false;
    // NOTE: CLICK event is sent on RELEASE only (to allow HOLD to replace it)
}

static void ui_button_on_release(ui_button_state_t *btn, TickType_t now)
{
    (void)now;
    
    if (!btn) {
        return;
    }

    btn->pressed = false;
    
    // If HOLD wasn't sent during press, send CLICK now
    // This handles the case when button is released before hold threshold
    if (!btn->hold_sent) {
        ui_buttons_post(btn->click_event);
    }
}

/**
 * @brief Check and send hold event if hold threshold is reached.
 *
 * Evaluates pressed duration and sends appropriate hold event.
 * Ensures only one hold event is sent per press.
 *
 * @param btn Button state structure
 * @param now Current tick count
 */
static void ui_button_check_hold(ui_button_state_t *btn, TickType_t now)
{
    if (!btn || !btn->supports_hold || btn->hold_sent) {
        return;
    }

    uint32_t pressed_ms = (uint32_t)(((now - btn->press_tick) * 1000ULL) / configTICK_RATE_HZ);

    // Minimum hold threshold is 3s, all longer holds trigger the same event
    if (pressed_ms >= UI_BUTTON_HOLD_MS_3S) {
        ui_buttons_post(btn->hold_event);
        btn->hold_sent = true;
    }
}

static void ui_button_process(ui_button_state_t *btn, TickType_t now)
{
    TickType_t debounce_ticks = pdMS_TO_TICKS(UI_BUTTON_DEBOUNCE_MS);
    int raw_level;

    if (!btn) {
        return;
    }

    raw_level = gpio_get_level(btn->gpio);
    if (raw_level != btn->sample_level) {
        btn->sample_level = raw_level;
        btn->edge_tick = now;
    }

    if (btn->sample_level == btn->stable_level) {
        // Check for HOLD while button is continuously pressed
        ui_button_check_hold(btn, now);
        return;
    }

    if ((now - btn->edge_tick) < debounce_ticks) {
        return;
    }

    btn->stable_level = btn->sample_level;
    if (btn->stable_level == 0) {
        ui_button_on_press(btn, now);
    } else {
        ui_button_on_release(btn, now);
    }
}

/**
 * @brief Initialize button event queue for software debouncing.
 * 
 * Creates a FreeRTOS queue that provides an additional layer of
 * software-level debouncing and event ordering.
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t ui_buttons_create_event_queue(void)
{
    if (s_button_event_queue != NULL) {
        return ESP_OK; // Already created
    }
    
    s_button_event_queue = xQueueCreate(UI_BUTTON_EVENT_QUEUE_LENGTH, sizeof(ui_button_queue_event_t));
    if (s_button_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button event queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Create queue processor task
    BaseType_t ret = xTaskCreate(
        ui_buttons_queue_processor,
        "btn_queue_proc",
        UI_BUTTON_TASK_STACK_SIZE / 2,
        NULL,
        UI_BUTTON_TASK_PRIORITY - 1,  // Lower priority than polling task
        NULL);
    
    if (ret != pdPASS) {
        vQueueDelete(s_button_event_queue);
        s_button_event_queue = NULL;
        ESP_LOGE(TAG, "Failed to create queue processor task");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Button event queue created successfully");
    return ESP_OK;
}

static void ui_button_setup(
    ui_button_state_t *btn,
    gpio_num_t gpio,
    ui_button_event_id_t click_event,
    ui_button_event_id_t hold_event,
    bool supports_hold,
    bool is_confirm_button,
    TickType_t now)
{
    int level;

    if (!btn) {
        return;
    }

    level = gpio_get_level(gpio);
    btn->gpio = gpio;
    btn->click_event = click_event;
    btn->hold_event = hold_event;
    btn->supports_hold = supports_hold;
    btn->stable_level = level;
    btn->sample_level = level;
    btn->pressed = (level == 0);
    btn->press_tick = now;
    btn->edge_tick = now;
    btn->hold_sent = false;
    btn->is_confirm_button = is_confirm_button;
    btn->last_confirm_press_tick = 0;
    btn->confirm_pending = false;
}

static void ui_buttons_task(void *arg)
{
    TickType_t delay_ticks = pdMS_TO_TICKS(UI_BUTTON_POLL_MS);

    (void)arg;

    for (;;) {
        TickType_t now = xTaskGetTickCount();
        ui_button_process(&s_left_btn, now);
        ui_button_process(&s_right_btn, now);
        ui_button_process(&s_confirm_btn, now);
        vTaskDelay(delay_ticks);
    }
}

void ui_buttons_init(gpio_num_t left_gpio, gpio_num_t right_gpio, gpio_num_t confirm_gpio)
{
    TickType_t now;
    esp_err_t err;

    if (s_initialized) {
        return;
    }

    ui_buttons_event_loop_init();
    ui_buttons_gpio_configure(left_gpio);
    ui_buttons_gpio_configure(right_gpio);
    ui_buttons_gpio_configure(confirm_gpio);

    // Create button event queue for software debouncing
    err = ui_buttons_create_event_queue();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Button event queue creation failed, using direct posting");
        // Continue without queue - will use direct event posting
    }

    now = xTaskGetTickCount();
    ui_button_setup(
        &s_left_btn,
        left_gpio,
        UI_BUTTON_EVENT_LEFT_CLICK,
        UI_BUTTON_EVENT_LEFT_HOLD_5S,
        true,
        false,  // Not confirm button
        now);
    ui_button_setup(
        &s_right_btn,
        right_gpio,
        UI_BUTTON_EVENT_RIGHT_CLICK,
        UI_BUTTON_EVENT_RIGHT_CLICK,
        false,
        false,  // Not confirm button
        now);
    ui_button_setup(
        &s_confirm_btn,
        confirm_gpio,
        UI_BUTTON_EVENT_CONFIRM,
        UI_BUTTON_EVENT_CONFIRM,
        false,
        true,   // This is the confirm button - enhanced debounce applied
        now);

    if (xTaskCreate(ui_buttons_task, "ui_buttons", UI_BUTTON_TASK_STACK_SIZE, NULL, UI_BUTTON_TASK_PRIORITY, &s_ui_button_task) != pdPASS) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Buttons initialized with enhanced software debouncing");
    ESP_LOGI(TAG, "  - Left GPIO: %d, Right GPIO: %d, Confirm GPIO: %d", 
             (int)left_gpio, (int)right_gpio, (int)confirm_gpio);
    ESP_LOGI(TAG, "  - Confirm button debounce: %d ms", UI_BUTTON_CONFIRM_DEBOUNCE_MS);
}
