#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"
#include "System_Config.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;
static lv_obj_t *s_tmp_debug_label = NULL; // TEMPORARY DEBUG SECTION
static bool s_tmp_debug_enabled = true;

static void set_button_text(lv_obj_t *button, const char *text);
static void apply_smoke_settings_button_state(bool enabled);

static void event_handler_cb_settings_set_start_temp_arc(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_arc_get_value(ta);
            set_var_set_start_temp_var(value);
            lv_label_set_text_fmt(objects.set_start_temp_label, "%ld", (long)value);
        }
    }
}

static void event_handler_cb_main_settings_page_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_go_to_settings(e);
    }
}

static void event_handler_cb_main_service_page_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_go_to_service(e);
    }
}

static void event_handler_cb_settings_modbus_page_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_go_to_modbus(e);
    }
}

static void event_handler_cb_settings_save_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_save_settings(e);
    }
}

static void event_handler_cb_modbus_addr_roller(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        action_set_modbus_addr(lv_roller_get_selected(ta) + 1);
    }
}

static void event_handler_cb_modbus_speed_roller(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        action_set_modbus_baud_index(lv_roller_get_selected(ta));
    }
}

static void event_handler_cb_modbus_save_reboot(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_save_modbus_and_reboot(e);
    }
}

static void event_handler_cb_service_clear_filter(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_clear_filter_hours(e);
    }
}

static void event_handler_cb_service_alarm_clr(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_clear_alarms(e);
    }
}

static void apply_debug_label_visibility(void) {
    if (!s_tmp_debug_label) {
        return;
    }
    if (s_tmp_debug_enabled) {
        lv_obj_clear_flag(s_tmp_debug_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_tmp_debug_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void event_handler_cb_service_telemetry_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        s_tmp_debug_enabled = !s_tmp_debug_enabled;
        apply_debug_label_visibility();
        set_button_text(objects.service_telemetry_button, s_tmp_debug_enabled ? "TEL:1" : "TEL:0");
    }
}

#if ENABLE_SERVICE_ALARM_TOGGLES
static void event_handler_cb_service_alarm_temp_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_alarm_temp_enabled();
        set_var_alarm_temp_enabled(new_value);
        set_button_text(objects.service_alarm_temp_button, new_value ? "TEMP:1" : "TEMP:0");
    }
}

static void event_handler_cb_service_alarm_humidity_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_alarm_humidity_enabled();
        set_var_alarm_humidity_enabled(new_value);
        set_button_text(objects.service_alarm_humidity_button, new_value ? "HUM:1" : "HUM:0");
    }
}

static void event_handler_cb_service_alarm_smoke_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_alarm_smoke_enabled();
        set_var_alarm_smoke_enabled(new_value);
        set_button_text(objects.service_alarm_smoke_button, new_value ? "SMOKE:1" : "SMOKE:0");
    }
}

static void event_handler_cb_service_alarm_fan_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_alarm_fan_enabled();
        set_var_alarm_fan_enabled(new_value);
        set_button_text(objects.service_alarm_fan_button, new_value ? "FAN:1" : "FAN:0");
    }
}

static void event_handler_cb_service_alarm_filter_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_alarm_filter_enabled();
        set_var_alarm_filter_enabled(new_value);
        set_button_text(objects.service_alarm_filter_button, new_value ? "FILTER:1" : "FILTER:0");
    }
}
#endif

static void event_handler_cb_service_main_page(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_go_to_main(e);
    }
}

static void event_handler_cb_back_to_main(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_go_to_main(e);
    }
}

static void create_back_to_main_button(lv_obj_t *parent_obj) {
    lv_obj_t *obj = lv_btn_create(parent_obj);
    lv_obj_set_pos(obj, 16, 16);
    lv_obj_set_size(obj, 64, 64);
    lv_obj_add_event_cb(obj, event_handler_cb_back_to_main, LV_EVENT_CLICKED, 0);

    lv_obj_t *label = lv_label_create(obj);
    lv_obj_set_style_align(label, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &ui_font_roboto72, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "<");
}

static void event_handler_cb_service_filter_limit_slider(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        int32_t value = lv_slider_get_value(ta);
        action_set_filter_limit_hours(value);
        lv_label_set_text_fmt(objects.service_hours_label, "%ldЧ.", (long)value);
    }
}

static void event_handler_cb_settings_set_start_humidity_arc(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_arc_get_value(ta);
            set_var_set_start_humidity_var(value);
            lv_label_set_text_fmt(objects.set_start_humidity_label, "%ld", (long)value);
        }
    }
}

static void event_handler_cb_settings_vent_power_arc(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_arc_get_value(ta);
            set_var_vent_power_var(value);
            lv_label_set_text_fmt(objects.obj0, "%ld", (long)value);
        }
    }
}

