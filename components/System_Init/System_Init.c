#include "System_Init.h"
#include "Peripherials.h"
#include "Modbus_RTU.h"

void System_Init_start(void)
{
    Peripherials_init();
    Modbus_RTU_start_task();
}
