/*
 * runtime_screen.c
 * Page registry, screen transitions, and menu/page event dispatch.
 */
#include "runtime_state.h"
#include "runtime_scan_controller.h"
#include "app_core/app_event_handlers.h"
#include <string.h>
#include "esp_err.h"
#include "ui/monitor.h"

#define APP_RUNTIME_BUILTIN_PAGE_COUNT 2

static app_runtime_page_t s_custom_pages[APP_RUNTIME_MAX_CUSTOM_PAGES] = { 0 };
static size_t s_custom_page_count = 0;

/**
 * @brief Convert app runtime page icon to monitor menu icon.
 * @param icon Source icon from app_runtime_page_icon_t
 * @return Corresponding monitor_menu_icon_t value
 */
static monitor_menu_icon_t app_runtime_icon_to_monitor_icon(app_runtime_page_icon_t icon)
{
    switch (icon) {
    case APP_RUNTIME_PAGE_ICON_BLUETOOTH:
        return MONITOR_MENU_ICON_BLUETOOTH;
    case APP_RUNTIME_PAGE_ICON_CUSTOM:
        return MONITOR_MENU_ICON_CUSTOM;
    case APP_RUNTIME_PAGE_ICON_WIFI:
    default:
        return MONITOR_MENU_ICON_WIFI;
    }
}

/**
 * @brief Stop a periodic timer if it is running.
 * @param timer Timer handle to stop
 * @param running_flag Flag indicating if timer is currently running
 */
static void app_timer_stop(esp_timer_handle_t timer, bool *running_flag)
{
    if (!timer || !running_flag || !(*running_flag)) {
        return;
    }

    ESP_ERROR_CHECK(esp_timer_stop(timer));
    *running_flag = false;
}

/**
 * @brief Start a periodic timer if it is not already running.
 * @param timer Timer handle to start
 * @param running_flag Flag indicating if timer is currently running
 * @param period_ms Timer period in milliseconds
 */
static void app_timer_start_periodic(esp_timer_handle_t timer, bool *running_flag, uint32_t period_ms)
{
    if (!timer || !running_flag || *running_flag) {
        return;
    }

    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, (uint64_t)period_ms * 1000ULL));
    *running_flag = true;
}

/**
 * @brief Stop the menu navigation timer.
 */
static void app_stop_menu_timer(void)
{
    app_timer_stop(s_ui_state.menu_timer, &s_ui_state.menu_timer_running);
}

/**
 * @brief Stop the page tick timer.
 */
static void app_stop_page_timer(void)
{
    app_timer_stop(s_ui_state.page_timer, &s_ui_state.page_timer_running);
}

/**
 * @brief Start the page tick timer.
 */
static void app_start_page_timer(void)
{
    app_timer_start_periodic(s_ui_state.page_timer, &s_ui_state.page_timer_running, PAGE_TICK_MS);
}

/**
 * @brief Get scan controller configured for WiFi networks.
 * @return runtime_scan_controller_t populated with WiFi state pointers
 */
static runtime_scan_controller_t app_wifi_scan_controller(void)
{
    return (runtime_scan_controller_t){
        .current_snapshot = &s_ui_state.current_networks,
        .pending_snapshot = &s_ui_state.pending_networks,
        .snapshot_size = sizeof(s_ui_state.current_networks),
        .shown_version = &s_ui_state.shown_wifi_version,
        .shown_page = &s_ui_state.shown_wifi_page,
        .pending_version = &s_ui_state.pending_networks.version,
        .has_pending_snapshot = &s_ui_state.has_pending_networks,
    };
}

/**
 * @brief Get scan controller configured for Bluetooth devices.
 * @return runtime_scan_controller_t populated with Bluetooth state pointers
 */
static runtime_scan_controller_t app_bluetooth_scan_controller(void)
{
    return (runtime_scan_controller_t){
        .current_snapshot = &s_ui_state.current_bluetooth_devices,
        .pending_snapshot = &s_ui_state.pending_bluetooth_devices,
        .snapshot_size = sizeof(s_ui_state.current_bluetooth_devices),
        .shown_version = &s_ui_state.shown_bluetooth_version,
        .shown_page = &s_ui_state.shown_bluetooth_page,
        .pending_version = &s_ui_state.pending_bluetooth_devices.version,
        .has_pending_snapshot = &s_ui_state.has_pending_bluetooth_devices,
    };
}

