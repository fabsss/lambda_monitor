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
        case SI_CAT_LEAN:
            stats->t_lean_s += delta_s;
            break;
        case SI_CAT_LAMBDA1:
            stats->t_lambda1_s += delta_s;
            break;
        case SI_CAT_RICH:
        case SI_CAT_VERY_RICH:
            stats->t_rich_s += delta_s;
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
