#include <unity.h>
#include "warmup_fsm.h"

void setUp(void) {}
void tearDown(void) {}

void test_starts_in_warmup_state(void)
{
    warmup_fsm_t fsm;
    warmup_fsm_init(&fsm, 90);
    TEST_ASSERT_EQUAL(WARMUP_STATE_WARMUP, fsm.state);
    TEST_ASSERT_EQUAL_UINT32(0, fsm.elapsed_s);
}

void test_stays_in_warmup_before_timeout(void)
{
    warmup_fsm_t fsm;
    warmup_fsm_init(&fsm, 90);
    warmup_fsm_tick(&fsm, 30, false);
    TEST_ASSERT_EQUAL(WARMUP_STATE_WARMUP, fsm.state);
    warmup_fsm_tick(&fsm, 30, false);
    TEST_ASSERT_EQUAL(WARMUP_STATE_WARMUP, fsm.state);
    TEST_ASSERT_EQUAL_UINT32(60, fsm.elapsed_s);
}

void test_transitions_on_timeout(void)
{
    warmup_fsm_t fsm;
    warmup_fsm_init(&fsm, 90);
    warmup_fsm_tick(&fsm, 89, false);
    TEST_ASSERT_EQUAL(WARMUP_STATE_WARMUP, fsm.state);
    warmup_fsm_tick(&fsm, 1, false);
    TEST_ASSERT_EQUAL(WARMUP_STATE_OPERATING, fsm.state);
}

void test_transitions_on_edge_before_timeout(void)
{
    warmup_fsm_t fsm;
    warmup_fsm_init(&fsm, 90);
    warmup_fsm_tick(&fsm, 10, false);
    TEST_ASSERT_EQUAL(WARMUP_STATE_WARMUP, fsm.state);
    warmup_fsm_tick(&fsm, 5, true);
    TEST_ASSERT_EQUAL(WARMUP_STATE_OPERATING, fsm.state);
}

void test_never_reenters_warmup_after_operating(void)
{
    warmup_fsm_t fsm;
    warmup_fsm_init(&fsm, 90);
    warmup_fsm_tick(&fsm, 5, true);
    TEST_ASSERT_EQUAL(WARMUP_STATE_OPERATING, fsm.state);

    uint32_t elapsed_before = fsm.elapsed_s;
    warmup_fsm_tick(&fsm, 1000, false);
    TEST_ASSERT_EQUAL(WARMUP_STATE_OPERATING, fsm.state);
    TEST_ASSERT_EQUAL_UINT32(elapsed_before, fsm.elapsed_s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_in_warmup_state);
    RUN_TEST(test_stays_in_warmup_before_timeout);
    RUN_TEST(test_transitions_on_timeout);
    RUN_TEST(test_transitions_on_edge_before_timeout);
    RUN_TEST(test_never_reenters_warmup_after_operating);
    return UNITY_END();
}
