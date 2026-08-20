#include <unity.h>
#include "crc32.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_crc32_of_empty_buffer_is_zero(void)
{
    uint32_t crc = crc32_compute(NULL, 0);
    TEST_ASSERT_EQUAL_UINT32(0x00000000u, crc);
}

void test_crc32_known_vector_123456789(void)
{
    const char *data = "123456789";
    uint32_t crc = crc32_compute(data, strlen(data));
    /* Standard CRC-32/ISO-HDLC check value */
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, crc);
}

void test_crc32_differs_for_different_input(void)
{
    const char *a = "hello world";
    const char *b = "hello worle";
    uint32_t crc_a = crc32_compute(a, strlen(a));
    uint32_t crc_b = crc32_compute(b, strlen(b));
    TEST_ASSERT_NOT_EQUAL(crc_a, crc_b);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc32_of_empty_buffer_is_zero);
    RUN_TEST(test_crc32_known_vector_123456789);
    RUN_TEST(test_crc32_differs_for_different_input);
    return UNITY_END();
}