/**
 * @brief Calculate page count for WiFi network snapshot.
 * @param current_snapshot Pointer to wifi_network_list_t
 * @return Number of pages required to display all networks
 */
static size_t app_wifi_snapshot_pages(const void *current_snapshot)
{
    const wifi_network_list_t *networks = (const wifi_network_list_t *)current_snapshot;
    if (!networks) {
        return 1U;
    }
    return wifi_page_count(networks->count);
}

/**
 * @brief Calculate page count for Bluetooth device snapshot.
 * @param current_snapshot Pointer to bluetooth_device_list_t
 * @return Number of pages required to display all devices
 */
static size_t app_bluetooth_snapshot_pages(const void *current_snapshot)
{
    const bluetooth_device_list_t *devices = (const bluetooth_device_list_t *)current_snapshot;
    if (!devices) {
        return 1U;
    }
    return bluetooth_page_count(devices->count);
}

static void app_scan_page_step(
    const runtime_scan_controller_t *controller,
    runtime_scan_page_count_fn_t page_count_fn,
    runtime_scan_render_fn_t render_fn,
    bool forward)
{
    size_t pages;
    size_t page;

    if (!controller || !page_count_fn || !render_fn) {
        return;
    }

    if (!controller->shown_version || !controller->shown_page) {
        return;
    }

    if (*controller->shown_version == 0U) {
        return;
    }

    pages = page_count_fn(controller->current_snapshot);
    if (pages <= 1U) {
        runtime_scan_controller_apply_pending(controller);
        render_fn();
        return;
    }

    page = *controller->shown_page;
    if (forward) {
        page = (page + 1U) % pages;
    } else {
        page = (page == 0U) ? (pages - 1U) : (page - 1U);
    }

    *controller->shown_page = page;
    if (page == 0U) {
        runtime_scan_controller_apply_pending(controller);
    }

    render_fn();
}

/**
 * @brief Render the current WiFi scan results page.
 */
static void ui_render_current_wifi_page(void)
{
    wifi_build_page_text(
        &s_ui_state.current_networks,
        s_ui_state.shown_wifi_page,
        s_ui_state.page_text,
        sizeof(s_ui_state.page_text));
    print(s_ui_state.page_text);
}

/**
 * @brief Reset WiFi UI state and clear displayed data.
 */
static void app_reset_wifi_ui_state(void)
{
    runtime_scan_controller_t controller = app_wifi_scan_controller();

    runtime_scan_controller_reset(&controller);
    memset(&s_ui_state.polled_networks, 0, sizeof(s_ui_state.polled_networks));
    memset(s_ui_state.page_text, 0, sizeof(s_ui_state.page_text));
}

/**
 * @brief Render the current Bluetooth scan results page.
 */
static void ui_render_current_bluetooth_page(void)
{
    bluetooth_build_page_text(
        &s_ui_state.current_bluetooth_devices,
        s_ui_state.shown_bluetooth_page,
        s_ui_state.page_text,
        sizeof(s_ui_state.page_text));
    print(s_ui_state.page_text);
}

/**
 * @brief Reset Bluetooth UI state and clear displayed data.
 */
static void app_reset_bluetooth_ui_state(void)
{
    runtime_scan_controller_t controller = app_bluetooth_scan_controller();

    runtime_scan_controller_reset(&controller);
    memset(&s_ui_state.polled_bluetooth_devices, 0, sizeof(s_ui_state.polled_bluetooth_devices));
    memset(s_ui_state.page_text, 0, sizeof(s_ui_state.page_text));
}

/**
 * @brief WiFi page enter handler - starts WiFi scanning.
 * @param ctx Unused context pointer
 */
static void app_builtin_wifi_on_enter(void *ctx)
{
    (void)ctx;
    app_reset_wifi_ui_state();
    print("WiFi scanning...");
    wifi_scan_start_periodic(WIFI_SCAN_INTERVAL_MS);
}

/**
 * @brief WiFi page leave handler - stops WiFi scanning.
 * @param ctx Unused context pointer
 */
