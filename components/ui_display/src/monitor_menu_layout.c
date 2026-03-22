/*
 * monitor_menu_layout.c
 * Pixel menu layout creation and header updates.
 */
#include "monitor_internal.h"
#include "monitor_menu_internal.h"
#include <stdio.h>

#define MONITOR_MENU_SCREEN_WIDTH 128
#define MONITOR_MENU_SCREEN_HEIGHT 64
#define MONITOR_MENU_BORDER_WIDTH 1
#define MONITOR_MENU_HEADER_DIVIDER_X 1
#define MONITOR_MENU_HEADER_DIVIDER_Y 16
#define MONITOR_MENU_HEADER_DIVIDER_WIDTH (MONITOR_MENU_SCREEN_WIDTH - 2)
#define MONITOR_MENU_HEADER_DIVIDER_HEIGHT 1
#define MONITOR_MENU_TITLE_X 4
#define MONITOR_MENU_TITLE_Y 4
#define MONITOR_MENU_COUNTDOWN_X -4
#define MONITOR_MENU_COUNTDOWN_Y 4
#define MONITOR_MENU_ARROW_LEFT_X 5
#define MONITOR_MENU_ARROW_RIGHT_X -5
#define MONITOR_MENU_ARROW_Y 6
#define MONITOR_MENU_COUNTDOWN_TEXT_SIZE 8
#define MONITOR_MENU_COUNTDOWN_PHASES 4

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

static void monitor_apply_menu_text_style(lv_obj_t *label, const lv_font_t *font)
{
    if (!label) {
        return;
    }

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
}

static lv_obj_t *monitor_create_menu_label(
    lv_obj_t *parent,
    const lv_font_t *font,
    lv_align_t align,
    lv_coord_t x_ofs,
    lv_coord_t y_ofs,
    const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    monitor_apply_menu_text_style(label, font);
    lv_obj_align(label, align, x_ofs, y_ofs);
    lv_label_set_text(label, text ? text : "");

    return label;
}

static lv_obj_t *monitor_create_pixel_block(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *pixel = lv_obj_create(parent);

    lv_obj_set_pos(pixel, x, y);
    lv_obj_set_size(pixel, w, h);
    monitor_apply_box_style(pixel);
    lv_obj_set_style_bg_color(pixel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(pixel, LV_OPA_COVER, 0);

    return pixel;
}

lv_obj_t *monitor_menu_layout_create_root(void)
{
    lv_obj_t *root = lv_obj_create(lv_scr_act());

    if (!root) {
        return NULL;
    }

    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, MONITOR_MENU_SCREEN_WIDTH, MONITOR_MENU_SCREEN_HEIGHT);
    monitor_apply_box_style(root);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, MONITOR_MENU_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(root, lv_color_white(), 0);

    return root;
}

void monitor_menu_layout_create_header(lv_obj_t *root)
{
    (void)monitor_create_pixel_block(
        root,
        MONITOR_MENU_HEADER_DIVIDER_X,
        MONITOR_MENU_HEADER_DIVIDER_Y,
        MONITOR_MENU_HEADER_DIVIDER_WIDTH,
        MONITOR_MENU_HEADER_DIVIDER_HEIGHT);

    s_monitor_ui.menu_title_label = monitor_create_menu_label(
        root,
        &lv_font_unscii_8,
        LV_ALIGN_TOP_LEFT,
        MONITOR_MENU_TITLE_X,
        MONITOR_MENU_TITLE_Y,
        "WiFi scan");

    s_monitor_ui.menu_countdown_label = monitor_create_menu_label(
        root,
        &lv_font_unscii_8,
        LV_ALIGN_TOP_RIGHT,
        MONITOR_MENU_COUNTDOWN_X,
        MONITOR_MENU_COUNTDOWN_Y,
        "");
}

void monitor_menu_layout_create_navigation(lv_obj_t *root)
{
    (void)monitor_create_menu_label(
        root,
        &lv_font_unscii_16,
        LV_ALIGN_LEFT_MID,
        MONITOR_MENU_ARROW_LEFT_X,
        MONITOR_MENU_ARROW_Y,
        "<");

    (void)monitor_create_menu_label(
        root,
        &lv_font_unscii_16,
        LV_ALIGN_RIGHT_MID,
        MONITOR_MENU_ARROW_RIGHT_X,
        MONITOR_MENU_ARROW_Y,
        ">");
}
void monitor_menu_layout_update_header(const monitor_menu_view_t *view)
{
    static const char *const countdown_suffix_by_phase[MONITOR_MENU_COUNTDOWN_PHASES] = {
        "...",
        ".. ",
        ".  ",
        "   ",
    };
    char countdown_text[MONITOR_MENU_COUNTDOWN_TEXT_SIZE];
    uint8_t phase = view->countdown_phase;

    if (phase >= MONITOR_MENU_COUNTDOWN_PHASES) {
        phase = 0U;
    }

    lv_label_set_text(s_monitor_ui.menu_title_label, view->title ? view->title : "");
    if (view->countdown_s == 0U) {
        lv_obj_add_flag(s_monitor_ui.menu_countdown_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_monitor_ui.menu_countdown_label, LV_OBJ_FLAG_HIDDEN);
    snprintf(
        countdown_text,
        sizeof(countdown_text),
        "%u%s",
        (unsigned)view->countdown_s,
        countdown_suffix_by_phase[phase]);
    lv_label_set_text(s_monitor_ui.menu_countdown_label, countdown_text);
}
