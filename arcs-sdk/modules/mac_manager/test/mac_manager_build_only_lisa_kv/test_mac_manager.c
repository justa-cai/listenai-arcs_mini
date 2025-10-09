#include "unity.h"
#include "mac_manager.h"
#include "mac_manager_mem_ops_sys_heap.h"
#include "mac_manager_content_ops_lisa_kv.h"
#include "mock_sysheap.h"
#include "mock_lisa_kv.h"
#include "fff.h"

DEFINE_FFF_GLOBALS;

static mac_manager_t *mgr;

static mac_manager_t *custom_mac_manager_init(mac_manager_config_t *config)
{
    mgr = mac_manager_init(&mac_manager_mem_ops_sys_heap, &mac_manager_content_ops_lisa_kv, config);

    return mgr;
}

void setUp(void)
{

    mock_sysheap_init();
    mock_lisa_kv_init();

    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};

    mgr = custom_mac_manager_init(&config);
}

void tearDown(void)
{
    if (mgr) {
        mac_manager_deinit(mgr);
        mgr = NULL;
    }
    mock_sysheap_reset();
    mock_lisa_kv_reset();
}

void test_mac_manager_lisa_kv_impl(void)
{
    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};

    mac_manager_t *mac_mgr = mac_manager_init(&mac_manager_mem_ops_sys_heap, &mac_manager_content_ops_lisa_kv, &config);

    TEST_ASSERT_NOT_NULL(mac_mgr);

    mac_manager_deinit(mac_mgr);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_mac_manager_lisa_kv_impl);

    return UNITY_END();
}
