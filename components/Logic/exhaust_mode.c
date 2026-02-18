#include "logic_modes.h"
#include "logic_internal.h"

void Logic_mode_exhaust(const system_config_t *cfg,
						 const logic_input_t *input,
						 bool *temp_alarm_active,
						 uint8_t *fan_percent)
{
	uint8_t min_percent = Logic_clamp_percent(cfg->fan_min_percent);

	*fan_percent = Logic_compute_percent(input->current_temp,
								 cfg->temp_desired,
								 cfg->temp_threshold,
								 cfg->temp_alarm,
							 min_percent,
								 temp_alarm_active);
}
