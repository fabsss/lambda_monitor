#include "lambda_stats.h"
#include "crc32.h"
#include <string.h>

void lambda_stats_reset(lambda_longterm_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->struct_version = LAMBDA_STATS_VERSION;
    stats->index_min = 100;
    stats->index_max = -100;
}

void lambda_stats_accumulate(lambda_longterm_stats_t *stats, si_category_t category, int32_t index, uint32_t delta_s, bool in_warmup)
{
    stats->total_runtime_s += delta_s;

    if (in_warmup) {
        stats->t_warmup_s += delta_s;
        return;
    }

    switch (category) {
        case SI_CAT_VERY_LEAN:
            stats->t_very_lean_s += delta_s;
            break;
        case SI_CAT_LEAN:
            stats->t_lean_s += delta_s;
            break;
        case SI_CAT_LAMBDA1:
            stats->t_lambda1_s += delta_s;
            break;
        case SI_CAT_RICH:
            stats->t_rich_s += delta_s;
            break;
        case SI_CAT_VERY_RICH:
            stats->t_very_rich_s += delta_s;
            break;
        default:
            break;
    }

    if (index < stats->index_min) {
        stats->index_min = (int16_t)index;
    }
    if (index > stats->index_max) {
        stats->index_max = (int16_t)index;
    }
}

void lambda_stats_track_avg2s(lambda_longterm_stats_t *stats, int32_t avg_2s, bool in_warmup)
{
    if (in_warmup) {
        return;
    }
    stats->avg2s_sum += avg_2s;
    stats->avg2s_count += 1;
}

void lambda_stats_finalize_crc(lambda_longterm_stats_t *stats)
{
    stats->crc32 = 0;
    stats->crc32 = crc32_compute(stats, sizeof(*stats));
}

bool lambda_stats_validate(const lambda_longterm_stats_t *stats)
{
    if (stats->struct_version != LAMBDA_STATS_VERSION) {
        return false;
    }

    lambda_longterm_stats_t copy = *stats;
    uint32_t stored_crc = copy.crc32;
    copy.crc32 = 0;
    uint32_t computed_crc = crc32_compute(&copy, sizeof(copy));

    return stored_crc == computed_crc;
}

/* Previous on-disk layouts of lambda_longterm_stats_t, kept only to let
 * lambda_stats_migrate_legacy() recover data written by an older firmware.
 * Every field here always has a struct_version (uint16_t) as its first
 * member and a crc32 (uint32_t) as its last, same convention as the
 * current struct. Do not change these once added - they describe layouts
 * that already shipped. */

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
} legacy_stats_v1_t;

typedef struct __attribute__((packed)) {
    uint16_t struct_version;
    uint32_t t_warmup_s;
    uint32_t t_lean_s;
    uint32_t t_lambda1_s;
    uint32_t t_rich_s;
    int16_t  index_min;
    int16_t  index_max;
    uint32_t total_runtime_s;
    int16_t  avg2s_min;
    int16_t  avg2s_max;
    uint32_t crc32;
} legacy_stats_v2_t;

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
    int16_t  avg2s_min;
    int16_t  avg2s_max;
    uint32_t crc32;
} legacy_stats_v3_t;

/* Largest legacy layout above; current lambda_longterm_stats_t is bigger
 * still (it's always the newest/largest layout by construction), so this
 * is just a safe stack scratch size for the CRC recheck below. */
#define LEGACY_STATS_MAX_SIZE 48

static bool legacy_record_valid(const void *raw, size_t size, uint16_t expected_version)
{
    if (size < sizeof(uint16_t) + sizeof(uint32_t) || size > LEGACY_STATS_MAX_SIZE) {
        return false;
    }

    uint16_t version;
    memcpy(&version, raw, sizeof(version));
    if (version != expected_version) {
        return false;
    }

    uint32_t stored_crc;
    memcpy(&stored_crc, (const uint8_t *)raw + size - sizeof(uint32_t), sizeof(stored_crc));

    uint8_t copy[LEGACY_STATS_MAX_SIZE];
    memcpy(copy, raw, size);
    memset(copy + size - sizeof(uint32_t), 0, sizeof(uint32_t));

    return crc32_compute(copy, size) == stored_crc;
}

bool lambda_stats_migrate_legacy(lambda_longterm_stats_t *out, const void *raw, size_t raw_len)
{
    if (raw_len == sizeof(legacy_stats_v3_t) && legacy_record_valid(raw, raw_len, 3)) {
        legacy_stats_v3_t rec;
        memcpy(&rec, raw, sizeof(rec));

        lambda_stats_reset(out);
        out->t_warmup_s = rec.t_warmup_s;
        out->t_very_lean_s = rec.t_very_lean_s;
        out->t_lean_s = rec.t_lean_s;
        out->t_lambda1_s = rec.t_lambda1_s;
        out->t_rich_s = rec.t_rich_s;
        out->t_very_rich_s = rec.t_very_rich_s;
        out->index_min = rec.index_min;
        out->index_max = rec.index_max;
        out->total_runtime_s = rec.total_runtime_s;
        /* avg2s_min/max (extremes) has no equivalent in avg2s_sum/count
         * (a running mean); left at reset's 0/0 rather than guessed at. */
        return true;
    }

    if (raw_len == sizeof(legacy_stats_v2_t) && legacy_record_valid(raw, raw_len, 2)) {
        legacy_stats_v2_t rec;
        memcpy(&rec, raw, sizeof(rec));

        lambda_stats_reset(out);
        out->t_warmup_s = rec.t_warmup_s;
        out->t_lean_s = rec.t_lean_s;
        out->t_lambda1_s = rec.t_lambda1_s;
        out->t_rich_s = rec.t_rich_s;
        out->index_min = rec.index_min;
        out->index_max = rec.index_max;
        out->total_runtime_s = rec.total_runtime_s;
        return true;
    }

    if (raw_len == sizeof(legacy_stats_v1_t) && legacy_record_valid(raw, raw_len, 1)) {
        legacy_stats_v1_t rec;
        memcpy(&rec, raw, sizeof(rec));

        lambda_stats_reset(out);
        out->t_warmup_s = rec.t_warmup_s;
        out->t_lean_s = rec.t_lean_s;
        out->t_lambda1_s = rec.t_lambda1_s;
        out->t_rich_s = rec.t_rich_s;
        out->index_min = rec.index_min;
        out->index_max = rec.index_max;
        out->total_runtime_s = rec.total_runtime_s;
        return true;
    }

    return false;
}
