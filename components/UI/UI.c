#include <stdio.h>
#include "esp_timer.h"
#include "esp_system.h"
#include "UI.h"
#include "actions.h"
#include "screens.h"
#include "vars.h"
#include "System_Config.h"
#include "Modbus_RTU.h"
#include "Peripherials.h"
#include "Logic.h"

static int64_t s_last_screen_switch_us = 0;
static int32_t s_modbus_addr = 1;
static int32_t s_modbus_baud_idx = 3;
static bool s_modbus_staged_initialized = false;

#define MODE_MANUAL_BIT              (1U << 0)
#define MODE_AUTOSTART_TEMP_BIT      (1U << 1)
#define MODE_AUTOSTART_HUMIDITY_BIT  (1U << 2)

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

void action_go_to_modbus(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.modbus);
}

void action_go_to_service(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.service);
}

void action_go_to_service_settings(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.service_settings);
}

void action_go_to_service_filter_confirm(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.service_filter_confirm);
}

void action_go_to_service_pin(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.service_pin);
}

void action_go_to_settings_pin(lv_event_t *e)
{
	(void)e;
	ui_load_screen_once(objects.settings_pin);
}

static system_config_t cfg_snapshot(void)
{
	system_config_source_t source;
	uint32_t version;
	(void)source;
	(void)version;
	return System_Config_get_snapshot(&source, &version);
}

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static bool ro_snapshot(modbus_rtu_ro_snapshot_t *out)
{
	if (!out) {
		return false;
	}
	return Modbus_RTU_get_ro_full_snapshot(out);
}

static void ensure_modbus_staged(void)
{
	if (s_modbus_staged_initialized) {
		return;
	}
	system_config_t cfg = cfg_snapshot();
	s_modbus_addr = (int32_t)cfg.modbus_addr;
	s_modbus_baud_idx = (int32_t)cfg.modbus_baud;
	s_modbus_staged_initialized = true;
}

void action_save_settings(lv_event_t *e)
{
	(void)e;
	system_config_t cfg = cfg_snapshot();
	System_Config_set_from_display_persist(&cfg);
	action_save_smoke_temp_only_stop_setting();
	ui_load_screen_once(objects.main);
}

void action_save_smoke_temp_only_stop_setting(void)
{
	System_Config_set_smoke_temp_only_stop_enabled_persist(
		System_Config_get_smoke_temp_only_stop_enabled());
}

void action_save_modbus_and_reboot(lv_event_t *e)
{
	(void)e;
	ensure_modbus_staged();

	system_config_t cfg = cfg_snapshot();
	cfg.modbus_addr = (uint16_t)clamp_i32(s_modbus_addr, 1, 247);
	cfg.modbus_baud = (uint16_t)clamp_i32(s_modbus_baud_idx, 0, 3);
	System_Config_set_from_display_persist(&cfg);
	esp_restart();
}

void action_clear_filter_hours(lv_event_t *e)
{
	(void)e;
	System_Config_set_service_hours(0);
}

void action_clear_alarms(lv_event_t *e)
{
	(void)e;
	System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_DISPLAY, 0);
}

int32_t action_get_modbus_addr(void)
{
	ensure_modbus_staged();
	return s_modbus_addr;
}

void action_set_modbus_addr(int32_t addr)
{
	ensure_modbus_staged();
	s_modbus_addr = clamp_i32(addr, 1, 247);
}

int32_t action_get_modbus_baud_index(void)
{
	ensure_modbus_staged();
	return s_modbus_baud_idx;
}

void action_set_modbus_baud_index(int32_t idx)
{
	ensure_modbus_staged();
	s_modbus_baud_idx = clamp_i32(idx, 0, 3);
}

int32_t action_get_filter_limit_hours(void)
{
	system_config_t cfg = cfg_snapshot();
	return (int32_t)cfg.filter_limit_hours;
}

void action_set_filter_limit_hours(int32_t hours)
{
	system_config_t cfg = cfg_snapshot();
	cfg.filter_limit_hours = (uint16_t)clamp_i32(hours, 0, 10000);
	System_Config_set_from_display_volatile(&cfg);
}

int32_t action_get_service_hours(void)
{
	return (int32_t)System_Config_get_service_hours();
}

