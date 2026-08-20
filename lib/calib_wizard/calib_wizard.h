#ifndef CALIB_WIZARD_H
#define CALIB_WIZARD_H

#include <stdint.h>
#include "signal_interpreter.h"

/**
 * @brief Sort an array of int32_t values in ascending order, in place.
 *
 * Insertion sort — fine for the small sample counts used by the
 * calibration wizard (spec §8.5b).
 *
 * @param values Array to sort in place.
 * @param n      Number of elements.
 */
void calib_sort_i32(int32_t *values, uint32_t n);

/**
 * @brief Compute a percentile of an already-sorted array via nearest-rank.
 *
 * @param sorted_values_mv Ascending-sorted array of mV samples.
 * @param n                Number of elements (must be > 0).
 * @param percentile       Percentile to compute, 0..100.
 * @return                 The value at the nearest-rank position.
 */
int32_t calib_percentile(int32_t *sorted_values_mv, uint32_t n, uint32_t percentile);

/**
 * @brief Derive calibration parameters from observed raw mV samples.
 *
 * Does not mutate the caller's input array (sorts a local copy).
 * Computes the 5th and 95th percentiles as u_min_mv/u_max_mv, their
 * midpoint as u_lambda1_mv, and a deadband proportional to the span
 * (5% of (u_max_mv - u_min_mv), matching the default calibration's
 * 150/3000 ratio). Classification thresholds are left untouched by
 * this function (caller may apply defaults separately).
 *
 * @param observed_mv Array of raw ADC mV samples (not modified).
 * @param n            Number of samples (must be > 0).
 * @param out_cal       Calibration struct to populate (u_min_mv, u_max_mv,
 *                       u_lambda1_mv, deadband_mv fields are written).
 */
void calib_auto_derive(int32_t *observed_mv, uint32_t n, si_calibration_t *out_cal);

#endif /* CALIB_WIZARD_H */
