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

#define MB_PORT_NUM     (CONFIG_MB_UART_PORT_NUM)
#define MB_SLAVE_ADDR   (CONFIG_MB_SLAVE_ADDR)
#define MB_DEV_SPEED    (CONFIG_MB_UART_BAUD_RATE)

static void *mbc_slave_handle = NULL;
static modbus_rtu_fan_control_cb_t s_fan_callback = NULL;

#define MB_PAR_INFO_GET_TOUT (10)

#define MB_HOLDING_RW_START  (0)
#define MB_HOLDING_RW_COUNT  (6)
#define MB_HOLDING_RO_START  (MB_HOLDING_RW_START + MB_HOLDING_RW_COUNT)
#define MB_HOLDING_RO_COUNT  (6)

typedef struct {
	uint16_t conifg_hist_low;
	uint16_t config_hist_high;
	uint16_t config_set_temp;
	uint16_t config_manual_start_cooler_one;
	uint16_t config_manual_start_cooler_two;
	uint16_t config_set_humidity;
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

static void modbus_task(void *arg)
{
	bool cooler_one_on = false;
	bool cooler_two_on = false;

	mb_param_info_t reg_info = {0};

	for (;;) {
		mb_event_group_t events = mbc_slave_check_event(mbc_slave_handle, MB_EVENT_HOLDING_REG_WR);
		if (events & MB_EVENT_HOLDING_REG_WR) {
			(void)mbc_slave_get_param_info(mbc_slave_handle, &reg_info, MB_PAR_INFO_GET_TOUT);
		}

		uint16_t current_temp = holding_ro.current_temp; // TODO: replace with sensor value
		uint16_t current_humidity = holding_ro.current_humidity; // TODO: replace with sensor value

		uint16_t set_temp = holding_rw.config_set_temp;
		uint16_t hist_high = holding_rw.config_hist_high;
		uint16_t hist_low = holding_rw.conifg_hist_low;

		bool should_enable = (current_temp >= (uint16_t)(set_temp + hist_high));
		bool should_disable = (current_temp <= (uint16_t)(set_temp - hist_low));

		if (holding_rw.config_manual_start_cooler_one) {
			cooler_one_on = true;
		} else if (should_enable) {
			cooler_one_on = true;
		} else if (should_disable) {
			cooler_one_on = false;
		}

		if (holding_rw.config_manual_start_cooler_two) {
			cooler_two_on = true;
		} else if (should_enable) {
			cooler_two_on = true;
		} else if (should_disable) {
			cooler_two_on = false;
		}

		(void)mbc_slave_lock(mbc_slave_handle);
		holding_ro.alarm_temp = (cooler_one_on || cooler_two_on) ? 1 : 0;
		holding_ro.alarm_cooler_one_failed = 0;
		holding_ro.alarm_cooler_two_failed = 0;
		holding_ro.alarm_humidity = 0;
		holding_ro.current_temp = current_temp;
		holding_ro.current_humidity = current_humidity;
		(void)mbc_slave_unlock(mbc_slave_handle);

		if (s_fan_callback) {
			s_fan_callback(cooler_one_on, cooler_two_on);
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
}

void Modbus_RTU_start_task(void)
{
	xTaskCreate(modbus_task, "modbus_task", 4096, NULL, 10, NULL);
}

void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback)
{
	s_fan_callback = callback;
}
