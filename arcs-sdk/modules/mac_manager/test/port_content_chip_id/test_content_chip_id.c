#include "unity.h"
#include "fff.h"

#include "mac_manager_content_ops_chip_id.h"
#include "mock_Driver_EFUSE.h"
#include "mock_task.h"


DEFINE_FFF_GLOBALS;


void setUp(void) {
    mock_driver_efuse_init();
    mock_task_init();
}

void tearDown(void) {
    mock_driver_efuse_reset();
    mock_task_reset();
}


void test_content_chip_id_get(void)
{
    uint8_t mac[20] = {0};
    size_t mac_len = 20;
    size_t expected_mac_len = 6;
    uint8_t expected_mac[6] = {0x26, 0x48, 0x1A, 0xF2, 0xF4, 0xF6};

    uint32_t high = 0x00F2F4F6;
    uint32_t low = 0x0000001A;

    efuse_read_uuid_fake.return_val = (uint64_t)high << 32 | low;

    mac_manager_content_ops_chip_id.get(mac, &mac_len);

    TEST_ASSERT_EQUAL_MEMORY(expected_mac, mac, mac_len);
    TEST_ASSERT_EQUAL(expected_mac_len, mac_len);
}

void test_content_chip_id_random(void)
{
    char mac[18] = {0};
    size_t mac_len = 18;

    mac_manager_content_ops_chip_id.random(mac, &mac_len);

    TEST_ASSERT_EQUAL(1, xTaskGetTickCount_fake.call_count);
    TEST_ASSERT_EQUAL(18, mac_len);
}

void test_content_chip_id_random_with_short_mac_len(void)
{
    char mac[18] = {0};
    size_t mac_len = 1;
    int ret = 0;

    ret = mac_manager_content_ops_chip_id.random(mac, &mac_len);

    TEST_ASSERT_EQUAL(-1, ret);
}

void test_content_chip_id_random_with_null_args(void)
{
    int ret = 0;
    size_t mac_len = 6;
    char mac[6] = {0};

    ret = mac_manager_content_ops_chip_id.random(NULL, NULL);

    TEST_ASSERT_EQUAL(-1, ret);

    ret = mac_manager_content_ops_chip_id.random(NULL, &mac_len);
    TEST_ASSERT_EQUAL(-1, ret);

    ret = mac_manager_content_ops_chip_id.random(mac, NULL);
    TEST_ASSERT_EQUAL(-1, ret);
}

void test_content_chip_id_get_with_null_args(void)
{
    int ret = 0;
    size_t mac_len = 6;
    char mac[6] = {0};

    ret = mac_manager_content_ops_chip_id.get(NULL, NULL);

    TEST_ASSERT_EQUAL(-1, ret);

    ret = mac_manager_content_ops_chip_id.get(NULL, &mac_len);
    TEST_ASSERT_EQUAL(-1, ret);

    ret = mac_manager_content_ops_chip_id.get(mac, NULL);
    TEST_ASSERT_EQUAL(-1, ret);
}

void test_content_chip_id_get_with_short_mac_len(void)
{
    uint8_t mac[20] = {0};
    size_t mac_len = 5;
    int ret = 0;

    ret = mac_manager_content_ops_chip_id.get(mac, &mac_len);

    TEST_ASSERT_EQUAL(-1, ret);
}

void test_content_chip_id_set(void)
{
    int ret = 0;
    uint8_t mac[6] = {0};
    size_t mac_len = 6;

    ret = mac_manager_content_ops_chip_id.set(mac, mac_len);

    // This implementation does not support set
    TEST_ASSERT_EQUAL(-1, ret);
}

void test_content_chip_id_del(void)
{
    int ret = 0;

    ret = mac_manager_content_ops_chip_id.del();

    // This implementation does not support del
    TEST_ASSERT_EQUAL(-1, ret);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_content_chip_id_get);
    RUN_TEST(test_content_chip_id_get_with_null_args);
    RUN_TEST(test_content_chip_id_get_with_short_mac_len);
    RUN_TEST(test_content_chip_id_set);
    RUN_TEST(test_content_chip_id_del);
    RUN_TEST(test_content_chip_id_random);
    RUN_TEST(test_content_chip_id_random_with_short_mac_len);
    RUN_TEST(test_content_chip_id_random_with_null_args);
    
    return UNITY_END();
}

