idf_component_register(SRCS "Peripherials.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_lcd System_Config SHT31 esp_timer)
