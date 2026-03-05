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

// 测试 mac_manager_set() 函数 - 设置有效的MAC地址
void test_mac_manager_set_valid_mac_succeeds(void) {
    uint8_t valid_mac[6] = {0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD};

    mock_content_ops_reset();

    int ret = mac_manager_set(mgr, valid_mac, 6);

    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Setting valid MAC should succeed");
    TEST_ASSERT_EQUAL_MESSAGE(1, mock_mac_set_fake.call_count,
        "content_ops.set should be called once");
}

// 测试 mac_manager_set() - 拒绝全0的MAC地址
void test_mac_manager_set_rejects_all_zeros_mac(void) {
    uint8_t zero_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    mock_content_ops_reset();

    int ret = mac_manager_set(mgr, zero_mac, 6);

    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "Setting all-zeros MAC should fail");
    TEST_ASSERT_EQUAL_MESSAGE(0, mock_mac_set_fake.call_count,
        "content_ops.set should not be called for invalid MAC");
}

// 测试 mac_manager_set() - 拒绝全F的MAC地址
void test_mac_manager_set_rejects_all_ff_mac(void) {
    uint8_t ff_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    mock_content_ops_reset();

    int ret = mac_manager_set(mgr, ff_mac, 6);

    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "Setting all-FF MAC should fail");
    TEST_ASSERT_EQUAL_MESSAGE(0, mock_mac_set_fake.call_count,
        "content_ops.set should not be called for invalid MAC");
}

// 测试 mac_manager_set() - 拒绝多播地址（第一字节LSB为1）
void test_mac_manager_set_rejects_multicast_address(void) {
    // 测试不同的多播地址模式
    uint8_t multicast_macs[][6] = {
        {0x01, 0x48, 0xAA, 0xBB, 0xCC, 0xDD},  // 0x01 - LSB为1
        {0x03, 0x48, 0xAA, 0xBB, 0xCC, 0xDD},  // 0x03 - LSB为1
        {0x05, 0x48, 0xAA, 0xBB, 0xCC, 0xDD},  // 0x05 - LSB为1
        {0xFF, 0x48, 0xAA, 0xBB, 0xCC, 0xDD},  // 0xFF - LSB为1
    };

    for (int i = 0; i < 4; i++) {
        mock_content_ops_reset();

        int ret = mac_manager_set(mgr, multicast_macs[i], 6);

        TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "Setting multicast MAC should fail");
        TEST_ASSERT_EQUAL_MESSAGE(0, mock_mac_set_fake.call_count,
            "content_ops.set should not be called for multicast MAC");
    }
}

// 测试 mac_manager_set() - NULL参数校验
void test_mac_manager_set_with_null_params(void) {
    uint8_t valid_mac[6] = {0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD};

    // 测试 NULL manager对象
    int ret = mac_manager_set(NULL, valid_mac, 6);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "NULL manager should fail");

    // 测试 NULL MAC地址
    ret = mac_manager_set(mgr, NULL, 6);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "NULL MAC address should fail");
}

// 测试 mac_manager_set() - 错误的MAC长度
void test_mac_manager_set_with_invalid_length(void) {
    uint8_t valid_mac[6] = {0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD};

    mock_content_ops_reset();

    // 测试长度不是6的情况
    int ret = mac_manager_set(mgr, valid_mac, 5);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "MAC length != 6 should fail");

    ret = mac_manager_set(mgr, valid_mac, 7);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "MAC length != 6 should fail");

    TEST_ASSERT_EQUAL_MESSAGE(0, mock_mac_set_fake.call_count,
        "content_ops.set should not be called for invalid length");
}

// 测试 mac_manager_set() - 验证边界值
void test_mac_manager_set_with_boundary_values(void) {
    // 测试包含边界值但有效的MAC
    uint8_t boundary_macs[][6] = {
        {0x26, 0x48, 0x00, 0x01, 0xFE, 0xFD},  // 包含0x00和0xFE但有效
        {0x00, 0x48, 0xAA, 0xBB, 0xCC, 0xDD},  // 第一字节为0x00（偶数，单播）
        {0x26, 0x48, 0xFF, 0xFF, 0xFF, 0xFE},  // 接近全F但有效
    };

    for (int i = 0; i < 3; i++) {
        mock_content_ops_reset();

        int ret = mac_manager_set(mgr, boundary_macs[i], 6);

        TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Valid boundary MAC should succeed");
        TEST_ASSERT_EQUAL_MESSAGE(1, mock_mac_set_fake.call_count,
            "content_ops.set should be called for valid boundary MAC");
    }
}

// 测试 mac_manager_del() 函数 - 正常删除操作
void test_mac_manager_del_succeeds(void) {
    // 先设置一个MAC地址
    uint8_t valid_mac[6] = {0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD};
    mac_manager_set(mgr, valid_mac, 6);

    mock_content_ops_reset();

    // 删除MAC地址
    int ret = mac_manager_del(mgr);

    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Deleting MAC should succeed");
    TEST_ASSERT_EQUAL_MESSAGE(1, mock_mac_del_fake.call_count,
        "content_ops.del should be called once");
}

// 测试 mac_manager_del() - NULL参数校验
void test_mac_manager_del_with_null_params(void) {
    int ret = mac_manager_del(NULL);

    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret, "Deleting with NULL manager should fail");
}

