#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void (*modbus_rtu_fan_control_cb_t)(uint8_t fan_percent);

void Modbus_RTU_init(void);
void Modbus_RTU_start_task(void);
void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback);
bool Modbus_RTU_get_ro_snapshot(uint16_t *current_temp, uint16_t *current_humidity);
void Modbus_RTU_register_ro_update_task(void *task_handle);
