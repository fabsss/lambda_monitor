#include <unity.h>
#include <string.h>
#include "lambda_stats.h"
#include "crc32.h"

void setUp(void) {}
void tearDown(void) {}

void test_reset_sets_version_and_zeroes_buckets(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    TEST_ASSERT_EQUAL_UINT16(LAMBDA_STATS_VERSION, stats.struct_version);
    TEST_ASSERT_EQUAL_UINT32(0, stats.t_warmup_s);
    TEST_ASSERT_EQUAL_UINT32(0, stats.t_very_lean_s);
    TEST_ASSERT_EQUAL_UINT32(0, stats.t_lean_s);
    TEST_ASSERT_EQUAL_UINT32(0, stats.t_lambda1_s);
    TEST_ASSERT_EQUAL_UINT32(0, stats.t_rich_s);
    TEST_ASSERT_EQUAL_UINT32(0, stats.t_very_rich_s);
    TEST_ASSERT_EQUAL_UINT32(0, stats.total_runtime_s);
    TEST_ASSERT_EQUAL_INT64(0, stats.avg2s_sum);
    TEST_ASSERT_EQUAL_UINT32(0, stats.avg2s_count);
}

void test_accumulate_warmup_only_adds_to_warmup_bucket(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, 0, 10, true);

    TEST_ASSERT_EQUAL_UINT32(10, stats.t_warmup_s);
    TEST_ASSERT_EQUAL_UINT32(0, stats.t_lambda1_s);
    TEST_ASSERT_EQUAL_UINT32(10, stats.total_runtime_s);
}

void test_accumulate_operating_adds_to_category_bucket(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_accumulate(&stats, SI_CAT_VERY_LEAN, -80, 5, false);
    lambda_stats_accumulate(&stats, SI_CAT_LEAN, -30, 3, false);
    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, 0, 7, false);
    lambda_stats_accumulate(&stats, SI_CAT_RICH, 30, 4, false);
    lambda_stats_accumulate(&stats, SI_CAT_VERY_RICH, 80, 2, false);

    TEST_ASSERT_EQUAL_UINT32(5, stats.t_very_lean_s);
    TEST_ASSERT_EQUAL_UINT32(3, stats.t_lean_s);
    TEST_ASSERT_EQUAL_UINT32(7, stats.t_lambda1_s);
    TEST_ASSERT_EQUAL_UINT32(4, stats.t_rich_s);
    TEST_ASSERT_EQUAL_UINT32(2, stats.t_very_rich_s);
    TEST_ASSERT_EQUAL_UINT32(21, stats.total_runtime_s);
}

void test_warmup_excludes_index_min_max_tracking(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, -100, 1, true);
    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, 100, 1, true);

    /* index_min/index_max should remain at their untouched sentinel values */
    TEST_ASSERT_EQUAL_INT16(100, stats.index_min);
    TEST_ASSERT_EQUAL_INT16(-100, stats.index_max);
}

void test_operating_updates_index_min_max(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, 10, 1, false);
    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, -40, 1, false);
    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, 75, 1, false);

    TEST_ASSERT_EQUAL_INT16(-40, stats.index_min);
    TEST_ASSERT_EQUAL_INT16(75, stats.index_max);
}

void test_track_avg2s_accumulates_sum_and_count(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_track_avg2s(&stats, 10, false);
    lambda_stats_track_avg2s(&stats, -35, false);
    lambda_stats_track_avg2s(&stats, 60, false);

    TEST_ASSERT_EQUAL_INT64(35, stats.avg2s_sum);
    TEST_ASSERT_EQUAL_UINT32(3, stats.avg2s_count);
}

void test_track_avg2s_excludes_warmup(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_track_avg2s(&stats, -100, true);
    lambda_stats_track_avg2s(&stats, 100, true);

    /* avg2s_sum/avg2s_count should remain untouched */
    TEST_ASSERT_EQUAL_INT64(0, stats.avg2s_sum);
    TEST_ASSERT_EQUAL_UINT32(0, stats.avg2s_count);
}

void test_finalize_and_validate_round_trip(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);
    lambda_stats_accumulate(&stats, SI_CAT_LAMBDA1, 5, 42, false);

    lambda_stats_finalize_crc(&stats);
    TEST_ASSERT_TRUE(lambda_stats_validate(&stats));

    /* Corrupt a field: validation should now fail */
    stats.t_lambda1_s += 1;
    TEST_ASSERT_FALSE(lambda_stats_validate(&stats));
}

void test_validate_fails_on_version_mismatch(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);
    lambda_stats_finalize_crc(&stats);

    stats.struct_version = LAMBDA_STATS_VERSION + 1;
    /* Even though CRC would need recompute, version check should fail first */
    TEST_ASSERT_FALSE(lambda_stats_validate(&stats));
}

/* Mirrors of the legacy on-disk layouts kept privately in lambda_stats.c,
 * used here only to build fixture bytes for lambda_stats_migrate_legacy()
 * without exposing those historical structs outside the module. */

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

