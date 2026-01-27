#include <stdio.h>
#include "Peripherials.h"
#include "Modbus_RTU.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_err.h"
#include "esp_log.h"

#define FAN_COOLER_ONE_GPIO  (GPIO_NUM_10)
#define FAN_COOLER_TWO_GPIO  (GPIO_NUM_11)

#define I2C_SDA_GPIO          (GPIO_NUM_7)
#define I2C_SCL_GPIO          (GPIO_NUM_8)
#define I2C_FREQ_HZ           (100000)

#define GT911_I2C_ADDR        (0x5D)
#define I2C_PROBE_TIMEOUT_MS  (50)

i2c_master_bus_handle_t i2c_bus_handle = NULL;
esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;

static const char *TAG = "Peripherials";

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
	init_i2c();
	if (i2c_master_probe(i2c_bus_handle, GT911_I2C_ADDR, I2C_PROBE_TIMEOUT_MS) == ESP_OK) {
		ESP_LOGI(TAG, "GT911 detected, init MIPI-DSI bus");
		init_dsi();
	} else {
		ESP_LOGW(TAG, "GT911 not detected, display not connected; skipping MIPI-DSI init");
	}
	Modbus_RTU_init();
	Modbus_RTU_set_fan_callback(Peripherials_set_fans);
}

void Peripherials_set_fans(bool cooler_one_on, bool cooler_two_on)
{
	gpio_set_level(FAN_COOLER_ONE_GPIO, cooler_one_on ? 1 : 0);
	gpio_set_level(FAN_COOLER_TWO_GPIO, cooler_two_on ? 1 : 0);
}
