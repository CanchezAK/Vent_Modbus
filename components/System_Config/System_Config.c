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
#define DEFAULT_TRIAC_MIN_DELAY_US   (20)
#define DEFAULT_TRIAC_PULSE_US       (100)

#define DEFAULT_MODE                 (0)
#define DEFAULT_FAN_MANUAL_PERCENT   (0)
#define DEFAULT_FAN_MIN_PERCENT      (0)
#define DEFAULT_FILTER_LIMIT_HOURS   (1)
#define DEFAULT_SMOKE_ENABLE         (1)
#define DEFAULT_SMOKE_TEMP_ONLY_STOP_ENABLED (1)
#define DEFAULT_SYSTEM_ENABLE        (1)
#define DEFAULT_MODBUS_BAUD_INDEX    (3)
#define MAX_SERVICE_HOURS            (10000U)
#define SECONDS_PER_HOUR             (3600U)
#define SERVICE_PERSIST_QUANTUM_SECONDS (300U)
#define RTC_TIMER_WRAP_US            (1ULL << 48)
#define SERVICE_STATE_VERSION        (1U)

/* Persist filter wear state in release builds by default.
 * Debug builds accelerate service time, so persistence is disabled there unless ENABLE_SERVICE_HOURS_PERSIST
 * is overridden manually.
 */
#if CONFIG_COMPILER_OPTIMIZATION_DEBUG
#define ENABLE_SERVICE_HOURS_PERSIST_DEFAULT (0)
#else
#define ENABLE_SERVICE_HOURS_PERSIST_DEFAULT (1)
#endif

#ifndef ENABLE_SERVICE_HOURS_PERSIST
// #define ENABLE_SERVICE_HOURS_PERSIST (ENABLE_SERVICE_HOURS_PERSIST_DEFAULT)
#define ENABLE_SERVICE_HOURS_PERSIST (1)
#endif

/* TEMP DEBUG: speed up service runtime accumulation only in debug builds.
 * 60x means 1 real minute equals 1 service hour.
 */
#if CONFIG_COMPILER_OPTIMIZATION_DEBUG
#define TEMP_DEBUG_SERVICE_TIME_SCALE (1ULL)
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
static bool s_smoke_temp_only_stop_enabled = DEFAULT_SMOKE_TEMP_ONLY_STOP_ENABLED;
static uint32_t s_service_base_seconds = 0;
static uint64_t s_service_base_us = 0;
static uint64_t s_service_last_rtc_raw_us = 0;
static uint64_t s_service_rtc_wrap_offset_us = 0;
#if ENABLE_SERVICE_HOURS_PERSIST
static uint32_t s_service_last_saved_bucket = 0;
#endif
static TaskHandle_t s_update_task = NULL;

uint16_t System_Config_get_service_hours(void);

#define NVS_NAMESPACE "system_config"
#define NVS_KEY_CFG   "cfg"
#define NVS_KEY_SERVICE "service_state"
#define NVS_KEY_SMOKE_TEMP_ONLY_STOP "smk_tmp_stop"

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

typedef struct {
	uint32_t version;
	uint32_t service_seconds;
	uint64_t rtc_raw_us;
} persisted_service_state_t;

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

static uint32_t clamp_service_seconds(uint64_t seconds)
{
	uint64_t max_seconds = (uint64_t)MAX_SERVICE_HOURS * SECONDS_PER_HOUR;
	if (seconds > max_seconds) {
		seconds = max_seconds;
	}
	return (uint32_t)seconds;
}

static uint32_t service_persist_bucket(uint32_t service_seconds)
{
	return service_seconds / SERVICE_PERSIST_QUANTUM_SECONDS;
}

static uint32_t current_filter_limit_seconds(void)
{
	uint16_t filter_limit_hours;
	portENTER_CRITICAL(&s_cfg_lock);
	filter_limit_hours = s_cfg.filter_limit_hours;
	portEXIT_CRITICAL(&s_cfg_lock);
	return (uint32_t)filter_limit_hours * SECONDS_PER_HOUR;
}

static uint64_t scale_rtc_delta_to_service_seconds(uint64_t delta_us)
{
	uint64_t delta_seconds = delta_us / 1000000ULL;
	delta_seconds *= TEMP_DEBUG_SERVICE_TIME_SCALE;
	return delta_seconds;
}

static uint64_t get_rtc_raw_us(void)
{
	return (uint64_t)esp_clk_rtc_time();
}

static uint64_t extend_rtc_raw_us(uint64_t rtc_raw_us)
{
	uint64_t extended;
	portENTER_CRITICAL(&s_cfg_lock);
	if (s_service_last_rtc_raw_us != 0 && rtc_raw_us < s_service_last_rtc_raw_us) {
		/* RTC time is 48-bit and can wrap; extend to monotonic 64-bit. */
		s_service_rtc_wrap_offset_us += RTC_TIMER_WRAP_US;
	}
	s_service_last_rtc_raw_us = rtc_raw_us;
	extended = s_service_rtc_wrap_offset_us + rtc_raw_us;
	portEXIT_CRITICAL(&s_cfg_lock);
	return extended;
}

