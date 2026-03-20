#ifndef UI_DISPLAY_MONITOR_MENU_INTERNAL_H
#define UI_DISPLAY_MONITOR_MENU_INTERNAL_H

#include <lvgl.h>
#include "ui/monitor.h"

lv_obj_t *monitor_menu_layout_create_root(void);
void monitor_menu_layout_create_header(lv_obj_t *root);
void monitor_menu_layout_create_navigation(lv_obj_t *root);
void monitor_menu_layout_update_header(const monitor_menu_view_t *view);

void monitor_menu_icons_reset_cache(void);
void monitor_menu_icons_create(lv_obj_t *root);
void monitor_menu_icons_update(const monitor_menu_view_t *view);

#endif /* UI_DISPLAY_MONITOR_MENU_INTERNAL_H */
