#ifndef LAMBDA_STATS_H
#define LAMBDA_STATS_H

#include <stdint.h>
#include <stdbool.h>
#include "signal_interpreter.h"

#define LAMBDA_STATS_VERSION 1

/**
 * @brief Versioned, checksummed long-term statistics struct (spec §4.4).
 *
 * Carries struct_version and crc32 so a corrupted or version-mismatched
 * record can be detected and the caller can fall back to a fresh reset
 * rather than trusting garbage data.
 */
typedef struct __attribute__((packed)) {
    uint16_t struct_version;
    uint32_t t_warmup_s;
    uint32_t t_lean_s;
    uint32_t t_lambda1_s;
    uint32_t t_rich_s;
    int16_t  index_min;
    int16_t  index_max;
    uint32_t total_runtime_s;
    uint32_t crc32;
} lambda_longterm_stats_t;

/**
 * @brief Reset the stats struct to its initial state.
 *
 * Sets struct_version to LAMBDA_STATS_VERSION, zeroes all time buckets
 * and total_runtime_s, and initializes index_min/index_max to the
 * extremes of the mixture-index range so any real sample updates them.
 * crc32 is left at 0 (call lambda_stats_finalize_crc() before persisting).
 *
 * @param stats Pointer to stats struct to reset.
 */
void lambda_stats_reset(lambda_longterm_stats_t *stats);

/**
 * @brief Accumulate one sample's worth of time/index data into the stats.
 *
 * While in_warmup is true, delta_s is added to t_warmup_s only, and
 * index_min/index_max are NOT updated (spec §3.2: warmup samples are
 * excluded from min/max tracking). Otherwise, delta_s is added to the
 * bucket matching category (lean/lambda1/rich) and index_min/index_max
 * are updated. total_runtime_s always accumulates delta_s regardless
 * of warmup state.
 *
 * Note: SI_CAT_VERY_LEAN and SI_CAT_LEAN both accumulate into t_lean_s;
 * SI_CAT_RICH and SI_CAT_VERY_RICH both accumulate into t_rich_s.
 *
 * @param stats    Pointer to stats struct to update.
 * @param category Current mixture category classification.
 * @param index    Current mixture index [-100, 100].
 * @param delta_s  Seconds elapsed since the previous sample.
 * @param in_warmup True if the FSM is currently in the WARMUP state.
 */
void lambda_stats_accumulate(lambda_longterm_stats_t *stats, si_category_t category, int32_t index, uint32_t delta_s, bool in_warmup);

/**
 * @brief Recompute and store the CRC-32 over the struct (excluding the
 *        crc32 field itself). Call before persisting to flash/NVS.
 *
 * @param stats Pointer to stats struct.
 */
void lambda_stats_finalize_crc(lambda_longterm_stats_t *stats);

/**
 * @brief Validate a loaded stats struct's version and CRC-32.
 *
 * @param stats Pointer to stats struct to validate.
 * @return      true if struct_version matches LAMBDA_STATS_VERSION and
 *              the stored crc32 matches the recomputed checksum.
 */
bool lambda_stats_validate(const lambda_longterm_stats_t *stats);

#endif /* LAMBDA_STATS_H */
