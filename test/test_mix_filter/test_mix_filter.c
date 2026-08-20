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

void test_slow_filter_no_average_before_window_elapses(void)
{
    slow_filter_t f;
    slow_filter_init(&f, 5);
    slow_filter_push(&f, 100, 2);
    TEST_ASSERT_EQUAL_INT32(0, slow_filter_average(&f));
}

void test_slow_filter_publishes_average_after_window(void)
{
    slow_filter_t f;
    slow_filter_init(&f, 5);
    slow_filter_push(&f, 100, 2);
    slow_filter_push(&f, 200, 3);
    /* elapsed_s = 5, window elapsed: avg = (100+200)/2 = 150 */
    TEST_ASSERT_EQUAL_INT32(150, slow_filter_average(&f));
    TEST_ASSERT_EQUAL_UINT32(0, f.elapsed_s);
    TEST_ASSERT_EQUAL_UINT32(0, f.count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fast_filter_single_sample_average_is_itself);
    RUN_TEST(test_fast_filter_averages_two_samples);
    RUN_TEST(test_fast_filter_caps_at_window_size);
    RUN_TEST(test_slow_filter_no_average_before_window_elapses);
    RUN_TEST(test_slow_filter_publishes_average_after_window);
    return UNITY_END();
}
