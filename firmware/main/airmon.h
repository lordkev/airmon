#pragma once
#include "airmon_core.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"

#define AM_VERSION "0.1.0"
typedef struct {
    uint32_t version;
    char name[33], ssid[33], password[65];
    char mqtt_uri[193], mqtt_user[65], mqtt_password[129];
    bool mqtt_enabled, co2_asc;
    uint8_t brightness;
    uint16_t dim_seconds, altitude_m;
    float temperature_offset;
} am_config;
typedef struct {
    SemaphoreHandle_t mutex;
    am_config config;
    am_reading readings[AM_METRICS];
    am_history history;
    char id[13], ap_ssid[33], ap_password[17], admin[33], ip[16];
    bool wifi_connected, ap_active, mqtt_connected, config_pending, ota_busy;
    char config_result[64];
    uint32_t sensor_errors[4];
    float touch_affine[6];
    bool touch_calibrated;
} am_app;
extern am_app app;
static inline void am_lock(void) {xSemaphoreTake(app.mutex,portMAX_DELAY);}
static inline void am_unlock(void) {xSemaphoreGive(app.mutex);}
uint64_t am_now(void);
uint32_t am_utc(void);
void am_config_init(void);
void am_config_get(am_config *config);
esp_err_t am_config_save(const am_config *config);
cJSON *am_config_json(void);
bool am_config_parse(const cJSON *json,am_config *candidate,char *error,size_t length);
void am_touch_save(const float affine[6]);
void am_sensors_start(void);
bool am_sensors_calibrate(uint16_t reference_ppm);
void am_network_start(void);
void am_network_setup(void);
bool am_network_configure(const am_config *candidate);
void am_web_start(void);
void am_display_start(void);
void am_display_recalibrate(void);
