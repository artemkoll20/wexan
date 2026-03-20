#ifndef UI_DISPLAY_MONITOR_INTERNAL_H
#define UI_DISPLAY_MONITOR_INTERNAL_H

#include <lvgl.h>
#include "ui/monitor.h"

typedef struct {
    lv_obj_t *text_label;
    lv_obj_t *menu_root;
    lv_obj_t *menu_title_label;
    lv_obj_t *menu_countdown_label;
    lv_obj_t *menu_wifi_icon;
    lv_obj_t *menu_bt_icon;
    lv_obj_t *menu_custom_icon;
} monitor_ui_handles_t;

extern monitor_ui_handles_t s_monitor_ui;

void monitor_set_text_mode_locked(void);
void monitor_set_menu_mode_locked(void);
void monitor_create_menu_ui(void);

#endif /* UI_DISPLAY_MONITOR_INTERNAL_H */
