/*
 * runtime_worker.c
 * Command queue, worker task, and timer creation for runtime orchestration.
 */
#include "runtime_state.h"
#include "esp_err.h"
#include "esp_log.h"

app_ui_state_t s_ui_state = { 0 };
QueueHandle_t s_runtime_cmd_queue = NULL;
TaskHandle_t s_runtime_task = NULL;

static const char *TAG = "app_runtime";

void app_runtime_post_cmd(app_runtime_cmd_t cmd)
{
    if (!s_runtime_cmd_queue) {
        return;
    }

    if (xQueueSend(s_runtime_cmd_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Command %d dropped: queue full", (int)cmd);
    }
}

static void menu_timer_callback(void *arg)
{
    (void)arg;
    app_runtime_post_cmd(APP_RUNTIME_CMD_MENU_TICK);
}

static void page_timer_callback(void *arg)
{
    (void)arg;
    app_runtime_post_cmd(APP_RUNTIME_CMD_PAGE_TICK);
}

static void app_runtime_task(void *arg)
{
    app_runtime_cmd_t cmd;

    (void)arg;

    for (;;) {
        if (xQueueReceive(s_runtime_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            app_runtime_process_cmd(cmd);
        }
    }
}

static void app_create_runtime_queue_and_task(void)
{
    s_runtime_cmd_queue = xQueueCreate(APP_RUNTIME_QUEUE_LENGTH, sizeof(app_runtime_cmd_t));
    ESP_ERROR_CHECK(s_runtime_cmd_queue ? ESP_OK : ESP_ERR_NO_MEM);

    if (xTaskCreate(app_runtime_task, "app_runtime", APP_RUNTIME_TASK_STACK_SIZE, NULL, APP_RUNTIME_TASK_PRIORITY, &s_runtime_task) != pdPASS) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}

static void app_create_runtime_timers(void)
{
    const esp_timer_create_args_t menu_timer_args = {
        .callback = menu_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "menu_tick",
    };
    const esp_timer_create_args_t page_timer_args = {
        .callback = page_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "page_tick",
    };

    ESP_ERROR_CHECK(esp_timer_create(&menu_timer_args, &s_ui_state.menu_timer));
    ESP_ERROR_CHECK(esp_timer_create(&page_timer_args, &s_ui_state.page_timer));

    s_ui_state.menu_timer_running = false;
    s_ui_state.page_timer_running = false;
}

void app_runtime_worker_init(void)
{
    app_create_runtime_queue_and_task();
    app_create_runtime_timers();
}
