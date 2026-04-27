#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void (*modbus_rtu_fan_control_cb_t)(uint8_t fan_percent);

typedef struct {
	uint16_t alarm_temp;
	uint16_t alarm_humidity;
	uint16_t alarm_smoke;
	uint16_t alarm_fan;
	uint16_t alarm_filter;
	uint16_t current_temp;
	uint16_t current_humidity;
	uint16_t door_state;
	uint16_t smoke_state;
	uint16_t vent_active;
	uint16_t fan_percent;
	uint16_t filter_clogging_percent;
	uint16_t service_hours;
} modbus_rtu_ro_snapshot_t;

void Modbus_RTU_init(void);
void Modbus_RTU_start_task(void);
void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback);
bool Modbus_RTU_get_ro_snapshot(uint16_t *current_temp, uint16_t *current_humidity);
bool Modbus_RTU_get_ro_full_snapshot(modbus_rtu_ro_snapshot_t *out);
void Modbus_RTU_register_ro_update_task(void *task_handle);

/* Runtime diagnostics for UI debug */
uint16_t Modbus_RTU_get_diag_addr(void);
uint16_t Modbus_RTU_get_diag_baud_index(void);
uint32_t Modbus_RTU_get_diag_baud_requested(void);
uint32_t Modbus_RTU_get_diag_baud_actual(void);
