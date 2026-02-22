idf_component_register(SRCS "Display.c"
                    INCLUDE_DIRS "include"
                    REQUIRES Peripherials UI System_Config Modbus_RTU waveshare__esp_lcd_hx8394 espressif__esp_lvgl_port espressif__esp_lcd_touch_gt911)
