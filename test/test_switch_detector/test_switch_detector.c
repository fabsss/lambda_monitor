#include <unity.h>
#include "switch_detector.h"

void setUp(void) {}
void tearDown(void) {}

void test_first_sample_is_never_an_edge(void)
{
    switch_detector_t sd;
    switch_detector_init(&sd);
    bool edge = switch_detector_update(&sd, 50, 1);
    TEST_ASSERT_FALSE(edge);
}

void test_crossing_zero_is_an_edge(void)
{
    switch_detector_t sd;
    switch_detector_init(&sd);
    switch_detector_update(&sd, 10, 1);
    bool edge = switch_detector_update(&sd, -10, 1);
    TEST_ASSERT_TRUE(edge);
}

void test_staying_on_same_side_is_not_an_edge(void)
{
    switch_detector_t sd;
    switch_detector_init(&sd);
    switch_detector_update(&sd, 10, 1);
    bool edge = switch_detector_update(&sd, 20, 1);
    TEST_ASSERT_FALSE(edge);
}

void test_seconds_since_last_edge_resets_on_edge(void)
{
    switch_detector_t sd;
    switch_detector_init(&sd);
    switch_detector_update(&sd, 10, 1);
    switch_detector_update(&sd, 10, 5);
    TEST_ASSERT_EQUAL_UINT32(6, sd.seconds_since_last_edge);

    switch_detector_update(&sd, -10, 3);
    TEST_ASSERT_EQUAL_UINT32(0, sd.seconds_since_last_edge);

    switch_detector_update(&sd, -10, 4);
    TEST_ASSERT_EQUAL_UINT32(4, sd.seconds_since_last_edge);
}

void test_switches_per_min_computed_after_60s_window(void)
{
    switch_detector_t sd;
    switch_detector_init(&sd);

    int32_t val = 10;
    switch_detector_update(&sd, val, 1);

    /* Alternate sign every second for 59 more seconds -> 59 edges, window not yet elapsed */
    for (int i = 0; i < 59; i++) {
        val = -val;
        switch_detector_update(&sd, val, 1);
    }
    /* window_elapsed_s == 60 now, so switches_per_min should be published */
    TEST_ASSERT_EQUAL_UINT32(59, sd.switches_per_min);
    TEST_ASSERT_EQUAL_UINT32(0, sd.window_elapsed_s);
    TEST_ASSERT_EQUAL_UINT32(0, sd.edge_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_is_never_an_edge);
    RUN_TEST(test_crossing_zero_is_an_edge);
    RUN_TEST(test_staying_on_same_side_is_not_an_edge);
    RUN_TEST(test_seconds_since_last_edge_resets_on_edge);
    RUN_TEST(test_switches_per_min_computed_after_60s_window);
    return UNITY_END();
}
