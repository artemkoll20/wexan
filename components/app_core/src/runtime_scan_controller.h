#ifndef APP_CORE_RUNTIME_SCAN_CONTROLLER_H
#define APP_CORE_RUNTIME_SCAN_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef size_t (*runtime_scan_page_count_fn_t)(const void *current_snapshot);
typedef void (*runtime_scan_render_fn_t)(void);

typedef struct {
    void *current_snapshot;
    void *pending_snapshot;
    size_t snapshot_size;
    uint32_t *shown_version;
    size_t *shown_page;
    const uint32_t *pending_version;
    bool *has_pending_snapshot;
} runtime_scan_controller_t;

void runtime_scan_controller_reset(const runtime_scan_controller_t *controller);
void runtime_scan_controller_apply_pending(const runtime_scan_controller_t *controller);
void runtime_scan_controller_tick(
    const runtime_scan_controller_t *controller,
    runtime_scan_page_count_fn_t page_count_fn,
    runtime_scan_render_fn_t render_fn);
void runtime_scan_controller_process_polled(
    const runtime_scan_controller_t *controller,
    const void *polled_snapshot,
    uint32_t polled_version,
    runtime_scan_render_fn_t render_fn);

#endif /* APP_CORE_RUNTIME_SCAN_CONTROLLER_H */
