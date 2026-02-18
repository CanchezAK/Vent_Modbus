#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "System_Config.h"
#include "Logic.h"

void Logic_mode_auto(const system_config_t *cfg,
					 const logic_input_t *input,
					 bool *temp_alarm_active,
					 bool *hum_alarm_active,
					 uint8_t *fan_percent);

void Logic_mode_ventilation(const system_config_t *cfg,
							 const logic_input_t *input,
							 logic_state_t *state,
							 bool *temp_alarm_active,
							 bool *hum_alarm_active,
							 uint8_t *fan_percent);

void Logic_mode_exhaust(const system_config_t *cfg,
						 const logic_input_t *input,
						 bool *temp_alarm_active,
						 uint8_t *fan_percent);
