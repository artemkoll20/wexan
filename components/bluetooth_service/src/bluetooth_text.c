/*
 * bluetooth_text.c
 * Text pagination and formatting for BLE snapshot rendering.
 * Refactored to use common pagination utilities.
 */
#include "bluetooth_internal.h"
#include "app_core/app_pagination.h"
#include <stdio.h>
#include <string.h>
#include "esp_err.h"

size_t bluetooth_page_count(size_t total_items)
{
    return app_pagination_page_count(total_items, BLUETOOTH_PAGE_SIZE);
}

static void bluetooth_format_short_addr(const uint8_t *addr, char *out, size_t out_size)
{
    if (!addr || !out || out_size == 0) {
        return;
    }

    snprintf(
        out,
        out_size,
        "%02X:%02X:%02X",
        (unsigned)addr[3],
        (unsigned)addr[4],
        (unsigned)addr[5]);
}

static void bluetooth_format_device_line(const bluetooth_device_t *device, char *out, size_t out_size)
{
    const char *name;
    char fallback_addr[16];
    char rssi_suffix[12];

    if (!device || !out || out_size == 0) {
        return;
    }

    out[0] = '\0';
    fallback_addr[0] = '\0';
    if (device->name[0] == '\0') {
        bluetooth_format_short_addr(device->addr, fallback_addr, sizeof(fallback_addr));
    }
    name = device->name[0] ? device->name : fallback_addr;

    snprintf(rssi_suffix, sizeof(rssi_suffix), " %ddBm", (int)device->rssi);

    size_t suffix_len = strlen(rssi_suffix);
    size_t name_len = strlen(name);

    /* Use common truncation utility */
    if ((name_len + suffix_len) <= BLUETOOTH_PAGE_LINE_MAX_CHARS) {
        snprintf(out, out_size, "%s%s", name, rssi_suffix);
        return;
    }

    if (BLUETOOTH_PAGE_LINE_MAX_CHARS <= (suffix_len + 3U)) {
        snprintf(out, out_size, "%s", rssi_suffix);
        return;
    }

    app_pagination_truncate_with_suffix(
        name, name_len,
        rssi_suffix, suffix_len,
        out, out_size);
}

void bluetooth_build_page_text(const bluetooth_device_list_t *list, size_t page_index, char *out, size_t out_size)
{
    size_t start;
    size_t end;
    size_t i;
    int written;
    size_t offset = 0;

    if (!out || out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (!list) {
        return;
    }

    if (list->last_error != ESP_OK) {
        if (list->last_error == ESP_ERR_NOT_SUPPORTED) {
            snprintf(out, out_size, "BLE disabled");
            return;
        }
        snprintf(out, out_size, "BLE scan err %ld", (long)list->last_error);
        return;
    }

    if (list->count == 0) {
        snprintf(out, out_size, "BLE 0/%lu\nNo devices", (unsigned long)list->total_found);
        return;
    }

    /* Use common pagination bounds calculation */
    app_pagination_get_bounds(page_index, list->count, BLUETOOTH_PAGE_SIZE, &start, &end);

    written = snprintf(out, out_size, "BLE %lu/%lu p%lu/%lu\n",
        (unsigned long)list->count,
        (unsigned long)list->total_found,
        (unsigned long)(page_index + 1U),
        (unsigned long)app_pagination_page_count(list->count, BLUETOOTH_PAGE_SIZE));
    if (written < 0) {
        return;
    }
    if ((size_t)written >= out_size) {
        out[out_size - 1U] = '\0';
        return;
    }
    offset = (size_t)written;

    for (i = start; i < end; ++i) {
        char line[BLUETOOTH_PAGE_LINE_MAX_CHARS + 12];

        bluetooth_format_device_line(&list->items[i], line, sizeof(line));
        written = snprintf(out + offset, out_size - offset, "%s\n", line);
        if (written < 0) {
            return;
        }
        if ((size_t)written >= (out_size - offset)) {
            out[out_size - 1U] = '\0';
            return;
        }
        offset += (size_t)written;
    }
}
