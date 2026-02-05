#include "System_Config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"

#define DEFAULT_TEMP_DESIRED_CENTI   (2500)
#define DEFAULT_TEMP_THRESHOLD_CENTI (5000)
#define DEFAULT_TEMP_ALARM_CENTI     (7000)

#define DEFAULT_HUM_DESIRED_CENTI    (3000)
#define DEFAULT_HUM_THRESHOLD_CENTI  (3500)
#define DEFAULT_HUM_ALARM_CENTI      (3800)

#define DEFAULT_AC_HALF_CYCLE_US     (10000)
#define DEFAULT_TRIAC_MIN_DELAY_US   (200)
#define DEFAULT_TRIAC_PULSE_US       (100)

static portMUX_TYPE s_cfg_lock = portMUX_INITIALIZER_UNLOCKED;
static system_config_t s_cfg = {0};
static system_config_source_t s_last_source = SYSTEM_CONFIG_SOURCE_INTERNAL;
static uint32_t s_version = 0;

static uint32_t s_ac_half_cycle_us = DEFAULT_AC_HALF_CYCLE_US;
static uint32_t s_triac_min_delay_us = DEFAULT_TRIAC_MIN_DELAY_US;
static uint32_t s_triac_pulse_us = DEFAULT_TRIAC_PULSE_US;
static uint16_t s_alarm_clr = 0;

#define NVS_NAMESPACE "system_config"
#define NVS_KEY_CFG   "cfg"

typedef struct {
	uint16_t temp_desired_centi;
	uint16_t temp_threshold_centi;
	uint16_t temp_alarm_centi;
	uint16_t humidity_desired_centi;
	uint16_t humidity_threshold_centi;
	uint16_t humidity_alarm_centi;
	uint8_t fan1_percent;
	uint8_t fan2_percent;
} persisted_config_t;

static uint8_t clamp_percent(uint8_t value)
{
	return (value > 100) ? 100 : value;
}

static void sanitize(system_config_t *cfg)
{
	if (!cfg) {
		return;
	}
	cfg->fan1_percent = clamp_percent(cfg->fan1_percent);
	cfg->fan2_percent = clamp_percent(cfg->fan2_percent);

	if (cfg->temp_threshold_centi < cfg->temp_desired_centi) {
		cfg->temp_threshold_centi = cfg->temp_desired_centi;
	}
	if (cfg->temp_alarm_centi < cfg->temp_threshold_centi) {
		cfg->temp_alarm_centi = cfg->temp_threshold_centi;
	}
	if (cfg->humidity_threshold_centi < cfg->humidity_desired_centi) {
		cfg->humidity_threshold_centi = cfg->humidity_desired_centi;
	}
	if (cfg->humidity_alarm_centi < cfg->humidity_threshold_centi) {
		cfg->humidity_alarm_centi = cfg->humidity_threshold_centi;
	}
}

static void load_persisted(system_config_t *cfg)
{
	nvs_handle_t handle;
	persisted_config_t stored = {0};
	size_t size = sizeof(stored);
	if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
		return;
	}
	if (nvs_get_blob(handle, NVS_KEY_CFG, &stored, &size) == ESP_OK && size == sizeof(stored)) {
		cfg->temp_desired_centi = stored.temp_desired_centi;
		cfg->temp_threshold_centi = stored.temp_threshold_centi;
		cfg->temp_alarm_centi = stored.temp_alarm_centi;
		cfg->humidity_desired_centi = stored.humidity_desired_centi;
		cfg->humidity_threshold_centi = stored.humidity_threshold_centi;
		cfg->humidity_alarm_centi = stored.humidity_alarm_centi;
		cfg->fan1_percent = stored.fan1_percent;
		cfg->fan2_percent = stored.fan2_percent;
	}
	nvs_close(handle);
}

