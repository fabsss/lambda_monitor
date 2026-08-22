#include <unity.h>
#include "mix_filter.h"

void setUp(void) {}
void tearDown(void) {}

void test_fast_filter_single_sample_average_is_itself(void)
{
    fast_filter_t f;
    fast_filter_init(&f);
    int32_t avg = fast_filter_push(&f, 100);
    TEST_ASSERT_EQUAL_INT32(100, avg);
}

void test_fast_filter_averages_two_samples(void)
{
    fast_filter_t f;
    fast_filter_init(&f);
    fast_filter_push(&f, 100);
    int32_t avg = fast_filter_push(&f, 200);
    TEST_ASSERT_EQUAL_INT32(150, avg);
}

void test_fast_filter_caps_at_window_size(void)
{
    fast_filter_t f;
    fast_filter_init(&f);
    /* Push 9 samples of value 10, then one of 90 -> window holds last 8 samples */
    for (int i = 0; i < 9; i++) {
        fast_filter_push(&f, 10);
    }
    int32_t avg = fast_filter_push(&f, 90);
    /* window now: seven 10s + one 90 = 70+90 = 160 / 8 = 20 */
    TEST_ASSERT_EQUAL_INT32(20, avg);
    TEST_ASSERT_EQUAL_UINT8(FAST_FILTER_SIZE, f.count);
}

void test_slow_filter_averages_immediately_not_only_after_window(void)
{
    /* A moving average must update on every push, not just once the
     * window has fully filled (that would be a tumbling/block average). */
    slow_filter_t f;
    slow_filter_init(&f, 5);
    slow_filter_push(&f, 100, 1);
    TEST_ASSERT_EQUAL_INT32(100, slow_filter_average(&f));
    slow_filter_push(&f, 200, 1);
    TEST_ASSERT_EQUAL_INT32(150, slow_filter_average(&f));
}

void test_slow_filter_caps_at_window_size(void)
{
    slow_filter_t f;
    slow_filter_init(&f, 5);
    for (int i = 0; i < 4; i++) {
        slow_filter_push(&f, 10, 1);
    }
    /* window now full of 10s; pushing a 60 should drop the oldest 10,
     * not reset the whole window like a tumbling average would. */
    slow_filter_push(&f, 60, 1);
    /* four 10s + one 60 = 100 / 5 = 20 */
    TEST_ASSERT_EQUAL_INT32(20, slow_filter_average(&f));

    /* Next push slides the window further instead of jumping/resetting. */
    slow_filter_push(&f, 60, 1);
    /* three 10s + two 60s = 150 / 5 = 30 */
    TEST_ASSERT_EQUAL_INT32(30, slow_filter_average(&f));
}

void test_slow_filter_does_not_reset_between_windows(void)
{
    /* Regression guard for the old tumbling-average bug: the average must
     * not jump back to a fresh block average once window_s samples have
     * been pushed - it keeps sliding smoothly. */
    slow_filter_t f;
    slow_filter_init(&f, 5);
    for (int i = 0; i < 5; i++) {
        slow_filter_push(&f, 100, 1);
    }
    TEST_ASSERT_EQUAL_INT32(100, slow_filter_average(&f));
    slow_filter_push(&f, 100, 1);
    /* Still all 100s in the window - no reset-induced jump. */
    TEST_ASSERT_EQUAL_INT32(100, slow_filter_average(&f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fast_filter_single_sample_average_is_itself);
    RUN_TEST(test_fast_filter_averages_two_samples);
    RUN_TEST(test_fast_filter_caps_at_window_size);
    RUN_TEST(test_slow_filter_averages_immediately_not_only_after_window);
    RUN_TEST(test_slow_filter_caps_at_window_size);
    RUN_TEST(test_slow_filter_does_not_reset_between_windows);
    return UNITY_END();
}