static void set_button_text(lv_obj_t *button, const char *text) {
    lv_obj_t *label = lv_obj_get_child(button, 0);
    if (label) {
        lv_label_set_text(label, text);
    }
}

static void event_handler_cb_settings_autostart_temp_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_autostart_temp_bool();
        set_var_autostart_temp_bool(new_value);
        set_button_text(objects.autostart_temp_button, new_value ? "ВЫКЛ" : "ВКЛ");
    }
}

static void event_handler_cb_settings_autostart_humidity_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_autostart_humidity_bool();
        set_var_autostart_humidity_bool(new_value);
        set_button_text(objects.autostart_humidity_button, new_value ? "ВЫКЛ" : "ВКЛ");
    }
}

static void event_handler_cb_settings_autostart_power_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_manual_power_bool();
        set_var_manual_power_bool(new_value);
        set_button_text(objects.autostart_power_button, new_value ? "ВЫКЛ" : "ВКЛ");
    }
}

static void apply_smoke_settings_button_state(bool enabled) {
    if (!objects.smoke_settings_button) {
        return;
    }

    lv_color_t bg_color = enabled ? lv_color_hex(0xffffff) : lv_color_hex(0xffd6d6);
    lv_color_t border_color = enabled ? lv_color_hex(0xd0d0d0) : lv_color_hex(0xe6aaaa);

    lv_obj_set_style_bg_color(objects.smoke_settings_button, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(objects.smoke_settings_button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(objects.smoke_settings_button, border_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(objects.smoke_settings_button, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(objects.smoke_settings_button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(objects.smoke_settings_button, bg_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(objects.smoke_settings_button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(objects.smoke_settings_button, border_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(objects.smoke_settings_button, 2, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(objects.smoke_settings_button, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t *label = lv_obj_get_child(objects.smoke_settings_button, 0);
    if (label) {
        lv_obj_set_style_text_color(label, lv_color_hex(0x1f1f1f), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_hex(0x1f1f1f), LV_PART_MAIN | LV_STATE_PRESSED);
    }
}

static void event_handler_cb_settings_smoke_button(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool new_value = !get_var_smoke_temp_only_stop_bool();
        set_var_smoke_temp_only_stop_bool(new_value);
        apply_smoke_settings_button_state(new_value);
    }
}

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        lv_obj_t *smoke_box = NULL;
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 431, 27);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_logo);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 51, 141);
            lv_obj_set_size(obj, 871, 172);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 345, 159);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ВЕНТИЛЯТОР");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 51, 363);
            lv_obj_set_size(obj, 871, 172);
        }
        {
            // filter_clogging_bar
            lv_obj_t *obj = lv_bar_create(parent_obj);
            objects.filter_clogging_bar = obj;
            lv_obj_set_pos(obj, 92, 486);
            lv_obj_set_size(obj, 674, 10);
            lv_bar_set_mode(obj, LV_BAR_MODE_SYMMETRICAL);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 353, 407);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ФИЛЬТР");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 972, 141);
            lv_obj_set_size(obj, 259, 172);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 1017, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ТЕМПЕРАТУРА");
        }
        {
            // temp_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_label = obj;
            lv_obj_set_pos(obj, 1037, 212);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto72, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 972, 363);
            lv_obj_set_size(obj, 259, 172);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 1028, 385);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ВЛАЖНОСТЬ");
        }
        {
            // humidity_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.humidity_label = obj;
            lv_obj_set_pos(obj, 1043, 435);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto72, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // filter_clogging_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.filter_clogging_label = obj;
            lv_obj_set_pos(obj, 816, 470);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 51, 586);
            lv_obj_set_size(obj, 257, 86);
        }
        {
            // door_led
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.door_led = obj;
            lv_obj_set_pos(obj, 252, 617);
            lv_obj_set_size(obj, 24, 24);
            lv_led_set_color(obj, lv_color_hex(0xff0000ff));
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 92, 608);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ДВЕРЬ");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            smoke_box = obj;
            lv_obj_set_pos(obj, 358, 586);
            lv_obj_set_size(obj, 257, 86);
        }
        {
            // filter_alarm_led
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.filter_alarm_led = obj;
            lv_obj_set_pos(obj, 526, 416);
            lv_obj_set_size(obj, 24, 24);
            lv_led_set_color(obj, lv_color_hex(0xff2196f3));
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 417, 608);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ДЫМ");
        }
        {
            // vent_active_led
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.vent_active_led = obj;
            lv_obj_set_pos(obj, 604, 168);
            lv_obj_set_size(obj, 24, 24);
            lv_led_set_color(obj, lv_color_hex(0xffff0044));
        }
        {
            // smoke_led
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.smoke_led = obj;
            lv_obj_set_pos(obj, 554, 617);
            lv_obj_set_size(obj, 24, 24);
            lv_led_set_color(obj, lv_color_hex(0xffff0044));
        }
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 1138, 453);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_hum);
        }
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 1132, 227);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_temp);
        }
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 422, 209);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_fun8080);
        }
        {
            // service_page_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.service_page_button = obj;
            lv_obj_set_pos(obj, 685, 586);
            lv_obj_set_size(obj, 262, 86);
            lv_obj_add_event_cb(obj, event_handler_cb_main_service_page_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "СЕРВИС");
                }
            }
        }
        {
            // settings_page_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.settings_page_button = obj;
            lv_obj_set_pos(obj, 972, 586);
            lv_obj_set_size(obj, 262, 86);
            lv_obj_add_event_cb(obj, event_handler_cb_main_settings_page_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "НАСТРОЙКИ");
                }
            }
        }
        {
            if (smoke_box && objects.service_page_button && objects.settings_page_button) {
                lv_obj_update_layout(parent_obj);
                int32_t smoke_right = lv_obj_get_x(smoke_box) + lv_obj_get_width(smoke_box);
                int32_t settings_x = lv_obj_get_x(objects.settings_page_button);
                int32_t service_w = lv_obj_get_width(objects.service_page_button);
                int32_t gap = (settings_x - smoke_right - service_w) / 2;
                lv_obj_set_x(objects.service_page_button, smoke_right + gap);
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 876, 470);
            lv_obj_set_size(obj, 36, 42);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "%");
        }
        {
            // TEMPORARY DEBUG SECTION
            lv_obj_t *obj = lv_label_create(parent_obj);
            s_tmp_debug_label = obj;
            lv_obj_set_pos(obj, 88, 16);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "[TEMP DEBUG]\ninit...");
            apply_debug_label_visibility();
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
    static int32_t last_temp = INT32_MIN;
    static int32_t last_humidity = INT32_MIN;
    static int32_t last_filter_pct = INT32_MIN;
    static int32_t last_vent_on = -1;
    static int32_t last_tmp_fan_percent = INT32_MIN;
    static int32_t last_tmp_fan_feedback = -1;
    static int32_t last_tmp_mode_bits = INT32_MIN;
    static int32_t last_tmp_manual = -1;
    static int32_t last_tmp_auto_temp = -1;
    static int32_t last_tmp_auto_hum = -1;
    static int32_t last_tmp_alarm_temp = -1;
    static int32_t last_tmp_alarm_hum = -1;
    static int32_t last_tmp_alarm_smoke = -1;
    static int32_t last_tmp_alarm_fan = -1;
    static int32_t last_tmp_alarm_filter = -1;
    static int32_t last_tmp_mb_addr = INT32_MIN;
    static int32_t last_tmp_mb_baud_idx = INT32_MIN;
    static int32_t last_tmp_mb_baud_req = INT32_MIN;
    static int32_t last_tmp_mb_baud_act = INT32_MIN;

    {
        int32_t new_val = get_var_filter_clogging();
        int32_t cur_val = lv_bar_get_value(objects.filter_clogging_bar);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.filter_clogging_bar;
            lv_bar_set_value(objects.filter_clogging_bar, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_temp();
        if (new_val != last_temp) {
            tick_value_change_obj = objects.temp_label;
            lv_label_set_text_fmt(objects.temp_label, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_temp = new_val;
        }
    }
    {
        int32_t new_val = get_var_humidity();
        if (new_val != last_humidity) {
            tick_value_change_obj = objects.humidity_label;
            lv_label_set_text_fmt(objects.humidity_label, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_humidity = new_val;
        }
    }
    {
        int32_t new_val = get_var_filter_clogging();
        if (new_val != last_filter_pct) {
            tick_value_change_obj = objects.filter_clogging_label;
            lv_label_set_text_fmt(objects.filter_clogging_label, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_filter_pct = new_val;
        }
    }
    {
        int32_t new_val = get_var_door();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.door_led);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.door_led;
            lv_led_set_brightness(objects.door_led, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_filter_alarm();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.filter_alarm_led);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.filter_alarm_led;
            lv_led_set_brightness(objects.filter_alarm_led, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t vent_on = get_var_vent_active() > 0 ? 1 : 0;
        int32_t cur_brightness = lv_led_get_brightness(objects.vent_active_led);

        if (cur_brightness != 255) {
            tick_value_change_obj = objects.vent_active_led;
            lv_led_set_brightness(objects.vent_active_led, 255);
            tick_value_change_obj = NULL;
        }

        if (vent_on != last_vent_on) {
            tick_value_change_obj = objects.vent_active_led;
            lv_led_set_color(objects.vent_active_led,
                             vent_on ? lv_color_hex(0xff00ff00) : lv_color_hex(0xffff0044));
            tick_value_change_obj = NULL;
            last_vent_on = vent_on;
        }
    }
    {
        int32_t new_val = get_var_smoke();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.smoke_led);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.smoke_led;
            lv_led_set_brightness(objects.smoke_led, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        // TEMPORARY DEBUG SECTION
        int32_t fan_percent = get_var_tmp_fan_percent();
        int32_t fan_feedback = get_var_tmp_fan_feedback();
        int32_t mode_bits = get_var_tmp_mode_bits();
        int32_t manual = get_var_manual_power_bool() ? 1 : 0;
        int32_t auto_temp = get_var_autostart_temp_bool() ? 1 : 0;
        int32_t auto_hum = get_var_autostart_humidity_bool() ? 1 : 0;
        int32_t alarm_temp = get_var_tmp_alarm_temp();
        int32_t alarm_hum = get_var_tmp_alarm_humidity();
        int32_t alarm_smoke = get_var_tmp_alarm_smoke();
        int32_t alarm_fan = get_var_tmp_alarm_fan();
        int32_t alarm_filter = get_var_tmp_alarm_filter();
        int32_t mb_addr = get_var_tmp_mb_addr();
        int32_t mb_baud_idx = get_var_tmp_mb_baud_idx();
        int32_t mb_baud_req = get_var_tmp_mb_baud_req();
        int32_t mb_baud_act = get_var_tmp_mb_baud_act();

        if (s_tmp_debug_enabled && s_tmp_debug_label &&
            (fan_percent != last_tmp_fan_percent ||
             fan_feedback != last_tmp_fan_feedback ||
             mode_bits != last_tmp_mode_bits ||
             manual != last_tmp_manual ||
             auto_temp != last_tmp_auto_temp ||
             auto_hum != last_tmp_auto_hum ||
                 alarm_temp != last_tmp_alarm_temp ||
                 alarm_hum != last_tmp_alarm_hum ||
                 alarm_smoke != last_tmp_alarm_smoke ||
                 alarm_fan != last_tmp_alarm_fan ||
                 alarm_filter != last_tmp_alarm_filter ||
                 mb_addr != last_tmp_mb_addr ||
                 mb_baud_idx != last_tmp_mb_baud_idx ||
                 mb_baud_req != last_tmp_mb_baud_req ||
                 mb_baud_act != last_tmp_mb_baud_act ||
             last_vent_on == -1)) {
            lv_label_set_text_fmt(
                s_tmp_debug_label,
                     "[TEMP DEBUG] fan=%ld%% cmd=%ld fb=%ld\nmode=0x%04lX M:%ld AT:%ld AH:%ld\nALR T:%ld H:%ld S:%ld F:%ld FL:%ld\nMB addr=%ld idx=%ld req=%ld act=%ld",
                (long)fan_percent,
                (long)(get_var_vent_active() > 0 ? 1 : 0),
                 (long)fan_feedback,
                (unsigned long)((uint16_t)mode_bits),
                (long)manual,
                (long)auto_temp,
                     (long)auto_hum,
                     (long)alarm_temp,
                     (long)alarm_hum,
                     (long)alarm_smoke,
                     (long)alarm_fan,
                     (long)alarm_filter,
                     (long)mb_addr,
                     (long)mb_baud_idx,
                     (long)mb_baud_req,
                     (long)mb_baud_act);

            last_tmp_fan_percent = fan_percent;
            last_tmp_fan_feedback = fan_feedback;
            last_tmp_mode_bits = mode_bits;
            last_tmp_manual = manual;
            last_tmp_auto_temp = auto_temp;
            last_tmp_auto_hum = auto_hum;
            last_tmp_alarm_temp = alarm_temp;
            last_tmp_alarm_hum = alarm_hum;
            last_tmp_alarm_smoke = alarm_smoke;
            last_tmp_alarm_fan = alarm_fan;
            last_tmp_alarm_filter = alarm_filter;
            last_tmp_mb_addr = mb_addr;
            last_tmp_mb_baud_idx = mb_baud_idx;
            last_tmp_mb_baud_req = mb_baud_req;
            last_tmp_mb_baud_act = mb_baud_act;
        }
    }
}

void create_screen_settings() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.settings = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        lv_obj_t *door_label_settings = NULL;
        lv_obj_t *smoke_label_settings = NULL;
        create_back_to_main_button(parent_obj);
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 431, 38);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_logo);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 972, 141);
            lv_obj_set_size(obj, 259, 172);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 1017, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ТЕМПЕРАТУРА");
        }
        {
            // temp_label_settings
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_label_settings = obj;
            lv_obj_set_pos(obj, 1037, 212);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto72, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 972, 363);
            lv_obj_set_size(obj, 259, 172);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 1028, 385);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ВЛАЖНОСТЬ");
        }
        {
            // humidity_label_settings
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.humidity_label_settings = obj;
            lv_obj_set_pos(obj, 1043, 435);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto72, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 51, 586);
            lv_obj_set_size(obj, 257, 86);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    door_label_settings = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "ДВЕРЬ");
                }
                {
                    // door_led_settings
                    lv_obj_t *obj = lv_led_create(parent_obj);
                    objects.door_led_settings = obj;
                    lv_obj_set_pos(obj, 214, 31);
                    lv_obj_set_size(obj, 24, 24);
                    lv_led_set_color(obj, lv_color_hex(0xff0000ff));
                }
            }
        }
        {
            // smoke_settings_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.smoke_settings_button = obj;
            lv_obj_set_pos(obj, 358, 586);
            lv_obj_set_size(obj, 257, 86);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_smoke_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    smoke_label_settings = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "ДЫМ");
                }
                {
                    // smoke_led_settings
                    lv_obj_t *obj = lv_led_create(parent_obj);
                    objects.smoke_led_settings = obj;
                    lv_obj_set_pos(obj, 214, 31);
                    lv_obj_set_size(obj, 24, 24);
                    lv_led_set_color(obj, lv_color_hex(0xffff0044));
                }
            }
        }
        {
            if (door_label_settings && objects.door_led_settings) {
                lv_obj_update_layout(door_label_settings);
                lv_obj_align_to(objects.door_led_settings, door_label_settings, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
            }
            if (smoke_label_settings && objects.smoke_led_settings) {
                lv_obj_update_layout(smoke_label_settings);
                lv_obj_align_to(objects.smoke_led_settings, smoke_label_settings, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
            }
        }
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 1138, 453);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_hum);
        }
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 1138, 226);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_temp);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 51, 141);
            lv_obj_set_size(obj, 257, 394);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 358, 141);
            lv_obj_set_size(obj, 257, 394);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 665, 141);
            lv_obj_set_size(obj, 257, 394);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 117, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "АВТОПУСК");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 94, 198);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ТЕМПЕРАТУРА");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 424, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "АВТОПУСК");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 412, 198);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ВЛАЖНОСТЬ");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 724, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "МОЩНОСТЬ");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 710, 198);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "ВЕНТИЛЯТОРА");
        }
        {
            // set_start_temp_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.set_start_temp_arc = obj;
            lv_obj_set_pos(obj, 84, 241);
            lv_obj_set_size(obj, 189, 190);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_set_start_temp_arc, LV_EVENT_ALL, 0);
        }
        {
            // set_start_humidity_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.set_start_humidity_arc = obj;
            lv_obj_set_pos(obj, 397, 241);
            lv_obj_set_size(obj, 182, 189);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_set_start_humidity_arc, LV_EVENT_ALL, 0);
        }
        {
            // vent_power_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.vent_power_arc = obj;
            lv_obj_set_pos(obj, 699, 241);
            lv_obj_set_size(obj, 189, 189);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_vent_power_arc, LV_EVENT_ALL, 0);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 758, 313);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // set_start_humidity_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.set_start_humidity_label = obj;
            lv_obj_set_pos(obj, 449, 313);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // set_start_temp_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.set_start_temp_label = obj;
            lv_obj_set_pos(obj, 142, 313);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 207, 313);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "˚C");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 509, 313);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "%");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 818, 313);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "%");
        }
        {
            // autostart_temp_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.autostart_temp_button = obj;
            lv_obj_set_pos(obj, 112, 458);
            lv_obj_set_size(obj, 135, 59);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_autostart_temp_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "ВКЛ");
                }
            }
        }
        {
            // autostart_power_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.autostart_power_button = obj;
            lv_obj_set_pos(obj, 732, 458);
            lv_obj_set_size(obj, 135, 59);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_autostart_power_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 1, 4);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "ВКЛ");
                }
            }
        }
        {
            // autostart_humidity_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.autostart_humidity_button = obj;
            lv_obj_set_pos(obj, 421, 458);
            lv_obj_set_size(obj, 135, 59);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_autostart_humidity_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "ВКЛ");
                }
            }
        }
        {
            // modbus_page_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.modbus_page_button = obj;
            lv_obj_set_pos(obj, 665, 586);
            lv_obj_set_size(obj, 257, 86);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_modbus_page_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "MODBUS");
                }
            }
        }
        {
            // save_button_settings
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.save_button_settings = obj;
            lv_obj_set_pos(obj, 972, 586);
            lv_obj_set_size(obj, 259, 86);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_save_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "СОХРАНИТЬ");
                }
            }
        }
    }
    
    tick_screen_settings();
}

