/**
 ****************************************************************************************
 *
 * @file nv_config_unit_test.c
 *
 * @brief Unit test for NV efuse configuration functions
 *
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */

#ifdef CFG_NV_EFUSE_UNIT_TEST

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nv_config.h"

/*
 * DEFINES
 ****************************************************************************************
 */
static uint32_t efuse_test_space[128] = {0};

typedef struct {
    ls_nv_fixzone_header_t hdr;
    uint8_t body[NV_FIXZONE_MAX_DATA_LEN];
} nv_fixzone_test_image_t;

/* Test mock functions for efuse read/write */
int test_efuse_read_word(uint8_t addr, uint32_t *val) {
    if (addr < 128) {
        *val = efuse_test_space[addr];
        return 0;
    }
    return -1;
}

int test_efuse_write_word(uint32_t addr, uint32_t val) {
    if (addr < 128) {
        efuse_test_space[addr] = val;
        return 0;
    }
    return -1;
}

/*
 * EXTERNAL VARIABLES
 ****************************************************************************************
 */
extern ls_nv_fixzone_efuse_t nv_efuse_cfg_env;
extern const efuse_cfg_t mfg_efuse_cfg_tb[];
extern int8_t ls_nv_fixzone_valid_flag;
extern uint32_t wf_conf_base_addr;

/*
 * EXTERNAL FUNCTIONS
 ****************************************************************************************
 */
