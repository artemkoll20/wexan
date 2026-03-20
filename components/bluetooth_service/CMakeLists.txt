idf_component_register(
    SRCS
        "src/bluetooth_cache.c"
        "src/bluetooth_gap_handlers.c"
        "src/bluetooth_init.c"
        "src/bluetooth_scan.c"
        "src/bluetooth_scan_engine.c"
        "src/bluetooth_text.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_event nvs_flash freertos app_core
    PRIV_REQUIRES bt
)