static void app_builtin_wifi_on_leave(void *ctx)
{
    (void)ctx;
    wifi_scan_stop();
    app_reset_wifi_ui_state();
}

static void app_builtin_wifi_on_left_click(void *ctx)
{
    runtime_scan_controller_t controller = app_wifi_scan_controller();

    (void)ctx;
    app_scan_page_step(&controller, app_wifi_snapshot_pages, ui_render_current_wifi_page, false);
}

static void app_builtin_wifi_on_right_click(void *ctx)
{
    runtime_scan_controller_t controller = app_wifi_scan_controller();

    (void)ctx;
    app_scan_page_step(&controller, app_wifi_snapshot_pages, ui_render_current_wifi_page, true);
}

/**
 * @brief WiFi scan updated handler - processes new scan results.
 * @param ctx Unused context pointer
 */
static void app_builtin_wifi_on_scan_updated(void *ctx)
{
    runtime_scan_controller_t controller = app_wifi_scan_controller();
    size_t pages;

    (void)ctx;

    if (!wifi_get_latest_networks(&s_ui_state.polled_networks)) {
        return;
    }

    runtime_scan_controller_process_polled(
        &controller,
        &s_ui_state.polled_networks,
        s_ui_state.polled_networks.version,
        ui_render_current_wifi_page);

    pages = wifi_page_count(s_ui_state.polled_networks.count);
    if (s_ui_state.has_pending_networks
        && (pages <= 1U || s_ui_state.shown_wifi_page == 0U)) {
        runtime_scan_controller_apply_pending(&controller);
        ui_render_current_wifi_page();
    }
}

/**
 * @brief BLE page enter handler - starts BLE scanning.
 * @param ctx Unused context pointer
 */
static void app_builtin_ble_on_enter(void *ctx)
{
    (void)ctx;
    app_reset_bluetooth_ui_state();
    print("BLE scanning...");
    bluetooth_scan_start_periodic(BLUETOOTH_SCAN_INTERVAL_MS);
}

/**
 * @brief BLE page leave handler - stops BLE scanning.
 * @param ctx Unused context pointer
 */
static void app_builtin_ble_on_leave(void *ctx)
{
    (void)ctx;
    bluetooth_scan_stop();
    app_reset_bluetooth_ui_state();
}

static void app_builtin_ble_on_left_click(void *ctx)
{
    runtime_scan_controller_t controller = app_bluetooth_scan_controller();

    (void)ctx;
    app_scan_page_step(&controller, app_bluetooth_snapshot_pages, ui_render_current_bluetooth_page, false);
}

static void app_builtin_ble_on_right_click(void *ctx)
{
    runtime_scan_controller_t controller = app_bluetooth_scan_controller();

    (void)ctx;
    app_scan_page_step(&controller, app_bluetooth_snapshot_pages, ui_render_current_bluetooth_page, true);
}

/**
 * @brief BLE scan updated handler - processes new scan results.
 * @param ctx Unused context pointer
 */
static void app_builtin_ble_on_scan_updated(void *ctx)
{
    runtime_scan_controller_t controller = app_bluetooth_scan_controller();
    size_t pages;

    (void)ctx;

    if (!bluetooth_get_latest_devices(&s_ui_state.polled_bluetooth_devices)) {
        return;
    }

    runtime_scan_controller_process_polled(
        &controller,
        &s_ui_state.polled_bluetooth_devices,
        s_ui_state.polled_bluetooth_devices.version,
        ui_render_current_bluetooth_page);

    pages = bluetooth_page_count(s_ui_state.polled_bluetooth_devices.count);
    if (s_ui_state.has_pending_bluetooth_devices
        && (pages <= 1U || s_ui_state.shown_bluetooth_page == 0U)) {
        runtime_scan_controller_apply_pending(&controller);
        ui_render_current_bluetooth_page();
    }
}