void tick_screen_settings() {
    static int32_t last_temp = INT32_MIN;
    static int32_t last_humidity = INT32_MIN;
    static int32_t last_power = INT32_MIN;
    static int32_t last_start_hum = INT32_MIN;
    static int32_t last_start_temp = INT32_MIN;
    static int32_t last_autostart_temp = -1;
    static int32_t last_autostart_humidity = -1;
    static int32_t last_manual_power = -1;
    static int32_t last_smoke_temp_only_stop = -1;

    {
        int32_t new_val = get_var_temp();
        if (new_val != last_temp) {
            tick_value_change_obj = objects.temp_label_settings;
            lv_label_set_text_fmt(objects.temp_label_settings, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_temp = new_val;
        }
    }
    {
        int32_t new_val = get_var_humidity();
        if (new_val != last_humidity) {
            tick_value_change_obj = objects.humidity_label_settings;
            lv_label_set_text_fmt(objects.humidity_label_settings, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_humidity = new_val;
        }
    }
    {
        int32_t new_val = get_var_door();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.door_led_settings);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.door_led_settings;
            lv_led_set_brightness(objects.door_led_settings, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_smoke();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.smoke_led_settings);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.smoke_led_settings;
            lv_led_set_brightness(objects.smoke_led_settings, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_smoke_temp_only_stop_bool() ? 1 : 0;
        if (new_val != last_smoke_temp_only_stop) {
            apply_smoke_settings_button_state(new_val != 0);
            last_smoke_temp_only_stop = new_val;
        }
    }
    {
        int32_t new_val = get_var_set_start_temp_var();
        int32_t cur_val = lv_arc_get_value(objects.set_start_temp_arc);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.set_start_temp_arc;
            lv_arc_set_value(objects.set_start_temp_arc, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_set_start_humidity_var();
        int32_t cur_val = lv_arc_get_value(objects.set_start_humidity_arc);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.set_start_humidity_arc;
            lv_arc_set_value(objects.set_start_humidity_arc, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_vent_power_var();
        int32_t cur_val = lv_arc_get_value(objects.vent_power_arc);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.vent_power_arc;
            lv_arc_set_value(objects.vent_power_arc, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_vent_power_var();
        if (new_val != last_power) {
            tick_value_change_obj = objects.obj0;
            lv_label_set_text_fmt(objects.obj0, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_power = new_val;
        }
    }
    {
        int32_t new_val = get_var_set_start_humidity_var();
        if (new_val != last_start_hum) {
            tick_value_change_obj = objects.set_start_humidity_label;
            lv_label_set_text_fmt(objects.set_start_humidity_label, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_start_hum = new_val;
        }
    }
    {
        int32_t new_val = get_var_set_start_temp_var();
        if (new_val != last_start_temp) {
            tick_value_change_obj = objects.set_start_temp_label;
            lv_label_set_text_fmt(objects.set_start_temp_label, "%ld", (long)new_val);
            tick_value_change_obj = NULL;
            last_start_temp = new_val;
        }
    }
    {
        int32_t new_val = get_var_autostart_temp_bool() ? 1 : 0;
        if (new_val != last_autostart_temp) {
            set_button_text(objects.autostart_temp_button, new_val ? "ВЫКЛ" : "ВКЛ");
            last_autostart_temp = new_val;
        }
    }
    {
        int32_t new_val = get_var_autostart_humidity_bool() ? 1 : 0;
        if (new_val != last_autostart_humidity) {
            set_button_text(objects.autostart_humidity_button, new_val ? "ВЫКЛ" : "ВКЛ");
            last_autostart_humidity = new_val;
        }
    }
    {
        int32_t new_val = get_var_manual_power_bool() ? 1 : 0;
        if (new_val != last_manual_power) {
            set_button_text(objects.autostart_power_button, new_val ? "ВЫКЛ" : "ВКЛ");
            last_manual_power = new_val;
        }
    }
}

void create_screen_modbus() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.modbus = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        create_back_to_main_button(parent_obj);
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 431, 38);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_logo);
        }
        {
            // modbus_speed_roller
            lv_obj_t *obj = lv_roller_create(parent_obj);
            objects.modbus_speed_roller = obj;
            lv_obj_set_pos(obj, 667, 192);
            lv_obj_set_size(obj, 400, 226);
            lv_roller_set_options(obj, "9600 bit/s\n19200 bit/s\n38400 bit/s\n115200 bit/s", LV_ROLLER_MODE_NORMAL);
            lv_obj_add_event_cb(obj, event_handler_cb_modbus_speed_roller, LV_EVENT_VALUE_CHANGED, 0);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 741, 147);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "СКОРОСТЬ");
        }
        {
            // modbus_address_roller
            lv_obj_t *obj = lv_roller_create(parent_obj);
            objects.modbus_address_roller = obj;
            lv_obj_set_pos(obj, 209, 192);
            lv_obj_set_size(obj, 400, 226);
            lv_roller_set_options(obj, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25", LV_ROLLER_MODE_NORMAL);
            lv_obj_add_event_cb(obj, event_handler_cb_modbus_addr_roller, LV_EVENT_VALUE_CHANGED, 0);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 388, 147);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "АДРЕС");
        }
        {
            // save_reboot_button_modbus
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.save_reboot_button_modbus = obj;
            lv_obj_set_pos(obj, 209, 479);
            lv_obj_set_size(obj, 858, 153);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_SCROLLED);
            lv_obj_set_style_text_font(obj, &ui_font_roboto72, LV_PART_MAIN | LV_STATE_SCROLLED);
            lv_obj_add_event_cb(obj, event_handler_cb_modbus_save_reboot, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "СОХРАНИТЬ И ПЕРЕЗАГРУЗИТЬ");
                }
            }
        }
    }
    
    tick_screen_modbus();
}