static uint32_t compute_service_seconds_from_snapshot(uint32_t base_seconds,
							  uint64_t base_us,
							  uint64_t now_us)
{
	uint64_t total_seconds = base_seconds;
	if (now_us > base_us) {
		total_seconds += scale_rtc_delta_to_service_seconds(now_us - base_us);
	}
	return clamp_service_seconds(total_seconds);
}

static uint32_t get_service_seconds_snapshot(uint64_t *rtc_raw_us)
{
	uint32_t base_seconds;
	uint64_t base_us;
	portENTER_CRITICAL(&s_cfg_lock);
	base_seconds = s_service_base_seconds;
	base_us = s_service_base_us;
	portEXIT_CRITICAL(&s_cfg_lock);

	uint64_t raw_now_us = get_rtc_raw_us();
	uint64_t now_us = extend_rtc_raw_us(raw_now_us);
	if (rtc_raw_us) {
		*rtc_raw_us = raw_now_us;
	}
	return compute_service_seconds_from_snapshot(base_seconds, base_us, now_us);
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

static void load_persisted(system_config_t *cfg, uint16_t *legacy_service_hours)
{
	nvs_handle_t handle;
	persisted_config_t stored = {0};
	size_t size = sizeof(stored);
	if (legacy_service_hours) {
		*legacy_service_hours = 0;
	}
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
		if (legacy_service_hours) {
			*legacy_service_hours = stored.service_hours;
		}
	}
	nvs_close(handle);
}

#if ENABLE_SERVICE_HOURS_PERSIST
static bool load_persisted_service_state(uint32_t *service_seconds, uint64_t *rtc_raw_us)
{
	nvs_handle_t handle;
	persisted_service_state_t stored = {0};
	size_t size = sizeof(stored);
	if (service_seconds) {
		*service_seconds = 0;
	}
	if (rtc_raw_us) {
		*rtc_raw_us = 0;
	}
	if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
		return false;
	}
	esp_err_t err = nvs_get_blob(handle, NVS_KEY_SERVICE, &stored, &size);
	nvs_close(handle);
	if (err != ESP_OK || size != sizeof(stored) || stored.version != SERVICE_STATE_VERSION) {
		return false;
	}
	if (service_seconds) {
		*service_seconds = clamp_service_seconds(stored.service_seconds);
	}
	if (rtc_raw_us) {
		*rtc_raw_us = stored.rtc_raw_us;
	}
	return true;
}

static void save_persisted_service_state(uint32_t service_seconds, uint64_t rtc_raw_us)
{
	nvs_handle_t handle;
	if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
		return;
	}
	persisted_service_state_t stored = {
		.version = SERVICE_STATE_VERSION,
		.service_seconds = clamp_service_seconds(service_seconds),
		.rtc_raw_us = rtc_raw_us,
	};
	(void)nvs_set_blob(handle, NVS_KEY_SERVICE, &stored, sizeof(stored));
	(void)nvs_commit(handle);
	nvs_close(handle);
}
#endif

static void save_persisted(const system_config_t *cfg)
{
	nvs_handle_t handle;
	uint32_t service_seconds = get_service_seconds_snapshot(NULL);
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
		.service_hours = (uint16_t)(service_seconds / SECONDS_PER_HOUR),
	};
	(void)nvs_set_blob(handle, NVS_KEY_CFG, &stored, sizeof(stored));
	(void)nvs_commit(handle);
	nvs_close(handle);
}

static void load_persisted_smoke_temp_only_stop(void)
{
	nvs_handle_t handle;
	uint8_t value = DEFAULT_SMOKE_TEMP_ONLY_STOP_ENABLED;

	if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
		s_smoke_temp_only_stop_enabled = DEFAULT_SMOKE_TEMP_ONLY_STOP_ENABLED;
		return;
	}

	if (nvs_get_u8(handle, NVS_KEY_SMOKE_TEMP_ONLY_STOP, &value) != ESP_OK) {
		value = DEFAULT_SMOKE_TEMP_ONLY_STOP_ENABLED;
	}

	nvs_close(handle);
	s_smoke_temp_only_stop_enabled = (value != 0U);
}

static void save_persisted_smoke_temp_only_stop(bool enabled)
{
	nvs_handle_t handle;

	if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
		return;
	}

	(void)nvs_set_u8(handle, NVS_KEY_SMOKE_TEMP_ONLY_STOP, enabled ? 1U : 0U);
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

