#include <unity.h>
#include "lambda_stats.h"

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
    TEST_ASSERT_EQUAL_INT16(100, stats.avg2s_min);
    TEST_ASSERT_EQUAL_INT16(-100, stats.avg2s_max);
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

void test_track_avg2s_updates_min_max(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_track_avg2s(&stats, 10, false);
    lambda_stats_track_avg2s(&stats, -35, false);
    lambda_stats_track_avg2s(&stats, 60, false);

    TEST_ASSERT_EQUAL_INT16(-35, stats.avg2s_min);
    TEST_ASSERT_EQUAL_INT16(60, stats.avg2s_max);
}

void test_track_avg2s_excludes_warmup(void)
{
    lambda_longterm_stats_t stats;
    lambda_stats_reset(&stats);

    lambda_stats_track_avg2s(&stats, -100, true);
    lambda_stats_track_avg2s(&stats, 100, true);

    /* avg2s_min/avg2s_max should remain at their untouched sentinel values */
    TEST_ASSERT_EQUAL_INT16(100, stats.avg2s_min);
    TEST_ASSERT_EQUAL_INT16(-100, stats.avg2s_max);
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_sets_version_and_zeroes_buckets);
    RUN_TEST(test_accumulate_warmup_only_adds_to_warmup_bucket);
    RUN_TEST(test_accumulate_operating_adds_to_category_bucket);
    RUN_TEST(test_warmup_excludes_index_min_max_tracking);
    RUN_TEST(test_operating_updates_index_min_max);
    RUN_TEST(test_track_avg2s_updates_min_max);
    RUN_TEST(test_track_avg2s_excludes_warmup);
    RUN_TEST(test_finalize_and_validate_round_trip);
    RUN_TEST(test_validate_fails_on_version_mismatch);
    return UNITY_END();
}