void tick_screen_modbus() {
    {
        int32_t new_val = action_get_modbus_baud_index();
        int32_t cur_val = lv_roller_get_selected(objects.modbus_speed_roller);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.modbus_speed_roller;
            lv_roller_set_selected(objects.modbus_speed_roller, (uint16_t)new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = action_get_modbus_addr() - 1;
        int32_t cur_val = lv_roller_get_selected(objects.modbus_address_roller);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.modbus_address_roller;
            lv_roller_set_selected(objects.modbus_address_roller, (uint16_t)new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_service() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.service = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        create_back_to_main_button(parent_obj);
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 431, 38);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_logo);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 204, 161);
            lv_obj_set_size(obj, 871, 132);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 507, 181);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "РЕСУРС ФИЛЬТРА");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.service_hours_label = obj;
            lv_obj_set_pos(obj, 921, 242);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "0Ч.");
        }
        {
            // clear_filter_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.clear_filter_button = obj;
            lv_obj_set_pos(obj, 204, 346);
            lv_obj_set_size(obj, 871, 132);
            lv_obj_add_event_cb(obj, event_handler_cb_service_clear_filter, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "СБРОС ФИЛЬТРА");
                }
            }
        }
        {
            lv_obj_t *obj = lv_slider_create(parent_obj);
            objects.service_filter_limit_slider = obj;
            lv_obj_set_pos(obj, 236, 258);
            lv_obj_set_size(obj, 655, 10);
            lv_slider_set_value(obj, 25, LV_ANIM_OFF);
            lv_slider_set_range(obj, 0, 10000);
            lv_obj_add_event_cb(obj, event_handler_cb_service_filter_limit_slider, LV_EVENT_VALUE_CHANGED, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ON_FOCUS);
        }
        {
            // alarm_clr_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            lv_obj_set_pos(obj, 1100, 16);
            lv_obj_set_size(obj, 160, 56);
            lv_obj_add_event_cb(obj, event_handler_cb_service_alarm_clr, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "ALARM_CLR");
                }
            }
        }
