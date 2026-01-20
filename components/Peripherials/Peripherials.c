#include <stdio.h>
#include "Peripherials.h"
#include "Modbus_RTU.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

#define FAN_COOLER_ONE_GPIO  (GPIO_NUM_2)
#define FAN_COOLER_TWO_GPIO  (GPIO_NUM_3)

#define I2C_PORT_NUM          (I2C_NUM_0)
#define I2C_SDA_GPIO          (GPIO_NUM_4)
#define I2C_SCL_GPIO          (GPIO_NUM_5)
#define I2C_FREQ_HZ           (100000)

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
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_SDA_GPIO,
		.scl_io_num = I2C_SCL_GPIO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_FREQ_HZ
	};
	ESP_ERROR_CHECK(i2c_param_config(I2C_PORT_NUM, &conf));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT_NUM, conf.mode, 0, 0, 0));
}

void Peripherials_init(void)
{
	init_gpio();
	init_i2c();
	Modbus_RTU_init();
	Modbus_RTU_set_fan_callback(Peripherials_set_fans);
}

void Peripherials_set_fans(bool cooler_one_on, bool cooler_two_on)
{
	gpio_set_level(FAN_COOLER_ONE_GPIO, cooler_one_on ? 1 : 0);
	gpio_set_level(FAN_COOLER_TWO_GPIO, cooler_two_on ? 1 : 0);
}
