#include "lisa_kv.h"
#include "mac_manager.h"
#include "lisa_log.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdlib.h>

#define TAG "port_content_lisa_kv"

#define ARCS_MAC_HEADER_0 0x26
#define ARCS_MAC_HEADER_1 0x48

#define MAC_KEY "mac_u8_arr"

static int lisa_kv_ops_set(const uint8_t *mac, size_t mac_len)
{

    return lisa_kv_set_blob(MAC_KEY, (uint8_t *)mac, mac_len);
}

static int lisa_kv_ops_get(uint8_t *mac_buf, size_t *mac_buf_len)
{
    uint8_t *value = NULL;
    size_t buf_len = *mac_buf_len;

    if (!mac_buf || !mac_buf_len) {
        return -1;
    }

    int ret = lisa_kv_get_blob(MAC_KEY, &value, (int *)mac_buf_len);
    if (ret != 0) {
        return ret;
    }
    if (*mac_buf_len == 0) {
        return -1;
    }
    if (*mac_buf_len > buf_len) {
        return -1;
    }
    memset(mac_buf, 0, *mac_buf_len);
    memcpy(mac_buf, value, *mac_buf_len);
    lisa_kv_free(value);
    return 0;
}

static int lisa_kv_ops_del(void)
{
    return lisa_kv_del(MAC_KEY);
}

static int lisa_kv_ops_random(uint8_t *mac, size_t *mac_len)
{
    unsigned int curTickCount = (unsigned int)xTaskGetTickCount();

    LISA_LOGI(TAG, "lisa_kv_ops_random curTickCount:%d\n", curTickCount);
    srand(curTickCount);

    mac[0] = ARCS_MAC_HEADER_0;
    mac[1] = ARCS_MAC_HEADER_1;

    for (int i = 2; i < 6; i++) {
        do {
            mac[i] = (uint8_t)rand() & 0xFF;
        } while (mac[i] == 0x00 || mac[i] == 0xFF);
    }

    return 0;
}

mac_manager_content_ops_t mac_manager_content_ops_lisa_kv = {
    .set = lisa_kv_ops_set,
    .get = lisa_kv_ops_get,
    .del = lisa_kv_ops_del,
    .random = lisa_kv_ops_random
};

