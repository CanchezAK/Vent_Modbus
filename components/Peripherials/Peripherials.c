#include <stdio.h>
#include "Peripherials.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_ldo_regulator.h"
#include "System_Config.h"
#include "SHT31.h"

#define FAN_COOLER_ONE_GPIO  (GPIO_NUM_6)
#define ZERO_CROSS_GPIO      (GPIO_NUM_48)
#define TRIAC_STATE_GPIO     (GPIO_NUM_47)
#define BUZZER_GPIO          (GPIO_NUM_4)
#define DOOR_SWITCH_GPIO     (GPIO_NUM_26)
#define SMOKE_SENSOR_GPIO    (GPIO_NUM_45)
#define DOOR_DEBOUNCE_US     (50000)
#define FAN_ALARM_DEBOUNCE_US (1000000)

#define DEFAULT_AC_HALF_CYCLE_US     (10000)
#define DEFAULT_TRIAC_PULSE_US       (100)
#define DEFAULT_TRIAC_MIN_DELAY_US   (200)

#define I2C_SDA_GPIO          (GPIO_NUM_3)
#define I2C_SCL_GPIO          (GPIO_NUM_2)
#define I2C_FREQ_HZ           (100000)

#define GT911_I2C_ADDR_5D     (0x5D)
#define GT911_I2C_ADDR_14     (0x14)
#define I2C_PROBE_TIMEOUT_MS  (50)

#define MIPI_DSI_PHY_LDO_CHAN       (3)
#define MIPI_DSI_PHY_LDO_VOLTAGE_MV (2500)
// DSI lane speed. 700 is stable baseline for 30Hz operation.
// Rollback: set back to 700.
#define MIPI_DSI_LANE_BIT_RATE_MBPS (700)

i2c_master_bus_handle_t i2c_bus_handle = NULL;
esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
static esp_timer_handle_t fan1_timer = NULL;
static TaskHandle_t zero_cross_task_handle = NULL;
static volatile uint8_t fan1_percent = 0;
static uint32_t s_ac_half_cycle_us = DEFAULT_AC_HALF_CYCLE_US;
static uint32_t s_triac_min_delay_us = DEFAULT_TRIAC_MIN_DELAY_US;
static uint32_t s_triac_pulse_us = DEFAULT_TRIAC_PULSE_US;
static bool s_door_state = false;
static bool s_door_last_raw = false;
static int64_t s_door_last_change_us = 0;
static bool s_door_initialized = false;
static esp_ldo_channel_handle_t s_mipi_phy_ldo = NULL;
static bool s_fan_alarm_state = false;
static int64_t s_fan_alarm_mismatch_since_us = 0;
static int64_t s_fan_alarm_match_since_us = 0;

static const char *TAG = "Peripherials";

static uint32_t phase_delay_from_percent(uint8_t percent)
{
	if (percent >= 100) {
		return s_triac_min_delay_us;
	}
	uint32_t max_delay = s_ac_half_cycle_us - s_triac_pulse_us;
	uint32_t range = max_delay - s_triac_min_delay_us;
	uint32_t scaled = (range * (100 - percent)) / 100;
	return s_triac_min_delay_us + scaled;
}

static void fan1_timer_cb(void *arg)
{
	(void)arg;
	gpio_set_level(FAN_COOLER_ONE_GPIO, 1);
	esp_rom_delay_us(s_triac_pulse_us);
	gpio_set_level(FAN_COOLER_ONE_GPIO, 0);
}

static void IRAM_ATTR zero_cross_isr(void *arg)
{
	(void)arg;
	BaseType_t higher_priority = pdFALSE;
	if (zero_cross_task_handle) {
		vTaskNotifyGiveFromISR(zero_cross_task_handle, &higher_priority);
	}
	if (higher_priority) {
		portYIELD_FROM_ISR();
	}
}

static void zero_cross_task(void *arg)
{
	(void)arg;
	for (;;) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		uint8_t p1 = fan1_percent;
		if (p1 > 0) {
			esp_timer_start_once(fan1_timer, phase_delay_from_percent(p1));
		}
	}
}


