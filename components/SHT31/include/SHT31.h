#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

bool SHT31_init(i2c_master_bus_handle_t i2c_bus);
bool SHT31_get_latest(uint16_t *temp_c, uint16_t *humidity_percent, bool *valid);
bool SHT31_is_present(void);

#ifdef __cplusplus
}
#endif
