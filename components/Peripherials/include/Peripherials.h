#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_lcd_mipi_dsi.h"

extern esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
extern i2c_master_bus_handle_t i2c_bus_handle;

void Peripherials_init(void);
void Peripherials_init_dsi(void);
void Peripherials_set_fans(uint8_t cooler_one_percent, uint8_t cooler_two_percent);
