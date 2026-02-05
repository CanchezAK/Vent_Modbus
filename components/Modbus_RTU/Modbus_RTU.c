#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "Modbus_RTU.h"
#include "esp_err.h"
#include "mbcontroller.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "System_Config.h"
#include "SHT31.h"

#define MB_PORT_NUM     (CONFIG_MB_UART_PORT_NUM)
#define MB_SLAVE_ADDR   (CONFIG_MB_SLAVE_ADDR)
#define MB_DEV_SPEED    (CONFIG_MB_UART_BAUD_RATE)

static void *mbc_slave_handle = NULL;
static modbus_rtu_fan_control_cb_t s_fan_callback = NULL;

#define MB_PAR_INFO_GET_TOUT (10)

#define MB_HOLDING_RW_START  (0)
#define MB_HOLDING_RW_COUNT  (11)
#define MB_HOLDING_RO_START  (MB_HOLDING_RW_START + MB_HOLDING_RW_COUNT)
#define MB_HOLDING_RO_COUNT  (6)

typedef struct {
	uint16_t config_temp_desired;
	uint16_t config_temp_threshold;
	uint16_t config_temp_alarm;
	uint16_t config_humidity_desired;
	uint16_t config_humidity_threshold;
	uint16_t config_humidity_alarm;
	uint16_t config_fan1_mode;
	uint16_t config_fan2_mode;
	uint16_t config_fan1_percent;
	uint16_t config_fan2_percent;
	uint16_t config_alarm_clr;
} holding_rw_params_t;

typedef struct {
	uint16_t alarm_temp;
	uint16_t alarm_cooler_one_failed;
	uint16_t alarm_cooler_two_failed;
	uint16_t alarm_humidity;
	uint16_t current_temp;
	uint16_t current_humidity;
} holding_ro_params_t;

static holding_rw_params_t holding_rw = {0};
static holding_ro_params_t holding_ro = {0};

static bool s_alarm_temp_latched = false;
static bool s_alarm_humidity_latched = false;

static uint8_t clamp_percent(uint16_t value)
{
	return (value > 100) ? 100 : (uint8_t)value;
}

static void write_holding_from_config(const system_config_t *cfg, uint16_t alarm_clr)
{
	if (!cfg) {
		return;
	}
	(void)mbc_slave_lock(mbc_slave_handle);
	holding_rw.config_temp_desired = cfg->temp_desired_centi;
	holding_rw.config_temp_threshold = cfg->temp_threshold_centi;
	holding_rw.config_temp_alarm = cfg->temp_alarm_centi;
	holding_rw.config_humidity_desired = cfg->humidity_desired_centi;
	holding_rw.config_humidity_threshold = cfg->humidity_threshold_centi;
	holding_rw.config_humidity_alarm = cfg->humidity_alarm_centi;
	holding_rw.config_fan1_mode = cfg->fan1_mode_manual;
	holding_rw.config_fan2_mode = cfg->fan2_mode_manual;
	holding_rw.config_fan1_percent = cfg->fan1_percent;
	holding_rw.config_fan2_percent = cfg->fan2_percent;
	holding_rw.config_alarm_clr = alarm_clr;
	(void)mbc_slave_unlock(mbc_slave_handle);
}

static system_config_t build_config_from_holding(void)
{
	system_config_t cfg = {
		.temp_desired_centi = holding_rw.config_temp_desired,
		.temp_threshold_centi = holding_rw.config_temp_threshold,
		.temp_alarm_centi = holding_rw.config_temp_alarm,
			.humidity_desired_centi = holding_rw.config_humidity_desired,
			.humidity_threshold_centi = holding_rw.config_humidity_threshold,
			.humidity_alarm_centi = holding_rw.config_humidity_alarm,
		.fan1_mode_manual = holding_rw.config_fan1_mode ? 1 : 0,
		.fan2_mode_manual = holding_rw.config_fan2_mode ? 1 : 0,
		.fan1_percent = clamp_percent(holding_rw.config_fan1_percent),
		.fan2_percent = clamp_percent(holding_rw.config_fan2_percent),
	};
	return cfg;
}

