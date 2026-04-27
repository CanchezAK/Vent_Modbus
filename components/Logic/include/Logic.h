#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "System_Config.h"

typedef struct {
	bool alarm_temp_latched;
	bool alarm_humidity_latched;
	bool alarm_smoke_latched;
	bool alarm_fan_latched;
	bool alarm_filter_latched;
	uint16_t last_alarm_clr;
	bool door_boost_active;
	uint8_t door_boost_base;
} logic_state_t;

typedef struct {
	uint16_t current_temp;
	uint16_t current_humidity;
	bool door_open;
	bool smoke_state;
	bool fan_alarm;
	bool filter_alarm;
} logic_input_t;

typedef struct {
	uint8_t fan_percent;
	bool alarm_temp;
	bool alarm_humidity;
	bool alarm_smoke;
	bool alarm_fan;
	bool alarm_filter;
} logic_output_t;

void Logic_init(logic_state_t *state);
void Logic_step(logic_state_t *state,
				const system_config_t *cfg,
				uint16_t alarm_clr,
				const logic_input_t *input,
				logic_output_t *output);
