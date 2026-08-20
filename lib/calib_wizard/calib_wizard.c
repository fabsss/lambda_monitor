#include "calib_wizard.h"

void calib_sort_i32(int32_t *values, uint32_t n)
{
    for (uint32_t i = 1; i < n; i++) {
        int32_t key = values[i];
        uint32_t j = i;
        while (j > 0 && values[j - 1] > key) {
            values[j] = values[j - 1];
            j--;
        }
        values[j] = key;
    }
}

int32_t calib_percentile(int32_t *sorted_values_mv, uint32_t n, uint32_t percentile)
{
    if (n == 0) {
        return 0;
    }
    if (percentile > 100) {
        percentile = 100;
    }

    /* Nearest-rank method: rank = ceil(percentile/100 * n), 1-based, clamped to [1, n] */
    uint64_t numerator = (uint64_t)percentile * n;
    uint32_t rank = (uint32_t)((numerator + 99) / 100); /* ceil division */
    if (rank < 1) {
        rank = 1;
    }
    if (rank > n) {
        rank = n;
    }

    return sorted_values_mv[rank - 1];
}

void calib_auto_derive(int32_t *observed_mv, uint32_t n, si_calibration_t *out_cal)
{
    if (n == 0) {
        return;
    }

    /* Sort a local copy so the caller's input array is never mutated. */
    int32_t local[n];
    for (uint32_t i = 0; i < n; i++) {
        local[i] = observed_mv[i];
    }
    calib_sort_i32(local, n);

    int32_t p5 = calib_percentile(local, n, 5);
    int32_t p95 = calib_percentile(local, n, 95);

    out_cal->u_min_mv = p5;
    out_cal->u_max_mv = p95;
    out_cal->u_lambda1_mv = (p5 + p95) / 2;

    int32_t span = p95 - p5;
    out_cal->deadband_mv = (span * 5) / 100;
}