static uint8_t compute_auto_percent(uint16_t current_temp,
							 uint16_t desired_temp,
							 uint16_t threshold_temp,
							 uint16_t alarm_temp,
							 uint8_t min_percent,
							 bool *alarm_active)
{
	if (alarm_active) {
		*alarm_active = (current_temp >= alarm_temp);
	}

	if (current_temp >= alarm_temp) {
		return 100;
	}

	if (current_temp >= threshold_temp) {
		return 100;
	}

	if (current_temp <= desired_temp) {
		return min_percent;
	}

	if (threshold_temp <= desired_temp) {
		return 100;
	}

	uint32_t delta = (uint32_t)(current_temp - desired_temp);
	uint32_t span = (uint32_t)(threshold_temp - desired_temp);
	uint32_t scale = (uint32_t)(100 - min_percent);
	uint32_t value = min_percent + (delta * scale) / span;
	if (value > 100) {
		value = 100;
	}
	return (uint8_t)value;
}

static void modbus_task(void *arg)
{
	uint8_t cooler_one_percent = 0;
	uint8_t cooler_two_percent = 0;

	mb_param_info_t reg_info = {0};

	for (;;) {
		uint16_t alarm_clr = System_Config_get_alarm_clr();
		int param_info = (int)mbc_slave_get_param_info(mbc_slave_handle, &reg_info, 0);
		if (param_info == 0) {
			if (holding_rw.config_alarm_clr != alarm_clr) {
				System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_MODBUS, holding_rw.config_alarm_clr);
				alarm_clr = System_Config_get_alarm_clr();
			}
			system_config_t updated = build_config_from_holding();
			System_Config_set_from_modbus(&updated);
		}

		if (alarm_clr == 0) {
			s_alarm_temp_latched = false;
			s_alarm_humidity_latched = false;
		}

		system_config_source_t source = SYSTEM_CONFIG_SOURCE_INTERNAL;
		system_config_t cfg = System_Config_get_snapshot(&source, NULL);
		write_holding_from_config(&cfg, alarm_clr);

		uint16_t current_temp = 0;
		uint16_t current_humidity = 0;
		if (!SHT31_get_latest(&current_temp, &current_humidity, NULL)) {
			current_temp = 25500;
			current_humidity = 10000;
		}

		uint16_t desired_temp = cfg.temp_desired_centi;
		uint16_t threshold_temp = cfg.temp_threshold_centi;
		uint16_t alarm_temp = cfg.temp_alarm_centi;
		uint16_t desired_hum = cfg.humidity_desired_centi;
		uint16_t threshold_hum = cfg.humidity_threshold_centi;
		uint16_t alarm_hum = cfg.humidity_alarm_centi;

		uint8_t fan1_mode = cfg.fan1_mode_manual ? 1 : 0;
		uint8_t fan2_mode = cfg.fan2_mode_manual ? 1 : 0;
		uint8_t fan1_user_percent = clamp_percent(cfg.fan1_percent);
		uint8_t fan2_user_percent = clamp_percent(cfg.fan2_percent);

		uint8_t fan1_min_percent = (fan1_user_percent == 0) ? 0 : fan1_user_percent;
		uint8_t fan2_min_percent = (fan2_user_percent == 0) ? 0 : fan2_user_percent;

		uint8_t temp_percent = compute_auto_percent(current_temp, desired_temp,
										 threshold_temp, alarm_temp,
										 fan1_min_percent, NULL);
		uint8_t hum_percent = compute_auto_percent(current_humidity, desired_hum,
										 threshold_hum, alarm_hum,
										 fan1_min_percent, NULL);
		uint8_t auto_percent = (temp_percent > hum_percent) ? temp_percent : hum_percent;
		cooler_one_percent = (fan1_mode && fan1_user_percent != 0) ?
				fan1_user_percent : auto_percent;

		temp_percent = compute_auto_percent(current_temp, desired_temp,
										 threshold_temp, alarm_temp,
										 fan2_min_percent, NULL);
		hum_percent = compute_auto_percent(current_humidity, desired_hum,
										 threshold_hum, alarm_hum,
										 fan2_min_percent, NULL);
		auto_percent = (temp_percent > hum_percent) ? temp_percent : hum_percent;
		cooler_two_percent = (fan2_mode && fan2_user_percent != 0) ?
				fan2_user_percent : auto_percent;

		if (current_temp >= alarm_temp) {
			s_alarm_temp_latched = true;
		}
		if (current_humidity >= alarm_hum) {
			s_alarm_humidity_latched = true;
		}

		if (s_alarm_temp_latched || s_alarm_humidity_latched) {
			System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_INTERNAL, 1);
			alarm_clr = System_Config_get_alarm_clr();
			write_holding_from_config(&cfg, alarm_clr);
		}

		if (s_alarm_temp_latched || s_alarm_humidity_latched) {
			cooler_one_percent = 100;
			cooler_two_percent = 100;
		}

		(void)mbc_slave_lock(mbc_slave_handle);
		holding_ro.alarm_temp = s_alarm_temp_latched ? 1 : 0;
		holding_ro.alarm_cooler_one_failed = 0;
		holding_ro.alarm_cooler_two_failed = 0;
		holding_ro.alarm_humidity = s_alarm_humidity_latched ? 1 : 0;
		holding_ro.current_temp = current_temp;
		holding_ro.current_humidity = current_humidity;
		(void)mbc_slave_unlock(mbc_slave_handle);

		if (s_fan_callback) {
			s_fan_callback(cooler_one_percent, cooler_two_percent);
		}

		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

