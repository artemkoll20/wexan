/*
 * monitor_boot.c
 * Startup animation shown before runtime menu starts.
 */
#include "monitor_internal.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MONITOR_BOOT_SCREEN_WIDTH 128
#define MONITOR_BOOT_SCREEN_HEIGHT 64
#define MONITOR_BOOT_TITLE_Y 8
#define MONITOR_BOOT_STATUS_Y 26
#define MONITOR_BOOT_BAR_X 16
#define MONITOR_BOOT_BAR_Y 44
#define MONITOR_BOOT_BAR_W 96
#define MONITOR_BOOT_BAR_H 8
#define MONITOR_BOOT_BAR_INSET 1
#define MONITOR_BOOT_DEFAULT_MS 1500U
#define MONITOR_BOOT_FRAME_MS 75U

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

void monitor_show_boot_animation(uint32_t duration_ms)
{
    static const char *const spinner_frames[] = { "-", "\\", "|", "/" };
    const uint32_t step_ms = MONITOR_BOOT_FRAME_MS;
    const uint32_t effective_ms = duration_ms == 0U ? MONITOR_BOOT_DEFAULT_MS : duration_ms;
    uint32_t total_steps = effective_ms / step_ms;
    lv_obj_t *overlay;
    lv_obj_t *title_label;
    lv_obj_t *status_label;
    lv_obj_t *bar_bg;
    lv_obj_t *bar_fill;
    uint32_t step;

    if (total_steps == 0U) {
        total_steps = 1U;
    }

    lvgl_port_lock(0);

    overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, MONITOR_BOOT_SCREEN_WIDTH, MONITOR_BOOT_SCREEN_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    monitor_apply_box_style(overlay);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);

    title_label = lv_label_create(overlay);
    lv_obj_set_style_text_font(title_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_label_set_text(title_label, "weXan");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, MONITOR_BOOT_TITLE_Y);

    status_label = lv_label_create(overlay);
    lv_obj_set_style_text_font(status_label, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
    lv_label_set_text(status_label, "INIT 0% -");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, MONITOR_BOOT_STATUS_Y);

    bar_bg = lv_obj_create(overlay);
    lv_obj_set_pos(bar_bg, MONITOR_BOOT_BAR_X, MONITOR_BOOT_BAR_Y);
    lv_obj_set_size(bar_bg, MONITOR_BOOT_BAR_W, MONITOR_BOOT_BAR_H);
    monitor_apply_box_style(bar_bg);
    lv_obj_set_style_bg_color(bar_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar_bg, 1, 0);
    lv_obj_set_style_border_color(bar_bg, lv_color_white(), 0);

    bar_fill = lv_obj_create(bar_bg);
    monitor_apply_box_style(bar_fill);
    lv_obj_set_pos(bar_fill, MONITOR_BOOT_BAR_INSET, MONITOR_BOOT_BAR_INSET);
    lv_obj_set_size(
        bar_fill,
        0,
        MONITOR_BOOT_BAR_H - (MONITOR_BOOT_BAR_INSET * 2));
    lv_obj_set_style_bg_color(bar_fill, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);

    lvgl_port_unlock();

    for (step = 0; step <= total_steps; ++step) {
        const uint32_t progress = (step * 100U) / total_steps;
        const uint32_t fill_max_w = MONITOR_BOOT_BAR_W - (MONITOR_BOOT_BAR_INSET * 2U);
        const uint32_t fill_w = (fill_max_w * progress) / 100U;

        lvgl_port_lock(0);
        lv_obj_set_size(bar_fill, (lv_coord_t)fill_w, MONITOR_BOOT_BAR_H - (MONITOR_BOOT_BAR_INSET * 2));
        lv_label_set_text_fmt(
            status_label,
            "INIT %u%% %s",
            (unsigned)progress,
            spinner_frames[step % (sizeof(spinner_frames) / sizeof(spinner_frames[0]))]);
        lvgl_port_unlock();

        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }

    lvgl_port_lock(0);
    lv_obj_del(overlay);
    lvgl_port_unlock();
}
