#include <unity.h>
#include "calib_wizard.h"

void setUp(void) {}
void tearDown(void) {}

void test_sort_ascending(void)
{
    int32_t values[] = {5, 3, 8, 1, 9, 2};
    calib_sort_i32(values, 6);
    int32_t expected[] = {1, 2, 3, 5, 8, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, values, 6);
}

void test_sort_already_sorted(void)
{
    int32_t values[] = {1, 2, 3, 4};
    calib_sort_i32(values, 4);
    int32_t expected[] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, values, 4);
}

void test_sort_single_element(void)
{
    int32_t values[] = {42};
    calib_sort_i32(values, 1);
    TEST_ASSERT_EQUAL_INT32(42, values[0]);
}

void test_percentile_min_and_max(void)
{
    int32_t sorted[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    TEST_ASSERT_EQUAL_INT32(10, calib_percentile(sorted, 10, 0));
    TEST_ASSERT_EQUAL_INT32(100, calib_percentile(sorted, 10, 100));
}

void test_percentile_5th_and_95th(void)
{
    int32_t sorted[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    /* n=10: rank = ceil(5/100*10) = ceil(0.5) = 1 -> sorted[0] = 10 */
    TEST_ASSERT_EQUAL_INT32(10, calib_percentile(sorted, 10, 5));
    /* rank = ceil(95/100*10) = ceil(9.5) = 10 -> sorted[9] = 100 */
    TEST_ASSERT_EQUAL_INT32(100, calib_percentile(sorted, 10, 95));
}

void test_auto_derive_computes_min_max_lambda1_deadband(void)
{
    /* 100 samples uniformly 0..990 in steps of 10 */
    int32_t observed[100];
    for (int i = 0; i < 100; i++) {
        observed[i] = i * 10;
    }

    si_calibration_t cal;
    calib_auto_derive(observed, 100, &cal);

    /* 5th percentile: rank = ceil(5) = 5 -> sorted[4] = 40 */
    TEST_ASSERT_EQUAL_INT32(40, cal.u_min_mv);
    /* 95th percentile: rank = ceil(95) = 95 -> sorted[94] = 940 */
    TEST_ASSERT_EQUAL_INT32(940, cal.u_max_mv);
    /* midpoint */
    TEST_ASSERT_EQUAL_INT32((40 + 940) / 2, cal.u_lambda1_mv);
    /* deadband = 5% of span (940-40=900) = 45 */
    TEST_ASSERT_EQUAL_INT32(45, cal.deadband_mv);
}

void test_auto_derive_does_not_mutate_input(void)
{
    int32_t observed[] = {50, 10, 30, 20, 40};
    int32_t original[] = {50, 10, 30, 20, 40};

    si_calibration_t cal;
    calib_auto_derive(observed, 5, &cal);

    TEST_ASSERT_EQUAL_INT32_ARRAY(original, observed, 5);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sort_ascending);
    RUN_TEST(test_sort_already_sorted);
    RUN_TEST(test_sort_single_element);
    RUN_TEST(test_percentile_min_and_max);
    RUN_TEST(test_percentile_5th_and_95th);
    RUN_TEST(test_auto_derive_computes_min_max_lambda1_deadband);
    RUN_TEST(test_auto_derive_does_not_mutate_input);
    return UNITY_END();
}
