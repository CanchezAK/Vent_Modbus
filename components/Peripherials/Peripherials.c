#include <stdio.h>
#include "Peripherials.h"
#include "Modbus_RTU.h"
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
#include "System_Config.h"
#include "SHT31.h"

#define FAN_COOLER_ONE_GPIO  (GPIO_NUM_10)
#define FAN_COOLER_TWO_GPIO  (GPIO_NUM_11)
#define ZERO_CROSS_GPIO      (GPIO_NUM_12)

#define DEFAULT_AC_HALF_CYCLE_US     (10000)
#define DEFAULT_TRIAC_PULSE_US       (100)
#define DEFAULT_TRIAC_MIN_DELAY_US   (200)

#define I2C_SDA_GPIO          (GPIO_NUM_7)
#define I2C_SCL_GPIO          (GPIO_NUM_8)
#define I2C_FREQ_HZ           (100000)

#define GT911_I2C_ADDR        (0x5D)
#define I2C_PROBE_TIMEOUT_MS  (50)

i2c_master_bus_handle_t i2c_bus_handle = NULL;
esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
static esp_timer_handle_t fan1_timer = NULL;
static esp_timer_handle_t fan2_timer = NULL;
static TaskHandle_t zero_cross_task_handle = NULL;
static volatile uint8_t fan1_percent = 0;
static volatile uint8_t fan2_percent = 0;
static uint32_t s_ac_half_cycle_us = DEFAULT_AC_HALF_CYCLE_US;
static uint32_t s_triac_min_delay_us = DEFAULT_TRIAC_MIN_DELAY_US;
static uint32_t s_triac_pulse_us = DEFAULT_TRIAC_PULSE_US;

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

static void fan2_timer_cb(void *arg)
{
	(void)arg;
	gpio_set_level(FAN_COOLER_TWO_GPIO, 1);
	esp_rom_delay_us(s_triac_pulse_us);
	gpio_set_level(FAN_COOLER_TWO_GPIO, 0);
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
		uint8_t p2 = fan2_percent;
		if (p1 > 0) {
			esp_timer_start_once(fan1_timer, phase_delay_from_percent(p1));
		}
		if (p2 > 0) {
			esp_timer_start_once(fan2_timer, phase_delay_from_percent(p2));
		}
	}
}


static void init_gpio(void)
{
	gpio_config_t io_conf = {
		.pin_bit_mask = (1ULL << FAN_COOLER_ONE_GPIO) | (1ULL << FAN_COOLER_TWO_GPIO),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK(gpio_config(&io_conf));
	ESP_ERROR_CHECK(gpio_set_level(FAN_COOLER_ONE_GPIO, 0));
	ESP_ERROR_CHECK(gpio_set_level(FAN_COOLER_TWO_GPIO, 0));

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
}

static void init_phase_control(void)
{
	esp_timer_create_args_t fan1_timer_args = {
		.callback = fan1_timer_cb,
		.name = "fan1_triac"
	};
	esp_timer_create_args_t fan2_timer_args = {
		.callback = fan2_timer_cb,
		.name = "fan2_triac"
	};
	ESP_ERROR_CHECK(esp_timer_create(&fan1_timer_args, &fan1_timer));
	ESP_ERROR_CHECK(esp_timer_create(&fan2_timer_args, &fan2_timer));
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
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
		.phy_clk_src = 0,
		.lane_bit_rate_mbps = 700,
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
	if (i2c_master_probe(i2c_bus_handle, GT911_I2C_ADDR, I2C_PROBE_TIMEOUT_MS) == ESP_OK) {
		ESP_LOGI(TAG, "GT911 detected, init MIPI-DSI bus");
		init_dsi();
	} else {
		ESP_LOGW(TAG, "GT911 not detected, display not connected; skipping MIPI-DSI init");
	}
	Modbus_RTU_init();
	Modbus_RTU_set_fan_callback(Peripherials_set_fans);
}

void Peripherials_set_fans(uint8_t cooler_one_percent, uint8_t cooler_two_percent)
{
	if (cooler_one_percent > 100) {
		cooler_one_percent = 100;
	}
	if (cooler_two_percent > 100) {
		cooler_two_percent = 100;
	}
	fan1_percent = cooler_one_percent;
	fan2_percent = cooler_two_percent;
	if (cooler_one_percent == 0) {
		gpio_set_level(FAN_COOLER_ONE_GPIO, 0);
	}
	if (cooler_two_percent == 0) {
		gpio_set_level(FAN_COOLER_TWO_GPIO, 0);
	}
}
