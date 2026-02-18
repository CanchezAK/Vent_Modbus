#include "logic_modes.h"
#include "logic_internal.h"

void Logic_mode_auto(const system_config_t *cfg,
					 const logic_input_t *input,
					 bool *temp_alarm_active,
					 bool *hum_alarm_active,
					 uint8_t *fan_percent)
{
	uint8_t min_percent = Logic_clamp_percent(cfg->fan_min_percent);

	uint8_t temp_percent = Logic_compute_percent(input->current_temp,
									 cfg->temp_desired,
									 cfg->temp_threshold,
									 cfg->temp_alarm,
							 min_percent,
									 temp_alarm_active);

	uint8_t hum_percent = Logic_compute_percent(input->current_humidity,
									 cfg->humidity_desired,
									 cfg->humidity_threshold,
									 cfg->humidity_alarm,
							 min_percent,
									 hum_alarm_active);

	*fan_percent = (temp_percent > hum_percent) ? temp_percent : hum_percent;
}
