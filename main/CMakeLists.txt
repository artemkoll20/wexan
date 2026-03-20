#
# main/CMakeLists.txt
# Build manifest for application sources of the main component.
#
set(srcs "main.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES app_core ui_display ui_input wifi_service bluetooth_service battery_service lvgl esp_lvgl_port esp_adc sdmmc fatfs)
