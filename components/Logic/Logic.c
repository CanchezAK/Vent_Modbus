#include "Logic.h"
#include "logic_internal.h"

#define MODE_MANUAL_BIT              (1U << 0)
#define MODE_AUTOSTART_TEMP_BIT      (1U << 1)
#define MODE_AUTOSTART_HUMIDITY_BIT  (1U << 2)
#define AUTO_ALARM_DELTA             (10U)

/* TEMPORARY: disable smoke alarm path until smoke sensor is physically installed */
#define TEMP_DISABLE_SMOKE_ALARM     (0U)
/* TEMPORARY: disable fan alarm path */
#define TEMP_DISABLE_FAN_ALARM       (0U)

#if ENABLE_SERVICE_ALARM_TOGGLES
static bool s_alarm_enable_temp = true;
static bool s_alarm_enable_humidity = true;
static bool s_alarm_enable_smoke = true;
static bool s_alarm_enable_fan = true;
static bool s_alarm_enable_filter = true;

bool Logic_get_alarm_temp_enabled(void) { return s_alarm_enable_temp; }
void Logic_set_alarm_temp_enabled(bool enabled) { s_alarm_enable_temp = enabled; }
bool Logic_get_alarm_humidity_enabled(void) { return s_alarm_enable_humidity; }
void Logic_set_alarm_humidity_enabled(bool enabled) { s_alarm_enable_humidity = enabled; }
bool Logic_get_alarm_smoke_enabled(void) { return s_alarm_enable_smoke; }
void Logic_set_alarm_smoke_enabled(bool enabled) { s_alarm_enable_smoke = enabled; }
bool Logic_get_alarm_fan_enabled(void) { return s_alarm_enable_fan; }
void Logic_set_alarm_fan_enabled(bool enabled) { s_alarm_enable_fan = enabled; }
bool Logic_get_alarm_filter_enabled(void) { return s_alarm_enable_filter; }
void Logic_set_alarm_filter_enabled(bool enabled) { s_alarm_enable_filter = enabled; }
#endif

static uint8_t compute_auto_channel_percent(uint16_t current_value,
										uint16_t desired_value,
										uint8_t min_percent,
										bool *alarm_active)
{
	if (current_value <= desired_value) {
		if (alarm_active) {
			*alarm_active = false;
		}
		return 0;
	}

	uint16_t alarm_value = (uint16_t)(desired_value + AUTO_ALARM_DELTA);
	if (alarm_value < desired_value) {
		alarm_value = UINT16_MAX;
	}

	if (alarm_active) {
		*alarm_active = (current_value >= alarm_value);
	}

	uint32_t exceed = (uint32_t)(current_value - desired_value);
	if (exceed >= AUTO_ALARM_DELTA) {
		return 100;
	}

	uint32_t scale = (uint32_t)(100U - min_percent);
	uint32_t value = (uint32_t)min_percent + ((exceed * scale) + (AUTO_ALARM_DELTA / 2U)) / AUTO_ALARM_DELTA;
	if (value > 100U) {
		value = 100U;
	}
	return (uint8_t)value;
}

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
	state->alarm_fan_latched = false;
	state->alarm_filter_latched = false;
	state->last_alarm_clr = 0;
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
		state->alarm_fan_latched = false;
		state->alarm_filter_latched = false;
		state->last_alarm_clr = alarm_clr;
		state->door_boost_active = false;
		return;
	}

	bool temp_alarm_enabled = true;
	bool hum_alarm_enabled = true;
	bool smoke_alarm_enabled = true;
	bool fan_alarm_enabled = true;
	bool filter_alarm_enabled = true;
#if ENABLE_SERVICE_ALARM_TOGGLES
	temp_alarm_enabled = s_alarm_enable_temp;
	hum_alarm_enabled = s_alarm_enable_humidity;
	smoke_alarm_enabled = s_alarm_enable_smoke;
	fan_alarm_enabled = s_alarm_enable_fan;
	filter_alarm_enabled = s_alarm_enable_filter;
#endif

	bool smoke_enabled = smoke_alarm_enabled;
#if TEMP_DISABLE_SMOKE_ALARM
	smoke_enabled = false;
#endif
	if (!smoke_enabled) {
		state->alarm_smoke_latched = false;
	}

	if (!fan_alarm_enabled) {
		state->alarm_fan_latched = false;
	}
	if (!filter_alarm_enabled) {
		state->alarm_filter_latched = false;
	}
#if TEMP_DISABLE_FAN_ALARM
	state->alarm_fan_latched = false;
