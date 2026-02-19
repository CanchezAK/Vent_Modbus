#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

static void event_handler_cb_main_settings_page_b(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_go_to_settings(e);
    }
}

static void event_handler_cb_settings_main_page_b(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_go_to_main(e);
    }
}

static void event_handler_cb_settings_alarm_clr_b(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        action_clear_alarms(e);
    }
}

static void event_handler_cb_settings_fan_percentage_slider(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            set_var_fan_percentage(value);
        }
    }
}

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff126714), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 34, 22);
            lv_obj_set_size(obj, 1212, 306);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            static const char *map[1] = {
                NULL,
            };
            lv_buttonmatrix_set_map(obj, map);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff38661b), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // settings_page_b
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.settings_page_b = obj;
            lv_obj_set_pos(obj, 83, 59);
            lv_obj_set_size(obj, 1137, 235);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(obj, event_handler_cb_main_settings_page_b, LV_EVENT_CLICKED, 0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff1f4e0b), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj1 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, LV_FONT_DEFAULT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xff00ffa4), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Settings");
                }
            }
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
}

void create_screen_settings() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.settings = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
            lv_obj_set_pos(obj, 520, 49);
            lv_obj_set_size(obj, 240, 550);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            static const char *map[1] = {
                NULL,
            };
            lv_buttonmatrix_set_map(obj, map);
        }
        {
            // fan_percentage_slider
            lv_obj_t *obj = lv_slider_create(parent_obj);
            objects.fan_percentage_slider = obj;
            lv_obj_set_pos(obj, 565, 196);
            lv_obj_set_size(obj, 150, 10);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_fan_percentage_slider, LV_EVENT_VALUE_CHANGED, 0);
        }
        {
            // main_page_b
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.main_page_b = obj;
            lv_obj_set_pos(obj, 590, 87);
            lv_obj_set_size(obj, 100, 50);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_main_page_b, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Main page");
                }
            }
        }
        {
            // alarm_clr_b
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.alarm_clr_b = obj;
            lv_obj_set_pos(obj, 590, 256);
            lv_obj_set_size(obj, 100, 50);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_alarm_clr_b, LV_EVENT_CLICKED, 0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Clear alarms");
                }
            }
        }
    }
    
    tick_screen_settings();
}

void tick_screen_settings() {
    {
        int32_t new_val = get_var_fan_percentage();
        int32_t cur_val = lv_slider_get_value(objects.fan_percentage_slider);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.fan_percentage_slider;
            lv_slider_set_value(objects.fan_percentage_slider, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    tick_screen_settings,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
    create_screen_settings();
}
