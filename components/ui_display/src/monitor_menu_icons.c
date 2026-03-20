/*
 * monitor_menu_icons.c
 * Pixel menu icon rendering and icon-state updates.
 */
#include "monitor_internal.h"
#include "monitor_menu_internal.h"
#include "monitor_menu_icon_factory.h"

typedef struct {
    const uint8_t *data;
    uint8_t width;
    uint8_t height;
} monitor_custom_icon_cache_t;

static monitor_custom_icon_cache_t s_custom_icon_cache = { 0 };

static void monitor_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (!obj) {
        return;
    }

    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void monitor_reset_custom_icon_cache(void)
{
    s_custom_icon_cache.data = NULL;
    s_custom_icon_cache.width = 0;
    s_custom_icon_cache.height = 0;
}

static bool monitor_custom_icon_is_valid(const monitor_menu_icon_bitmap_t *icon)
{
    return icon && icon->data && icon->width > 0U && icon->height > 0U;
}

static bool monitor_custom_icon_changed(const monitor_menu_icon_bitmap_t *icon)
{
    if (!monitor_custom_icon_is_valid(icon)) {
        return s_custom_icon_cache.data != NULL || s_custom_icon_cache.width != 0U || s_custom_icon_cache.height != 0U;
    }

    return s_custom_icon_cache.data != icon->data
        || s_custom_icon_cache.width != icon->width
        || s_custom_icon_cache.height != icon->height;
}

static void monitor_render_custom_icon(const monitor_menu_icon_bitmap_t *icon)
{
    if (!s_monitor_ui.menu_custom_icon) {
        return;
    }

    if (!monitor_custom_icon_is_valid(icon)) {
        monitor_menu_icon_factory_clear(s_monitor_ui.menu_custom_icon);
        monitor_reset_custom_icon_cache();
        return;
    }

    if (!monitor_custom_icon_changed(icon)) {
        return;
    }

    monitor_menu_icon_factory_render_bitmap(s_monitor_ui.menu_custom_icon, icon->data, icon->width, icon->height);

    s_custom_icon_cache.data = icon->data;
    s_custom_icon_cache.width = icon->width;
    s_custom_icon_cache.height = icon->height;
}

static monitor_menu_icon_t monitor_resolve_icon_to_show(const monitor_menu_view_t *view)
{
    if (!view) {
        return MONITOR_MENU_ICON_WIFI;
    }

    if (view->icon == MONITOR_MENU_ICON_BLUETOOTH) {
        return MONITOR_MENU_ICON_BLUETOOTH;
    }

    if (view->icon == MONITOR_MENU_ICON_CUSTOM && monitor_custom_icon_is_valid(&view->custom_icon)) {
        return MONITOR_MENU_ICON_CUSTOM;
    }

    return MONITOR_MENU_ICON_WIFI;
}

static void monitor_apply_menu_icon_visibility(monitor_menu_icon_t icon)
{
    monitor_set_hidden(s_monitor_ui.menu_wifi_icon, icon != MONITOR_MENU_ICON_WIFI);
    monitor_set_hidden(s_monitor_ui.menu_bt_icon, icon != MONITOR_MENU_ICON_BLUETOOTH);
    monitor_set_hidden(s_monitor_ui.menu_custom_icon, icon != MONITOR_MENU_ICON_CUSTOM);
}

void monitor_menu_icons_reset_cache(void)
{
    monitor_reset_custom_icon_cache();
}

void monitor_menu_icons_create(lv_obj_t *root)
{
    s_monitor_ui.menu_wifi_icon = monitor_menu_icon_factory_create_wifi(root);
    s_monitor_ui.menu_bt_icon = monitor_menu_icon_factory_create_bluetooth(root);
    s_monitor_ui.menu_custom_icon = monitor_menu_icon_factory_create_custom(root);
    monitor_set_hidden(s_monitor_ui.menu_bt_icon, true);
    monitor_set_hidden(s_monitor_ui.menu_custom_icon, true);
}

void monitor_menu_icons_update(const monitor_menu_view_t *view)
{
    const monitor_menu_icon_t icon = monitor_resolve_icon_to_show(view);

    monitor_apply_menu_icon_visibility(icon);
    if (icon == MONITOR_MENU_ICON_CUSTOM) {
        monitor_render_custom_icon(&view->custom_icon);
    }
}
