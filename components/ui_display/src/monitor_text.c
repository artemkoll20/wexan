/*
 * monitor_text.c
 * Text mode rendering and print(...) backend.
 */
#include "monitor_internal.h"
#include <stdio.h>
#include "esp_lvgl_port.h"

#define MONITOR_TEXT_BUFFER_SIZE 512
#define MONITOR_TEXT_VALUE_TMP_SIZE 64

static void monitor_append_text(char *dst, size_t dst_size, size_t *offset, const char *text)
{
    int written;

    if (!dst || dst_size == 0 || !offset || *offset >= (dst_size - 1)) {
        return;
    }

    written = snprintf(dst + *offset, dst_size - *offset, "%s", text ? text : "None");
    if (written < 0) {
        return;
    }

    if ((size_t)written >= (dst_size - *offset)) {
        *offset = dst_size - 1;
        return;
    }

    *offset += (size_t)written;
}

static const char *monitor_format_print_arg(const monitor_print_arg_t *arg, char *tmp, size_t tmp_size)
{
    if (!arg || !tmp || tmp_size == 0U) {
        return "<unsupported>";
    }

    switch (arg->type) {
    case MONITOR_PRINT_ARG_STR:
        return arg->value.str ? arg->value.str : "None";
    case MONITOR_PRINT_ARG_SIGNED:
        snprintf(tmp, tmp_size, "%lld", arg->value.signed_value);
        return tmp;
    case MONITOR_PRINT_ARG_UNSIGNED:
        snprintf(tmp, tmp_size, "%llu", arg->value.unsigned_value);
        return tmp;
    case MONITOR_PRINT_ARG_FLOAT:
        snprintf(tmp, tmp_size, "%g", arg->value.float_value);
        return tmp;
    case MONITOR_PRINT_ARG_BOOL:
        return arg->value.bool_value ? "True" : "False";
    case MONITOR_PRINT_ARG_CHAR:
        tmp[0] = arg->value.char_value;
        tmp[1] = '\0';
        return tmp;
    default:
        return "<unsupported>";
    }
}

static void monitor_set_text(const char *text)
{
    if (!s_monitor_ui.text_label) {
        return;
    }

    lvgl_port_lock(0);
    monitor_set_text_mode_locked();
    lv_label_set_text(s_monitor_ui.text_label, text ? text : "");
    lvgl_port_unlock();
}

void monitor_print(size_t count, const monitor_print_arg_t *args)
{
    char text_buf[MONITOR_TEXT_BUFFER_SIZE];
    size_t offset = 0;
    size_t i;

    if (!args || count == 0) {
        monitor_set_text("");
        return;
    }

    text_buf[0] = '\0';

    for (i = 0; i < count; ++i) {
        char tmp[MONITOR_TEXT_VALUE_TMP_SIZE];
        const char *formatted;

        if (i > 0) {
            monitor_append_text(text_buf, sizeof(text_buf), &offset, " ");
        }

        formatted = monitor_format_print_arg(&args[i], tmp, sizeof(tmp));
        monitor_append_text(text_buf, sizeof(text_buf), &offset, formatted);
    }

    monitor_set_text(text_buf);
}
