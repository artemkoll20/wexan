#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

/*
 * runtime.h
 * Runtime orchestration for menu navigation and mode-specific UI behavior.
 */
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define APP_RUNTIME_MAX_CUSTOM_PAGES 8
#define APP_RUNTIME_PAGE_TICK_MS 2200
#define APP_RUNTIME_MENU_ICON_MAX_WIDTH 34
#define APP_RUNTIME_MENU_ICON_MAX_HEIGHT 22
#define APP_RUNTIME_ICON_BITMAP_STRIDE(width) (((width) + 7U) / 8U)

typedef enum {
    APP_RUNTIME_PAGE_ICON_WIFI = 0,
    APP_RUNTIME_PAGE_ICON_BLUETOOTH,
    APP_RUNTIME_PAGE_ICON_CUSTOM,
} app_runtime_page_icon_t;

typedef struct {
    const uint8_t *data;
    uint8_t width;
    uint8_t height;
} app_runtime_page_icon_bitmap_t;

typedef struct {
    const char *title;
    app_runtime_page_icon_t icon;
    /* Used when icon == APP_RUNTIME_PAGE_ICON_CUSTOM (1-bit rows, MSB-first). */
    app_runtime_page_icon_bitmap_t custom_icon;
    /* Called when page becomes active from menu. */
    void (*on_enter)(void *ctx);
    /* Called when leaving the page (for menu/back navigation). */
    void (*on_leave)(void *ctx);
    /* Optional button handlers while page is active. */
    void (*on_left_click)(void *ctx);
    void (*on_right_click)(void *ctx);
    /* Optional periodic callback (period: APP_RUNTIME_PAGE_TICK_MS). */
    void (*on_tick)(void *ctx);
    /* Optional callback on WIFI_SCAN_EVENT_UPDATED. */
    void (*on_wifi_scan_updated)(void *ctx);
    /* Optional callback on BLUETOOTH_SCAN_EVENT_UPDATED. */
    void (*on_bluetooth_scan_updated)(void *ctx);
    void *ctx;
} app_runtime_page_t;

/* Starts runtime services after display initialization. */
void app_runtime_start(void);

/* Initializes all application services (wifi, bluetooth, monitor). Returns error on failure. */
esp_err_t app_services_init(void);

/* Registers a custom page that appears after built-in pages in the runtime menu. */
esp_err_t app_runtime_register_page(const app_runtime_page_t *page);

/* Returns total number of pages (built-in + custom). */
size_t app_runtime_page_count(void);

#endif /* APP_RUNTIME_H */
