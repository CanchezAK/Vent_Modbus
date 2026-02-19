#include <stdio.h>
#include "esp_log.h"
#include "UI.h"
#include "actions.h"
#include "screens.h"
#include "System_Config.h"

static const char *TAG = "UI";

void action_clear_alarms(lv_event_t *e)
{
	(void)e;
	System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_DISPLAY, 0);
}

void action_go_to_settings(lv_event_t *e)
{
	(void)e;
	if (objects.settings) {
		lv_scr_load_anim(objects.settings, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
	}
}

void action_go_to_main(lv_event_t *e)
{
	(void)e;
	if (objects.main) {
		lv_scr_load_anim(objects.main, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
	}
}

int32_t get_var_fan_percentage(void)
{
	system_config_source_t source;
	uint32_t version;
	(void)source;
	(void)version;
	system_config_t cfg = System_Config_get_snapshot(&source, &version);
	return (int32_t)cfg.fan_manual_percent;
}

void set_var_fan_percentage(int32_t value)
{
	if (value < 0) {
		value = 0;
	} else if (value > 100) {
		value = 100;
	}

	ESP_LOGI(TAG, "fan_percentage=%ld", (long)value);

	system_config_source_t source;
	uint32_t version;
	(void)source;
	(void)version;
	system_config_t cfg = System_Config_get_snapshot(&source, &version);
	cfg.fan_manual_percent = (uint16_t)value;
	System_Config_set_from_display(&cfg);
}
