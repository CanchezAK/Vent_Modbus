#include <stdio.h>
#include "Display.h"
#include "Peripherials.h"
#include "esp_lcd_hx8394.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "lvgl.h"
#include "ui.h"
#include "esp_log.h"
#include "esp_err.h"

#define LCD_H_RES 720
#define LCD_V_RES 1280
#define LCD_DRAW_BUF_LINES_FAST 80
#define LCD_DRAW_BUF_LINES_SAFE 40
#define LCD_TRANS_BUF_LINES_SAFE 20
#define GT911_I2C_ADDR_5D 0x5D
#define GT911_I2C_ADDR_14 0x14
#define LCD_BL_GPIO GPIO_NUM_26
#define LCD_BL_ON_LEVEL 1
#define DISPLAY_ENABLE_TOUCH 1

// Rotate UI by 90° clockwise using LVGL software rotation.
// HX8394 panel driver doesn't support esp_lcd hw swap/mirror.
#define DISPLAY_ROTATE_90_CW 1
#define DISPLAY_SW_ROTATE DISPLAY_ROTATE_90_CW

// Keep touch raw mapping; LVGL display rotation handles UI orientation.
#define TOUCH_SWAP_XY 0
#define TOUCH_MIRROR_X 0
#define TOUCH_MIRROR_Y 0

static const char *TAG = "Display";

static void log_i2c_scan_for_touch(void)
{
    if (i2c_bus_handle == NULL) {
        ESP_LOGW(TAG, "I2C bus handle is NULL, skip scan");
        return;
    }

    int found = 0;
    ESP_LOGI(TAG, "I2C scan start (0x03..0x77)");
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        if (i2c_master_probe(i2c_bus_handle, addr, 10) == ESP_OK) {
            found++;
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
        }
    }
    ESP_LOGI(TAG, "I2C scan done, devices found: %d", found);
}

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

    gpio_config_t bk_gpio_cfg = {
        .pin_bit_mask = 1ULL << LCD_BL_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_cfg));
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL_GPIO, LCD_BL_ON_LEVEL));
    ESP_LOGI(TAG, "Backlight GPIO enabled: %d", (int)LCD_BL_GPIO);

    ESP_LOGI(TAG, "Init panel");
    // Create LCD panel handle
    esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = HX8394_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

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
    esp_err_t ret = esp_lcd_new_panel_hx8394(mipi_dbi_io, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_hx8394 failed: %s", esp_err_to_name(ret));
        return;
    }

    // Reset and init panel
    ESP_LOGI(TAG, "Panel reset");
    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_reset failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Panel init");
    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Panel display on");
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_disp_on_off failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_lcd_touch_handle_t touch_handle = NULL;
#if DISPLAY_ENABLE_TOUCH
    // Initialize touch
    ESP_LOGI(TAG, "Init touch IO");
    esp_lcd_panel_io_handle_t touch_io_handle = NULL;
    uint8_t touch_addr = GT911_I2C_ADDR_5D;

    log_i2c_scan_for_touch();

    esp_err_t probe_err = i2c_master_probe(i2c_bus_handle, GT911_I2C_ADDR_5D, 20);
    if (probe_err != ESP_OK) {
        probe_err = i2c_master_probe(i2c_bus_handle, GT911_I2C_ADDR_14, 20);
        if (probe_err == ESP_OK) {
            touch_addr = GT911_I2C_ADDR_14;
        }
    }
    if (probe_err != ESP_OK) {
        ESP_LOGW(TAG, "GT911 probe failed on 0x%02X and 0x%02X, continue without touch",
                 GT911_I2C_ADDR_5D, GT911_I2C_ADDR_14);
    }

    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    touch_io_config.dev_addr = touch_addr;
    touch_io_config.scl_speed_hz = 100000;

    esp_err_t touch_err = ESP_FAIL;
    if (probe_err == ESP_OK) {
        touch_err = esp_lcd_new_panel_io_i2c(i2c_bus_handle, &touch_io_config, &touch_io_handle);
        if (touch_err != ESP_OK) {
            ESP_LOGW(TAG, "GT911 IO create failed on 0x%02X: %s", touch_addr, esp_err_to_name(touch_err));
        }
    }

    ESP_LOGI(TAG, "Init touch controller");
    esp_lcd_touch_io_gt911_config_t gt911_cfg = {
        .dev_addr = touch_addr,
    };
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
            .swap_xy = TOUCH_SWAP_XY,
            .mirror_x = TOUCH_MIRROR_X,
            .mirror_y = TOUCH_MIRROR_Y,
        },
        .driver_data = &gt911_cfg,
    };
    if (touch_err == ESP_OK) {
        touch_err = esp_lcd_touch_new_i2c_gt911(touch_io_handle, &touch_config, &touch_handle);
    }
    if (touch_err != ESP_OK) {
        ESP_LOGW(TAG, "GT911 init failed on both addresses, continue without touch: %s", esp_err_to_name(touch_err));
        touch_handle = NULL;
    }
#else
    ESP_LOGW(TAG, "GT911 touch is temporarily disabled (DISPLAY_ENABLE_TOUCH=0)");
#endif

    // LVGL init
    ESP_LOGI(TAG, "LVGL port init");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    // Add display to LVGL
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_DRAW_BUF_LINES_FAST,
        .double_buffer = true,
        .trans_size = 0,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = DISPLAY_SW_ROTATE,
        },
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    ESP_LOGI(TAG, "LVGL add display (DSI)");
    const lvgl_port_display_dsi_cfg_t dsi_disp_cfg = {
        .flags = {
            .avoid_tearing = 0,
        },
    };
    lv_disp_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_disp_cfg);
    if (!disp) {
        ESP_LOGW(TAG, "Fast LVGL buffers failed, fallback to PSRAM buffers");
        disp_cfg.buffer_size = LCD_H_RES * LCD_DRAW_BUF_LINES_SAFE;
        disp_cfg.trans_size = LCD_H_RES * LCD_TRANS_BUF_LINES_SAFE;
        disp_cfg.flags.buff_dma = 0;
        disp_cfg.flags.buff_spiram = 1;
        disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_disp_cfg);
    }
    if (!disp) {
        ESP_LOGE(TAG, "Failed to add LVGL display");
        return;
    }

#if DISPLAY_ROTATE_90_CW
    lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_90);
#endif

    // Add touch to LVGL
    if (touch_handle) {
        ESP_LOGI(TAG, "LVGL add touch");
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = touch_handle,
        };
        lv_indev_t *touch_indev = lvgl_port_add_touch(&touch_cfg);
        assert(touch_indev);
    }

    // EEZ UI init
    ESP_LOGI(TAG, "UI init start");
    if (lvgl_port_lock(0)) {
        ui_init();
        lv_timer_create(ui_timer_cb, 20, NULL);
        lvgl_port_unlock();
        ESP_LOGI(TAG, "UI init done");
    } else {
        ESP_LOGE(TAG, "Failed to lock LVGL for UI init");
    }
}
