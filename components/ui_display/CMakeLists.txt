idf_component_register(
    SRCS
        "src/monitor_hw.c"
        "src/monitor_menu.c"
        "src/monitor_menu_layout.c"
        "src/monitor_menu_icons.c"
        "src/monitor_menu_icon_factory.c"
        "src/monitor_text.c"
        "src/monitor_boot.c"
        "${CMAKE_CURRENT_LIST_DIR}/../../icon/monitor_menu_icon_bitmaps.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
        "src"
        "${CMAKE_CURRENT_LIST_DIR}/../../icon"
    REQUIRES esp_lvgl_port esp_lcd driver lvgl
)
