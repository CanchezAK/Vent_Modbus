idf_component_register(SRCS
                        "UI.c"
                        "src/ui.c"
                        "src/screens.c"
                        "src/images.c"
                        "src/styles.c"
                    INCLUDE_DIRS "include" "src"
                    REQUIRES lvgl System_Config Modbus_RTU)