void test_migrate_legacy_v1_recovers_time_buckets(void)
{
    legacy_stats_v1_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.struct_version = 1;
    rec.t_warmup_s = 11;
    rec.t_lean_s = 22;
    rec.t_lambda1_s = 33;
    rec.t_rich_s = 44;
    rec.index_min = -60;
    rec.index_max = 70;
    rec.total_runtime_s = 110;
    rec.crc32 = crc32_compute(&rec, sizeof(rec));

    lambda_longterm_stats_t out;
    memset(&out, 0xAA, sizeof(out));
    TEST_ASSERT_TRUE(lambda_stats_migrate_legacy(&out, &rec, sizeof(rec)));

    TEST_ASSERT_EQUAL_UINT16(LAMBDA_STATS_VERSION, out.struct_version);
    TEST_ASSERT_EQUAL_UINT32(11, out.t_warmup_s);
    TEST_ASSERT_EQUAL_UINT32(0, out.t_very_lean_s);
    TEST_ASSERT_EQUAL_UINT32(22, out.t_lean_s);
    TEST_ASSERT_EQUAL_UINT32(33, out.t_lambda1_s);
    TEST_ASSERT_EQUAL_UINT32(44, out.t_rich_s);
    TEST_ASSERT_EQUAL_UINT32(0, out.t_very_rich_s);
    TEST_ASSERT_EQUAL_INT16(-60, out.index_min);
    TEST_ASSERT_EQUAL_INT16(70, out.index_max);
    TEST_ASSERT_EQUAL_UINT32(110, out.total_runtime_s);
    TEST_ASSERT_EQUAL_INT64(0, out.avg2s_sum);
    TEST_ASSERT_EQUAL_UINT32(0, out.avg2s_count);
}

void test_migrate_legacy_v3_recovers_all_dwell_buckets(void)
{
    legacy_stats_v3_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.struct_version = 3;
    rec.t_warmup_s = 1;
    rec.t_very_lean_s = 2;
    rec.t_lean_s = 3;
    rec.t_lambda1_s = 4;
    rec.t_rich_s = 5;
    rec.t_very_rich_s = 6;
    rec.index_min = -80;
    rec.index_max = 90;
    rec.total_runtime_s = 21;
    rec.avg2s_min = -10;
    rec.avg2s_max = 10;
    rec.crc32 = crc32_compute(&rec, sizeof(rec));

    lambda_longterm_stats_t out;
    memset(&out, 0xAA, sizeof(out));
    TEST_ASSERT_TRUE(lambda_stats_migrate_legacy(&out, &rec, sizeof(rec)));

    TEST_ASSERT_EQUAL_UINT32(1, out.t_warmup_s);
    TEST_ASSERT_EQUAL_UINT32(2, out.t_very_lean_s);
    TEST_ASSERT_EQUAL_UINT32(3, out.t_lean_s);
    TEST_ASSERT_EQUAL_UINT32(4, out.t_lambda1_s);
    TEST_ASSERT_EQUAL_UINT32(5, out.t_rich_s);
    TEST_ASSERT_EQUAL_UINT32(6, out.t_very_rich_s);
    TEST_ASSERT_EQUAL_INT16(-80, out.index_min);
    TEST_ASSERT_EQUAL_INT16(90, out.index_max);
    TEST_ASSERT_EQUAL_UINT32(21, out.total_runtime_s);
}

void test_migrate_legacy_rejects_corrupt_blob(void)
{
    legacy_stats_v1_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.struct_version = 1;
    rec.t_warmup_s = 5;
    rec.crc32 = crc32_compute(&rec, sizeof(rec));
    rec.t_warmup_s += 1; /* corrupt after CRC was computed */

    lambda_longterm_stats_t out;
    TEST_ASSERT_FALSE(lambda_stats_migrate_legacy(&out, &rec, sizeof(rec)));
}

void test_migrate_legacy_rejects_unknown_size(void)
{
    uint8_t junk[7] = {0};
    lambda_longterm_stats_t out;
    TEST_ASSERT_FALSE(lambda_stats_migrate_legacy(&out, junk, sizeof(junk)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_sets_version_and_zeroes_buckets);
    RUN_TEST(test_accumulate_warmup_only_adds_to_warmup_bucket);
    RUN_TEST(test_accumulate_operating_adds_to_category_bucket);
    RUN_TEST(test_warmup_excludes_index_min_max_tracking);
    RUN_TEST(test_operating_updates_index_min_max);
    RUN_TEST(test_track_avg2s_accumulates_sum_and_count);
    RUN_TEST(test_track_avg2s_excludes_warmup);
    RUN_TEST(test_finalize_and_validate_round_trip);
    RUN_TEST(test_validate_fails_on_version_mismatch);
    RUN_TEST(test_migrate_legacy_v1_recovers_time_buckets);
    RUN_TEST(test_migrate_legacy_v3_recovers_all_dwell_buckets);
    RUN_TEST(test_migrate_legacy_rejects_corrupt_blob);
    RUN_TEST(test_migrate_legacy_rejects_unknown_size);
    return UNITY_END();
}