static void save_persisted(const system_config_t *cfg)
{
	nvs_handle_t handle;
	if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
		return;
	}
	persisted_config_t stored = {
		.temp_desired_centi = cfg->temp_desired_centi,
		.temp_threshold_centi = cfg->temp_threshold_centi,
		.temp_alarm_centi = cfg->temp_alarm_centi,
		.humidity_desired_centi = cfg->humidity_desired_centi,
		.humidity_threshold_centi = cfg->humidity_threshold_centi,
		.humidity_alarm_centi = cfg->humidity_alarm_centi,
		.fan1_percent = cfg->fan1_percent,
		.fan2_percent = cfg->fan2_percent,
	};
	(void)nvs_set_blob(handle, NVS_KEY_CFG, &stored, sizeof(stored));
	(void)nvs_commit(handle);
	nvs_close(handle);
}

static void set_config(const system_config_t *cfg, system_config_source_t source)
{
	system_config_t copy = *cfg;
	sanitize(&copy);
	portENTER_CRITICAL(&s_cfg_lock);
	s_cfg = copy;
	s_last_source = source;
	s_version++;
	portEXIT_CRITICAL(&s_cfg_lock);
	save_persisted(&copy);
}

void System_Config_init(void)
{
	system_config_t defaults = {
		.temp_desired_centi = DEFAULT_TEMP_DESIRED_CENTI,
		.temp_threshold_centi = DEFAULT_TEMP_THRESHOLD_CENTI,
		.temp_alarm_centi = DEFAULT_TEMP_ALARM_CENTI,
		.humidity_desired_centi = DEFAULT_HUM_DESIRED_CENTI,
		.humidity_threshold_centi = DEFAULT_HUM_THRESHOLD_CENTI,
		.humidity_alarm_centi = DEFAULT_HUM_ALARM_CENTI,
		.fan1_mode_manual = 0,
		.fan2_mode_manual = 0,
		.fan1_percent = 0,
		.fan2_percent = 0,
	};
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		(void)nvs_flash_erase();
		(void)nvs_flash_init();
	}
	load_persisted(&defaults);
	s_alarm_clr = 0;
	set_config(&defaults, SYSTEM_CONFIG_SOURCE_INTERNAL);
}

void System_Config_set_from_modbus(const system_config_t *cfg)
{
	if (cfg) {
		set_config(cfg, SYSTEM_CONFIG_SOURCE_MODBUS);
	}
}

void System_Config_set_from_display(const system_config_t *cfg)
{
	if (cfg) {
		set_config(cfg, SYSTEM_CONFIG_SOURCE_DISPLAY);
	}
}

void System_Config_set_from_internal(const system_config_t *cfg)
{
	if (cfg) {
		set_config(cfg, SYSTEM_CONFIG_SOURCE_INTERNAL);
	}
}

system_config_t System_Config_get_snapshot(system_config_source_t *source, uint32_t *version)
{
	system_config_t snapshot;
	portENTER_CRITICAL(&s_cfg_lock);
	snapshot = s_cfg;
	if (source) {
		*source = s_last_source;
	}
	if (version) {
		*version = s_version;
	}
	portEXIT_CRITICAL(&s_cfg_lock);
	return snapshot;
}

uint16_t System_Config_get_alarm_clr(void)
{
	uint16_t value;
	portENTER_CRITICAL(&s_cfg_lock);
	value = s_alarm_clr;
	portEXIT_CRITICAL(&s_cfg_lock);
	return value;
}

void System_Config_set_alarm_clr(system_config_source_t source, uint16_t value)
{
	uint16_t new_value = value ? 1 : 0;
	portENTER_CRITICAL(&s_cfg_lock);
	s_alarm_clr = new_value;
	s_last_source = source;
	s_version++;
	portEXIT_CRITICAL(&s_cfg_lock);
}

void System_Config_get_phase_params(uint32_t *ac_half_cycle_us,
									 uint32_t *triac_min_delay_us,
									 uint32_t *triac_pulse_us)
{
	if (ac_half_cycle_us) {
		*ac_half_cycle_us = s_ac_half_cycle_us;
	}
	if (triac_min_delay_us) {
		*triac_min_delay_us = s_triac_min_delay_us;
	}
	if (triac_pulse_us) {
		*triac_pulse_us = s_triac_pulse_us;
	}
}
