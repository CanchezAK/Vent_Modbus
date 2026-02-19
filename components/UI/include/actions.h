#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_clear_alarms(lv_event_t * e);
extern void action_go_to_settings(lv_event_t * e);
extern void action_go_to_main(lv_event_t * e);



#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/