extern uint32_t crc32(uint32_t val, const uint8_t *buf, size_t len);
extern int8_t nv_efuse_read_mac(uint8_t *mac_addr);
extern int8_t nv_efuse_burn_mac(void);
extern int8_t nv_efuse_read_wf_ppa_cap(uint8_t *cap);
extern int8_t nv_efuse_read_wf_power_offset(int8_t *offset);
extern int8_t nv_efuse_read_wf_rssi_offset(int8_t *offset);
extern int8_t nv_efuse_read_bt_power_offset(int8_t *offset);
extern int8_t nv_efuse_read_xo24m_cap(int8_t *cap);
extern int8_t nv_efuse_burn_common_item(void *env_field, const efuse_cfg_t *cfg, char *fn);
extern int8_t nv_fixzone_head_check(uint32_t base_addr, uint32_t magic_code);
extern int8_t nv_fixzone_init(void);
extern int8_t nv_fixzone_get_wf_mac(uint8_t *mac_addr);
extern int8_t nv_fixzone_get_bt_mac(uint8_t *mac_addr);

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
static void print_mac(const char *prefix, const uint8_t *mac) {
    printf("%s %02X:%02X:%02X:%02X:%02X:%02X\n", prefix, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void print_test_result(const char *name, bool pass) {
    printf("%s %s\n", name, pass ? "PASS" : "FAIL");
}

static void reset_test_env(void) {
    memset(efuse_test_space, 0, sizeof(efuse_test_space));
    memset(&nv_efuse_cfg_env, 0, sizeof(nv_efuse_cfg_env));
    ls_nv_fixzone_valid_flag = false;
    wf_conf_base_addr = 0;
}

static void init_fixzone_test_image(nv_fixzone_test_image_t *image) {
    memset(image, 0, sizeof(*image));
    image->hdr.magic = NV_MAGIC_PATTERN;
    image->hdr.version = NV_FIXZONE_VER;
    image->hdr.length = 0;
}

static bool append_fixzone_test_tlv(nv_fixzone_test_image_t *image, uint16_t tag, const void *payload, uint16_t len) {
    ls_nv_fixzone_tlv_hdr_t tlv_hdr = {0};
    uint32_t offset = 0;

    if (!image)
        return false;

    offset = image->hdr.length;
    if ((offset + sizeof(tlv_hdr) + len) > sizeof(image->body))
        return false;

    tlv_hdr.tag = tag;
    tlv_hdr.len = len;
    memcpy(image->body + offset, &tlv_hdr, sizeof(tlv_hdr));
    offset += sizeof(tlv_hdr);
    if (len && payload)
        memcpy(image->body + offset, payload, len);
    image->hdr.length += sizeof(tlv_hdr) + len;
    return true;
}

static void update_fixzone_test_crc(nv_fixzone_test_image_t *image) {
    image->hdr.crc32 = 0;
    image->hdr.crc32 = crc32(image->hdr.crc32, (const uint8_t *)&image->hdr,
        sizeof(image->hdr) - sizeof(image->hdr.crc32));
    image->hdr.crc32 = crc32(image->hdr.crc32, image->body, image->hdr.length);
}

/*
 * TEST FUNCTIONS
 ****************************************************************************************
 */
void test_nv_efuse_mac_rw() {
    uint8_t mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint8_t read_mac[6] = {0};

    memcpy(&nv_efuse_cfg_env.mac, mac, 6);
    // 测试写入
    int ret = nv_efuse_burn_mac();
    printf("burn_mac ret=%d\n", ret);
    // 测试读取
    memset(read_mac, 0, 6);
    ret = nv_efuse_read_mac(read_mac);
    printf("read_mac ret=%d\n", ret);
    print_mac("read_mac:", read_mac);
    // 对比结果
    if (memcmp(mac, read_mac, 6) == 0) {
        printf("MAC read/write test PASS\n");
    } else {
        printf("MAC read/write test FAIL\n");
    }
}

void test_nv_efuse_wf_ppa_cap_rw() {
    uint8_t cap_write[WF_PPA_CAP_DIM] = {0x1f, 0x11, 0x5};
    uint8_t cap_read[WF_PPA_CAP_DIM] = {0};

    nv_efuse_burn_common_item(cap_write, &mfg_efuse_cfg_tb[EFUSE_ITEM_WF_PPA_CAP], "wf_ppa_cap");
    int ret = nv_efuse_read_wf_ppa_cap(cap_read);
    printf("wf_ppa_cap read ret=%d, data=", ret);
    for (int i = 0; i < WF_PPA_CAP_DIM; i++) printf("%02X ", cap_read[i]);
    printf("\n");
    if (memcmp(cap_write, cap_read, WF_PPA_CAP_DIM) == 0) {
        printf("wf_ppa_cap read/write test PASS\n");
    } else {
        printf("wf_ppa_cap read/write test FAIL\n");
    }
}

void test_nv_efuse_wf_power_offset_rw() {
    int8_t offset_write[WF_POWER_OFFSET_DIM] = {-8, 7, 3};
    int8_t offset_read[WF_POWER_OFFSET_DIM] = {0};

    nv_efuse_burn_common_item(offset_write, &mfg_efuse_cfg_tb[EFUSE_ITEM_WF_POWER_OFFSET], "wf_power_offset");
    int ret = nv_efuse_read_wf_power_offset(offset_read);
    printf("wf_power_offset read ret=%d, data=", ret);
    for (int i = 0; i < WF_POWER_OFFSET_DIM; i++) printf("%d ", offset_read[i]);
    printf("\n");
    if (memcmp(offset_write, offset_read, WF_POWER_OFFSET_DIM) == 0) {
        printf("wf_power_offset read/write test PASS\n");
    } else {
        printf("wf_power_offset read/write test FAIL\n");
    }
}

void test_nv_efuse_wf_rssi_offset_rw() {
    int8_t offset_write[WF_RSSI_OFFSET_DIM] = {-5, 4};
    int8_t offset_read[WF_RSSI_OFFSET_DIM] = {0};

    nv_efuse_burn_common_item(offset_write, &mfg_efuse_cfg_tb[EFUSE_ITEM_WF_RSSI_OFFSET], "wf_rssi_offset");
    int ret = nv_efuse_read_wf_rssi_offset(offset_read);
    printf("wf_rssi_offset read ret=%d, data=", ret);
    for (int i = 0; i < WF_RSSI_OFFSET_DIM; i++) printf("%d ", offset_read[i]);
    printf("\n");
    if (memcmp(offset_write, offset_read, WF_RSSI_OFFSET_DIM) == 0) {
        printf("wf_rssi_offset read/write test PASS\n");
    } else {
        printf("wf_rssi_offset read/write test FAIL\n");
    }
}

void test_nv_efuse_bt_power_offset_rw() {
    int8_t offset_write[BT_POWER_OFFSET_DIM] = {1, -9};
    int8_t offset_read[BT_POWER_OFFSET_DIM] = {0};

    nv_efuse_burn_common_item(offset_write, &mfg_efuse_cfg_tb[EFUSE_ITEM_BT_POWER_OFFSET], "bt_power_offset");
    int ret = nv_efuse_read_bt_power_offset(offset_read);
    printf("bt_power_offset read ret=%d, data=", ret);
    for (int i = 0; i < BT_POWER_OFFSET_DIM; i++) printf("%d ", offset_read[i]);
    printf("\n");
    if (memcmp(offset_write, offset_read, BT_POWER_OFFSET_DIM) == 0) {
        printf("bt_power_offset read/write test PASS\n");
    } else {
        printf("bt_power_offset read/write test FAIL\n");
    }
}

void test_nv_efuse_xo24m_cap_rw() {
    int8_t cap_write[XO24M_CAP_DIM] = {-3};
    int8_t cap_read[XO24M_CAP_DIM] = {0};

    nv_efuse_burn_common_item(cap_write, &mfg_efuse_cfg_tb[EFUSE_ITEM_XO24M_CAP], "xo24m_cap");
    int ret = nv_efuse_read_xo24m_cap(cap_read);
    printf("xo24m_cap read ret=%d, data=", ret);
    for (int i = 0; i < XO24M_CAP_DIM; i++) printf("%d ", cap_read[i]);
    printf("\n");
    if (memcmp(cap_write, cap_read, XO24M_CAP_DIM) == 0) {
        printf("xo24m_cap read/write test PASS\n");
    } else {
        printf("xo24m_cap read/write test FAIL\n");
    }
}

void test_nv_fixzone_head_check_tlv_guard() {
    nv_fixzone_test_image_t image = {0};
    uint8_t wf_mac[6] = {0x10, 0x22, 0x33, 0x44, 0x55, 0x66};
    int ret = 0;
    bool current_ok = false;
    bool legacy_ver_reject = false;
    bool zero_len_reject = false;
    bool oversize_reject = false;

    reset_test_env();
    init_fixzone_test_image(&image);
    append_fixzone_test_tlv(&image, NV_FIXZONE_TAG_WF_MAC, wf_mac, sizeof(wf_mac));
    update_fixzone_test_crc(&image);

    ret = nv_fixzone_head_check((uint32_t)(uintptr_t)&image.hdr, NV_MAGIC_PATTERN);
    current_ok = (ret == 0);

    image.hdr.version = NV_FIXZONE_VER - 1;
    update_fixzone_test_crc(&image);
    ret = nv_fixzone_head_check((uint32_t)(uintptr_t)&image.hdr, NV_MAGIC_PATTERN);
    legacy_ver_reject = (ret == -4);

    image.hdr.version = NV_FIXZONE_VER;
    image.hdr.length = 0;
    image.hdr.crc32 = 0;
    ret = nv_fixzone_head_check((uint32_t)(uintptr_t)&image.hdr, NV_MAGIC_PATTERN);
    zero_len_reject = (ret == -5);

    image.hdr.length = NV_FIXZONE_MAX_DATA_LEN + 1;
    image.hdr.crc32 = 0;
    ret = nv_fixzone_head_check((uint32_t)(uintptr_t)&image.hdr, NV_MAGIC_PATTERN);
    oversize_reject = (ret == -5);

    print_test_result("fixzone tlv head check test", current_ok && legacy_ver_reject && zero_len_reject && oversize_reject);
}

void test_nv_fixzone_tlv_unknown_and_duplicate_tags() {
    nv_fixzone_test_image_t image = {0};
    uint8_t wf_mac_first[6] = {0x10, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t wf_mac_last[6] = {0x21, 0x32, 0x43, 0x54, 0x65, 0x76};
    uint8_t unknown_payload[5] = {1, 2, 3, 4, 5};
    uint8_t bt_efuse_mac[6] = {0x20, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t read_mac[6] = {0};
    uint8_t bt_expect[6] = {0};
    bool fixzone_valid_ok = false;
    bool wf_duplicate_ok = false;
    bool bt_fallback_ok = false;

    reset_test_env();
    init_fixzone_test_image(&image);
    append_fixzone_test_tlv(&image, NV_FIXZONE_TAG_WF_MAC, wf_mac_first, sizeof(wf_mac_first));
    append_fixzone_test_tlv(&image, 0x7777, unknown_payload, sizeof(unknown_payload));
    append_fixzone_test_tlv(&image, NV_FIXZONE_TAG_WF_MAC, wf_mac_last, sizeof(wf_mac_last));
    memcpy(&nv_efuse_cfg_env.mac, bt_efuse_mac, sizeof(bt_efuse_mac));
    nv_efuse_burn_mac();
    update_fixzone_test_crc(&image);

    wf_conf_base_addr = (uint32_t)(uintptr_t)&image.hdr;
    nv_fixzone_init();
    fixzone_valid_ok = (ls_nv_fixzone_valid_flag == true);

    if (!nv_fixzone_get_wf_mac(read_mac) && !memcmp(read_mac, wf_mac_last, sizeof(wf_mac_last)))
        wf_duplicate_ok = true;

    memcpy(bt_expect, bt_efuse_mac, sizeof(bt_expect));
    bt_expect[5] = bt_expect[5] + 1;
    memset(read_mac, 0, sizeof(read_mac));
    if (!nv_fixzone_get_bt_mac(read_mac) && !memcmp(read_mac, bt_expect, sizeof(bt_expect)))
        bt_fallback_ok = true;

    print_test_result("fixzone tlv unknown/duplicate test", fixzone_valid_ok && wf_duplicate_ok && bt_fallback_ok);
}

void test_nv_fixzone_tlv_bad_len_skip_item() {
    nv_fixzone_test_image_t image = {0};
    uint8_t wf_mac_bad[5] = {0x10, 0x22, 0x33, 0x44, 0x55};
    uint8_t bt_mac[6] = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66};
    uint8_t read_mac[6] = {0};
    bool wf_skip_ok = false;
    bool bt_ok = false;

    reset_test_env();
    init_fixzone_test_image(&image);
    append_fixzone_test_tlv(&image, NV_FIXZONE_TAG_WF_MAC, wf_mac_bad, sizeof(wf_mac_bad));
    append_fixzone_test_tlv(&image, NV_FIXZONE_TAG_BT_MAC, bt_mac, sizeof(bt_mac));
    update_fixzone_test_crc(&image);

    wf_conf_base_addr = (uint32_t)(uintptr_t)&image.hdr;
    nv_fixzone_init();
    wf_skip_ok = (nv_fixzone_get_wf_mac(read_mac) == -1);
    memset(read_mac, 0, sizeof(read_mac));
    if (!nv_fixzone_get_bt_mac(read_mac) && !memcmp(read_mac, bt_mac, sizeof(bt_mac)))
        bt_ok = true;

    print_test_result("fixzone tlv bad len skip test", wf_skip_ok && bt_ok);
}

void test_nv_fixzone_tlv_truncated_image() {
    nv_fixzone_test_image_t image = {0};
    uint8_t wf_mac[6] = {0x30, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t read_mac[6] = {0};
    bool invalid_ok = false;
    bool getter_fail_ok = false;

    reset_test_env();
    init_fixzone_test_image(&image);
    append_fixzone_test_tlv(&image, NV_FIXZONE_TAG_WF_MAC, wf_mac, sizeof(wf_mac));
    image.hdr.length -= 1;
    update_fixzone_test_crc(&image);

    wf_conf_base_addr = (uint32_t)(uintptr_t)&image.hdr;
    nv_fixzone_init();
    invalid_ok = (ls_nv_fixzone_valid_flag == false);
    getter_fail_ok = (nv_fixzone_get_wf_mac(read_mac) == -1);

    print_test_result("fixzone tlv truncated image test", invalid_ok && getter_fail_ok);
}

#endif /* CFG_NV_EFUSE_UNIT_TEST */
