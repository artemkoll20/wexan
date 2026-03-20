/*
 * monitor_hw.c
 * SSD1306 transport and LVGL display bootstrap.
 */
#include "monitor_internal.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"

#define MONITOR_I2C_BUS_PORT 0
#define MONITOR_LCD_RST_GPIO 21
#define MONITOR_I2C_SDA_GPIO 17
#define MONITOR_I2C_SCL_GPIO 18
#define MONITOR_VEXT_ENABLE_GPIO 36
#define MONITOR_VEXT_ENABLE_LEVEL 0
#define MONITOR_DISPLAY_INVERT_COLORS true
#define MONITOR_SCREEN_WIDTH 128
#define MONITOR_SCREEN_HEIGHT 64

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static esp_lcd_panel_io_handle_t s_io_handle = NULL;

static const i2c_master_bus_config_t s_i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = MONITOR_I2C_BUS_PORT,
    .scl_io_num = MONITOR_I2C_SCL_GPIO,
    .sda_io_num = MONITOR_I2C_SDA_GPIO,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

static const esp_lcd_panel_io_i2c_config_t s_io_config = {
    .dev_addr = 0x3C,
    .scl_speed_hz = 400000, /* 400kHz */
    .control_phase_bytes = 1,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .dc_bit_offset = 6,
};

static const esp_lcd_panel_dev_config_t s_panel_config = {
    .bits_per_pixel = 1,
    .reset_gpio_num = MONITOR_LCD_RST_GPIO,
};

static const lvgl_port_cfg_t s_lvgl_cfg = {
    .task_priority = 5,
    .task_stack = 4096,
    .task_affinity = -1,
    .task_max_sleep_ms = 500,
    .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
    .timer_period_ms = 2,
};

monitor_ui_handles_t s_monitor_ui = { 0 };

static i2c_master_bus_handle_t monitor_create_i2c_bus(void)
{
    i2c_master_bus_handle_t bus_handle = NULL;

    ESP_ERROR_CHECK(i2c_new_master_bus(&s_i2c_mst_config, &bus_handle));
    return bus_handle;
}

static void monitor_enable_display_power(void)
{
    gpio_set_direction(MONITOR_VEXT_ENABLE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(MONITOR_VEXT_ENABLE_GPIO, MONITOR_VEXT_ENABLE_LEVEL);
}

static void monitor_create_display_panel(i2c_master_bus_handle_t bus_handle)
{
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(bus_handle, &s_io_config, &s_io_handle));
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(s_io_handle, &s_panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, MONITOR_DISPLAY_INVERT_COLORS));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
}

static lv_display_t *monitor_create_lvgl_display(void)
{
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = NULL,
        .buffer_size = MONITOR_SCREEN_WIDTH * MONITOR_SCREEN_HEIGHT,
        .hres = MONITOR_SCREEN_WIDTH,
        .vres = MONITOR_SCREEN_HEIGHT,
        .monochrome = true,
    };

    ESP_ERROR_CHECK(lvgl_port_init(&s_lvgl_cfg));
    disp_cfg.io_handle = s_io_handle;
    disp_cfg.panel_handle = s_panel_handle;
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
    i2c_master_bus_handle_t bus_handle = monitor_create_i2c_bus();
    lv_display_t *disp;

    monitor_enable_display_power();
    monitor_create_display_panel(bus_handle);
    disp = monitor_create_lvgl_display();
    monitor_create_ui(disp);
}
