#include "memory_init.h"

#include <stdio.h>

#include "esp_log.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>

#include "sdmmc_cmd.h"
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>

static const char *TAG = "memory";

#define PIN_MISO  4
#define PIN_MOSI  5
#define PIN_CLK   6
#define PIN_CS    7
#define SPI SPI3_HOST

void memory_sdcard_init()
{
    ESP_LOGI(TAG, "Initializing SD card");
    
    // TODO: Initialize SPI bus
    esp_err_t TEMP;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 10000,
    };

    TEMP = spi_bus_initialize(
        SPI,
        &bus_cfg,
        SPI_DMA_CH_AUTO
    );

    if (TEMP != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(TEMP));
        return;
    }
    
    // TODO: Initialize SD card
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_CS;
    slot_config.host_id = SPI;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // IDEA: СДЕЛАТЬ ДИАЛОГОВОЕ ОКНО НА ЭКРАНЕ ДЛЯ ФОРМАТИРОВАНИЯ  ЕСЛИ ОШИБКА
        .max_files = 16,
        .allocation_unit_size = 4096
    };

    sdmmc_card_t *card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    TEMP = esp_vfs_fat_sdspi_mount( "/sdcard", &host, &slot_config, &mount_config, &card );

    if (TEMP != ESP_OK)
    {
        if (TEMP == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        }
        else
        {
            ESP_LOGE(TAG,
                     "Failed to initialize card (%s)",
                     esp_err_to_name(TEMP));
        }

        return;
    }

    ESP_LOGI(TAG, "SD card initialized successfully");
    
}
