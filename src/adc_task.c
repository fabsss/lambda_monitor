#include "adc_task.h"
#include "signal_interpreter.h"
#include "mix_filter.h"
#include "warmup_fsm.h"
#include "switch_detector.h"
#include "ring_buffer.h"
#include "lambda_stats.h"
#include "nvs_store.h"
#include "calib_wizard.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"

#define SAMPLE_PERIOD_MS 10
#define WARMUP_TIMEOUT_S 90

static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_cali_handle;
static SemaphoreHandle_t s_mutex;

static si_calibration_t s_cal;
static fast_filter_t s_fast;
static slow_filter_t s_slow;
static warmup_fsm_t s_warmup;
static switch_detector_t s_switch;
static ring_buffer_t s_ring;
static lambda_longterm_stats_t s_longterm;
static lambda_longterm_stats_t s_session;
static adc_snapshot_t s_snapshot;
static uint32_t s_dirty_seconds = 0;
#define STATS_COMMIT_INTERVAL_S 30

#define AUTOCAL_BUFFER_SIZE 100
static int32_t s_autocal_buffer[AUTOCAL_BUFFER_SIZE];
static uint32_t s_autocal_count = 0;
static bool s_autocal_active = false;

static void adc_task_fn(void *arg)
{
    (void)arg;
    int64_t last_second_us = esp_timer_get_time();

    while (1) {
        int raw = 0;
        int raw_mv = 0;
        adc_oneshot_read(s_adc_handle, ADC_CHANNEL_0, &raw);
        adc_cali_raw_to_voltage(s_cali_handle, raw, &raw_mv);

        int32_t fast_mv = fast_filter_push(&s_fast, raw_mv);
        int32_t index_fast = si_mv_to_index(&s_cal, fast_mv);
        si_category_t category = si_index_to_category(&s_cal, index_fast);

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (s_autocal_active && s_autocal_count < AUTOCAL_BUFFER_SIZE) {
            s_autocal_buffer[s_autocal_count++] = raw_mv;
        }
        s_snapshot.raw_mv = raw_mv;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_second_us >= 1000000) {
            uint32_t delta_s = (uint32_t)((now_us - last_second_us) / 1000000);
            last_second_us = now_us;

            bool edge = switch_detector_update(&s_switch, index_fast, delta_s);
            warmup_fsm_tick(&s_warmup, delta_s, edge);
            slow_filter_push(&s_slow, index_fast, delta_s);
            ring_buffer_push(&s_ring, index_fast, (uint32_t)(now_us / 1000000));
            bool in_warmup = s_warmup.state == WARMUP_STATE_WARMUP;
            lambda_stats_accumulate(&s_longterm, category, index_fast, delta_s, in_warmup);
            lambda_stats_accumulate(&s_session, category, index_fast, delta_s, in_warmup);

            s_dirty_seconds += delta_s;
            if (s_dirty_seconds >= STATS_COMMIT_INTERVAL_S) {
                nvs_store_save_stats(&s_longterm);
                s_dirty_seconds = 0;
            }

            s_snapshot.index_fast = index_fast;
            s_snapshot.index_slow_avg = slow_filter_average(&s_slow);
            s_snapshot.category = category;
            s_snapshot.warmup_state = s_warmup.state;
            s_snapshot.switches_per_min = s_switch.switches_per_min;
            s_snapshot.seconds_since_last_edge = s_switch.seconds_since_last_edge;
        }
        xSemaphoreGive(s_mutex);

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

void adc_task_start(int adc1_channel)
{
    s_mutex = xSemaphoreCreateMutex();

    fast_filter_init(&s_fast);
    slow_filter_init(&s_slow, 5);
    warmup_fsm_init(&s_warmup, WARMUP_TIMEOUT_S);
    switch_detector_init(&s_switch);
    ring_buffer_init(&s_ring);
    nvs_store_load_stats(&s_longterm);
    lambda_stats_reset(&s_session);
    nvs_store_load_config(&s_cal);

    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_cfg, &s_adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(s_adc_handle, (adc_channel_t)adc1_channel, &chan_cfg);

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = (adc_channel_t)adc1_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);

    xTaskCreate(adc_task_fn, "adc_task", 4096, NULL, 5, NULL);
}

void adc_task_get_snapshot(adc_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snapshot;
    xSemaphoreGive(s_mutex);
}

void adc_task_get_curve(int32_t *out_values, uint32_t *out_timestamps, uint16_t max_points, uint16_t *out_count)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint16_t count = ring_buffer_count(&s_ring);
    if (count > max_points) count = max_points;

    uint16_t start = ring_buffer_count(&s_ring) - count;
    for (uint16_t i = 0; i < count; i++) {
        int32_t val; uint32_t ts;
        ring_buffer_get(&s_ring, (uint16_t)(start + i), &val, &ts);
        out_values[i] = val;
        out_timestamps[i] = ts;
    }
    *out_count = count;
    xSemaphoreGive(s_mutex);
}

void adc_task_set_calibration(const si_calibration_t *cal)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_cal = *cal;
    xSemaphoreGive(s_mutex);
}

void adc_task_get_session_stats(lambda_longterm_stats_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_session;
    xSemaphoreGive(s_mutex);
}

void adc_task_autocal_start(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_autocal_active = true;
    s_autocal_count = 0;
    xSemaphoreGive(s_mutex);
}

void adc_task_autocal_stop_and_derive(si_calibration_t *out_cal)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_autocal_active = false;
    uint32_t count = s_autocal_count;
    xSemaphoreGive(s_mutex);

    if (count > 0) {
        calib_auto_derive(s_autocal_buffer, count, out_cal);
    } else {
        si_default_calibration(out_cal);
    }
}

uint32_t adc_task_autocal_count(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t count = s_autocal_count;
    xSemaphoreGive(s_mutex);
    return count;
}
