#include <unity.h>
#include "signal_interpreter.h"

void setUp(void) {}
void tearDown(void) {}

static si_calibration_t default_cal(void)
{
    si_calibration_t cal;
    si_default_calibration(&cal);
    return cal;
}

void test_default_calibration_values(void)
{
    si_calibration_t cal = default_cal();
    TEST_ASSERT_EQUAL_INT32(0, cal.u_min_mv);
    TEST_ASSERT_EQUAL_INT32(3000, cal.u_max_mv);
    TEST_ASSERT_EQUAL_INT32(1500, cal.u_lambda1_mv);
    TEST_ASSERT_EQUAL_INT32(150, cal.deadband_mv);
}

void test_mv_to_index_at_lambda1_is_zero(void)
{
    si_calibration_t cal = default_cal();
    TEST_ASSERT_EQUAL_INT32(0, si_mv_to_index(&cal, 1500));
}

void test_mv_to_index_at_u_min_is_minus_100(void)
{
    si_calibration_t cal = default_cal();
    TEST_ASSERT_EQUAL_INT32(-100, si_mv_to_index(&cal, 0));
}

void test_mv_to_index_at_u_max_is_plus_100(void)
{
    si_calibration_t cal = default_cal();
    TEST_ASSERT_EQUAL_INT32(100, si_mv_to_index(&cal, 3000));
}

void test_mv_to_index_midpoint_lean_side(void)
{
    si_calibration_t cal = default_cal();
    /* 750mV is halfway between u_min(0) and u_lambda1(1500) -> index -50 */
    TEST_ASSERT_EQUAL_INT32(-50, si_mv_to_index(&cal, 750));
}

void test_mv_to_index_midpoint_rich_side(void)
{
    si_calibration_t cal = default_cal();
    /* 2250mV is halfway between u_lambda1(1500) and u_max(3000) -> index +50 */
    TEST_ASSERT_EQUAL_INT32(50, si_mv_to_index(&cal, 2250));
}

void test_mv_to_index_clamps_below_u_min(void)
{
    si_calibration_t cal = default_cal();
    TEST_ASSERT_EQUAL_INT32(-100, si_mv_to_index(&cal, -500));
}

void test_mv_to_index_clamps_above_u_max(void)
{
    si_calibration_t cal = default_cal();
    TEST_ASSERT_EQUAL_INT32(100, si_mv_to_index(&cal, 5000));
}

void test_category_boundaries(void)
{
    si_calibration_t cal = default_cal();
    TEST_ASSERT_EQUAL(SI_CAT_VERY_LEAN, si_index_to_category(&cal, -100));
    TEST_ASSERT_EQUAL(SI_CAT_VERY_LEAN, si_index_to_category(&cal, -61));
    TEST_ASSERT_EQUAL(SI_CAT_LEAN, si_index_to_category(&cal, -60));
    TEST_ASSERT_EQUAL(SI_CAT_LEAN, si_index_to_category(&cal, -21));
    TEST_ASSERT_EQUAL(SI_CAT_LAMBDA1, si_index_to_category(&cal, -20));
    TEST_ASSERT_EQUAL(SI_CAT_LAMBDA1, si_index_to_category(&cal, 0));
    TEST_ASSERT_EQUAL(SI_CAT_LAMBDA1, si_index_to_category(&cal, 19));
    TEST_ASSERT_EQUAL(SI_CAT_RICH, si_index_to_category(&cal, 20));
    TEST_ASSERT_EQUAL(SI_CAT_RICH, si_index_to_category(&cal, 59));
    TEST_ASSERT_EQUAL(SI_CAT_VERY_RICH, si_index_to_category(&cal, 60));
    TEST_ASSERT_EQUAL(SI_CAT_VERY_RICH, si_index_to_category(&cal, 100));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_calibration_values);
    RUN_TEST(test_mv_to_index_at_lambda1_is_zero);
    RUN_TEST(test_mv_to_index_at_u_min_is_minus_100);
    RUN_TEST(test_mv_to_index_at_u_max_is_plus_100);
    RUN_TEST(test_mv_to_index_midpoint_lean_side);
    RUN_TEST(test_mv_to_index_midpoint_rich_side);
    RUN_TEST(test_mv_to_index_clamps_below_u_min);
    RUN_TEST(test_mv_to_index_clamps_above_u_max);
    RUN_TEST(test_category_boundaries);
    return UNITY_END();
}
