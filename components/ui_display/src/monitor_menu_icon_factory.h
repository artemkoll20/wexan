#ifndef MONITOR_MENU_ICON_FACTORY_H
#define MONITOR_MENU_ICON_FACTORY_H

#include <stdint.h>
#include <lvgl.h>

lv_obj_t *monitor_menu_icon_factory_create_wifi(lv_obj_t *parent);
lv_obj_t *monitor_menu_icon_factory_create_bluetooth(lv_obj_t *parent);
lv_obj_t *monitor_menu_icon_factory_create_custom(lv_obj_t *parent);

void monitor_menu_icon_factory_render_bitmap(lv_obj_t *icon, const uint8_t *data, uint8_t width, uint8_t height);
void monitor_menu_icon_factory_clear(lv_obj_t *icon);

#endif /* MONITOR_MENU_ICON_FACTORY_H */
