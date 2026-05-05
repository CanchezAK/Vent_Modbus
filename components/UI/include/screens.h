#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *settings;
    lv_obj_t *modbus;
    lv_obj_t *service;
    lv_obj_t *filter_clogging_bar;
    lv_obj_t *temp_label;
    lv_obj_t *humidity_label;
    lv_obj_t *filter_clogging_label;
    lv_obj_t *door_led;
    lv_obj_t *filter_alarm_led;
    lv_obj_t *vent_active_led;
    lv_obj_t *smoke_led;
    lv_obj_t *service_page_button;
    lv_obj_t *settings_page_button;
    lv_obj_t *temp_label_settings;
    lv_obj_t *humidity_label_settings;
    lv_obj_t *door_led_settings;
    lv_obj_t *smoke_settings_button;
    lv_obj_t *smoke_led_settings;
    lv_obj_t *set_start_temp_arc;
    lv_obj_t *set_start_humidity_arc;
    lv_obj_t *vent_power_arc;
    lv_obj_t *set_start_humidity_label;
    lv_obj_t *set_start_temp_label;
    lv_obj_t *autostart_temp_button;
    lv_obj_t *autostart_power_button;
    lv_obj_t *autostart_humidity_button;
    lv_obj_t *modbus_page_button;
    lv_obj_t *save_button_settings;
    lv_obj_t *obj0;
    lv_obj_t *modbus_speed_roller;
    lv_obj_t *modbus_address_roller;
    lv_obj_t *save_reboot_button_modbus;
    lv_obj_t *service_hours_label;
    lv_obj_t *service_filter_limit_slider;
    lv_obj_t *clear_filter_button;
    lv_obj_t *service_alarm_temp_button;
    lv_obj_t *service_alarm_humidity_button;
    lv_obj_t *service_alarm_smoke_button;
    lv_obj_t *service_alarm_fan_button;
    lv_obj_t *service_alarm_filter_button;
    lv_obj_t *service_telemetry_button;
    lv_obj_t *main_page_button;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SETTINGS = 2,
    SCREEN_ID_MODBUS = 3,
    SCREEN_ID_SERVICE = 4,
};

void create_screen_main();
void tick_screen_main();

void create_screen_settings();
void tick_screen_settings();

void create_screen_modbus();
void tick_screen_modbus();

void create_screen_service();
void tick_screen_service();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);
void tick_all_screens();

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/