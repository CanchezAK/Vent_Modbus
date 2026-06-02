#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef ENABLE_SERVICE_ALARM_TOGGLES
#define ENABLE_SERVICE_ALARM_TOGGLES 1
#endif

#define SYSTEM_CONFIG_SERVICE_PIN "0000"
#define SYSTEM_CONFIG_SETTINGS_PIN "6812"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SYSTEM_CONFIG_SOURCE_MODBUS = 0,
	SYSTEM_CONFIG_SOURCE_DISPLAY = 1,
	SYSTEM_CONFIG_SOURCE_INTERNAL = 2,
} system_config_source_t;

typedef struct {
	uint16_t mode;
	uint16_t temp_desired;
	uint16_t temp_threshold;
	uint16_t temp_alarm;
	uint16_t humidity_desired;
	uint16_t humidity_threshold;
	uint16_t humidity_alarm;
	uint16_t fan_manual_percent;
	uint16_t fan_min_percent;
	uint16_t filter_limit_hours;
	uint16_t system_enable;
	uint16_t smoke_enable;
	uint16_t modbus_addr;
	uint16_t modbus_baud;
} system_config_t;

void System_Config_init(void);
void System_Config_set_from_modbus(const system_config_t *cfg);
void System_Config_set_from_display(const system_config_t *cfg);
void System_Config_set_from_display_volatile(const system_config_t *cfg);
void System_Config_set_from_display_persist(const system_config_t *cfg);
void System_Config_set_from_internal(const system_config_t *cfg);

system_config_t System_Config_get_snapshot(system_config_source_t *source, uint32_t *version);

uint16_t System_Config_get_alarm_clr(void);
void System_Config_set_alarm_clr(system_config_source_t source, uint16_t value);

bool System_Config_get_smoke_temp_only_stop_enabled(void);
void System_Config_set_smoke_temp_only_stop_enabled_volatile(bool enabled);
void System_Config_set_smoke_temp_only_stop_enabled_persist(bool enabled);

#if ENABLE_SERVICE_ALARM_TOGGLES
bool System_Config_get_alarm_temp_enabled(void);
void System_Config_set_alarm_temp_enabled_volatile(bool enabled);
bool System_Config_get_alarm_humidity_enabled(void);
void System_Config_set_alarm_humidity_enabled_volatile(bool enabled);
bool System_Config_get_alarm_smoke_enabled(void);
void System_Config_set_alarm_smoke_enabled_volatile(bool enabled);
bool System_Config_get_alarm_fan_enabled(void);
void System_Config_set_alarm_fan_enabled_volatile(bool enabled);
bool System_Config_get_alarm_filter_enabled(void);
void System_Config_set_alarm_filter_enabled_volatile(bool enabled);
#endif

bool System_Config_get_telemetry_enabled(void);
void System_Config_set_telemetry_enabled_volatile(bool enabled);
void System_Config_save_ui_flags(void);

uint16_t System_Config_get_service_hours(void);
uint32_t System_Config_get_service_seconds(void);
void System_Config_set_service_hours(uint16_t value);

void System_Config_get_phase_params(uint32_t *ac_half_cycle_us,
									 uint32_t *triac_min_delay_us,
									 uint32_t *triac_pulse_us);

/* Register one task to be notified on config/alarm/service-hours changes */
void System_Config_register_update_task(void *task_handle);

#ifdef __cplusplus
}
#endif
