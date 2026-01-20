#pragma once

#include <stdbool.h>

typedef void (*modbus_rtu_fan_control_cb_t)(bool cooler_one_on, bool cooler_two_on);

void Modbus_RTU_init(void);
void Modbus_RTU_start_task(void);
void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback);
