#include "System_Init.h"
#include "Peripherials.h"
#include "Modbus_RTU.h"
#include "Display.h"
#include "System_Config.h"

void System_Init_start(void)
{
    System_Config_init();
    Peripherials_init();
    Modbus_RTU_init();
    Modbus_RTU_set_fan_callback(Peripherials_set_fan);
    Modbus_RTU_start_task();
    Display_init();
}
