#include "System_Config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_private/esp_clk.h"
#include "sdkconfig.h"

#define DEFAULT_TEMP_DESIRED         (25)
#define DEFAULT_TEMP_THRESHOLD       (50)
#define DEFAULT_TEMP_ALARM           (70)

#define DEFAULT_HUM_DESIRED          (30)
#define DEFAULT_HUM_THRESHOLD        (35)
#define DEFAULT_HUM_ALARM            (38)

#define DEFAULT_AC_HALF_CYCLE_US     (10000)
#define DEFAULT_TRIAC_MIN_DELAY_US   (200)
#define DEFAULT_TRIAC_PULSE_US       (100)

#define DEFAULT_MODE                 (0)
#define DEFAULT_FAN_MANUAL_PERCENT   (0)
#define DEFAULT_FAN_MIN_PERCENT      (0)
#define DEFAULT_FILTER_LIMIT_HOURS   (1)
#define DEFAULT_SMOKE_ENABLE         (1)
#define DEFAULT_SYSTEM_ENABLE        (1)
#define DEFAULT_MODBUS_BAUD_INDEX    (3)

/* TEMP DEBUG: speed up service runtime accumulation only in debug builds.
 * 60x means 1 real minute equals 1 service hour.
 */
#if CONFIG_COMPILER_OPTIMIZATION_DEBUG
#define TEMP_DEBUG_SERVICE_TIME_SCALE (60ULL)
#else
#define TEMP_DEBUG_SERVICE_TIME_SCALE (1ULL)
#endif

#define MODE_MANUAL_BIT              (1U << 0)
#define MODE_AUTOSTART_TEMP_BIT      (1U << 1)
#define MODE_AUTOSTART_HUMIDITY_BIT  (1U << 2)
#define MODE_MASK_BITS (MODE_MANUAL_BIT | MODE_AUTOSTART_TEMP_BIT | MODE_AUTOSTART_HUMIDITY_BIT)

static portMUX_TYPE s_cfg_lock = portMUX_INITIALIZER_UNLOCKED;
static system_config_t s_cfg = {0};
static system_config_source_t s_last_source = SYSTEM_CONFIG_SOURCE_INTERNAL;
static uint32_t s_version = 0;

static uint32_t s_ac_half_cycle_us = DEFAULT_AC_HALF_CYCLE_US;
static uint32_t s_triac_min_delay_us = DEFAULT_TRIAC_MIN_DELAY_US;
static uint32_t s_triac_pulse_us = DEFAULT_TRIAC_PULSE_US;
static uint16_t s_alarm_clr = 0;
static uint16_t s_service_hours_base = 0;
static int64_t s_service_hours_base_us = 0;
static uint64_t s_service_hours_last_rtc_us = 0;
static uint64_t s_service_hours_rtc_wrap_offset_us = 0;
static TaskHandle_t s_update_task = NULL;

uint16_t System_Config_get_service_hours(void);

#define NVS_NAMESPACE "system_config"
#define NVS_KEY_CFG   "cfg"

typedef struct {
	uint16_t mode;
	uint16_t temp_desired;
	uint16_t temp_threshold;
	uint16_t temp_alarm;
	uint16_t humidity_desired;
	uint16_t humidity_threshold;
	uint16_t humidity_alarm;
	uint16_t fan_manual_percent;
	uint16_t fan_min_percent;
	uint16_t filter_limit_hours;
	uint16_t system_enable;
	uint16_t smoke_enable;
	uint16_t modbus_addr;
	uint16_t modbus_baud;
	uint16_t service_hours;
} persisted_config_t;

static uint16_t clamp_percent(uint16_t value)
{
	return (value > 100) ? 100 : value;
}

static uint16_t sanitize_baud_index(uint16_t index)
{
	if (index > 3) {
		return DEFAULT_MODBUS_BAUD_INDEX;
	}
	return index;
}

static uint16_t sanitize_addr(uint16_t addr)
{
	if (addr < 1 || addr > 247) {
		return 1;
	}
	return addr;
}

static uint64_t get_rtc_monotonic_us(void)
{
	uint64_t now = (uint64_t)esp_clk_rtc_time();
	portENTER_CRITICAL(&s_cfg_lock);
	if (s_service_hours_last_rtc_us != 0 && now < s_service_hours_last_rtc_us) {
		/* RTC time is 48-bit and can wrap; extend to monotonic 64-bit. */
		s_service_hours_rtc_wrap_offset_us += (1ULL << 48);
	}
	s_service_hours_last_rtc_us = now;
	uint64_t extended = s_service_hours_rtc_wrap_offset_us + now;
	portEXIT_CRITICAL(&s_cfg_lock);
	return extended;
}

