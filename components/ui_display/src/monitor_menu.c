/*
 * monitor_menu.c
 * Pixel menu orchestration and mode switching.
 */
#include "monitor_internal.h"
#include "monitor_menu_internal.h"
#include "esp_lvgl_port.h"

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

void monitor_set_text_mode_locked(void)
{
    monitor_set_hidden(s_monitor_ui.menu_root, true);
    monitor_set_hidden(s_monitor_ui.text_label, false);
}

void monitor_set_menu_mode_locked(void)
{
    monitor_set_hidden(s_monitor_ui.text_label, true);
    monitor_set_hidden(s_monitor_ui.menu_root, false);
}

static bool monitor_menu_widgets_ready(void)
{
    return s_monitor_ui.menu_root
        && s_monitor_ui.menu_title_label
        && s_monitor_ui.menu_countdown_label
        && s_monitor_ui.menu_wifi_icon
        && s_monitor_ui.menu_bt_icon
        && s_monitor_ui.menu_custom_icon;
}

void monitor_create_menu_ui(void)
{
    monitor_menu_icons_reset_cache();
    s_monitor_ui.menu_root = monitor_menu_layout_create_root();
    if (!s_monitor_ui.menu_root) {
        return;
    }

    monitor_menu_layout_create_header(s_monitor_ui.menu_root);
    monitor_menu_layout_create_navigation(s_monitor_ui.menu_root);
    monitor_menu_icons_create(s_monitor_ui.menu_root);
    monitor_set_hidden(s_monitor_ui.menu_root, true);
}

void monitor_show_menu(const monitor_menu_view_t *view)
{
    if (!view || !monitor_menu_widgets_ready()) {
        return;
    }

    lvgl_port_lock(0);
    monitor_set_menu_mode_locked();
    monitor_menu_layout_update_header(view);
    monitor_menu_icons_update(view);
    lvgl_port_unlock();
}