void Modbus_RTU_init(void)
{
	mb_register_area_descriptor_t reg_area = {0};

	mb_communication_info_t comm_config = {
		.ser_opts.port = MB_PORT_NUM,
#if CONFIG_MB_COMM_MODE_ASCII
		.ser_opts.mode = MB_ASCII,
#elif CONFIG_MB_COMM_MODE_RTU
		.ser_opts.mode = MB_RTU,
#endif
		.ser_opts.baudrate = MB_DEV_SPEED,
		.ser_opts.parity = MB_PARITY_NONE,
		.ser_opts.uid = MB_SLAVE_ADDR,
		.ser_opts.data_bits = UART_DATA_8_BITS,
		.ser_opts.stop_bits = UART_STOP_BITS_1
	};

	ESP_ERROR_CHECK(mbc_slave_create_serial(&comm_config, &mbc_slave_handle));

	reg_area.type = MB_PARAM_HOLDING;
	reg_area.start_offset = MB_HOLDING_RW_START;
	reg_area.address = (void *)&holding_rw;
	reg_area.size = sizeof(holding_rw);
	reg_area.access = MB_ACCESS_RW;
	ESP_ERROR_CHECK(mbc_slave_set_descriptor(mbc_slave_handle, reg_area));

	reg_area.type = MB_PARAM_HOLDING;
	reg_area.start_offset = MB_HOLDING_RO_START;
	reg_area.address = (void *)&holding_ro;
	reg_area.size = sizeof(holding_ro);
	reg_area.access = MB_ACCESS_RO;
	ESP_ERROR_CHECK(mbc_slave_set_descriptor(mbc_slave_handle, reg_area));

	ESP_ERROR_CHECK(uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD,
								 CONFIG_MB_UART_RXD, CONFIG_MB_UART_RTS,
								 UART_PIN_NO_CHANGE));

	ESP_ERROR_CHECK(uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));

	ESP_ERROR_CHECK(mbc_slave_start(mbc_slave_handle));

	// if (!SHT31_is_present()) {
	// 	(void)mbc_slave_lock(mbc_slave_handle);
	// 	holding_ro.current_temp = 25500;
	// 	holding_ro.current_humidity = 10000;
	// 	holding_ro.alarm_temp = 1;
	// 	holding_ro.alarm_humidity = 1;
	// 	s_alarm_temp_latched = true;
	// 	s_alarm_humidity_latched = true;
	// 	holding_rw.config_alarm_clr = 1;
	// 	(void)mbc_slave_unlock(mbc_slave_handle);
	// }
}

void Modbus_RTU_start_task(void)
{
	xTaskCreate(modbus_task, "modbus_task", 4096, NULL, 10, NULL);
}

void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback)
{
	s_fan_callback = callback;
}

