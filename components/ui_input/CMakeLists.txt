idf_component_register(
    SRCS "src/buttons.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event driver freertos
)