static void init_gpio(void)
{
	gpio_config_t io_conf = {
		.pin_bit_mask = (1ULL << FAN_COOLER_ONE_GPIO) | (1ULL << BUZZER_GPIO),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK(gpio_config(&io_conf));
	ESP_ERROR_CHECK(gpio_set_level(FAN_COOLER_ONE_GPIO, 0));
	ESP_ERROR_CHECK(gpio_set_level(BUZZER_GPIO, 0));

	gpio_config_t zc_conf = {
		.pin_bit_mask = (1ULL << ZERO_CROSS_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_POSEDGE
	};
	ESP_ERROR_CHECK(gpio_config(&zc_conf));
	ESP_ERROR_CHECK(gpio_install_isr_service(0));
	ESP_ERROR_CHECK(gpio_isr_handler_add(ZERO_CROSS_GPIO, zero_cross_isr, NULL));

	gpio_config_t input_conf = {
		.pin_bit_mask = (1ULL << DOOR_SWITCH_GPIO) | (1ULL << SMOKE_SENSOR_GPIO) |
					(1ULL << TRIAC_STATE_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK(gpio_config(&input_conf));
}

static void init_phase_control(void)
{
	esp_timer_create_args_t fan1_timer_args = {
		.callback = fan1_timer_cb,
		.name = "fan1_triac"
	};
	ESP_ERROR_CHECK(esp_timer_create(&fan1_timer_args, &fan1_timer));
	xTaskCreate(zero_cross_task, "zero_cross_task", 2048, NULL, 12, &zero_cross_task_handle);
}

static void init_i2c(void)
{
	ESP_LOGI(TAG, "Init I2C bus");
	i2c_master_bus_config_t bus_config = {
		.i2c_port = I2C_NUM_0,
		.sda_io_num = I2C_SDA_GPIO,
		.scl_io_num = I2C_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags = {
			.enable_internal_pullup = true,
		},
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_handle));
	ESP_LOGI(TAG, "I2C bus ready");
}

static void init_sht31(void)
{
	if (!SHT31_init(i2c_bus_handle)) {
		ESP_LOGW(TAG, "SHT31 not detected, sensor disabled");
	}
}

static void init_dsi(void)
{
	ESP_LOGI(TAG, "Init MIPI-DSI bus");
	if (s_mipi_phy_ldo == NULL) {
		esp_ldo_channel_config_t ldo_cfg = {
			.chan_id = MIPI_DSI_PHY_LDO_CHAN,
			.voltage_mv = MIPI_DSI_PHY_LDO_VOLTAGE_MV,
		};
		ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &s_mipi_phy_ldo));
		ESP_LOGI(TAG, "MIPI-DSI PHY LDO enabled (chan=%d, %dmV)",
				 MIPI_DSI_PHY_LDO_CHAN, MIPI_DSI_PHY_LDO_VOLTAGE_MV);
	}

    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
		.phy_clk_src = 0,
		.lane_bit_rate_mbps = MIPI_DSI_LANE_BIT_RATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));
	ESP_LOGI(TAG, "MIPI-DSI bus ready");
}

void Peripherials_init(void)
{
	init_gpio();
	init_phase_control();
	System_Config_get_phase_params(&s_ac_half_cycle_us, &s_triac_min_delay_us, &s_triac_pulse_us);
	init_i2c();
	init_sht31();
	if (i2c_master_probe(i2c_bus_handle, GT911_I2C_ADDR_5D, I2C_PROBE_TIMEOUT_MS) == ESP_OK ||
		i2c_master_probe(i2c_bus_handle, GT911_I2C_ADDR_14, I2C_PROBE_TIMEOUT_MS) == ESP_OK) {
		ESP_LOGI(TAG, "GT911 detected (0x%02X or 0x%02X), init MIPI-DSI bus",
				 GT911_I2C_ADDR_5D, GT911_I2C_ADDR_14);
		init_dsi();
	} else {
		ESP_LOGW(TAG, "GT911 not detected, display not connected; skipping MIPI-DSI init");
	}
}

void Peripherials_set_fan(uint8_t percent)
{
	if (percent > 100) {
		percent = 100;
	}
	fan1_percent = percent;
	if (percent == 0) {
		gpio_set_level(FAN_COOLER_ONE_GPIO, 0);
	}
}

void Peripherials_set_buzzer(bool enabled)
{
	gpio_set_level(BUZZER_GPIO, enabled ? 1 : 0);
}

uint8_t Peripherials_get_fan_percent(void)
{
	return fan1_percent;
}

bool Peripherials_get_door_state(void)
{
	bool raw_closed = gpio_get_level(DOOR_SWITCH_GPIO) != 0;
	bool raw = !raw_closed;
	int64_t now_us = esp_timer_get_time();
	if (!s_door_initialized) {
		s_door_initialized = true;
		s_door_state = raw;
		s_door_last_raw = raw;
		s_door_last_change_us = now_us;
		return s_door_state;
	}
	if (raw != s_door_last_raw) {
		s_door_last_raw = raw;
		s_door_last_change_us = now_us;
	}
	if ((now_us - s_door_last_change_us) >= DOOR_DEBOUNCE_US) {
		s_door_state = s_door_last_raw;
	}
	return s_door_state;
}

bool Peripherials_get_smoke_state(void)
{
	return gpio_get_level(SMOKE_SENSOR_GPIO) != 0;
}

bool Peripherials_get_fan_alarm_state(void)
{
	bool output_present = gpio_get_level(TRIAC_STATE_GPIO) != 0;
	bool should_be_on = (fan1_percent > 0);
	bool mismatch = (should_be_on != output_present);
	int64_t now_us = esp_timer_get_time();

	if (mismatch) {
		s_fan_alarm_match_since_us = 0;
		if (s_fan_alarm_mismatch_since_us == 0) {
			s_fan_alarm_mismatch_since_us = now_us;
		}
		if (!s_fan_alarm_state &&
			(now_us - s_fan_alarm_mismatch_since_us) >= FAN_ALARM_DEBOUNCE_US) {
			s_fan_alarm_state = true;
		}
	} else {
		s_fan_alarm_mismatch_since_us = 0;
		if (s_fan_alarm_state) {
			if (s_fan_alarm_match_since_us == 0) {
				s_fan_alarm_match_since_us = now_us;
			}
			if ((now_us - s_fan_alarm_match_since_us) >= FAN_ALARM_DEBOUNCE_US) {
				s_fan_alarm_state = false;
			}
		} else {
			s_fan_alarm_match_since_us = 0;
		}
	}

	return s_fan_alarm_state;
}
