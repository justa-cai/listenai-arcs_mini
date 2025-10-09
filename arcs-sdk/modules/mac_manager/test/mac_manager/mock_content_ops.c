/**
 * @file mock_content_ops.c
 * @brief MAC管理器测试中使用的内容操作实现
 */
#include "mock_content_ops.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define ARCS_MAC_HEADER_0 0x26
#define ARCS_MAC_HEADER_1 0x48

#define UINT8_MAC_LEN 6

int custom_mac_del(void);

DEFINE_FAKE_VALUE_FUNC(int, mock_mac_set, const uint8_t*, size_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_mac_get, uint8_t* , size_t*);
DEFINE_FAKE_VALUE_FUNC(int, mock_mac_del);
DEFINE_FAKE_VALUE_FUNC(int, mock_mac_random, uint8_t*, size_t*);


void mock_content_ops_reset(void)
{
    custom_mac_del();
    mock_content_ops_init();
}

/**
 * @brief 内存模式MAC存储结构
 */
typedef struct {
    char mac_value[18];
    size_t mac_len;
    bool has_value;
} memory_mac_storage_t;


// 内存模式MAC存储
static memory_mac_storage_t mem_storage = {0};


/**
 * @brief 设置MAC地址到内存存储中
 */
int custom_mac_set(const uint8_t* value, size_t value_len) {
    if (!value) {
        return -1;
    }
    
    memcpy(mem_storage.mac_value, value, value_len);
    mem_storage.mac_len = value_len;
    mem_storage.has_value = true;
    
    return mock_mac_set_fake.return_val;
}

/**
 * @brief 从内存存储中获取MAC地址
 */
int custom_mac_get(uint8_t* out_value, size_t* out_value_len) {
    if (!out_value || !out_value_len) {
        return -1;
    }
    
    if (!mem_storage.has_value) {
        return -1;
    }
    
    if (*out_value_len < mem_storage.mac_len) {
        return -2;
    }
    
    memcpy(out_value, mem_storage.mac_value, mem_storage.mac_len);
    *out_value_len = mem_storage.mac_len;
    
    return mock_mac_get_fake.return_val;
}

/**
 * @brief 删除内存存储中的MAC地址
 */
int custom_mac_del(void) {
    memset(mem_storage.mac_value, 0, sizeof(mem_storage.mac_value));
    mem_storage.mac_len = 0;
    mem_storage.has_value = false;
    
    return 0;
}


int custom_mac_random(uint8_t *mac, size_t *mac_len)
{
    if (mac == NULL || mac_len == NULL) {
        return -1;
    }

    // no need to impl real random
    mac[0] = ARCS_MAC_HEADER_0;
    mac[1] = ARCS_MAC_HEADER_1;

    srand(time(NULL));

    for (int i = 2; i < UINT8_MAC_LEN; i++) {
        do {
            mac[i] = (uint8_t)rand() & 0xFF;
        } while (mac[i] == 0x00 || mac[i] == 0xFF);
    }

    *mac_len = UINT8_MAC_LEN;
    return 0;
}

/**
 * @brief 内容操作结构体
 */
static mac_manager_content_ops_t content_ops = {
    .set = mock_mac_set,
    .get = mock_mac_get,
    .del = mock_mac_del,
    .random = mock_mac_random
};

/**
 * @brief 获取预先配置好的内容操作结构体
 */
mac_manager_content_ops_t* mock_content_get_ops(void) {
    return &content_ops;
}



void mock_content_ops_init(void)
{
    RESET_FAKE(mock_mac_set);
    RESET_FAKE(mock_mac_get);
    RESET_FAKE(mock_mac_del);
    RESET_FAKE(mock_mac_random);

    content_ops.set = mock_mac_set;
    content_ops.get = mock_mac_get;
    content_ops.del = mock_mac_del;
    content_ops.random = mock_mac_random;

    mock_mac_set_fake.custom_fake = custom_mac_set;
    mock_mac_get_fake.custom_fake = custom_mac_get;
    mock_mac_del_fake.custom_fake = custom_mac_del;
    mock_mac_random_fake.custom_fake = custom_mac_random;
}