#endif
	if (!temp_alarm_enabled) {
		state->alarm_temp_latched = false;
	}
	if (!hum_alarm_enabled) {
		state->alarm_humidity_latched = false;
	}

	bool temp_alarm_active = false;
	bool hum_alarm_active = false;
	uint8_t fan_percent = 0;
	uint16_t mode_flags = cfg->mode;
	bool manual_enabled = (mode_flags & MODE_MANUAL_BIT) != 0;
	bool auto_temp_enabled = (mode_flags & MODE_AUTOSTART_TEMP_BIT) != 0;
	bool auto_humidity_enabled = (mode_flags & MODE_AUTOSTART_HUMIDITY_BIT) != 0;
	uint8_t manual_percent = Logic_clamp_percent(cfg->fan_manual_percent);
	/*
	 * Single source of truth for fan setpoint from UI/Modbus:
	 * auto-min and manual setpoint are the same parameter.
	 */
	uint8_t min_percent = manual_percent;

	if (manual_enabled) {
		fan_percent = manual_percent;
		uint32_t temp_alarm_value = (uint32_t)cfg->temp_desired + AUTO_ALARM_DELTA;
		uint32_t hum_alarm_value = (uint32_t)cfg->humidity_desired + AUTO_ALARM_DELTA;
		temp_alarm_active = (uint32_t)input->current_temp >= temp_alarm_value;
		hum_alarm_active = (uint32_t)input->current_humidity >= hum_alarm_value;
	} else {
		uint8_t temp_percent = 0;
		uint8_t hum_percent = 0;

		if (auto_temp_enabled) {
			temp_percent = compute_auto_channel_percent(input->current_temp,
											cfg->temp_desired,
											min_percent,
											&temp_alarm_active);
		}
		if (auto_humidity_enabled) {
			hum_percent = compute_auto_channel_percent(input->current_humidity,
										cfg->humidity_desired,
										min_percent,
										&hum_alarm_active);
		}

		fan_percent = (temp_percent > hum_percent) ? temp_percent : hum_percent;
	}
	if (!temp_alarm_enabled) {
		temp_alarm_active = false;
	}
	if (!hum_alarm_enabled) {
		hum_alarm_active = false;
	}

	if (temp_alarm_enabled && temp_alarm_active) {
		state->alarm_temp_latched = true;
	}
	if (hum_alarm_enabled && hum_alarm_active) {
		state->alarm_humidity_latched = true;
	}
	if (smoke_enabled && input->smoke_state) {
		state->alarm_smoke_latched = true;
	}

	if (fan_alarm_enabled) {
#if !TEMP_DISABLE_FAN_ALARM
		if (input->fan_alarm) {
			state->alarm_fan_latched = true;
		}
#endif
	}
	if (filter_alarm_enabled && input->filter_alarm) {
		state->alarm_filter_latched = true;
	}

	bool clear_request = (state->last_alarm_clr != 0U) && (alarm_clr == 0U);
	state->last_alarm_clr = alarm_clr;

	if (clear_request) {
		if (!temp_alarm_active || !temp_alarm_enabled) {
			state->alarm_temp_latched = false;
		}
		if (!hum_alarm_active || !hum_alarm_enabled) {
			state->alarm_humidity_latched = false;
		}
		if (!(smoke_enabled && input->smoke_state)) {
			state->alarm_smoke_latched = false;
		}

		if (!fan_alarm_enabled) {
			state->alarm_fan_latched = false;
		} else {
#if !TEMP_DISABLE_FAN_ALARM
			if (!input->fan_alarm) {
				state->alarm_fan_latched = false;
			}
#else
			state->alarm_fan_latched = false;
#endif
		}
		if (!filter_alarm_enabled || !input->filter_alarm) {
			state->alarm_filter_latched = false;
		}
	}

	bool any_alarm = state->alarm_temp_latched || state->alarm_humidity_latched || state->alarm_smoke_latched;
	if (smoke_enabled && input->smoke_state) {
		fan_percent = 100;
	}

	if (fan_alarm_enabled) {
#if !TEMP_DISABLE_FAN_ALARM
		if (input->fan_alarm) {
			fan_percent = 0;
		}
#endif
	}

	output->fan_percent = fan_percent;
	output->alarm_temp = temp_alarm_enabled ? state->alarm_temp_latched : false;
	output->alarm_humidity = hum_alarm_enabled ? state->alarm_humidity_latched : false;
	output->alarm_smoke = smoke_alarm_enabled ? state->alarm_smoke_latched : false;
	output->alarm_fan = fan_alarm_enabled ? state->alarm_fan_latched : false;
	output->alarm_filter = filter_alarm_enabled ? state->alarm_filter_latched : false;

	(void)any_alarm;
}
