#ifndef ADC_TASK_H
#define ADC_TASK_H

#include <stdint.h>
#include "signal_interpreter.h"
#include "warmup_fsm.h"
#include "lambda_stats.h"

typedef struct {
    int32_t index_fast;
    int32_t index_slow_avg;
    int32_t index_avg_2s;
    si_category_t category;
    warmup_state_t warmup_state;
    uint32_t switches_per_min;
    uint32_t seconds_since_last_edge;
    int32_t raw_mv;
} adc_snapshot_t;

void adc_task_start(int adc1_channel);
void adc_task_get_snapshot(adc_snapshot_t *out);
void adc_task_get_curve(int32_t *out_values, uint32_t *out_timestamps, uint16_t max_points, uint16_t *out_count);
void adc_task_set_calibration(const si_calibration_t *cal);
void adc_task_get_session_stats(lambda_longterm_stats_t *out);
void adc_task_reset_longterm_stats(void);
void adc_task_autocal_start(void);
void adc_task_autocal_stop_and_derive(si_calibration_t *out_cal);
uint32_t adc_task_autocal_count(void);
uint32_t adc_task_autocal_max(void);

#endif
