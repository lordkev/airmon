#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AM_METRICS 8
#define AM_HISTORY 1440
#define AM_INVALID INT32_MIN
typedef enum { AM_TEMP, AM_RH, AM_PM1, AM_PM25, AM_PM10, AM_CO2, AM_VOC, AM_NOX } am_metric_id;
typedef enum { AM_ABSENT, AM_WARMING, AM_VALID, AM_STALE, AM_FAILED } am_state;
typedef struct { float value; uint64_t at_ms; am_state state; } am_reading;
typedef struct { uint32_t uptime_s, utc_s; int32_t values[AM_METRICS]; } am_history_point;
typedef struct {
    am_history_point points[AM_HISTORY];
    size_t head, count;
    uint64_t minute;
    double sum[AM_METRICS];
    uint32_t samples[AM_METRICS];
    bool started;
} am_history;
extern const char *const am_keys[AM_METRICS];
extern const char *const am_units[AM_METRICS];
extern const char *const am_state_names[5];
extern const int am_scales[AM_METRICS];
uint8_t am_crc8(const uint8_t *data, size_t length);
bool am_sensirion_words(const uint8_t *data, size_t length, uint16_t *words);
bool am_decode_sht4x(const uint8_t data[6], float *temperature, float *humidity);
bool am_decode_pmsa003i(const uint8_t *data, size_t length, uint16_t pm[3]);
void am_history_sample(am_history *history, uint64_t ms, uint32_t utc_s,
                       const am_reading readings[AM_METRICS]);
const am_history_point *am_history_at(const am_history *history, size_t oldest_index);
bool am_reading_current(const am_reading *reading, uint64_t now_ms, uint32_t max_age_ms);
/* Four-point resistive calibration: affine fit from three targets, validate fourth. */
bool am_touch_calibrate(const float raw[4][2], const float screen[4][2], float affine[6]);
void am_touch_map(const float affine[6], float raw_x, float raw_y, float *x, float *y);
bool am_constant_time_equal(const char *a, const char *b, size_t max_length);
/* Nonempty decimal digits only; rejects signs, suffixes and uint32 overflow. */
bool am_parse_u32(const char *text, uint32_t *out);
