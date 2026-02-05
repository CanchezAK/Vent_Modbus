idf_component_register(SRCS "Peripherials.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver Modbus_RTU esp_lcd System_Config SHT31)
