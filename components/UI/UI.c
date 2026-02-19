#include <stdio.h>
#include "UI.h"
#include "actions.h"
#include "System_Config.h"

void action_clear_alarms(lv_event_t *e)
{
	(void)e;
	System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_DISPLAY, 0);
}
