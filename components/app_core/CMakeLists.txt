idf_component_register(
    SRCS
        "src/runtime_api.c"
        "src/runtime_worker.c"
        "src/runtime_events.c"
        "src/runtime_screen.c"
        "src/runtime_scan_controller.c"
        "src/runtime_handlers.c"
        "src/app_event_handlers.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES ui_display ui_input wifi_service bluetooth_service esp_event esp_timer freertos
)
