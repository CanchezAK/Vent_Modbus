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
#include "Peripherials.h"
#include "Logic.h"

#define MB_PORT_NUM     (CONFIG_MB_UART_PORT_NUM)
#define MB_SLAVE_ADDR   (CONFIG_MB_SLAVE_ADDR)
#define MB_DEV_SPEED    (CONFIG_MB_UART_BAUD_RATE)

static void *mbc_slave_handle = NULL;
static modbus_rtu_fan_control_cb_t s_fan_callback = NULL;

#define MB_PAR_INFO_GET_TOUT (10)

#define MB_HOLDING_RW_START  (0)
#define MB_HOLDING_RO_START  (MB_HOLDING_RW_START + MB_HOLDING_RW_COUNT)


#define MB_REG_COUNT(type) (sizeof(type) / sizeof(uint16_t))

typedef struct __attribute__((packed)) {
	uint16_t config_mode;
	uint16_t config_temp_desired;
	uint16_t config_temp_threshold;
	uint16_t config_temp_alarm;
	uint16_t config_humidity_desired;
	uint16_t config_humidity_threshold;
	uint16_t config_humidity_alarm;
	uint16_t config_fan_manual_percent;
	uint16_t config_fan_min_percent;
	uint16_t config_filter_limit_hours;
	uint16_t config_system_enable;
	uint16_t config_smoke_enable;
	uint16_t config_modbus_addr;
	uint16_t config_modbus_baud;
	uint16_t config_service_hours;
	uint16_t config_alarm_clr;
} holding_rw_params_t;

typedef struct __attribute__((packed)) {
	uint16_t alarm_temp;
	uint16_t alarm_humidity;
	uint16_t alarm_smoke;
	uint16_t alarm_fan;
	uint16_t alarm_filter;
	uint16_t current_temp;
	uint16_t current_humidity;
	uint16_t smoke_state;
} holding_ro_params_t;

#define MB_HOLDING_RW_COUNT  (MB_REG_COUNT(holding_rw_params_t))
#define MB_HOLDING_RO_COUNT  (MB_REG_COUNT(holding_ro_params_t))

static holding_rw_params_t holding_rw = {0};
static holding_ro_params_t holding_ro = {0};

static logic_state_t s_logic_state;

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
	holding_rw.config_mode = cfg->mode;
	holding_rw.config_temp_desired = cfg->temp_desired;
	holding_rw.config_temp_threshold = cfg->temp_threshold;
	holding_rw.config_temp_alarm = cfg->temp_alarm;
	holding_rw.config_humidity_desired = cfg->humidity_desired;
	holding_rw.config_humidity_threshold = cfg->humidity_threshold;
	holding_rw.config_humidity_alarm = cfg->humidity_alarm;
	holding_rw.config_fan_manual_percent = cfg->fan_manual_percent;
	holding_rw.config_fan_min_percent = cfg->fan_min_percent;
	holding_rw.config_filter_limit_hours = cfg->filter_limit_hours;
	holding_rw.config_system_enable = cfg->system_enable;
	holding_rw.config_smoke_enable = cfg->smoke_enable;
	holding_rw.config_modbus_addr = cfg->modbus_addr;
	holding_rw.config_modbus_baud = cfg->modbus_baud;
	holding_rw.config_service_hours = System_Config_get_service_hours();
	holding_rw.config_alarm_clr = alarm_clr;
	(void)mbc_slave_unlock(mbc_slave_handle);
}

static system_config_t build_config_from_holding(void)
{
	system_config_t cfg = {
		.mode = holding_rw.config_mode,
		.temp_desired = holding_rw.config_temp_desired,
		.temp_threshold = holding_rw.config_temp_threshold,
		.temp_alarm = holding_rw.config_temp_alarm,
		.humidity_desired = holding_rw.config_humidity_desired,
		.humidity_threshold = holding_rw.config_humidity_threshold,
		.humidity_alarm = holding_rw.config_humidity_alarm,
		.fan_manual_percent = clamp_percent(holding_rw.config_fan_manual_percent),
		.fan_min_percent = clamp_percent(holding_rw.config_fan_min_percent),
		.filter_limit_hours = holding_rw.config_filter_limit_hours,
		.system_enable = holding_rw.config_system_enable ? 1 : 0,
		.smoke_enable = holding_rw.config_smoke_enable ? 1 : 0,
		.modbus_addr = holding_rw.config_modbus_addr,
		.modbus_baud = holding_rw.config_modbus_baud,
	};
	return cfg;
}

