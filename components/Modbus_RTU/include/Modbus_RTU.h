#pragma once

#include <stdint.h>

typedef void (*modbus_rtu_fan_control_cb_t)(uint8_t fan_percent);

void Modbus_RTU_init(void);
void Modbus_RTU_start_task(void);
void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback);
