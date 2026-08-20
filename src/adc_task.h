#ifndef ADC_TASK_H
#define ADC_TASK_H

#include <stdint.h>
#include "signal_interpreter.h"
#include "warmup_fsm.h"

typedef struct {
    int32_t index_fast;
    int32_t index_slow_avg;
    si_category_t category;
    warmup_state_t warmup_state;
    uint32_t switches_per_min;
    uint32_t seconds_since_last_edge;
} adc_snapshot_t;

void adc_task_start(int adc1_channel);
void adc_task_get_snapshot(adc_snapshot_t *out);
void adc_task_get_curve(int32_t *out_values, uint32_t *out_timestamps, uint16_t max_points, uint16_t *out_count);
void adc_task_set_calibration(const si_calibration_t *cal);

#endif
