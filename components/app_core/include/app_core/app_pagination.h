/*
 * app_pagination.h
 * Shared pagination utilities for scan results rendering.
 */
#ifndef APP_PAGINATION_H
#define APP_PAGINATION_H

#include <stddef.h>
#include <stdint.h>

/**
 * Calculate the number of pages needed for a given item count.
 * @param total_items Total number of items to paginate
 * @param page_size Number of items per page
 * @return Number of pages (minimum 1)
 */
static inline size_t app_pagination_page_count(size_t total_items, size_t page_size)
{
    if (total_items == 0 || page_size == 0) {
        return 1;
    }
    return (total_items + page_size - 1U) / page_size;
}

/**
 * Calculate page boundaries for pagination.
 * @param page_index Desired page index (will be clamped to valid range)
 * @param total_items Total number of items
 * @param page_size Number of items per page
 * @param out_start Pointer to store starting index (can be NULL)
 * @param out_end Pointer to store ending index (exclusive, can be NULL)
 */
static inline void app_pagination_get_bounds(
    size_t page_index,
    size_t total_items,
    size_t page_size,
    size_t *out_start,
    size_t *out_end)
{
    size_t total_pages = app_pagination_page_count(total_items, page_size);
    
    /* Clamp page index to valid range */
    if (page_index >= total_pages) {
        page_index = 0;
    }
    
    size_t start = page_index * page_size;
    size_t end = start + page_size;
    if (end > total_items) {
        end = total_items;
    }
    
    if (out_start) {
        *out_start = start;
    }
    if (out_end) {
        *out_end = end;
    }
}

/**
 * Truncate text to fit within max_chars, preserving end suffix if truncation occurs.
 * @param src Source string
 * @param src_len Length of source string
 * @param suffix Suffix to preserve at end
 * @param suffix_len Length of suffix
 * @param dst Destination buffer
 * @param dst_size Size of destination buffer
 * @return Number of characters written
 */
static inline size_t app_pagination_truncate_with_suffix(
    const char *src,
    size_t src_len,
    const char *suffix,
    size_t suffix_len,
    char *dst,
    size_t dst_size)
{
    if (!dst || dst_size == 0) {
        return 0;
    }
    
    dst[0] = '\0';
    
    if (!src || src_len == 0) {
        return 0;
    }
    
    size_t max_chars = dst_size - 1;
    
    if (suffix_len > 0 && src_len > max_chars) {
        /* Need room for suffix + ellipsis */
        size_t keep_len = (suffix_len + 3U < max_chars) 
            ? (max_chars - suffix_len - 3U) 
            : 0;
        
        if (keep_len > 0) {
            for (size_t i = 0; i < keep_len && i < src_len; i++) {
                dst[i] = src[i];
            }
            dst[keep_len] = '.';
            dst[keep_len + 1] = '.';
            dst[keep_len + 2] = '.';
            for (size_t i = 0; i < suffix_len && (keep_len + 3 + i) < max_chars; i++) {
                dst[keep_len + 3 + i] = suffix[i];
            }
            dst[keep_len + 3 + suffix_len] = '\0';
            return keep_len + 3 + suffix_len;
        }
    }
    
    /* No truncation needed or possible */
    size_t copy_len = (src_len < max_chars) ? src_len : max_chars;
    for (size_t i = 0; i < copy_len; i++) {
        dst[i] = src[i];
    }
    dst[copy_len] = '\0';
    return copy_len;
}

#endif /* APP_PAGINATION_H */