static const app_runtime_page_t s_builtin_pages[APP_RUNTIME_BUILTIN_PAGE_COUNT] = {
    {
        .title = "WiFi scan",
        .icon = APP_RUNTIME_PAGE_ICON_WIFI,
        .on_enter = app_builtin_wifi_on_enter,
        .on_leave = app_builtin_wifi_on_leave,
        .on_left_click = app_builtin_wifi_on_left_click,
        .on_right_click = app_builtin_wifi_on_right_click,
        .on_tick = NULL,
        .on_wifi_scan_updated = app_builtin_wifi_on_scan_updated,
        .on_bluetooth_scan_updated = NULL,
        .ctx = NULL,
    },
    {
        .title = "BLE scan",
        .icon = APP_RUNTIME_PAGE_ICON_BLUETOOTH,
        .on_enter = app_builtin_ble_on_enter,
        .on_leave = app_builtin_ble_on_leave,
        .on_left_click = app_builtin_ble_on_left_click,
        .on_right_click = app_builtin_ble_on_right_click,
        .on_tick = NULL,
        .on_wifi_scan_updated = NULL,
        .on_bluetooth_scan_updated = app_builtin_ble_on_scan_updated,
        .ctx = NULL,
    },
};

size_t app_runtime_page_count(void)
{
    return APP_RUNTIME_BUILTIN_PAGE_COUNT + s_custom_page_count;
}