int32_t get_var_temp(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return (int32_t)ro.current_temp;
	}
	return 0;
}

void set_var_temp(int32_t value)
{
	(void)value;
}

int32_t get_var_humidity(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return (int32_t)ro.current_humidity;
	}
	return 0;
}

void set_var_humidity(int32_t value)
{
	(void)value;
}

int32_t get_var_filter_clogging(void)
{
	system_config_t cfg = cfg_snapshot();
	uint16_t limit = cfg.filter_limit_hours;
	uint32_t service_seconds = System_Config_get_service_seconds();
	if (limit == 0) {
		return 0;
	}
	/* Use fractional hours: percent = (service_hours / limit_hours) * 100 */
	uint32_t limit_seconds = (uint32_t)limit * 3600U;
	if (limit_seconds == 0U) {
		return 0;
	}
	uint32_t percent = (service_seconds * 100U) / limit_seconds;
	if (percent > 100U) {
		percent = 100U;
	}
	return (int32_t)percent;
}

void set_var_filter_clogging(int32_t value)
{
	(void)value;
}

int32_t get_var_door(void)
{
	return Peripherials_get_door_state() ? 255 : 0;
}

void set_var_door(int32_t value)
{
	(void)value;
}

int32_t get_var_smoke(void)
{
	return Peripherials_get_smoke_state() ? 255 : 0;
}

void set_var_smoke(int32_t value)
{
	(void)value;
}

int32_t get_var_vent_active(void)
{
	return (Peripherials_get_fan_percent() > 0) ? 255 : 0;
}

void set_var_vent_active(int32_t value)
{
	(void)value;
}

int32_t get_var_filter_alarm(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return ro.alarm_filter ? 255 : 0;
	}
	return 0;
}

void set_var_filter_alarm(int32_t value)
{
	(void)value;
}

int32_t get_var_set_start_temp_var(void)
{
	system_config_t cfg = cfg_snapshot();
	return (int32_t)cfg.temp_desired;
}

void set_var_set_start_temp_var(int32_t value)
{
	system_config_t cfg = cfg_snapshot();
	cfg.temp_desired = (uint16_t)clamp_i32(value, 0, 200);
	System_Config_set_from_display_volatile(&cfg);
}

int32_t get_var_set_start_humidity_var(void)
{
	system_config_t cfg = cfg_snapshot();
	return (int32_t)cfg.humidity_desired;
}

void set_var_set_start_humidity_var(int32_t value)
{
	system_config_t cfg = cfg_snapshot();
	cfg.humidity_desired = (uint16_t)clamp_i32(value, 0, 100);
	System_Config_set_from_display_volatile(&cfg);
}

int32_t get_var_vent_power_var(void)
{
	system_config_t cfg = cfg_snapshot();
	return (int32_t)cfg.fan_manual_percent;
}

void set_var_vent_power_var(int32_t value)
{
	system_config_t cfg = cfg_snapshot();
	cfg.fan_manual_percent = (uint16_t)clamp_i32(value, 0, 100);
	cfg.fan_min_percent = cfg.fan_manual_percent;
	System_Config_set_from_display_volatile(&cfg);
}

bool get_var_autostart_temp_bool(void)
{
	system_config_t cfg = cfg_snapshot();
	return (cfg.mode & MODE_AUTOSTART_TEMP_BIT) != 0;
}

void set_var_autostart_temp_bool(bool value)
{
	system_config_t cfg = cfg_snapshot();
	if (value) {
		cfg.mode &= (uint16_t)~MODE_MANUAL_BIT;
		cfg.mode |= MODE_AUTOSTART_TEMP_BIT;
	} else {
		cfg.mode &= (uint16_t)~MODE_AUTOSTART_TEMP_BIT;
	}
	System_Config_set_from_display_volatile(&cfg);
}

bool get_var_autostart_humidity_bool(void)
{
	system_config_t cfg = cfg_snapshot();
	return (cfg.mode & MODE_AUTOSTART_HUMIDITY_BIT) != 0;
}

