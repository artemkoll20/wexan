idf_component_register(
    SRCS
        "src/wifi_scan.c"
        "src/wifi_text.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_event esp_wifi esp_netif nvs_flash freertos app_core
)
