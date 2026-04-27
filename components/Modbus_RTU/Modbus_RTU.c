#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "Modbus_RTU.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mbcontroller.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
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
static const char *TAG = "Modbus_RTU";
static uint16_t s_diag_addr = 0;
static uint16_t s_diag_baud_idx = 0;
static uint32_t s_diag_baud_req = 0;
static uint32_t s_diag_baud_actual = 0;

#define MB_PAR_INFO_GET_TOUT (10)

#define MB_HOLDING_RW_START  (0)
#define MB_HOLDING_RO_START  (MB_HOLDING_RW_START + MB_HOLDING_RW_COUNT)


#define MB_REG_COUNT(type) (sizeof(type) / sizeof(uint16_t))

typedef struct __attribute__((packed)) {
	uint16_t config_mode;
	uint16_t config_temp_desired;
	uint16_t config_humidity_desired;
	uint16_t config_fan_manual_percent;
	uint16_t config_filter_limit_hours;
	uint16_t config_modbus_addr;
	uint16_t config_modbus_baud;
	uint16_t cmd_clear_filter_hours;
	uint16_t cmd_clear_alarms;
} holding_rw_params_t;

typedef struct __attribute__((packed)) {
	uint16_t alarm_temp;
	uint16_t alarm_humidity;
	uint16_t alarm_smoke;
	uint16_t alarm_fan;
	uint16_t alarm_filter;
	uint16_t current_temp;
	uint16_t current_humidity;
	uint16_t door_state;
	uint16_t smoke_state;
	uint16_t vent_active;
	uint16_t fan_percent;
	uint16_t filter_clogging_percent;
	uint16_t service_hours;
} holding_ro_params_t;

#define MB_HOLDING_RW_COUNT  (MB_REG_COUNT(holding_rw_params_t))
#define MB_HOLDING_RO_COUNT  (MB_REG_COUNT(holding_ro_params_t))

static holding_rw_params_t holding_rw = {0};
static holding_ro_params_t holding_ro = {0};
static portMUX_TYPE s_holding_ro_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_ro_update_task = NULL;
static holding_rw_params_t s_last_published_rw = {0};
static bool s_last_published_rw_valid = false;

static logic_state_t s_logic_state;

static void notify_ro_update_task(void)
{
	TaskHandle_t task = NULL;
	portENTER_CRITICAL(&s_holding_ro_lock);
	task = s_ro_update_task;
	portEXIT_CRITICAL(&s_holding_ro_lock);

	if (task) {
		xTaskNotifyGive(task);
	}
}

static uint8_t clamp_percent(uint16_t value)
{
	return (value > 100) ? 100 : (uint8_t)value;
}

static uint16_t normalize_bool_u16(uint16_t value)
{
	return value ? 1U : 0U;
}

static uint16_t calc_filter_clogging_percent(uint16_t limit_hours)
{
	if (limit_hours == 0U) {
		return 0U;
	}

	uint32_t service_seconds = System_Config_get_service_seconds();
	uint32_t limit_seconds = (uint32_t)limit_hours * 3600U;
	if (limit_seconds == 0U) {
		return 0U;
	}

	uint32_t percent = (service_seconds * 100U) / limit_seconds;
	if (percent > 100U) {
		percent = 100U;
	}
	return (uint16_t)percent;
}

static holding_rw_params_t build_rw_from_config(const system_config_t *cfg)
{
	holding_rw_params_t rw = {0};
	if (!cfg) {
		return rw;
	}

	rw.config_mode = cfg->mode;
	rw.config_temp_desired = cfg->temp_desired;
	rw.config_humidity_desired = cfg->humidity_desired;
	rw.config_fan_manual_percent = cfg->fan_manual_percent;
	rw.config_filter_limit_hours = cfg->filter_limit_hours;
	rw.config_modbus_addr = cfg->modbus_addr;
	rw.config_modbus_baud = cfg->modbus_baud;
	rw.cmd_clear_filter_hours = 0;
	rw.cmd_clear_alarms = 0;

	return rw;
}

static void write_holding_from_config(const system_config_t *cfg)
{
	if (!cfg) {
		return;
	}
	holding_rw_params_t rw = build_rw_from_config(cfg);
	(void)mbc_slave_lock(mbc_slave_handle);
	holding_rw = rw;
	(void)mbc_slave_unlock(mbc_slave_handle);
	s_last_published_rw = rw;
	s_last_published_rw_valid = true;
}

