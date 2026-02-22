#include <stdio.h>
#include "esp_timer.h"
#include "UI.h"
#include "actions.h"
#include "screens.h"
#include "System_Config.h"

static int64_t s_last_screen_switch_us = 0;
static int32_t s_last_fan_percentage = -1;

static void ui_load_screen_once(lv_obj_t *target)
{
	if (!target) {
		return;
	}

	lv_obj_t *current = lv_scr_act();
	if (current == target) {
		return;
	}

	int64_t now = esp_timer_get_time();
	if ((now - s_last_screen_switch_us) < 150000) {
		return;
	}
	s_last_screen_switch_us = now;

	lv_scr_load(target);
}

void action_clear_alarms(lv_event_t *e)
{
	(void)e;
	System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_DISPLAY, 0);
}

void action_go_to_settings(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.settings);
}

void action_go_to_main(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.main);
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

	if (s_last_fan_percentage == value) {
		return;
	}
	s_last_fan_percentage = value;

	system_config_source_t source;
	uint32_t version;
	(void)source;
	(void)version;
	system_config_t cfg = System_Config_get_snapshot(&source, &version);
	cfg.fan_manual_percent = (uint16_t)value;
	System_Config_set_from_display_volatile(&cfg);
}

void commit_var_fan_percentage(void)
{
	if (s_last_fan_percentage < 0) {
		return;
	}

	system_config_source_t source;
	uint32_t version;
	(void)source;
	(void)version;
	system_config_t cfg = System_Config_get_snapshot(&source, &version);
	cfg.fan_manual_percent = (uint16_t)s_last_fan_percentage;
	System_Config_set_from_display(&cfg);
}