// 测试 mac_manager_del() - 删除后get应该失败
void test_mac_manager_del_then_get_fails(void) {
    uint8_t mac[6] = {0};

    // 先设置一个MAC
    uint8_t valid_mac[6] = {0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD};
    mac_manager_set(mgr, valid_mac, 6);

    // 删除MAC
    int ret = mac_manager_del(mgr);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Delete should succeed");

    // 尝试获取应该失败
    ret = mac_manager_get(mgr, mac, 6);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret,
        "Getting MAC after delete should fail");
}

// 测试 mac_manager_del() - 重复删除
void test_mac_manager_del_multiple_times(void) {
    // 先设置一个MAC
    uint8_t valid_mac[6] = {0x26, 0x48, 0xAA, 0xBB, 0xCC, 0xDD};
    mac_manager_set(mgr, valid_mac, 6);

    // 第一次删除
    int ret = mac_manager_del(mgr);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "First delete should succeed");

    // 第二次删除（没有MAC的情况）
    ret = mac_manager_del(mgr);
    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "Second delete should also succeed");
}

// 测试内存泄漏 - 验证 deinit 正确释放内存
void test_memory_leak_on_deinit(void) {
    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};

    mock_mem_ops_reset();
    mock_content_ops_reset();

    // 初始化 manager
    mac_manager_t *test_mgr = custom_mac_manager_init(&config);

    TEST_ASSERT_NOT_NULL_MESSAGE(test_mgr, "Manager should be initialized");
    TEST_ASSERT_EQUAL_MESSAGE(1, mock_malloc_fake.call_count,
        "malloc should be called once during init");

    // 清理并验证 free 被调用
    mac_manager_deinit(test_mgr);

    TEST_ASSERT_EQUAL_MESSAGE(1, mock_free_fake.call_count,
        "free should be called once during deinit");
}

// 测试内存泄漏 - 多次初始化和清理
void test_memory_leak_multiple_init_deinit_cycles(void) {
    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};

    mock_mem_ops_reset();
    mock_content_ops_reset();

    // 执行多次初始化和清理循环
    for (int i = 0; i < 5; i++) {
        mac_manager_t *test_mgr = custom_mac_manager_init(&config);
        TEST_ASSERT_NOT_NULL_MESSAGE(test_mgr, "Manager should be initialized");
        mac_manager_deinit(test_mgr);
    }

    // 验证 malloc 和 free 调用次数匹配
    TEST_ASSERT_EQUAL_MESSAGE(5, mock_malloc_fake.call_count,
        "malloc should be called 5 times");
    TEST_ASSERT_EQUAL_MESSAGE(5, mock_free_fake.call_count,
        "free should be called 5 times to match malloc");
}

// 测试内存泄漏 - deinit NULL 指针不应该崩溃
void test_deinit_with_null_does_not_crash(void) {
    // 这个测试主要验证不会崩溃
    mac_manager_deinit(NULL);

    // 如果到达这里，说明没有崩溃
    TEST_ASSERT_TRUE_MESSAGE(true, "Deinit with NULL should not crash");
}

// 测试内存泄漏 - deinit 后不应该再能使用
void test_manager_unusable_after_deinit(void) {
    mac_manager_config_t config = {.random_mac_if_mac_invalid = false};
    uint8_t mac[6] = {0};

    mock_mem_ops_reset();
    mock_content_ops_reset();

    mac_manager_t *test_mgr = custom_mac_manager_init(&config);
    TEST_ASSERT_NOT_NULL(test_mgr);

    // deinit 之后
    mac_manager_deinit(test_mgr);

    // 尝试使用应该失败（因为 init_done 被设为 false）
    int ret = mac_manager_get(test_mgr, mac, 6);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret,
        "Operations should fail after deinit");

    ret = mac_manager_set(test_mgr, mac, 6);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret,
        "Set operation should fail after deinit");

    ret = mac_manager_del(test_mgr);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, ret,
        "Del operation should fail after deinit");
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

    // mac_manager_set() 测试
    RUN_TEST(test_mac_manager_set_valid_mac_succeeds);
    RUN_TEST(test_mac_manager_set_rejects_all_zeros_mac);
    RUN_TEST(test_mac_manager_set_rejects_all_ff_mac);
    RUN_TEST(test_mac_manager_set_rejects_multicast_address);
    RUN_TEST(test_mac_manager_set_with_null_params);
    RUN_TEST(test_mac_manager_set_with_invalid_length);
    RUN_TEST(test_mac_manager_set_with_boundary_values);

    // mac_manager_del() 测试
    RUN_TEST(test_mac_manager_del_succeeds);
    RUN_TEST(test_mac_manager_del_with_null_params);
    RUN_TEST(test_mac_manager_del_then_get_fails);
    RUN_TEST(test_mac_manager_del_multiple_times);

    // 内存泄漏测试
    RUN_TEST(test_memory_leak_on_deinit);
    RUN_TEST(test_memory_leak_multiple_init_deinit_cycles);
    RUN_TEST(test_deinit_with_null_does_not_crash);
    RUN_TEST(test_manager_unusable_after_deinit);

    return UNITY_END();
}