static void apply_rw_to_config(system_config_t *cfg, const holding_rw_params_t *rw)
{
	if (!cfg || !rw) {
		return;
	}

	cfg->mode = rw->config_mode;
	cfg->temp_desired = rw->config_temp_desired;
	cfg->humidity_desired = rw->config_humidity_desired;
	cfg->fan_manual_percent = clamp_percent(rw->config_fan_manual_percent);
	cfg->fan_min_percent = cfg->fan_manual_percent;
	cfg->filter_limit_hours = rw->config_filter_limit_hours;
	cfg->modbus_addr = rw->config_modbus_addr;
	cfg->modbus_baud = rw->config_modbus_baud;
}

static bool holding_rw_cfg_differs(const holding_rw_params_t *a, const holding_rw_params_t *b)
{
	if (!a || !b) {
		return false;
	}

	return a->config_mode != b->config_mode ||
	       a->config_temp_desired != b->config_temp_desired ||
	       a->config_humidity_desired != b->config_humidity_desired ||
	       a->config_fan_manual_percent != b->config_fan_manual_percent ||
	       a->config_filter_limit_hours != b->config_filter_limit_hours ||
	       a->config_modbus_addr != b->config_modbus_addr ||
	       a->config_modbus_baud != b->config_modbus_baud;
}

static uint32_t modbus_baud_from_index(uint16_t index)
{
	switch (index) {
	case 0:
		return 9600;
	case 1:
		return 19200;
	case 2:
		return 38400;
	case 3:
		return 115200;
	default:
		return 115200;
	}
}

static void modbus_task(void *arg)
{
	logic_output_t logic_out = {0};

	for (;;) {
		uint16_t alarm_clr = System_Config_get_alarm_clr();
		holding_rw_params_t rw_copy;
		(void)mbc_slave_lock(mbc_slave_handle);
		rw_copy = holding_rw;
		(void)mbc_slave_unlock(mbc_slave_handle);

		if (s_last_published_rw_valid && holding_rw_cfg_differs(&rw_copy, &s_last_published_rw)) {
			system_config_t updated = System_Config_get_snapshot(NULL, NULL);
			apply_rw_to_config(&updated, &rw_copy);
			System_Config_set_from_modbus(&updated);
		}

		if (rw_copy.cmd_clear_filter_hours != 0U) {
			System_Config_set_service_hours(0);
		}
		if (rw_copy.cmd_clear_alarms != 0U) {
			System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_MODBUS, 0);
		}

		system_config_source_t source = SYSTEM_CONFIG_SOURCE_INTERNAL;
		system_config_t cfg = System_Config_get_snapshot(&source, NULL);
		( void )source;
		write_holding_from_config(&cfg);
		alarm_clr = System_Config_get_alarm_clr();

		uint16_t current_temp = 0;
		uint16_t current_humidity = 0;
		if (!SHT31_get_latest(&current_temp, &current_humidity, NULL)) {
			current_temp = 255;
			current_humidity = 100;
		}
		uint32_t service_seconds = System_Config_get_service_seconds();
		uint16_t service_hours = System_Config_get_service_hours();
		uint32_t filter_limit_seconds = (uint32_t)cfg.filter_limit_hours * 3600U;
		bool filter_alarm = (cfg.filter_limit_hours > 0U) && (service_seconds >= filter_limit_seconds);
		bool door_state = Peripherials_get_door_state();
		bool smoke_state = Peripherials_get_smoke_state();
		logic_input_t logic_in = {
			.current_temp = current_temp,
			.current_humidity = current_humidity,
			.door_open = door_state,
			.smoke_state = smoke_state,
			.fan_alarm = Peripherials_get_fan_alarm_state(),
			.filter_alarm = filter_alarm,
		};
		Logic_step(&s_logic_state, &cfg, alarm_clr, &logic_in, &logic_out);
		bool any_alarm = logic_out.alarm_temp || logic_out.alarm_humidity || logic_out.alarm_smoke ||
			logic_out.alarm_fan || logic_out.alarm_filter;
		Peripherials_set_buzzer(any_alarm);
		if (any_alarm) {
			System_Config_set_alarm_clr(SYSTEM_CONFIG_SOURCE_INTERNAL, 1);
			write_holding_from_config(&cfg);
		}

		uint16_t clogging_percent = calc_filter_clogging_percent(cfg.filter_limit_hours);
		uint16_t fan_percent = logic_out.fan_percent;
		uint16_t vent_active = (fan_percent > 0U) ? 1U : 0U;

		(void)mbc_slave_lock(mbc_slave_handle);
		portENTER_CRITICAL(&s_holding_ro_lock);
		holding_ro.alarm_temp = normalize_bool_u16(logic_out.alarm_temp ? 1U : 0U);
		holding_ro.alarm_humidity = normalize_bool_u16(logic_out.alarm_humidity ? 1U : 0U);
		holding_ro.alarm_smoke = normalize_bool_u16(logic_out.alarm_smoke ? 1U : 0U);
		holding_ro.alarm_fan = normalize_bool_u16(logic_out.alarm_fan ? 1U : 0U);
		holding_ro.alarm_filter = normalize_bool_u16(logic_out.alarm_filter ? 1U : 0U);
		holding_ro.current_temp = current_temp;
		holding_ro.current_humidity = current_humidity;
		holding_ro.door_state = normalize_bool_u16(door_state ? 1U : 0U);
		holding_ro.smoke_state = normalize_bool_u16(smoke_state ? 1U : 0U);
		holding_ro.vent_active = vent_active;
		holding_ro.fan_percent = fan_percent;
		holding_ro.filter_clogging_percent = clogging_percent;
		holding_ro.service_hours = service_hours;
		portEXIT_CRITICAL(&s_holding_ro_lock);
		(void)mbc_slave_unlock(mbc_slave_handle);
		notify_ro_update_task();

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
	s_diag_addr = modbus_addr;
	s_diag_baud_idx = cfg.modbus_baud;
	s_diag_baud_req = modbus_baud;
	ESP_LOGI(TAG, "Init serial slave: cfg.addr=%u cfg.baud_idx=%u -> baud=%lu",
			 (unsigned)cfg.modbus_addr,
			 (unsigned)cfg.modbus_baud,
			 (unsigned long)modbus_baud);
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
	/*
	 * Re-apply baud rate after RS485 mode setup.
	 * On some targets/drivers, mode switch may leave UART at default timing.
	 */
	ESP_ERROR_CHECK(uart_set_baudrate(MB_PORT_NUM, modbus_baud));
	uint32_t actual_baud = 0;
	ESP_ERROR_CHECK(uart_get_baudrate(MB_PORT_NUM, &actual_baud));
	s_diag_baud_actual = actual_baud;
	ESP_LOGI(TAG, "UART configured: port=%u addr=%u baud_req=%lu baud_actual=%lu",
			 (unsigned)MB_PORT_NUM,
			 (unsigned)modbus_addr,
			 (unsigned long)modbus_baud,
			 (unsigned long)actual_baud);

	ESP_ERROR_CHECK(mbc_slave_start(mbc_slave_handle));
}

