#include <stdio.h>
#include "Display.h"
#include "Peripherials.h"
#include "esp_lcd_hx8394.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui.h"
#include "esp_log.h"

#define LCD_H_RES 720
#define LCD_V_RES 1280

static const char *TAG = "Display";

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_tick();
}

void Display_init(void)
{
    if (mipi_dsi_bus == NULL) {
        ESP_LOGW(TAG, "MIPI-DSI bus not initialized; display not connected");
        return;
    }
    ESP_LOGI(TAG, "Init panel");
    // Create LCD panel handle
    esp_lcd_panel_handle_t panel_handle = NULL;
    static const esp_lcd_dpi_panel_config_t dpi_config =
        HX8394_720_1280_PANEL_30HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    hx8394_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = 2,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_hx8394(NULL, &panel_config, &panel_handle));

    // Reset and init panel
    ESP_LOGI(TAG, "Panel reset");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_LOGI(TAG, "Panel init");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_LOGI(TAG, "Panel display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Initialize touch
    ESP_LOGI(TAG, "Init touch IO");
    esp_lcd_panel_io_handle_t touch_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t touch_io_config = {
        .dev_addr = 0x5D, // GT911 default address
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 1,
        },
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &touch_io_config, &touch_io_handle));

    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_LOGI(TAG, "Init touch controller");
    esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1, // No reset
        .int_gpio_num = -1, // No interrupt
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(touch_io_handle, &touch_config, &touch_handle));

    // LVGL init
    ESP_LOGI(TAG, "LVGL port init");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    // Add display to LVGL
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES * sizeof(lv_color_t),
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    ESP_LOGI(TAG, "LVGL add display");
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    assert(disp);

    // Add touch to LVGL
    ESP_LOGI(TAG, "LVGL add touch");
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch_handle,
    };
    lv_indev_t *touch_indev = lvgl_port_add_touch(&touch_cfg);
    assert(touch_indev);

    // EEZ UI init
    ui_init();
    lv_timer_create(ui_timer_cb, 20, NULL);
}