void set_var_autostart_humidity_bool(bool value)
{
	system_config_t cfg = cfg_snapshot();
	if (value) {
		cfg.mode &= (uint16_t)~MODE_MANUAL_BIT;
		cfg.mode |= MODE_AUTOSTART_HUMIDITY_BIT;
	} else {
		cfg.mode &= (uint16_t)~MODE_AUTOSTART_HUMIDITY_BIT;
	}
	System_Config_set_from_display_volatile(&cfg);
}

bool get_var_manual_power_bool(void)
{
	system_config_t cfg = cfg_snapshot();
	return (cfg.mode & MODE_MANUAL_BIT) != 0;
}

void set_var_manual_power_bool(bool value)
{
	system_config_t cfg = cfg_snapshot();
	if (value) {
		cfg.mode &= (uint16_t)~(MODE_AUTOSTART_TEMP_BIT | MODE_AUTOSTART_HUMIDITY_BIT);
		cfg.mode |= MODE_MANUAL_BIT;
	} else {
		cfg.mode &= (uint16_t)~MODE_MANUAL_BIT;
	}
	System_Config_set_from_display_volatile(&cfg);
}

bool get_var_smoke_temp_only_stop_bool(void)
{
	return System_Config_get_smoke_temp_only_stop_enabled();
}

void set_var_smoke_temp_only_stop_bool(bool value)
{
	System_Config_set_smoke_temp_only_stop_enabled_volatile(value);
}

#if ENABLE_SERVICE_ALARM_TOGGLES
bool get_var_alarm_temp_enabled(void)
{
	return Logic_get_alarm_temp_enabled();
}

void set_var_alarm_temp_enabled(bool value)
{
	Logic_set_alarm_temp_enabled(value);
}

bool get_var_alarm_humidity_enabled(void)
{
	return Logic_get_alarm_humidity_enabled();
}

void set_var_alarm_humidity_enabled(bool value)
{
	Logic_set_alarm_humidity_enabled(value);
}

bool get_var_alarm_smoke_enabled(void)
{
	return Logic_get_alarm_smoke_enabled();
}

void set_var_alarm_smoke_enabled(bool value)
{
	Logic_set_alarm_smoke_enabled(value);
}

bool get_var_alarm_fan_enabled(void)
{
	return Logic_get_alarm_fan_enabled();
}

void set_var_alarm_fan_enabled(bool value)
{
	Logic_set_alarm_fan_enabled(value);
}

bool get_var_alarm_filter_enabled(void)
{
	return Logic_get_alarm_filter_enabled();
}

void set_var_alarm_filter_enabled(bool value)
{
	Logic_set_alarm_filter_enabled(value);
}
#endif

int32_t get_var_tmp_fan_percent(void)
{
	return (int32_t)Peripherials_get_fan_percent();
}

int32_t get_var_tmp_fan_feedback(void)
{
	return Peripherials_get_fan_output_present_state() ? 1 : 0;
}

int32_t get_var_tmp_mode_bits(void)
{
	system_config_t cfg = cfg_snapshot();
	return (int32_t)cfg.mode;
}

int32_t get_var_tmp_alarm_temp(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return ro.alarm_temp ? 1 : 0;
	}
	return 0;
}

int32_t get_var_tmp_alarm_humidity(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return ro.alarm_humidity ? 1 : 0;
	}
	return 0;
}

int32_t get_var_tmp_alarm_smoke(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return ro.alarm_smoke ? 1 : 0;
	}
	return 0;
}

int32_t get_var_tmp_alarm_fan(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return ro.alarm_fan ? 1 : 0;
	}
	return 0;
}

int32_t get_var_tmp_alarm_filter(void)
{
	modbus_rtu_ro_snapshot_t ro;
	if (ro_snapshot(&ro)) {
		return ro.alarm_filter ? 1 : 0;
	}
	return 0;
}

int32_t get_var_tmp_mb_addr(void)
{
	return (int32_t)Modbus_RTU_get_diag_addr();
}

int32_t get_var_tmp_mb_baud_idx(void)
{
	return (int32_t)Modbus_RTU_get_diag_baud_index();
}

int32_t get_var_tmp_mb_baud_req(void)
{
	return (int32_t)Modbus_RTU_get_diag_baud_requested();
}

int32_t get_var_tmp_mb_baud_act(void)
{
	return (int32_t)Modbus_RTU_get_diag_baud_actual();
}