static void sanitize(system_config_t *cfg)
{
	if (!cfg) {
		return;
	}
	cfg->fan_manual_percent = clamp_percent(cfg->fan_manual_percent);
	cfg->fan_min_percent = cfg->fan_manual_percent;
	cfg->filter_limit_hours = (cfg->filter_limit_hours > 10000) ? 10000 : cfg->filter_limit_hours;
	cfg->mode = cfg->mode & MODE_MASK_BITS;
	/* UI has no system-enable control; keep runtime logic enabled to avoid hidden-off conflicts. */
	cfg->system_enable = 1;
	cfg->smoke_enable = cfg->smoke_enable ? 1 : 0;
	cfg->modbus_addr = sanitize_addr(cfg->modbus_addr);
	cfg->modbus_baud = sanitize_baud_index(cfg->modbus_baud);

	if (cfg->temp_threshold < cfg->temp_desired) {
		cfg->temp_threshold = cfg->temp_desired;
	}
	if (cfg->temp_alarm < cfg->temp_threshold) {
		cfg->temp_alarm = cfg->temp_threshold;
	}
	if (cfg->humidity_threshold < cfg->humidity_desired) {
		cfg->humidity_threshold = cfg->humidity_desired;
	}
	if (cfg->humidity_alarm < cfg->humidity_threshold) {
		cfg->humidity_alarm = cfg->humidity_threshold;
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
		cfg->mode = stored.mode;
		cfg->temp_desired = stored.temp_desired;
		cfg->temp_threshold = stored.temp_threshold;
		cfg->temp_alarm = stored.temp_alarm;
		cfg->humidity_desired = stored.humidity_desired;
		cfg->humidity_threshold = stored.humidity_threshold;
		cfg->humidity_alarm = stored.humidity_alarm;
		cfg->fan_manual_percent = stored.fan_manual_percent;
		cfg->fan_min_percent = stored.fan_min_percent;
		cfg->filter_limit_hours = stored.filter_limit_hours;
		cfg->system_enable = stored.system_enable;
		cfg->smoke_enable = stored.smoke_enable;
		cfg->modbus_addr = stored.modbus_addr;
		cfg->modbus_baud = stored.modbus_baud;
		s_service_hours_base = stored.service_hours;
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
		.mode = cfg->mode,
		.temp_desired = cfg->temp_desired,
		.temp_threshold = cfg->temp_threshold,
		.temp_alarm = cfg->temp_alarm,
		.humidity_desired = cfg->humidity_desired,
		.humidity_threshold = cfg->humidity_threshold,
		.humidity_alarm = cfg->humidity_alarm,
		.fan_manual_percent = cfg->fan_manual_percent,
		.fan_min_percent = cfg->fan_min_percent,
		.filter_limit_hours = cfg->filter_limit_hours,
		.system_enable = cfg->system_enable,
		.smoke_enable = cfg->smoke_enable,
		.modbus_addr = cfg->modbus_addr,
		.modbus_baud = cfg->modbus_baud,
		.service_hours = System_Config_get_service_hours(),
	};
	(void)nvs_set_blob(handle, NVS_KEY_CFG, &stored, sizeof(stored));
	(void)nvs_commit(handle);
	nvs_close(handle);
}

static void notify_update_task(void)
{
	TaskHandle_t task = NULL;
	portENTER_CRITICAL(&s_cfg_lock);
	task = s_update_task;
	portEXIT_CRITICAL(&s_cfg_lock);

	if (task) {
		xTaskNotifyGive(task);
	}
}

static void set_config_ex(const system_config_t *cfg, system_config_source_t source, bool persist)
{
	system_config_t copy = *cfg;
	bool changed = false;
	sanitize(&copy);
	portENTER_CRITICAL(&s_cfg_lock);
	changed = (memcmp(&s_cfg, &copy, sizeof(system_config_t)) != 0);
	if (changed) {
		s_cfg = copy;
		s_last_source = source;
		s_version++;
	}
	portEXIT_CRITICAL(&s_cfg_lock);
	if (persist) {
		save_persisted(&copy);
	}
	if (changed) {
		notify_update_task();
	}
}

static void set_config(const system_config_t *cfg, system_config_source_t source)
{
	set_config_ex(cfg, source, true);
}

