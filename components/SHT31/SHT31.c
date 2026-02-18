#include <stdio.h>
#include "SHT31.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SHT31_I2C_ADDR_LOW    (0x44)
#define SHT31_I2C_ADDR_HIGH   (0x45)
#define SHT31_CMD_SOFT_RESET  (0x30A2)
#define SHT31_CMD_MEAS_HIGH   (0x2400)
#define SHT31_READ_LEN        (6)
#define SHT31_POLL_MS         (500)

static const char *TAG = "SHT31";

static i2c_master_dev_handle_t s_sht31_handle = NULL;
static uint8_t s_sht31_addr = 0;
static bool s_present = false;
static bool s_valid = false;
static uint16_t s_temp_c = 0;
static uint16_t s_humidity_percent = 0;
static i2c_master_bus_handle_t s_i2c_bus = NULL;

static uint8_t sht31_crc8(const uint8_t *data, size_t len)
{
	uint8_t crc = 0xFF;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t bit = 0; bit < 8; bit++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ 0x31;
			} else {
				crc <<= 1;
			}
		}
	}
	return crc;
}

static esp_err_t sht31_send_cmd(uint16_t cmd)
{
	uint8_t buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
	return i2c_master_transmit(s_sht31_handle, buf, sizeof(buf), -1);
}

static esp_err_t sht31_read_measurement(uint16_t *temp_c, uint16_t *humidity_percent)
{
	uint8_t data[SHT31_READ_LEN] = {0};
	ESP_RETURN_ON_ERROR(sht31_send_cmd(SHT31_CMD_MEAS_HIGH), TAG,
					"measure command failed");
	vTaskDelay(pdMS_TO_TICKS(15));
	ESP_RETURN_ON_ERROR(i2c_master_receive(s_sht31_handle, data, sizeof(data), -1), TAG,
					"read failed");

	if (sht31_crc8(&data[0], 2) != data[2] || sht31_crc8(&data[3], 2) != data[5]) {
		ESP_LOGW(TAG, "CRC mismatch");
		return ESP_ERR_INVALID_CRC;
	}

	uint16_t raw_temp = (uint16_t)(data[0] << 8) | data[1];
	uint16_t raw_hum = (uint16_t)(data[3] << 8) | data[4];

	int32_t temp_scaled = -4500 + (17500 * (int32_t)raw_temp) / 65535;
	uint32_t hum_scaled = (10000UL * raw_hum) / 65535;

	if (temp_scaled < 0) {
		temp_scaled = 0;
	} else if (temp_scaled > 65535) {
		temp_scaled = 65535;
	}

	uint16_t temp_int = (uint16_t)((temp_scaled + 50) / 100);
	uint16_t hum_int = (uint16_t)((hum_scaled + 50) / 100);

	if (temp_c) {
		*temp_c = temp_int;
	}
	if (humidity_percent) {
		*humidity_percent = hum_int;
	}

	return ESP_OK;
}

static void sht31_task(void *arg)
{
	(void)arg;
	uint16_t temp_c = 0;
	uint16_t humidity_percent = 0;
	int32_t temp_filtered = -1;
	int32_t hum_filtered = -1;

	for (;;) {
		bool ok = false;
		if (s_sht31_handle &&
			sht31_read_measurement(&temp_c, &humidity_percent) == ESP_OK) {
			if (temp_filtered < 0) {
				temp_filtered = temp_c;
				hum_filtered = humidity_percent;
			} else {
				temp_filtered += (int32_t)(temp_c - temp_filtered) / 5;
				hum_filtered += (int32_t)(humidity_percent - hum_filtered) / 5;
			}
			if (temp_filtered < 0) {
				temp_filtered = 0;
			}
			if (hum_filtered < 0) {
				hum_filtered = 0;
			}
			s_temp_c = (uint16_t)temp_filtered;
			s_humidity_percent = (uint16_t)hum_filtered;
			s_valid = true;
			s_present = true;
			ok = true;
		}

		if (!ok) {
			s_valid = false;
			bool present = false;
			if (s_i2c_bus) {
				if (i2c_master_probe(s_i2c_bus, SHT31_I2C_ADDR_LOW, 50) == ESP_OK ||
					i2c_master_probe(s_i2c_bus, SHT31_I2C_ADDR_HIGH, 50) == ESP_OK) {
					present = true;
				}
			}
			s_present = present;
		}
		vTaskDelay(pdMS_TO_TICKS(SHT31_POLL_MS));
	}
}

bool SHT31_init(i2c_master_bus_handle_t i2c_bus)
{
	if (!i2c_bus) {
		return false;
	}
	s_i2c_bus = i2c_bus;

	if (i2c_master_probe(i2c_bus, SHT31_I2C_ADDR_LOW, 50) == ESP_OK) {
		s_sht31_addr = SHT31_I2C_ADDR_LOW;
	} else if (i2c_master_probe(i2c_bus, SHT31_I2C_ADDR_HIGH, 50) == ESP_OK) {
		s_sht31_addr = SHT31_I2C_ADDR_HIGH;
	} else {
		ESP_LOGW(TAG, "SHT31 not detected on I2C");
		s_present = false;
		s_valid = false;
		return false;
	}

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = s_sht31_addr,
		.scl_speed_hz = 100000,
	};
	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &s_sht31_handle));
	ESP_ERROR_CHECK(sht31_send_cmd(SHT31_CMD_SOFT_RESET));
	vTaskDelay(pdMS_TO_TICKS(5));
	ESP_LOGI(TAG, "SHT31 ready at 0x%02X", s_sht31_addr);
	s_present = true;
	s_valid = false;
	xTaskCreate(sht31_task, "sht31_task", 3072, NULL, 8, NULL);
	return true;
}

bool SHT31_get_latest(uint16_t *temp_c, uint16_t *humidity_percent, bool *valid)
{
	if (temp_c) {
		*temp_c = s_temp_c;
	}
	if (humidity_percent) {
		*humidity_percent = s_humidity_percent;
	}
	if (valid) {
		*valid = s_valid;
	}
	return s_present && s_valid;
}

bool SHT31_is_present(void)
{
	return s_present;
}
