#include "signal_interpreter.h"

/**
 * @brief Initialize calibration with default values.
 */
void si_default_calibration(si_calibration_t *cal)
{
    cal->u_min_mv = 320;      /* Bosch step lambda: 0.1V × 3.2 gain = 320 mV (lean) */
    cal->u_max_mv = 2880;     /* Bosch step lambda: 0.9V × 3.2 gain = 2880 mV (rich) */
    cal->u_lambda1_mv = 1600; /* Midpoint of 320–2880 mV range (stoichiometric) */
    cal->deadband_mv = 160;   /* ~5.6% of range for hysteresis tolerance */
    cal->thresh_very_lean = -60;
    cal->thresh_lean = -20;
    cal->thresh_rich = 20;
    cal->thresh_very_rich = 60;
}

/**
 * @brief Clamp a value to a range.
 */
static int32_t clamp(int32_t value, int32_t min, int32_t max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Convert millivolts to mixture index using linear interpolation.
 *
 * Piecewise linear:
 *  - On lean side (mv <= u_lambda1):
 *    index = ((mv - u_lambda1) * 100) / (u_lambda1 - u_min)
 *  - On rich side (mv > u_lambda1):
 *    index = ((mv - u_lambda1) * 100) / (u_max - u_lambda1)
 *  - Clamp result to [-100, 100]
 */
int32_t si_mv_to_index(const si_calibration_t *cal, int32_t mv)
{
    int32_t index;

    if (mv <= cal->u_lambda1_mv) {
        /* Lean side: interpolate between u_min and u_lambda1 */
        int32_t numerator = (mv - cal->u_lambda1_mv) * 100;
        int32_t denominator = cal->u_lambda1_mv - cal->u_min_mv;

        if (denominator != 0) {
            index = numerator / denominator;
        } else {
            index = 0; /* Avoid division by zero */
        }
    } else {
        /* Rich side: interpolate between u_lambda1 and u_max */
        int32_t numerator = (mv - cal->u_lambda1_mv) * 100;
        int32_t denominator = cal->u_max_mv - cal->u_lambda1_mv;

        if (denominator != 0) {
            index = numerator / denominator;
        } else {
            index = 0; /* Avoid division by zero */
        }
    }

    /* Clamp to [-100, 100] */
    return clamp(index, -100, 100);
}

/**
 * @brief Classify mixture index into one of five categories.
 */
si_category_t si_index_to_category(const si_calibration_t *cal, int32_t index)
{
    if (index < cal->thresh_very_lean) {
        return SI_CAT_VERY_LEAN;
    } else if (index < cal->thresh_lean) {
        return SI_CAT_LEAN;
    } else if (index < cal->thresh_rich) {
        return SI_CAT_LAMBDA1;
    } else if (index < cal->thresh_very_rich) {
        return SI_CAT_RICH;
    } else {
        return SI_CAT_VERY_RICH;
    }
}
