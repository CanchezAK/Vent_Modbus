#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations



// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_TEMP = 0,
    FLOW_GLOBAL_VARIABLE_HUMIDITY = 1,
    FLOW_GLOBAL_VARIABLE_FILTER_CLOGGING = 2,
    FLOW_GLOBAL_VARIABLE_DOOR = 3,
    FLOW_GLOBAL_VARIABLE_SMOKE = 4,
    FLOW_GLOBAL_VARIABLE_VENT_ACTIVE = 5,
    FLOW_GLOBAL_VARIABLE_FILTER_ALARM = 6,
    FLOW_GLOBAL_VARIABLE_SET_START_TEMP_VAR = 7,
    FLOW_GLOBAL_VARIABLE_SET_START_HUMIDITY_VAR = 8,
    FLOW_GLOBAL_VARIABLE_VENT_POWER_VAR = 9,
    FLOW_GLOBAL_VARIABLE_AUTOSTART_TEMP_BOOL = 10,
    FLOW_GLOBAL_VARIABLE_AUTOSTART_HUMIDITY_BOOL = 11,
    FLOW_GLOBAL_VARIABLE_MANUAL_POWER_BOOL = 12
};

// Native global variables

extern int32_t get_var_temp();
extern void set_var_temp(int32_t value);
extern int32_t get_var_humidity();
extern void set_var_humidity(int32_t value);
extern int32_t get_var_filter_clogging();
extern void set_var_filter_clogging(int32_t value);
extern int32_t get_var_door();
extern void set_var_door(int32_t value);
extern int32_t get_var_smoke();
extern void set_var_smoke(int32_t value);
extern int32_t get_var_vent_active();
extern void set_var_vent_active(int32_t value);
extern int32_t get_var_filter_alarm();
extern void set_var_filter_alarm(int32_t value);
extern int32_t get_var_set_start_temp_var();
extern void set_var_set_start_temp_var(int32_t value);
extern int32_t get_var_set_start_humidity_var();
extern void set_var_set_start_humidity_var(int32_t value);
extern int32_t get_var_vent_power_var();
extern void set_var_vent_power_var(int32_t value);
extern bool get_var_autostart_temp_bool();
extern void set_var_autostart_temp_bool(bool value);
extern bool get_var_autostart_humidity_bool();
extern void set_var_autostart_humidity_bool(bool value);
extern bool get_var_manual_power_bool();
extern void set_var_manual_power_bool(bool value);

// TEMPORARY DEBUG HELPERS
extern int32_t get_var_tmp_fan_percent();
extern int32_t get_var_tmp_mode_bits();
extern int32_t get_var_tmp_alarm_temp();
extern int32_t get_var_tmp_alarm_humidity();
extern int32_t get_var_tmp_alarm_smoke();
extern int32_t get_var_tmp_alarm_fan();
extern int32_t get_var_tmp_alarm_filter();
extern int32_t get_var_tmp_mb_addr();
extern int32_t get_var_tmp_mb_baud_idx();
extern int32_t get_var_tmp_mb_baud_req();
extern int32_t get_var_tmp_mb_baud_act();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/