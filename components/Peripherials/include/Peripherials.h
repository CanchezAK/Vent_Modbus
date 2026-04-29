#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_lcd_mipi_dsi.h"

extern esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
extern i2c_master_bus_handle_t i2c_bus_handle;
extern i2c_master_bus_handle_t i2c_bus_sensors_handle;

void Peripherials_init(void);
void Peripherials_init_dsi(void);
void Peripherials_set_fan(uint8_t percent);
void Peripherials_set_buzzer(bool enabled);
uint8_t Peripherials_get_fan_percent(void);
bool Peripherials_get_door_state(void);
bool Peripherials_get_smoke_state(void);
bool Peripherials_get_fan_alarm_state(void);