esp_err_t app_runtime_register_page(const app_runtime_page_t *page)
{
    if (!page || !page->title || page->title[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (page->icon != APP_RUNTIME_PAGE_ICON_WIFI
        && page->icon != APP_RUNTIME_PAGE_ICON_BLUETOOTH
        && page->icon != APP_RUNTIME_PAGE_ICON_CUSTOM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (page->icon == APP_RUNTIME_PAGE_ICON_CUSTOM) {
        if (!page->custom_icon.data || page->custom_icon.width == 0U || page->custom_icon.height == 0U) {
            return ESP_ERR_INVALID_ARG;
        }

        if (page->custom_icon.width > APP_RUNTIME_MENU_ICON_MAX_WIDTH || page->custom_icon.height > APP_RUNTIME_MENU_ICON_MAX_HEIGHT) {
            return ESP_ERR_INVALID_SIZE;
        }
    }

    if (s_custom_page_count >= APP_RUNTIME_MAX_CUSTOM_PAGES) {
        return ESP_ERR_NO_MEM;
    }

    s_custom_pages[s_custom_page_count] = *page;
    s_custom_page_count++;
    return ESP_OK;
}

/**
 * @brief Get page by index (builtin or custom).
 * @param page_index Index of page to retrieve
 * @return Pointer to page structure, or NULL if not found
 */
static const app_runtime_page_t *app_get_page_by_index(size_t page_index)
{
    if (page_index < APP_RUNTIME_BUILTIN_PAGE_COUNT) {
        return &s_builtin_pages[page_index];
    }

    page_index -= APP_RUNTIME_BUILTIN_PAGE_COUNT;
    if (page_index >= s_custom_page_count) {
        return NULL;
    }

    return &s_custom_pages[page_index];
}

/**
 * @brief Get currently active page (only valid in APP_SCREEN_PAGE state).
 * @return Pointer to active page structure, or NULL if not in page screen
 */
static const app_runtime_page_t *app_get_active_page(void)
{
    if (s_ui_state.screen != APP_SCREEN_PAGE) {
        return NULL;
    }

    return app_get_page_by_index(s_ui_state.active_page_index);
}

/**
 * @brief Render the menu view with current selection.
 */
static void app_render_menu(void)
{
    monitor_menu_view_t view;
    const app_runtime_page_t *page = app_get_page_by_index(s_ui_state.selected_page_index);

    view.title = page ? page->title : "No pages";
    view.countdown_s = s_ui_state.menu_countdown_s;
    view.countdown_phase = s_ui_state.menu_countdown_phase;
    view.icon = page ? app_runtime_icon_to_monitor_icon(page->icon) : MONITOR_MENU_ICON_WIFI;
    view.custom_icon.data = page ? page->custom_icon.data : NULL;
    view.custom_icon.width = page ? page->custom_icon.width : 0U;
    view.custom_icon.height = page ? page->custom_icon.height : 0U;
    monitor_show_menu(&view);
}

/**
 * @brief Leave current screen - stops timers and calls on_leave callbacks.
 */
static void app_leave_current_screen(void)
{
    const app_runtime_page_t *active_page;

    switch (s_ui_state.screen) {
    case APP_SCREEN_MENU:
        app_stop_menu_timer();
        break;
    case APP_SCREEN_PAGE:
        app_stop_page_timer();
        active_page = app_get_page_by_index(s_ui_state.active_page_index);
        if (active_page && active_page->on_leave) {
            active_page->on_leave(active_page->ctx);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief Enter a page by index - leaves current screen and enters new page.
 * @param page_index Index of page to enter
 */
static void app_enter_page_by_index(size_t page_index)
{
    const app_runtime_page_t *page = app_get_page_by_index(page_index);
    if (!page) {
        print("Page not found");
        return;
    }

    app_leave_current_screen();
    s_ui_state.screen = APP_SCREEN_PAGE;
    s_ui_state.active_page_index = page_index;
    app_start_page_timer();
    if (page->on_enter) {
        page->on_enter(page->ctx);
    }
}

void app_enter_menu_screen(void)
{
    size_t pages = app_runtime_page_count();

    app_leave_current_screen();
    s_ui_state.screen = APP_SCREEN_MENU;

    if (pages > 0U && s_ui_state.selected_page_index >= pages) {
        s_ui_state.selected_page_index = 0;
    }

    s_ui_state.menu_countdown_s = 0U;
    s_ui_state.menu_countdown_phase = 0U;
    app_render_menu();
}

void app_enter_selected_mode(void)
{
    if (app_runtime_page_count() == 0U) {
        print("No pages\nregistered");
        return;
    }

    app_enter_page_by_index(s_ui_state.selected_page_index);
}

void app_menu_select_prev(void)
{
    size_t pages = app_runtime_page_count();
    if (pages == 0U) {
        return;
    }

    if (s_ui_state.selected_page_index == 0U) {
        s_ui_state.selected_page_index = pages - 1U;
    } else {
        s_ui_state.selected_page_index--;
    }
}

void app_menu_select_next(void)
{
    size_t pages = app_runtime_page_count();
    if (pages == 0U) {
        return;
    }

    s_ui_state.selected_page_index = (s_ui_state.selected_page_index + 1U) % pages;
}

void app_menu_reset_countdown(void)
{
    s_ui_state.menu_countdown_s = 0U;
    s_ui_state.menu_countdown_phase = 0U;
    if (s_ui_state.screen == APP_SCREEN_MENU) {
        app_render_menu();
    }
}

void app_handle_menu_tick(void)
{
    if (s_ui_state.screen != APP_SCREEN_MENU) {
        return;
    }
    if (s_ui_state.menu_countdown_s == 0U) {
        return;
    }

    s_ui_state.menu_countdown_phase = (uint8_t)((s_ui_state.menu_countdown_phase + 1U) % MENU_COUNTDOWN_ANIMATION_STEPS);
    if (s_ui_state.menu_countdown_phase == 0U) {
        if (s_ui_state.menu_countdown_s > 0U) {
            s_ui_state.menu_countdown_s--;
        }
        // Countdown no longer triggers auto-select - selection is done by button press
    }

    app_render_menu();
}

void app_handle_active_page_tick(void)
{
    const app_runtime_page_t *page = app_get_active_page();
    if (page && page->on_tick) {
        page->on_tick(page->ctx);
    }
}

void app_handle_active_page_left_click(void)
{
    const app_runtime_page_t *page = app_get_active_page();
    if (page && page->on_left_click) {
        page->on_left_click(page->ctx);
    }
}

void app_handle_active_page_right_click(void)
{
    const app_runtime_page_t *page = app_get_active_page();
    if (page && page->on_right_click) {
        page->on_right_click(page->ctx);
    }
}

void app_handle_active_page_wifi_updated(void)
{
    const app_runtime_page_t *page = app_get_active_page();
    if (page && page->on_wifi_scan_updated) {
        page->on_wifi_scan_updated(page->ctx);
    }
}

void app_handle_active_page_bluetooth_updated(void)
{
    const app_runtime_page_t *page = app_get_active_page();
    if (page && page->on_bluetooth_scan_updated) {
        page->on_bluetooth_scan_updated(page->ctx);
    }
}

void app_handle_menu_confirm(void)
{
    if (s_ui_state.screen == APP_SCREEN_MENU) {
        app_enter_selected_mode();
    }
}
