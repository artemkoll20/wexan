/*
 * wifi_text.c
 * Text pagination and formatting for Wi-Fi snapshot rendering.
 * Refactored to use common pagination utilities.
 */
#include "wifi_internal.h"
#include "app_core/app_pagination.h"
#include <stdio.h>
#include <string.h>
#include "esp_err.h"

size_t wifi_page_count(size_t total_items)
{
    return app_pagination_page_count(total_items, WIFI_PAGE_SIZE);
}

static void wifi_format_network_line(const wifi_network_t *network, char *out, size_t out_size)
{
    const char *ssid;
    char channel_suffix[8];

    if (!network || !out || out_size == 0) {
        return;
    }

    out[0] = '\0';
    ssid = network->ssid[0] ? network->ssid : "<hidden>";
    snprintf(channel_suffix, sizeof(channel_suffix), "c%u", (unsigned)network->channel);

    size_t suffix_len = strlen(channel_suffix);
    size_t ssid_len = strlen(ssid);

    /* Use common truncation utility */
    if ((ssid_len + 1U + suffix_len) <= WIFI_PAGE_LINE_MAX_CHARS) {
        snprintf(out, out_size, "%s %s", ssid, channel_suffix);
        return;
    }

    if (WIFI_PAGE_LINE_MAX_CHARS <= (suffix_len + 4U)) {
        snprintf(out, out_size, "%s", channel_suffix);
        return;
    }

    app_pagination_truncate_with_suffix(
        ssid, ssid_len,
        channel_suffix, suffix_len,
        out, out_size);
}

void wifi_build_page_text(const wifi_network_list_t *list, size_t page_index, char *out, size_t out_size)
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
        snprintf(out, out_size, "WiFi scan err %ld", (long)list->last_error);
        return;
    }

    if (list->count == 0) {
        snprintf(out, out_size, "WiFi 0/%lu\nNo networks", (unsigned long)list->total_found);
        return;
    }

    /* Use common pagination bounds calculation */
    app_pagination_get_bounds(page_index, list->count, WIFI_PAGE_SIZE, &start, &end);

    written = snprintf(out, out_size, "WiFi %lu/%lu p%lu/%lu\n",
        (unsigned long)list->count,
        (unsigned long)list->total_found,
        (unsigned long)(page_index + 1U),
        (unsigned long)app_pagination_page_count(list->count, WIFI_PAGE_SIZE));
    if (written < 0) {
        return;
    }
    if ((size_t)written >= out_size) {
        out[out_size - 1U] = '\0';
        return;
    }
    offset = (size_t)written;

    for (i = start; i < end; ++i) {
        char line[WIFI_PAGE_LINE_MAX_CHARS + 8];

        wifi_format_network_line(&list->items[i], line, sizeof(line));
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
