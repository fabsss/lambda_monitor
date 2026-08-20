#include <unity.h>
#include "ring_buffer.h"

void setUp(void) {}
void tearDown(void) {}

void test_starts_empty(void)
{
    ring_buffer_t rb;
    ring_buffer_init(&rb);
    TEST_ASSERT_EQUAL_UINT16(0, ring_buffer_count(&rb));
}

void test_push_and_get_fifo_order(void)
{
    ring_buffer_t rb;
    ring_buffer_init(&rb);
    ring_buffer_push(&rb, 10, 100);
    ring_buffer_push(&rb, 20, 101);
    ring_buffer_push(&rb, 30, 102);

    TEST_ASSERT_EQUAL_UINT16(3, ring_buffer_count(&rb));

    int32_t idx;
    uint32_t ts;
    TEST_ASSERT_TRUE(ring_buffer_get(&rb, 0, &idx, &ts));
    TEST_ASSERT_EQUAL_INT32(10, idx);
    TEST_ASSERT_EQUAL_UINT32(100, ts);

    TEST_ASSERT_TRUE(ring_buffer_get(&rb, 2, &idx, &ts));
    TEST_ASSERT_EQUAL_INT32(30, idx);
    TEST_ASSERT_EQUAL_UINT32(102, ts);
}

void test_get_out_of_range_returns_false(void)
{
    ring_buffer_t rb;
    ring_buffer_init(&rb);
    ring_buffer_push(&rb, 1, 1);

    int32_t idx;
    uint32_t ts;
    TEST_ASSERT_FALSE(ring_buffer_get(&rb, 1, &idx, &ts));
    TEST_ASSERT_FALSE(ring_buffer_get(&rb, 999, &idx, &ts));
}

void test_count_caps_at_capacity(void)
{
    ring_buffer_t rb;
    ring_buffer_init(&rb);
    for (int i = 0; i < RING_BUFFER_CAPACITY + 50; i++) {
        ring_buffer_push(&rb, i, (uint32_t)i);
    }
    TEST_ASSERT_EQUAL_UINT16(RING_BUFFER_CAPACITY, ring_buffer_count(&rb));
}

void test_overwrites_oldest_when_full(void)
{
    ring_buffer_t rb;
    ring_buffer_init(&rb);
    for (int i = 0; i < RING_BUFFER_CAPACITY; i++) {
        ring_buffer_push(&rb, i, (uint32_t)i);
    }
    /* buffer full: oldest is 0, newest is CAPACITY-1 */
    int32_t idx;
    uint32_t ts;
    ring_buffer_get(&rb, 0, &idx, &ts);
    TEST_ASSERT_EQUAL_INT32(0, idx);

    /* push one more: should overwrite the old index 0 */
    ring_buffer_push(&rb, 9999, 9999);
    TEST_ASSERT_EQUAL_UINT16(RING_BUFFER_CAPACITY, ring_buffer_count(&rb));

    ring_buffer_get(&rb, 0, &idx, &ts);
    TEST_ASSERT_EQUAL_INT32(1, idx); /* old oldest (0) is gone, now 1 is oldest */

    ring_buffer_get(&rb, (uint16_t)(RING_BUFFER_CAPACITY - 1), &idx, &ts);
    TEST_ASSERT_EQUAL_INT32(9999, idx); /* newest point */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_empty);
    RUN_TEST(test_push_and_get_fifo_order);
    RUN_TEST(test_get_out_of_range_returns_false);
    RUN_TEST(test_count_caps_at_capacity);
    RUN_TEST(test_overwrites_oldest_when_full);
    return UNITY_END();
}
