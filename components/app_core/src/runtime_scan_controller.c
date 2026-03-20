/*
 * runtime_scan_controller.c
 * Shared page and pending-snapshot flow for Wi-Fi and BLE runtime pages.
 */
#include "runtime_scan_controller.h"
#include <string.h>

static bool runtime_scan_controller_ready(const runtime_scan_controller_t *controller)
{
    if (!controller) {
        return false;
    }

    return controller
        && controller->current_snapshot
        && controller->pending_snapshot
        && controller->snapshot_size > 0U
        && controller->shown_version
        && controller->shown_page
        && controller->pending_version
        && controller->has_pending_snapshot;
}

void runtime_scan_controller_reset(const runtime_scan_controller_t *controller)
{
    if (!runtime_scan_controller_ready(controller)) {
        return;
    }

    memset(controller->current_snapshot, 0, controller->snapshot_size);
    memset(controller->pending_snapshot, 0, controller->snapshot_size);
    *controller->shown_version = 0U;
    *controller->shown_page = 0U;
    *controller->has_pending_snapshot = false;
}

void runtime_scan_controller_apply_pending(const runtime_scan_controller_t *controller)
{
    if (!runtime_scan_controller_ready(controller) || !(*controller->has_pending_snapshot)) {
        return;
    }

    memcpy(controller->current_snapshot, controller->pending_snapshot, controller->snapshot_size);
    *controller->shown_version = *controller->pending_version;
    *controller->shown_page = 0U;
    *controller->has_pending_snapshot = false;
}

void runtime_scan_controller_tick(
    const runtime_scan_controller_t *controller,
    runtime_scan_page_count_fn_t page_count_fn,
    runtime_scan_render_fn_t render_fn)
{
    size_t pages;

    if (!runtime_scan_controller_ready(controller) || !page_count_fn || !render_fn) {
        return;
    }

    if (*controller->shown_version == 0U) {
        return;
    }

    pages = page_count_fn(controller->current_snapshot);
    if (pages <= 1U) {
        runtime_scan_controller_apply_pending(controller);
        render_fn();
        return;
    }

    *controller->shown_page = (*controller->shown_page + 1U) % pages;
    if (*controller->shown_page == 0U) {
        runtime_scan_controller_apply_pending(controller);
    }

    render_fn();
}

void runtime_scan_controller_process_polled(
    const runtime_scan_controller_t *controller,
    const void *polled_snapshot,
    uint32_t polled_version,
    runtime_scan_render_fn_t render_fn)
{
    if (!runtime_scan_controller_ready(controller) || !polled_snapshot || !render_fn) {
        return;
    }

    if (polled_version == 0U || polled_version == *controller->shown_version) {
        return;
    }

    if (*controller->shown_version == 0U) {
        memcpy(controller->current_snapshot, polled_snapshot, controller->snapshot_size);
        *controller->shown_version = polled_version;
        *controller->shown_page = 0U;
        render_fn();
        return;
    }

    memcpy(controller->pending_snapshot, polled_snapshot, controller->snapshot_size);
    *controller->has_pending_snapshot = true;
}