#if ENABLE_SERVICE_ALARM_TOGGLES
        {
            // service_alarm_temp_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.service_alarm_temp_button = obj;
            lv_obj_set_pos(obj, 1100, 80);
            lv_obj_set_size(obj, 160, 56);
            lv_obj_add_event_cb(obj, event_handler_cb_service_alarm_temp_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, get_var_alarm_temp_enabled() ? "TEMP:1" : "TEMP:0");
                }
            }
        }
        {
            // service_alarm_humidity_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.service_alarm_humidity_button = obj;
            lv_obj_set_pos(obj, 1100, 144);
            lv_obj_set_size(obj, 160, 56);
            lv_obj_add_event_cb(obj, event_handler_cb_service_alarm_humidity_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, get_var_alarm_humidity_enabled() ? "HUM:1" : "HUM:0");
                }
            }
        }
        {
            // service_alarm_smoke_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.service_alarm_smoke_button = obj;
            lv_obj_set_pos(obj, 1100, 208);
            lv_obj_set_size(obj, 160, 56);
            lv_obj_add_event_cb(obj, event_handler_cb_service_alarm_smoke_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, get_var_alarm_smoke_enabled() ? "SMOKE:1" : "SMOKE:0");
                }
            }
        }
        {
            // service_alarm_fan_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.service_alarm_fan_button = obj;
            lv_obj_set_pos(obj, 1100, 272);
            lv_obj_set_size(obj, 160, 56);
            lv_obj_add_event_cb(obj, event_handler_cb_service_alarm_fan_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, get_var_alarm_fan_enabled() ? "FAN:1" : "FAN:0");
                }
            }
        }
        {
            // service_alarm_filter_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.service_alarm_filter_button = obj;
            lv_obj_set_pos(obj, 1100, 336);
            lv_obj_set_size(obj, 160, 56);
            lv_obj_add_event_cb(obj, event_handler_cb_service_alarm_filter_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, get_var_alarm_filter_enabled() ? "FILTER:1" : "FILTER:0");
                }
            }
        }