static void set_smoke_temp_only_stop_enabled(system_config_source_t source, bool enabled, bool persist)
{
	bool changed = false;
	bool normalized = enabled ? true : false;

	portENTER_CRITICAL(&s_cfg_lock);
	if (s_smoke_temp_only_stop_enabled != normalized) {
		s_smoke_temp_only_stop_enabled = normalized;
		s_last_source = source;
		s_version++;
		changed = true;
	}
	portEXIT_CRITICAL(&s_cfg_lock);

	if (persist) {
		save_persisted_smoke_temp_only_stop(normalized);
	}

	if (changed) {
		notify_update_task();
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
	uint16_t legacy_service_hours = 0;
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		(void)nvs_flash_erase();
		(void)nvs_flash_init();
	}
	load_persisted(&defaults, &legacy_service_hours);
	load_persisted_smoke_temp_only_stop();
	if (defaults.filter_limit_hours == 0) {
		defaults.filter_limit_hours = DEFAULT_FILTER_LIMIT_HOURS;
	}
	uint64_t rtc_raw_now = get_rtc_raw_us();
	uint64_t rtc_now_us = extend_rtc_raw_us(rtc_raw_now);
#if ENABLE_SERVICE_HOURS_PERSIST
	uint32_t persisted_service_seconds = 0;
	uint64_t persisted_service_rtc_raw_us = 0;
	bool has_service_state = load_persisted_service_state(&persisted_service_seconds,
												 &persisted_service_rtc_raw_us);
	if (has_service_state) {
		uint64_t total_seconds = persisted_service_seconds;
		if (persisted_service_rtc_raw_us != 0 && rtc_raw_now >= persisted_service_rtc_raw_us) {
			total_seconds += scale_rtc_delta_to_service_seconds(rtc_raw_now - persisted_service_rtc_raw_us);
		}
		s_service_base_seconds = clamp_service_seconds(total_seconds);
	} else {
		s_service_base_seconds = clamp_service_seconds((uint64_t)legacy_service_hours * SECONDS_PER_HOUR);
		save_persisted_service_state(s_service_base_seconds, rtc_raw_now);
	}
	s_service_last_saved_bucket = service_persist_bucket(s_service_base_seconds);
#else
	(void)legacy_service_hours;
	s_service_base_seconds = 0;
#endif
	s_service_base_us = rtc_now_us;
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

bool System_Config_get_smoke_temp_only_stop_enabled(void)
{
	bool enabled;

	portENTER_CRITICAL(&s_cfg_lock);
	enabled = s_smoke_temp_only_stop_enabled;
	portEXIT_CRITICAL(&s_cfg_lock);

	return enabled;
}

void System_Config_set_smoke_temp_only_stop_enabled_volatile(bool enabled)
{
	set_smoke_temp_only_stop_enabled(SYSTEM_CONFIG_SOURCE_DISPLAY, enabled, false);
}

void System_Config_set_smoke_temp_only_stop_enabled_persist(bool enabled)
{
	set_smoke_temp_only_stop_enabled(SYSTEM_CONFIG_SOURCE_DISPLAY, enabled, true);
}

uint16_t System_Config_get_service_hours(void)
{
	uint32_t total_seconds = System_Config_get_service_seconds();
	uint32_t total_hours = total_seconds / SECONDS_PER_HOUR;
	if (total_hours > MAX_SERVICE_HOURS) {
		total_hours = MAX_SERVICE_HOURS;
	}
	return (uint16_t)total_hours;
}

uint32_t System_Config_get_service_seconds(void)
{
	uint64_t rtc_raw_us = 0;
	uint32_t total_seconds = get_service_seconds_snapshot(&rtc_raw_us);

#if ENABLE_SERVICE_HOURS_PERSIST
	{
		uint32_t filter_limit_seconds = current_filter_limit_seconds();
		if (filter_limit_seconds > 0U && total_seconds >= filter_limit_seconds) {
			uint32_t limit_bucket = service_persist_bucket(filter_limit_seconds);
			if (s_service_last_saved_bucket < limit_bucket) {
				s_service_last_saved_bucket = limit_bucket;
				save_persisted_service_state(filter_limit_seconds, rtc_raw_us);
			}
			return total_seconds;
		}

		uint32_t bucket = service_persist_bucket(total_seconds);
		if (bucket != s_service_last_saved_bucket) {
			s_service_last_saved_bucket = bucket;
			save_persisted_service_state(total_seconds, rtc_raw_us);
		}
	}
#endif

	return total_seconds;
}

void System_Config_set_service_hours(uint16_t value)
{
	uint16_t clamped = (value > 10000) ? 10000 : value;
	bool changed = false;
	uint64_t rtc_raw_now = get_rtc_raw_us();
	uint64_t now_us = extend_rtc_raw_us(rtc_raw_now);
	uint32_t new_base_seconds = (uint32_t)clamped * SECONDS_PER_HOUR;
	portENTER_CRITICAL(&s_cfg_lock);
	changed = (s_service_base_seconds != new_base_seconds) || (now_us > s_service_base_us);
	/*
	 * Even when the integer hour value is unchanged (e.g. clear 0 -> 0),
	 * we still need to reset elapsed fractional time by updating base_us.
	 */
	s_service_base_seconds = new_base_seconds;
	s_service_base_us = now_us;
	if (changed) {
		s_last_source = SYSTEM_CONFIG_SOURCE_INTERNAL;
		s_version++;
	}
	portEXIT_CRITICAL(&s_cfg_lock);
	if (changed) {
		system_config_t snapshot = System_Config_get_snapshot(NULL, NULL);
		save_persisted(&snapshot);
	#if ENABLE_SERVICE_HOURS_PERSIST
		s_service_last_saved_bucket = service_persist_bucket(new_base_seconds);
		save_persisted_service_state(new_base_seconds, rtc_raw_now);
	#endif
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
