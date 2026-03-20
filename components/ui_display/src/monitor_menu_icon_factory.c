#include "monitor_menu_icon_factory.h"
#include "monitor_menu_icon_bitmaps.h"
#include <string.h>

#define MONITOR_MENU_CUSTOM_ICON_BOX_WIDTH 32
#define MONITOR_MENU_CUSTOM_ICON_BOX_HEIGHT 32
#define MONITOR_MENU_WIFI_ICON_BOX_WIDTH 32
#define MONITOR_MENU_WIFI_ICON_BOX_HEIGHT 32
#define MONITOR_MENU_BT_ICON_BOX_WIDTH 32
#define MONITOR_MENU_BT_ICON_BOX_HEIGHT 32
#define MONITOR_MENU_ICON_Y_OFFSET 7
#define MONITOR_MENU_ICON_A8_BUF_SIZE(w, h) ((size_t)(w) * (size_t)(h))

static uint8_t s_monitor_menu_wifi_icon_a8_buf[
    MONITOR_MENU_ICON_A8_BUF_SIZE(MONITOR_MENU_WIFI_ICON_BOX_WIDTH, MONITOR_MENU_WIFI_ICON_BOX_HEIGHT)];
static uint8_t s_monitor_menu_bt_icon_a8_buf[
    MONITOR_MENU_ICON_A8_BUF_SIZE(MONITOR_MENU_BT_ICON_BOX_WIDTH, MONITOR_MENU_BT_ICON_BOX_HEIGHT)];
static uint8_t s_monitor_menu_custom_icon_a8_buf[
    MONITOR_MENU_ICON_A8_BUF_SIZE(MONITOR_MENU_CUSTOM_ICON_BOX_WIDTH, MONITOR_MENU_CUSTOM_ICON_BOX_HEIGHT)];

static void monitor_apply_box_style(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *monitor_create_icon_canvas(
    lv_obj_t *parent,
    lv_coord_t width,
    lv_coord_t height,
    uint8_t *buf,
    size_t buf_size)
{
    lv_obj_t *icon = lv_canvas_create(parent);

    if (!icon) {
        return NULL;
    }

    lv_obj_set_size(icon, width, height);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, MONITOR_MENU_ICON_Y_OFFSET);
    monitor_apply_box_style(icon);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_image_recolor(icon, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_canvas_set_buffer(icon, buf, width, height, LV_COLOR_FORMAT_A8);
    memset(buf, 0, buf_size);

    return icon;
}

void monitor_menu_icon_factory_render_bitmap(lv_obj_t *icon, const uint8_t *data, uint8_t width, uint8_t height)
{
    lv_draw_buf_t *draw_buf;
    uint8_t *canvas_data;
    size_t row_stride;
    size_t canvas_stride;
    int16_t raw_x_offset;
    int16_t raw_y_offset;
    int16_t x_offset;
    int16_t y_offset;
    int16_t canvas_width;
    int16_t canvas_height;
    size_t y;

    if (!icon || !data || width == 0U || height == 0U) {
        return;
    }

    draw_buf = lv_canvas_get_draw_buf(icon);
    if (!draw_buf || !draw_buf->data) {
        return;
    }

    canvas_data = (uint8_t *)draw_buf->data;
    canvas_stride = (size_t)draw_buf->header.stride;
    canvas_width = (int16_t)draw_buf->header.w;
    canvas_height = (int16_t)draw_buf->header.h;
    memset(canvas_data, 0, canvas_stride * (size_t)draw_buf->header.h);

    row_stride = (size_t)((width + 7U) / 8U);
    raw_x_offset = (canvas_width - (int16_t)width) / 2;
    raw_y_offset = (canvas_height - (int16_t)height) / 2;
    x_offset = raw_x_offset < 0 ? 0 : raw_x_offset;
    y_offset = raw_y_offset < 0 ? 0 : raw_y_offset;

    for (y = 0; y < height; ++y) {
        const int16_t dst_y = y_offset + (int16_t)y;
        size_t x;

        if (dst_y < 0 || dst_y >= canvas_height) {
            continue;
        }

        for (x = 0; x < width; ++x) {
            const int16_t dst_x = x_offset + (int16_t)x;
            const uint8_t byte = data[(y * row_stride) + (x / 8U)];
            const uint8_t bit = (uint8_t)(0x80U >> (x % 8U));
            if ((byte & bit) != 0U && dst_x >= 0 && dst_x < canvas_width) {
                canvas_data[(size_t)dst_y * canvas_stride + (size_t)dst_x] = 0xFF;
            }
        }
    }

    lv_obj_invalidate(icon);
}

void monitor_menu_icon_factory_clear(lv_obj_t *icon)
{
    lv_draw_buf_t *draw_buf;
    size_t buf_size;

    if (!icon) {
        return;
    }

    draw_buf = lv_canvas_get_draw_buf(icon);
    if (!draw_buf || !draw_buf->data) {
        return;
    }

    buf_size = (size_t)draw_buf->header.stride * (size_t)draw_buf->header.h;
    memset(draw_buf->data, 0, buf_size);
    lv_obj_invalidate(icon);
}

lv_obj_t *monitor_menu_icon_factory_create_wifi(lv_obj_t *parent)
{
    lv_obj_t *icon = monitor_create_icon_canvas(
        parent,
        MONITOR_MENU_WIFI_ICON_BOX_WIDTH,
        MONITOR_MENU_WIFI_ICON_BOX_HEIGHT,
        s_monitor_menu_wifi_icon_a8_buf,
        sizeof(s_monitor_menu_wifi_icon_a8_buf));

    if (!icon) {
        return NULL;
    }

    monitor_menu_icon_factory_render_bitmap(
        icon,
        MONITOR_MENU_WIFI_ICON_BITMAP_32,
        MONITOR_MENU_ICON_BITMAP_WIDTH,
        MONITOR_MENU_ICON_BITMAP_HEIGHT);

    return icon;
}

lv_obj_t *monitor_menu_icon_factory_create_bluetooth(lv_obj_t *parent)
{
    lv_obj_t *icon = monitor_create_icon_canvas(
        parent,
        MONITOR_MENU_BT_ICON_BOX_WIDTH,
        MONITOR_MENU_BT_ICON_BOX_HEIGHT,
        s_monitor_menu_bt_icon_a8_buf,
        sizeof(s_monitor_menu_bt_icon_a8_buf));

    if (!icon) {
        return NULL;
    }

    monitor_menu_icon_factory_render_bitmap(
        icon,
        MONITOR_MENU_BT_ICON_BITMAP_32,
        MONITOR_MENU_ICON_BITMAP_WIDTH,
        MONITOR_MENU_ICON_BITMAP_HEIGHT);

    return icon;
}

lv_obj_t *monitor_menu_icon_factory_create_custom(lv_obj_t *parent)
{
    return monitor_create_icon_canvas(
        parent,
        MONITOR_MENU_CUSTOM_ICON_BOX_WIDTH,
        MONITOR_MENU_CUSTOM_ICON_BOX_HEIGHT,
        s_monitor_menu_custom_icon_a8_buf,
        sizeof(s_monitor_menu_custom_icon_a8_buf));
}
