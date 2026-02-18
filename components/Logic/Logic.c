#include "Logic.h"
#include "logic_internal.h"
#include "logic_modes.h"

#define MODE_AUTO        (0)
#define MODE_VENTILATION (1)
#define MODE_EXHAUST     (2)

uint8_t Logic_clamp_percent(uint16_t value)
{
	return (value > 100) ? 100 : (uint8_t)value;
}

uint8_t Logic_compute_percent(uint16_t current_value,
						  uint16_t desired_value,
						  uint16_t threshold_value,
						  uint16_t alarm_value,
					  uint8_t min_percent,
						  bool *alarm_active)
{
	uint32_t error = (current_value > desired_value) ?
			(uint32_t)(current_value - desired_value) :
			(uint32_t)(desired_value - current_value);
	uint32_t threshold_error = (threshold_value > desired_value) ?
			(uint32_t)(threshold_value - desired_value) :
			(uint32_t)(desired_value - threshold_value);
	uint32_t alarm_error = (alarm_value > desired_value) ?
			(uint32_t)(alarm_value - desired_value) :
			(uint32_t)(desired_value - alarm_value);

	if (alarm_active) {
		*alarm_active = (error >= alarm_error);
	}

	if (current_value <= desired_value) {
		return min_percent;
	}
	if (error >= alarm_error && alarm_error > 0) {
		return 100;
	}
	if (threshold_error == 0) {
		return (error > 0) ? 100 : min_percent;
	}
	if (error >= threshold_error) {
		return 100;
	}
	// Вся ошибка нормализуется к диапазону 0..100, затем смещается на min_percent.
	uint32_t raw_percent = (error * 100U) / threshold_error;
	uint32_t scale = (uint32_t)(100 - min_percent);
	uint32_t value = min_percent + ((raw_percent * scale) + 50U) / 100U;
	if (value > 100) {
		value = 100;
	}
	return (uint8_t)value;
}

void Logic_init(logic_state_t *state)
{
	if (!state) {
		return;
	}
	state->alarm_temp_latched = false;
	state->alarm_humidity_latched = false;
	state->alarm_smoke_latched = false;
	state->door_boost_active = false;
	state->door_boost_base = 0;
}

void Logic_step(logic_state_t *state,
				const system_config_t *cfg,
				uint16_t alarm_clr,
				const logic_input_t *input,
				logic_output_t *output)
{
	if (!state || !cfg || !input || !output) {
		return;
	}

	output->fan_percent = 0;
	output->alarm_temp = false;
	output->alarm_humidity = false;
	output->alarm_smoke = false;
	output->alarm_fan = false;
	output->alarm_filter = false;

	if (!cfg->system_enable) {
		state->alarm_temp_latched = false;
		state->alarm_humidity_latched = false;
		state->alarm_smoke_latched = false;
		state->door_boost_active = false;
		return;
	}

	bool smoke_enabled = cfg->smoke_enable != 0;
	if (!smoke_enabled) {
		state->alarm_smoke_latched = false;
	}

	bool temp_alarm_active = false;
	bool hum_alarm_active = false;
	uint8_t fan_percent = 0;

	switch (cfg->mode) {
	case MODE_VENTILATION:
		Logic_mode_ventilation(cfg, input, state, &temp_alarm_active, &hum_alarm_active, &fan_percent);
		break;
	case MODE_EXHAUST:
		Logic_mode_exhaust(cfg, input, &temp_alarm_active, &fan_percent);
		break;
	case MODE_AUTO:
	default:
		Logic_mode_auto(cfg, input, &temp_alarm_active, &hum_alarm_active, &fan_percent);
		break;
	}

	if (temp_alarm_active) {
		state->alarm_temp_latched = true;
	}
	if (hum_alarm_active) {
		state->alarm_humidity_latched = true;
	}
	if (smoke_enabled && input->smoke_state) {
		state->alarm_smoke_latched = true;
	}

	if (alarm_clr == 0) {
		if (!temp_alarm_active) {
			state->alarm_temp_latched = false;
		}
		if (!hum_alarm_active) {
			state->alarm_humidity_latched = false;
		}
		if (!(smoke_enabled && input->smoke_state)) {
			state->alarm_smoke_latched = false;
		}
	}

	bool any_alarm = state->alarm_temp_latched || state->alarm_humidity_latched || state->alarm_smoke_latched;
	if (smoke_enabled && input->smoke_state) {
		fan_percent = (cfg->mode == MODE_EXHAUST) ? 0 : 100;
	} else if (cfg->mode != MODE_VENTILATION && (state->alarm_temp_latched || state->alarm_humidity_latched)) {
		fan_percent = 100;
	} else if (cfg->mode == MODE_VENTILATION && (state->alarm_temp_latched || state->alarm_humidity_latched)) {
		fan_percent = 100;
	}
	if (input->fan_alarm) {
		fan_percent = 0;
	}

	output->fan_percent = fan_percent;
	output->alarm_temp = state->alarm_temp_latched;
	output->alarm_humidity = state->alarm_humidity_latched;
	output->alarm_smoke = state->alarm_smoke_latched;
	output->alarm_fan = input->fan_alarm;
	output->alarm_filter = input->filter_alarm;

	(void)any_alarm;
}
