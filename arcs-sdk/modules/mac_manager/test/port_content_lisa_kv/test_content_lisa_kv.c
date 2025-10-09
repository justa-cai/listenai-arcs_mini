#include "unity.h"
#include "fff.h"

#include "mac_manager_content_ops_lisa_kv.h"
#include "mock_lisa_kv.h"
#include "mock_task.h"

DEFINE_FFF_GLOBALS;

void setUp(void)
{
    mock_lisa_kv_init();
    mock_task_init();
}

void tearDown(void)
{
    mock_lisa_kv_reset();
    mock_task_reset();
}

void test_content_lisa_kv_set(void)
{
    const uint8_t mac[6] = {0x26, 0x48, 0x1A, 0xF2, 0xF4, 0xF6};
    size_t mac_len = 6;
    int ret = 0;

    ret = mac_manager_content_ops_lisa_kv.set(mac, mac_len);

    TEST_ASSERT_EQUAL(1, lisa_kv_set_blob_fake.call_count);
    TEST_ASSERT_EQUAL_MEMORY(mac, lisa_kv_set_blob_fake.arg1_val, lisa_kv_set_blob_fake.arg2_val);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_content_lisa_kv_get(void)
{
    const uint8_t mac[6] = {0x26, 0x48, 0x1A, 0xF2, 0xF4, 0xF6};
    uint8_t mac_get[6] = {0};
    size_t mac_len = 6;
    int ret = 0;

    mac_manager_content_ops_lisa_kv.set(mac, mac_len);


    ret = mac_manager_content_ops_lisa_kv.get(mac_get, &mac_len);

    TEST_ASSERT_EQUAL(1, lisa_kv_get_blob_fake.call_count);
    TEST_ASSERT_EQUAL_MEMORY(mac, mac_get, mac_len);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_content_lisa_kv_free_after_get(void)
{
    const uint8_t mac[6] = {0x26, 0x48, 0x1A, 0xF2, 0xF4, 0xF6};
    size_t mac_len = 6;
    uint8_t mac_get[6] = {0};
    int ret = 0;

    mac_manager_content_ops_lisa_kv.set(mac, mac_len);

    ret = mac_manager_content_ops_lisa_kv.get(mac_get, &mac_len);

    TEST_ASSERT_EQUAL(1, lisa_kv_free_fake.call_count);
    TEST_ASSERT_EQUAL(0, ret);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_content_lisa_kv_set);
    RUN_TEST(test_content_lisa_kv_get);
    RUN_TEST(test_content_lisa_kv_free_after_get);

    return UNITY_END();
}
