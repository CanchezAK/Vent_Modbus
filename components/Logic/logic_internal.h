#pragma once

#include <stdbool.h>
#include <stdint.h>

uint8_t Logic_clamp_percent(uint16_t value);
uint8_t Logic_compute_percent(uint16_t current_value,
						  uint16_t desired_value,
						  uint16_t threshold_value,
						  uint16_t alarm_value,
					  uint8_t min_percent,
						  bool *alarm_active);
