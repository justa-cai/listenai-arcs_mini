#include "unity.h"
#include "mac_manager.h"
#include "mock_mem_ops.h"
#include "mock_content_ops.h"


DEFINE_FFF_GLOBALS;

// Force to use the same struct as in src
typedef struct mac_manager_s {
    mac_manager_content_ops_t content_ops;
    mac_manager_mem_ops_t mem_ops;
    mac_manager_config_t config;
    bool init_done;
} mac_manager_t;


static mac_manager_t* mgr;

static mac_manager_t *custom_mac_manager_init(mac_manager_config_t *config)
{
    mgr = mac_manager_init(mock_mem_ops_get(), mock_content_get_ops(), config);

    return mgr;
}

void setUp(void) {
    mock_mem_ops_init();
    mock_content_ops_init();

    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};
    
    mgr = custom_mac_manager_init(&config);
}

// 测试后清理
void tearDown(void) {
    if (mgr) {
        mac_manager_deinit(mgr);
        mgr = NULL;
    }
    mock_mem_ops_reset();
    mock_content_ops_reset();
}


void test_mac_manager_lisa_kv_impl(void)
{
    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};

    mock_mem_ops_reset();
    
    mac_manager_t *mac_mgr = custom_mac_manager_init(&config);

    TEST_ASSERT_EQUAL(1, mock_malloc_fake.call_count);
    TEST_ASSERT_NOT_NULL(mac_mgr);

    mac_manager_deinit(mac_mgr);
}

void test_mac_manager_init_with_copy_params(void)
{

    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};

    mock_mem_ops_reset();

    mac_manager_mem_ops_t mem_ops = {
        .free = mock_free,
        .malloc = mock_malloc
    };
    mac_manager_content_ops_t content_ops = {
        .del = mock_mac_del,
        .get = mock_mac_get,
        .random = mock_mac_random,
        .set = mock_mac_set
    };
    
    mac_manager_t *mac_mgr = mac_manager_init(&mem_ops, &content_ops, &config);

    TEST_ASSERT_NOT_NULL(mac_mgr);
    TEST_ASSERT_NOT_EQUAL(&mem_ops, &(mac_mgr->mem_ops));
    TEST_ASSERT_NOT_EQUAL(&content_ops, &(mac_mgr->content_ops));
    TEST_ASSERT_NOT_EQUAL(&config, &(mac_mgr->config));

    mac_manager_deinit(mac_mgr);
}

void test_mac_manager_random_mac_in_init(void)
{
    mac_manager_config_t config = {.random_mac_if_mac_invalid = true};

    mock_mac_get_fake.return_val = -2;

    mac_manager_t *mac_mgr = custom_mac_manager_init(&config);

    TEST_ASSERT_EQUAL(1, mock_mac_random_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_mac_set_fake.call_count);
    
    mac_manager_deinit(mac_mgr);
}

void test_mac_manager_init_with_wrong_params(void) {
    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};
    mac_manager_t *null_mgr = NULL;
    mac_manager_mem_ops_t* mem_ops = mock_mem_ops_get();
    mac_manager_content_ops_t* content_ops = mock_content_get_ops();
    
    null_mgr = mac_manager_init(NULL, content_ops, &config);
    TEST_ASSERT_NULL(null_mgr);
    
    null_mgr = mac_manager_init(mem_ops, NULL, &config);
    TEST_ASSERT_NULL(null_mgr);

    null_mgr = mac_manager_init(mem_ops, content_ops, NULL);
    TEST_ASSERT_NULL(null_mgr);

    mem_ops->malloc = NULL;
    null_mgr = mac_manager_init(mem_ops, content_ops, &config);
    TEST_ASSERT_NULL(null_mgr);

    mem_ops->malloc = mock_malloc;
    mem_ops->free = NULL;
    null_mgr = mac_manager_init(mem_ops, content_ops, &config);
    TEST_ASSERT_NULL(null_mgr);

    mem_ops->malloc = mock_malloc;
    mem_ops->free = mock_free;
    content_ops->random = NULL;
    null_mgr = mac_manager_init(mem_ops, content_ops, &config);
    TEST_ASSERT_NULL(null_mgr);
}

// 测试MAC地址获取功能
void test_mac_manager_get(void) {
    uint8_t mac[6] = {0};

    TEST_ASSERT_NOT_NULL(mgr);
    
    // 当没有设置MAC时，获取应该失败
    int ret = mac_manager_get(mgr, mac, 6);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    // 设置有效MAC
    mock_mac_set((uint8_t[]){0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD}, 6);
    
    // 获取MAC地址
    ret = mac_manager_get(mgr, mac, 6);
    TEST_ASSERT_EQUAL(0, ret);
    
    // 验证MAC内容
    uint8_t expected_mac[6] = {0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD};
    TEST_ASSERT_EQUAL_MEMORY(expected_mac, mac, 6);
    
    // 测试参数错误
    ret = mac_manager_get(NULL, mac, 6);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    ret = mac_manager_get(mgr, NULL, 6);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    ret = mac_manager_get(mgr, mac, 5); // 错误的长度
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void test_mac_manager_get_generate_random_mac(void) {
    uint8_t mac[6] = {0};
    mac_manager_config_t config = {.random_mac_if_mac_invalid = true};

    mac_manager_deinit(mgr);
    mgr = custom_mac_manager_init(&config);

    mock_content_ops_reset();
    
    mock_mac_set((uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6);
    mock_mac_set_fake.call_count = 0;

    int ret = mac_manager_get(mgr, mac, 6);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, mock_mac_random_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_mac_set_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_mac_get_fake.call_count);

}

void test_mac_validation(void) {
    uint8_t mac[6] = {0};
    
    // 测试全0的MAC
    mock_mac_set((uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6);
    int ret = mac_manager_get(mgr, mac, 6);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    // 测试全F的MAC
    mock_mac_set((uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
    ret = mac_manager_get(mgr, mac, 6);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    // 测试多播MAC
    mock_mac_set((uint8_t[]){0x01, 0x48, 0xAA, 0xBB, 0xCC, 0xDD}, 6);
    ret = mac_manager_get(mgr, mac, 6);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    // 测试有效MAC格式
    mock_mac_set((uint8_t[]){0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD}, 6);
    ret = mac_manager_get(mgr, mac, 6);
    TEST_ASSERT_EQUAL(0, ret);
}


int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_mac_manager_lisa_kv_impl);
    RUN_TEST(test_mac_manager_init_with_copy_params);
    RUN_TEST(test_mac_manager_random_mac_in_init);
    RUN_TEST(test_mac_manager_init_with_wrong_params);
    RUN_TEST(test_mac_manager_get);
    RUN_TEST(test_mac_manager_get_generate_random_mac);
    RUN_TEST(test_mac_validation);
    
    return UNITY_END();
}