#endif
        {
            // service_telemetry_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.service_telemetry_button = obj;
            lv_obj_set_pos(obj, 1100, 400);
            lv_obj_set_size(obj, 160, 56);
            lv_obj_add_event_cb(obj, event_handler_cb_service_telemetry_button, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, s_tmp_debug_enabled ? "TEL:1" : "TEL:0");
                }
            }
        }
        {
            // main_page_button
            lv_obj_t *obj = lv_btn_create(parent_obj);
            objects.main_page_button = obj;
            lv_obj_set_pos(obj, 205, 534);
            lv_obj_set_size(obj, 871, 132);
            lv_obj_add_event_cb(obj, event_handler_cb_service_main_page, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto362, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "МЕНЮ");
                }
            }
        }
    }
    
    tick_screen_service();
}

void tick_screen_service() {
    static int32_t last_filter_limit = INT32_MIN;
#if ENABLE_SERVICE_ALARM_TOGGLES
    static int32_t last_alarm_temp_enabled = -1;
    static int32_t last_alarm_humidity_enabled = -1;
    static int32_t last_alarm_smoke_enabled = -1;
    static int32_t last_alarm_fan_enabled = -1;
    static int32_t last_alarm_filter_enabled = -1;
#endif
    static int32_t last_telemetry_enabled = -1;
    {
        int32_t new_val = action_get_filter_limit_hours();
        int32_t cur_val = lv_slider_get_value(objects.service_filter_limit_slider);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.service_filter_limit_slider;
            lv_slider_set_value(objects.service_filter_limit_slider, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
        if (new_val != last_filter_limit) {
            tick_value_change_obj = objects.service_hours_label;
            lv_label_set_text_fmt(objects.service_hours_label, "%ldЧ.", (long)new_val);
            tick_value_change_obj = NULL;
            last_filter_limit = new_val;
        }
    }
#if ENABLE_SERVICE_ALARM_TOGGLES
    {
        int32_t new_val = get_var_alarm_temp_enabled() ? 1 : 0;
        if (new_val != last_alarm_temp_enabled) {
            set_button_text(objects.service_alarm_temp_button, new_val ? "TEMP:1" : "TEMP:0");
            last_alarm_temp_enabled = new_val;
        }
    }
    {
        int32_t new_val = get_var_alarm_humidity_enabled() ? 1 : 0;
        if (new_val != last_alarm_humidity_enabled) {
            set_button_text(objects.service_alarm_humidity_button, new_val ? "HUM:1" : "HUM:0");
            last_alarm_humidity_enabled = new_val;
        }
    }
    {
        int32_t new_val = get_var_alarm_smoke_enabled() ? 1 : 0;
        if (new_val != last_alarm_smoke_enabled) {
            set_button_text(objects.service_alarm_smoke_button, new_val ? "SMOKE:1" : "SMOKE:0");
            last_alarm_smoke_enabled = new_val;
        }
    }
    {
        int32_t new_val = get_var_alarm_fan_enabled() ? 1 : 0;
        if (new_val != last_alarm_fan_enabled) {
            set_button_text(objects.service_alarm_fan_button, new_val ? "FAN:1" : "FAN:0");
            last_alarm_fan_enabled = new_val;
        }
    }
    {
        int32_t new_val = get_var_alarm_filter_enabled() ? 1 : 0;
        if (new_val != last_alarm_filter_enabled) {
            set_button_text(objects.service_alarm_filter_button, new_val ? "FILTER:1" : "FILTER:0");
            last_alarm_filter_enabled = new_val;
        }
    }
#endif
    {
        int32_t new_val = s_tmp_debug_enabled ? 1 : 0;
        if (new_val != last_telemetry_enabled) {
            set_button_text(objects.service_telemetry_button, new_val ? "TEL:1" : "TEL:0");
            last_telemetry_enabled = new_val;
        }
    }
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    tick_screen_settings,
    tick_screen_modbus,
    tick_screen_service,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}
void tick_all_screens() {
    for (int screenId = SCREEN_ID_MAIN; screenId <= SCREEN_ID_SERVICE; ++screenId) {
        tick_screen_by_id((enum ScreensEnum)screenId);
    }
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
    create_screen_settings();
    create_screen_modbus();
    create_screen_service();
}
