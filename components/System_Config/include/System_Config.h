#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SYSTEM_CONFIG_SOURCE_MODBUS = 0,
	SYSTEM_CONFIG_SOURCE_DISPLAY = 1,
	SYSTEM_CONFIG_SOURCE_INTERNAL = 2,
} system_config_source_t;

typedef struct {
	uint16_t temp_desired_centi;
	uint16_t temp_threshold_centi;
	uint16_t temp_alarm_centi;
	uint16_t humidity_desired_centi;
	uint16_t humidity_threshold_centi;
	uint16_t humidity_alarm_centi;
	uint8_t fan1_mode_manual;
	uint8_t fan2_mode_manual;
	uint8_t fan1_percent;
	uint8_t fan2_percent;
} system_config_t;

void System_Config_init(void);
void System_Config_set_from_modbus(const system_config_t *cfg);
void System_Config_set_from_display(const system_config_t *cfg);
void System_Config_set_from_internal(const system_config_t *cfg);

system_config_t System_Config_get_snapshot(system_config_source_t *source, uint32_t *version);

uint16_t System_Config_get_alarm_clr(void);
void System_Config_set_alarm_clr(system_config_source_t source, uint16_t value);

void System_Config_get_phase_params(uint32_t *ac_half_cycle_us,
									 uint32_t *triac_min_delay_us,
									 uint32_t *triac_pulse_us);

#ifdef __cplusplus
}
#endif
