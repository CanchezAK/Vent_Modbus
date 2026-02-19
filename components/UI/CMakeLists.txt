idf_component_register(SRCS
                        "UI.c"
                        "src/ui.c"
                        "src/screens.c"
                        "src/images.c"
                        "src/styles.c"
                        "src/ui_font_de_ja_vu_italic.c"
                        "src/eez-flow.cpp"
                    INCLUDE_DIRS "include" "src"
                    REQUIRES lvgl System_Config)