static uint32_t modbus_baud_from_index(uint16_t index)
{
	switch (index) {
	case 0:
		return 4800;
	case 1:
		return 9600;
	case 2:
		return 57600;
	case 3:
		return 115200;
	default:
		return 115200;
	}
}

static void modbus_task(void *arg)
{
	logic_output_t logic_out = {0};

	mb_param_info_t reg_info = {0};

	for (;;) {
		uint16_t alarm_clr = System_Config_get_alarm_clr();
		int param_info = (int)mbc_slave_get_param_info(mbc_slave_handle, &reg_info, 0);
		if (param_info == 0) {
			if (holding_rw.config_alarm_clr != alarm_clr) {
				System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_MODBUS, holding_rw.config_alarm_clr);
				alarm_clr = System_Config_get_alarm_clr();
			}
			if (holding_rw.config_service_hours != System_Config_get_service_hours()) {
				System_Config_set_service_hours(holding_rw.config_service_hours);
			}
			system_config_t updated = build_config_from_holding();
			System_Config_set_from_modbus(&updated);
		}

		system_config_source_t source = SYSTEM_CONFIG_SOURCE_INTERNAL;
		system_config_t cfg = System_Config_get_snapshot(&source, NULL);
		write_holding_from_config(&cfg, alarm_clr);

		uint16_t current_temp = 0;
		uint16_t current_humidity = 0;
		if (!SHT31_get_latest(&current_temp, &current_humidity, NULL)) {
			current_temp = 255;
			current_humidity = 100;
		}



		uint16_t service_hours = System_Config_get_service_hours();
		bool filter_alarm = (cfg.filter_limit_hours > 0) && (service_hours >= cfg.filter_limit_hours);
		logic_input_t logic_in = {
			.current_temp = current_temp,
			.current_humidity = current_humidity,
			.door_open = Peripherials_get_door_state(),
			.smoke_state = Peripherials_get_smoke_state(),
			.fan_alarm = Peripherials_get_fan_alarm_state(),
			.filter_alarm = filter_alarm,
		};
		Logic_step(&s_logic_state, &cfg, alarm_clr, &logic_in, &logic_out);
		if (logic_out.alarm_temp || logic_out.alarm_humidity || logic_out.alarm_smoke) {
			System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_INTERNAL, 1);
			alarm_clr = System_Config_get_alarm_clr();
			write_holding_from_config(&cfg, alarm_clr);
		}

		(void)mbc_slave_lock(mbc_slave_handle);
		holding_ro.alarm_temp = logic_out.alarm_temp ? 1 : 0;
		holding_ro.alarm_humidity = logic_out.alarm_humidity ? 1 : 0;
		holding_ro.alarm_smoke = logic_out.alarm_smoke ? 1 : 0;
		holding_ro.alarm_fan = logic_out.alarm_fan ? 1 : 0;
		holding_ro.alarm_filter = logic_out.alarm_filter ? 1 : 0;
		holding_ro.current_temp = current_temp;
		holding_ro.current_humidity = current_humidity;
		holding_ro.smoke_state = logic_in.smoke_state ? 1 : 0;
		(void)mbc_slave_unlock(mbc_slave_handle);

		if (s_fan_callback) {
			s_fan_callback(logic_out.fan_percent);
		}

		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

void Modbus_RTU_init(void)
{
	mb_register_area_descriptor_t reg_area = {0};
	system_config_t cfg = System_Config_get_snapshot(NULL, NULL);
	uint16_t modbus_addr = cfg.modbus_addr ? cfg.modbus_addr : MB_SLAVE_ADDR;
	uint32_t modbus_baud = modbus_baud_from_index(cfg.modbus_baud);
	Logic_init(&s_logic_state);

	mb_communication_info_t comm_config = {
		.ser_opts.port = MB_PORT_NUM,
#if CONFIG_MB_COMM_MODE_ASCII
		.ser_opts.mode = MB_ASCII,
#elif CONFIG_MB_COMM_MODE_RTU
		.ser_opts.mode = MB_RTU,
#endif
		.ser_opts.baudrate = modbus_baud,
		.ser_opts.parity = MB_PARITY_NONE,
		.ser_opts.uid = modbus_addr,
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
}

void Modbus_RTU_start_task(void)
{
	xTaskCreate(modbus_task, "modbus_task", 4096, NULL, 10, NULL);
}

void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback)
{
	s_fan_callback = callback;
}

