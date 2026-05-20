/*
 * monitor_hw.c
 * ST7735S TFT display transport and LVGL display bootstrap.
 */
#include "monitor_internal.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"

#define MONITOR_SPI_BUS_HOST    SPI2_HOST
#define MONITOR_SPI_MOSI_GPIO   11
#define MONITOR_SPI_CLK_GPIO    12
#define MONITOR_SPI_CS_GPIO     10
#define MONITOR_LCD_DC_GPIO     13
#define MONITOR_LCD_RST_GPIO    14
#define MONITOR_LCD_BL_GPIO     15
#define MONITOR_SCREEN_WIDTH    128
#define MONITOR_SCREEN_HEIGHT   128

static spi_host_device_t s_spi_host = SPI2_HOST;
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;

static const lvgl_port_cfg_t s_lvgl_cfg = {
    .task_priority = 5,
    .task_stack = 4096,
    .task_affinity = -1,
    .task_max_sleep_ms = 500,
    .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
    .timer_period_ms = 2,
};

monitor_ui_handles_t s_monitor_ui = { 0 };

static void monitor_create_spi_bus(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = MONITOR_SPI_MOSI_GPIO,
        .miso_io_num = -1,
        .sclk_io_num = MONITOR_SPI_CLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MONITOR_SCREEN_WIDTH * MONITOR_SCREEN_HEIGHT * 2,
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(MONITOR_SPI_BUS_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

static void monitor_enable_display_power(void)
{
    gpio_set_direction(MONITOR_LCD_RST_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(MONITOR_LCD_DC_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(MONITOR_LCD_BL_GPIO, GPIO_MODE_OUTPUT);
    
    gpio_set_level(MONITOR_LCD_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(MONITOR_LCD_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(MONITOR_LCD_BL_GPIO, 1);
}

static void monitor_create_display_panel(void)
{
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = MONITOR_LCD_DC_GPIO,
        .cs_gpio_num = MONITOR_SPI_CS_GPIO,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .flags = {},
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(s_spi_host, &io_config, &s_io_handle));
    
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = MONITOR_LCD_RST_GPIO,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io_handle, &panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
}

static lv_display_t *monitor_create_lvgl_display(void)
{
    ESP_ERROR_CHECK(lvgl_port_init(&s_lvgl_cfg));
    
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_io_handle,
        .panel_handle = s_panel_handle,
        .buffer_size = MONITOR_SCREEN_WIDTH * MONITOR_SCREEN_HEIGHT * sizeof(uint16_t),
        .hres = MONITOR_SCREEN_WIDTH,
        .vres = MONITOR_SCREEN_HEIGHT,
        .monochrome = false,
    };
    
    return lvgl_port_add_disp(&disp_cfg);
}

static void monitor_create_main_label(void)
{
    s_monitor_ui.text_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(s_monitor_ui.text_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(s_monitor_ui.text_label, lv_color_white(), 0);
    lv_obj_set_width(s_monitor_ui.text_label, MONITOR_SCREEN_WIDTH);
    lv_label_set_long_mode(s_monitor_ui.text_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_monitor_ui.text_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(s_monitor_ui.text_label, "monitor ready");
}

static void monitor_create_ui(lv_display_t *disp)
{
    if (!disp) {
        return;
    }

    lvgl_port_lock(0);
    lv_display_set_default(disp);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    monitor_create_main_label();
    monitor_create_menu_ui();
    lvgl_port_unlock();
}

void monitor_init(void)
{
    monitor_create_spi_bus();
    lv_display_t *disp;

    monitor_enable_display_power();
    monitor_create_display_panel();
    disp = monitor_create_lvgl_display();
    monitor_create_ui(disp);
}
