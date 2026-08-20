#ifndef NVS_STORE_H
#define NVS_STORE_H

#include "lambda_stats.h"
#include "signal_interpreter.h"

void nvs_store_init(void);
void nvs_store_load_stats(lambda_longterm_stats_t *out);
void nvs_store_save_stats(const lambda_longterm_stats_t *stats);
void nvs_store_load_config(si_calibration_t *out);
void nvs_store_save_config(const si_calibration_t *cal);
void nvs_store_reset_longterm(void);

#endif