void System_Config_init(void)
{
	system_config_t defaults = {
		.temp_desired = DEFAULT_TEMP_DESIRED,
		.temp_threshold = DEFAULT_TEMP_THRESHOLD,
		.temp_alarm = DEFAULT_TEMP_ALARM,
		.humidity_desired = DEFAULT_HUM_DESIRED,
		.humidity_threshold = DEFAULT_HUM_THRESHOLD,
		.humidity_alarm = DEFAULT_HUM_ALARM,
		.mode = DEFAULT_MODE,
		.fan_manual_percent = DEFAULT_FAN_MANUAL_PERCENT,
		.fan_min_percent = DEFAULT_FAN_MIN_PERCENT,
		.filter_limit_hours = DEFAULT_FILTER_LIMIT_HOURS,
		.system_enable = DEFAULT_SYSTEM_ENABLE,
		.smoke_enable = DEFAULT_SMOKE_ENABLE,
		.modbus_addr = 1,
		.modbus_baud = DEFAULT_MODBUS_BAUD_INDEX,
	};
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		(void)nvs_flash_erase();
		(void)nvs_flash_init();
	}
	load_persisted(&defaults);
	if (defaults.filter_limit_hours == 0) {
		defaults.filter_limit_hours = DEFAULT_FILTER_LIMIT_HOURS;
	}
	s_service_hours_base_us = (int64_t)get_rtc_monotonic_us();
	s_alarm_clr = 0;
	set_config_ex(&defaults, SYSTEM_CONFIG_SOURCE_INTERNAL, false);
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
		/*
		 * Display-origin updates are RAM-only by policy.
		 * Persistent save from UI must be implemented explicitly (e.g. dedicated Save button).
		 */
		set_config_ex(cfg, SYSTEM_CONFIG_SOURCE_DISPLAY, false);
	}
}

void System_Config_set_from_display_volatile(const system_config_t *cfg)
{
	if (cfg) {
		set_config_ex(cfg, SYSTEM_CONFIG_SOURCE_DISPLAY, false);
	}
}

void System_Config_set_from_display_persist(const system_config_t *cfg)
{
	if (cfg) {
		set_config_ex(cfg, SYSTEM_CONFIG_SOURCE_DISPLAY, true);
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
	bool changed = false;
	portENTER_CRITICAL(&s_cfg_lock);
	if (s_alarm_clr != new_value) {
		s_alarm_clr = new_value;
		s_last_source = source;
		s_version++;
		changed = true;
	}
	portEXIT_CRITICAL(&s_cfg_lock);
	if (changed) {
		notify_update_task();
	}
}

uint16_t System_Config_get_service_hours(void)
{
	uint32_t total_seconds = System_Config_get_service_seconds();
	uint32_t total_hours = total_seconds / 3600U;
	if (total_hours > 10000U) {
		total_hours = 10000U;
	}
	return (uint16_t)total_hours;
}

uint32_t System_Config_get_service_seconds(void)
{
	uint16_t base;
	int64_t base_us;
	portENTER_CRITICAL(&s_cfg_lock);
	base = s_service_hours_base;
	base_us = s_service_hours_base_us;
	portEXIT_CRITICAL(&s_cfg_lock);

	int64_t now_us = (int64_t)get_rtc_monotonic_us();
	int64_t delta_us = now_us - base_us;
	if (delta_us < 0) {
		delta_us = 0;
	}

	uint64_t base_seconds = (uint64_t)base * 3600ULL;
	uint64_t added_seconds = (uint64_t)(delta_us / 1000000LL);
	added_seconds *= TEMP_DEBUG_SERVICE_TIME_SCALE;
	uint64_t total_seconds = base_seconds + added_seconds;
	uint64_t max_seconds = 10000ULL * 3600ULL;
	if (total_seconds > max_seconds) {
		total_seconds = max_seconds;
	}

	return (uint32_t)total_seconds;
}

void System_Config_set_service_hours(uint16_t value)
{
	uint16_t clamped = (value > 10000) ? 10000 : value;
	bool changed = false;
	bool base_changed = false;
	int64_t now_us = (int64_t)get_rtc_monotonic_us();
	portENTER_CRITICAL(&s_cfg_lock);
	base_changed = (s_service_hours_base != clamped);
	/*
	 * Even when the integer hour value is unchanged (e.g. clear 0 -> 0),
	 * we still need to reset elapsed fractional time by updating base_us.
	 */
	changed = base_changed || (now_us > s_service_hours_base_us);
	s_service_hours_base = clamped;
	s_service_hours_base_us = now_us;
	if (changed) {
		s_last_source = SYSTEM_CONFIG_SOURCE_INTERNAL;
		s_version++;
	}
	portEXIT_CRITICAL(&s_cfg_lock);
	if (base_changed) {
		save_persisted(&s_cfg);
	}
	if (changed) {
		notify_update_task();
	}
}

void System_Config_register_update_task(void *task_handle)
{
	portENTER_CRITICAL(&s_cfg_lock);
	s_update_task = (TaskHandle_t)task_handle;
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
