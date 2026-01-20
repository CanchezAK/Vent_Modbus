idf_component_register(SRCS "Modbus_RTU.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos espressif__esp-modbus)