void Modbus_RTU_start_task(void)
{
	xTaskCreatePinnedToCore(modbus_task, "modbus_task", 4096, NULL, 10, NULL, 0);
}

void Modbus_RTU_set_fan_callback(modbus_rtu_fan_control_cb_t callback)
{
	s_fan_callback = callback;
}

bool Modbus_RTU_get_ro_snapshot(uint16_t *current_temp, uint16_t *current_humidity)
{
	modbus_rtu_ro_snapshot_t snap;
	if (!current_temp || !current_humidity) {
		return false;
	}
	if (!Modbus_RTU_get_ro_full_snapshot(&snap)) {
		return false;
	}
	*current_temp = snap.current_temp;
	*current_humidity = snap.current_humidity;
	return true;
}

bool Modbus_RTU_get_ro_full_snapshot(modbus_rtu_ro_snapshot_t *out)
{
	if (!out) {
		return false;
	}

	portENTER_CRITICAL(&s_holding_ro_lock);
	out->alarm_temp = holding_ro.alarm_temp;
	out->alarm_humidity = holding_ro.alarm_humidity;
	out->alarm_smoke = holding_ro.alarm_smoke;
	out->alarm_fan = holding_ro.alarm_fan;
	out->alarm_filter = holding_ro.alarm_filter;
	out->current_temp = holding_ro.current_temp;
	out->current_humidity = holding_ro.current_humidity;
	out->door_state = holding_ro.door_state;
	out->smoke_state = holding_ro.smoke_state;
	out->vent_active = holding_ro.vent_active;
	out->fan_percent = holding_ro.fan_percent;
	out->filter_clogging_percent = holding_ro.filter_clogging_percent;
	out->service_hours = holding_ro.service_hours;
	portEXIT_CRITICAL(&s_holding_ro_lock);

	return true;
}

void Modbus_RTU_register_ro_update_task(void *task_handle)
{
	portENTER_CRITICAL(&s_holding_ro_lock);
	s_ro_update_task = (TaskHandle_t)task_handle;
	portEXIT_CRITICAL(&s_holding_ro_lock);
}

uint16_t Modbus_RTU_get_diag_addr(void)
{
	return s_diag_addr;
}

uint16_t Modbus_RTU_get_diag_baud_index(void)
{
	return s_diag_baud_idx;
}

uint32_t Modbus_RTU_get_diag_baud_requested(void)
{
	return s_diag_baud_req;
}

uint32_t Modbus_RTU_get_diag_baud_actual(void)
{
	return s_diag_baud_actual;
}

