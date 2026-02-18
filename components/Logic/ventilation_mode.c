#include "logic_modes.h"
#include "logic_internal.h"

void Logic_mode_ventilation(const system_config_t *cfg,
							 const logic_input_t *input,
							 logic_state_t *state,
							 bool *temp_alarm_active,
							 bool *hum_alarm_active,
							 uint8_t *fan_percent)
{
	uint8_t manual_percent = Logic_clamp_percent(cfg->fan_manual_percent);
	(void)Logic_compute_percent(input->current_temp,
							 cfg->temp_desired,
							 cfg->temp_threshold,
							 cfg->temp_alarm,
							 0,
							 temp_alarm_active);
	(void)Logic_compute_percent(input->current_humidity,
							 cfg->humidity_desired,
							 cfg->humidity_threshold,
							 cfg->humidity_alarm,
							 0,
							 hum_alarm_active);

	if (!input->door_open) {
		state->door_boost_active = false;
		state->door_boost_base = manual_percent;
		*fan_percent = manual_percent;
		return;
	}

	if (!state->door_boost_active) {
		state->door_boost_active = true;
		state->door_boost_base = manual_percent;
	}
	uint16_t boosted = (uint16_t)state->door_boost_base * 2U;
	*fan_percent = (boosted > 100) ? 100 : (uint8_t)boosted;
}
