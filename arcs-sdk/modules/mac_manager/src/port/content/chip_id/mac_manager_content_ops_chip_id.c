#include "Driver_EFUSE.h"
#include "mac_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"

#include <stdlib.h>

#define TAG "mac_chip_id"

#define STRING_MAC_LEN 18
#define UINT8_MAC_LEN  6

#define ARCS_MAC_HEADER_0 0x26
#define ARCS_MAC_HEADER_1 0x48

// lot id
// uuid bit0-bit7
static uint8_t get_lotid(uint64_t uuid)
{
    return (uuid & 0xFF);
}

// wafer id
// (uuid >> 32), then get bit16 - bit23
static uint8_t get_wafer_id(uint64_t uuid)
{
    return (uuid >> 32 >> 16) & 0xFF;
}

// wafer x
// (uuid >> 32), then get bit8 - bit15
static uint8_t get_wafer_x(uint64_t uuid)
{
    return (uuid >> 32 >> 8) & 0xFF;
}

// wafer y
// (uuid >> 32), then get bit0 - bit7
static uint8_t get_wafer_y(uint64_t uuid)
{
    return (uuid >> 32) & 0xFF;
}

static int set(const uint8_t *value, size_t value_len)
{
    return -1;
}

static int get(uint8_t *mac, size_t *mac_len)
{
    char uuid_str[18] = {0};

    if (mac == NULL || mac_len == NULL) {
        return -1;
    }
    if (*mac_len < UINT8_MAC_LEN) {
        return -1;
    }

    uint64_t uuid = efuse_read_uuid();
    mac[0] = ARCS_MAC_HEADER_0;
    mac[1] = ARCS_MAC_HEADER_1;
    mac[2] = get_lotid(uuid);
    mac[3] = get_wafer_id(uuid);
    mac[4] = get_wafer_x(uuid);
    mac[5] = get_wafer_y(uuid);

    *mac_len = UINT8_MAC_LEN;

    return 0;
}

static int del(void)
{
    return -1;
}

static int chip_id_random(uint8_t *mac, size_t *mac_len)
{
    if (mac == NULL || mac_len == NULL) {
        return -1;
    }
    if (*mac_len < UINT8_MAC_LEN) {
        return -1;
    }

    mac[0] = ARCS_MAC_HEADER_0;
    mac[1] = ARCS_MAC_HEADER_1;

    srand((unsigned int)xTaskGetTickCount());
    for (int i = 2; i < 6; i++) {
        do {
            mac[i] = (uint8_t)rand() & 0xFF;
        } while (mac[i] == 0x00 || mac[i] == 0xFF);
    }

    return 0;
}

mac_manager_content_ops_t mac_manager_content_ops_chip_id = {
    .set = set,
    .get = get,
    .del = del,
    .random = chip_id_random
};
