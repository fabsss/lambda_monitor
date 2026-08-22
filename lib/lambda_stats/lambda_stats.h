#ifndef LAMBDA_STATS_H
#define LAMBDA_STATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "signal_interpreter.h"

#define LAMBDA_STATS_VERSION 4

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
    uint32_t t_very_lean_s;
    uint32_t t_lean_s;
    uint32_t t_lambda1_s;
    uint32_t t_rich_s;
    uint32_t t_very_rich_s;
    int16_t  index_min;
    int16_t  index_max;
    uint32_t total_runtime_s;
    int64_t  avg2s_sum;    /* Running sum of 2s rolling-average samples (index units), see adc_task.c AVG_WINDOW_S */
    uint32_t avg2s_count;  /* Number of avg2s samples summed; avg2s_sum/avg2s_count = long-term control-quality average */
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
 * bucket matching category (very_lean/lean/lambda1/rich/very_rich) and
 * index_min/index_max are updated. total_runtime_s always accumulates
 * delta_s regardless of warmup state.
 *
 * @param stats    Pointer to stats struct to update.
 * @param category Current mixture category classification.
 * @param index    Current mixture index [-100, 100].
 * @param delta_s  Seconds elapsed since the previous sample.
 * @param in_warmup True if the FSM is currently in the WARMUP state.
 */
void lambda_stats_accumulate(lambda_longterm_stats_t *stats, si_category_t category, int32_t index, uint32_t delta_s, bool in_warmup);

/**
 * @brief Accumulate one sample of the rolling 2s control-quality average
 *        into the running sum/count used to compute its long-term mean.
 *
 * A symmetrically-dithering closed loop should keep this average near 0
 * even while instantaneous readings swing lean/rich; a sustained non-zero
 * long-term mean indicates a real mixture bias rather than normal dither.
 * Excluded during warmup, same rationale as index_min/index_max.
 *
 * @param stats   Pointer to stats struct to update.
 * @param avg_2s  Current 2s rolling-average mixture index.
 * @param in_warmup True if the FSM is currently in the WARMUP state.
 */
void lambda_stats_track_avg2s(lambda_longterm_stats_t *stats, int32_t avg_2s, bool in_warmup);

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

/**
 * @brief Recover long-term stats from an older on-disk struct layout.
 *
 * lambda_longterm_stats_t has changed shape several times as
 * LAMBDA_STATS_VERSION was bumped (1->2->3->4); a blob written by an
 * older firmware no longer round-trips through lambda_stats_validate().
 * This checks raw/raw_len against each known older layout (by size,
 * embedded struct_version, and that version's own CRC-32) and, on a
 * match, fills *out (current layout) with whatever fields the old
 * layout actually had - so a firmware update doesn't discard long-term
 * counters just because the struct grew a field.
 *
 * On a match, *out is first reset via lambda_stats_reset() (so any
 * field the old layout lacks, e.g. a metric added since, is 0/its
 * default) and then populated from the recovered fields. On no match,
 * *out is left untouched.
 *
 * Whenever LAMBDA_STATS_VERSION is bumped, add the *previous* layout
 * here as a new legacy_stats_vN_t case (see lambda_stats.c) instead of
 * accepting data loss.
 *
 * @param out     Destination struct (current layout).
 * @param raw     Raw bytes as read from storage.
 * @param raw_len Number of bytes in raw.
 * @return        true if raw matched a known older layout and *out was
 *                populated; false otherwise.
 */
bool lambda_stats_migrate_legacy(lambda_longterm_stats_t *out, const void *raw, size_t raw_len);

#endif /* LAMBDA_STATS_H */
