#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void action_go_to_main(lv_event_t *e);
void action_go_to_settings(lv_event_t *e);
void action_go_to_modbus(lv_event_t *e);
void action_go_to_service(lv_event_t *e);
void action_go_to_service_settings(lv_event_t *e);
void action_go_to_service_filter_confirm(lv_event_t *e);
void action_go_to_service_pin(lv_event_t *e);
void action_go_to_settings_pin(lv_event_t *e);

void action_save_settings(lv_event_t *e);
void action_save_modbus_and_reboot(lv_event_t *e);
void action_clear_filter_hours(lv_event_t *e);
void action_clear_alarms(lv_event_t *e);
void action_save_smoke_temp_only_stop_setting(void);

int32_t action_get_modbus_addr(void);
void action_set_modbus_addr(int32_t addr);
int32_t action_get_modbus_baud_index(void);
void action_set_modbus_baud_index(int32_t idx);

int32_t action_get_filter_limit_hours(void);
void action_set_filter_limit_hours(int32_t hours);
int32_t action_get_service_hours(void);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/