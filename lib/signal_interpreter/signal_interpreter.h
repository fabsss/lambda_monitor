#ifndef SIGNAL_INTERPRETER_H
#define SIGNAL_INTERPRETER_H

#include <stdint.h>

/**
 * @brief Calibration parameters for signal interpretation.
 *
 * All voltage values are in millivolts (mV).
 * Threshold values are mixture indices in the range [-100, 100].
 */
typedef struct {
    int32_t u_min_mv;           /* Minimum ADC voltage (maps to -100 index) */
    int32_t u_max_mv;           /* Maximum ADC voltage (maps to +100 index) */
    int32_t u_lambda1_mv;       /* Lambda=1 voltage (maps to 0 index) */
    int32_t deadband_mv;        /* Deadband around lambda=1 (not used in basic conversion) */
    int32_t thresh_very_lean;   /* Lower threshold for very lean category */
    int32_t thresh_lean;        /* Lower threshold for lean category */
    int32_t thresh_rich;        /* Lower threshold for rich category */
    int32_t thresh_very_rich;   /* Lower threshold for very rich category */
} si_calibration_t;

/**
 * @brief Air-fuel mixture categories based on lambda (λ).
 */
typedef enum {
    SI_CAT_VERY_LEAN,   /* -100 to -60 */
    SI_CAT_LEAN,        /* -60 to -20 */
    SI_CAT_LAMBDA1,     /* -20 to +20 (λ ≈ 1) */
    SI_CAT_RICH,        /* +20 to +60 */
    SI_CAT_VERY_RICH    /* +60 to +100 */
} si_category_t;

/**
 * @brief Initialize calibration structure with default values.
 *
 * @param cal Pointer to calibration structure to initialize.
 *
 * Default values:
 *  - u_min_mv = 0
 *  - u_max_mv = 3000
 *  - u_lambda1_mv = 1500
 *  - deadband_mv = 150
 *  - thresh_very_lean = -60
 *  - thresh_lean = -20
 *  - thresh_rich = 20
 *  - thresh_very_rich = 60
 */
void si_default_calibration(si_calibration_t *cal);

/**
 * @brief Convert millivolts to mixture index [-100, 100].
 *
 * Linear interpolation:
 *  - u_min_mv maps to -100
 *  - u_lambda1_mv maps to 0
 *  - u_max_mv maps to +100
 *
 * Values outside [u_min_mv, u_max_mv] are clamped to [-100, 100].
 *
 * @param cal Calibration parameters.
 * @param mv  ADC voltage in millivolts.
 * @return    Mixture index in range [-100, 100].
 */
int32_t si_mv_to_index(const si_calibration_t *cal, int32_t mv);

/**
 * @brief Classify mixture index into one of five categories.
 *
 * Classification based on calibration thresholds:
 *  - index < thresh_very_lean: SI_CAT_VERY_LEAN
 *  - thresh_very_lean <= index < thresh_lean: SI_CAT_LEAN
 *  - thresh_lean <= index < thresh_rich: SI_CAT_LAMBDA1
 *  - thresh_rich <= index < thresh_very_rich: SI_CAT_RICH
 *  - index >= thresh_very_rich: SI_CAT_VERY_RICH
 *
 * @param cal   Calibration parameters (contains thresholds).
 * @param index Mixture index [-100, 100].
 * @return      Category classification.
 */
si_category_t si_index_to_category(const si_calibration_t *cal, int32_t index);

#endif /* SIGNAL_INTERPRETER_H */
