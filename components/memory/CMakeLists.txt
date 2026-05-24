idf_component_register(
    SRCS
        "src/memory.c"

    INCLUDE_DIRS
        "include"

    PRIV_INCLUDE_DIRS
        "src"

    PRIV_REQUIRES
        fatfs
        sdmmc
        esp_driver_sdspi
        esp_driver_spi
        driver
)