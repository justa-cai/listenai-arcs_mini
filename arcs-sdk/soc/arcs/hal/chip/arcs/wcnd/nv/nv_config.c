/**
 ****************************************************************************************
 *
 * @file nv_config.c
 *
 * @brief NV configuraton functions implement.
 *
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "nvs_priv.h"
#include "arcs_ap.h"
#include "rf_drv.h"
#include "log_print.h"
#include "nv_config.h"
#include "nvs.h"
#include "nvds_tag_def.h"
#include "flash_if.h"
#include "nv_otp.h"

#include "ClockManager.h"
#include "Driver_TRNG.h"
#include "ls_misc.h"
#include "crc32_sw.h"

/*
 * DEFINES
 ****************************************************************************************
 */
/* Unit test mode: use test mock functions */
#ifdef CFG_NV_EFUSE_UNIT_TEST
extern int test_efuse_read_word(uint8_t addr, uint32_t *val);
extern int test_efuse_write_word(uint32_t addr, uint32_t val);
#define EFUSE_RD32 test_efuse_read_word
#define EFUSE_WR32 test_efuse_write_word
#else
#define EFUSE_RD32 ls_efuse_read_word//efuse_read_word
/* TODO use efuse_write_word define in bsp driver later */
//extern int efuse_write_word_simple(uint32_t addr, uint32_t val);
#define EFUSE_WR32 ls_efuse_write_word //efuse_write_word_simple
#endif

#define MAX_SEC_LEN FLASH_OTP_NV_LENGTH
#define WR_SEC_LEN  256
uint8_t nv_self_cali_cfg_buf[MAX_SEC_LEN] = {0};
uint32_t g_magic_code = 0;

/** Golden value define */
int8_t wf_golden_val_set = WIFI_RF_SET_GOLDEN;
int8_t wf_xo_cap_golden_val = XO_CAP_GOLDEN_VAL;
int8_t wf_pwr_offset_low_golden_val = PWR_OFFSET_LOW_GOLDEN_VAL;
int8_t wf_pwr_offset_mid_golden_val = PWR_OFFSET_MID_GOLDEN_VAL;
int8_t wf_pwr_offset_high_golden_val = PWR_OFFSET_HIGH_GOLDEN_VAL;
int8_t wf_rssi_offset_dsss_golden_val = RSSI_OFFSET_DSSS_GOLDEN_VAL;
int8_t wf_rssi_offset_ofdm_golden_val = RSSI_OFFSET_OFDM_GOLDEN_VAL;

/*
 * Factory mfg conf base address in flash.
 * config: mac address, xo_cap, power offset, rssi offset, ppa gain...
 */
uint32_t wf_conf_base_addr = FLASH_WF_MFG_CONF_BASE_ADDR;
/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */
typedef struct {
    bool xo_cap;
    bool wf_ppa_cap;
    bool wf_ppa_gain;
    bool wf_power_offset;
    bool wf_rssi_offset;
    bool wf_target_power;
} nv_fixzone_rf_load_state_t;

typedef enum {
    NV_FIXZONE_PARSE_OK = 0,
    NV_FIXZONE_PARSE_NOT_TRIED = 1,
    NV_FIXZONE_PARSE_NO_BASE_ADDR = 2,
    NV_FIXZONE_PARSE_TLV_FAIL = -6,
} nv_fixzone_parse_result_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
int8_t ls_nv_fixzone_valid_flag = false;
int8_t ls_nv_selfcali_valid_flag = false;
ls_nv_fixzone_efuse_t nv_efuse_cfg_env = {0};

/* Base address configurations (reusable) */
static const uint8_t efuse_slot_addrs[] = {EFUSE_NV_SLOT0_ADDR, EFUSE_NV_SLOT1_ADDR};
static const efuse_base_addrs_t g_efuse_nv_slots = {
    .addr_count = sizeof(efuse_slot_addrs) / sizeof(efuse_slot_addrs[0]),
    .addrs = efuse_slot_addrs
};

/* Main configuration table: combines base addresses pointer and bit field layout
 * order: base_addrs, bits_mask, offset_idx, field_dim, bit_valid, bit_start, bits_width, is_signed
 */
const efuse_cfg_t mfg_efuse_cfg_tb[] = {
    /* WF_PPA_CAP */
    {&g_efuse_nv_slots, 1, WF_PPA_CAP_DIM, WF_PPA_CAP_VALID_BIT_OFFSET, WF_PPA_CAP_0_BIT_OFFSET, WF_PPA_CAP_BITS_MASK, WF_PPA_CAP_BITS_WIDTH, false},
    /* WF_POWER_OFFSET */
    {&g_efuse_nv_slots, 2, WF_POWER_OFFSET_DIM, WF_POWER_OFFSET_VALID_BIT_OFFSET, WF_POWER_OFFSET_0_BIT_OFFSET, WF_POWER_OFFSET_BITS_MASK, WF_POWER_OFFSET_BITS_WIDTH, true},
    /* WF_RSSI_OFFSET */
    {&g_efuse_nv_slots, 2, WF_RSSI_OFFSET_DIM, WF_RSSI_OFFSET_VALID_BIT_OFFSET, WF_RSSI_OFFSET_0_BIT_OFFSET, WF_RSSI_OFFSET_BITS_MASK, WF_RSSI_OFFSET_BITS_WIDTH, true},
    /* BT_POWER_OFFSET */
    {&g_efuse_nv_slots, 3, BT_POWER_OFFSET_DIM, BT_POWER_OFFSET_VALID_BIT_OFFSET, BT_POWER_OFFSET_0_BIT_OFFSET, BT_POWER_OFFSET_BITS_MASK, BT_POWER_OFFSET_BITS_WIDTH, true},
    /* XO24M_CAP */
    {&g_efuse_nv_slots, 3, XO24M_CAP_DIM, XO24M_CAP_VALID_BIT_OFFSET, XO24M_CAP_BIT_OFFSET, XO24M_CAP_BITS_MASK, XO24M_CAP_BITS_WIDTH, true},
};


uint8_t wf_power_offset_en = 0;
int8_t  wf_power_offset_fake_reg[3] = {0};
#ifdef RF_SELF_CALI_FROM_NV
complexint16 nv_tx_pred_table_chan_low[DPD_COMP_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_table_chan_mid[DPD_COMP_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_table_chan_hig[DPD_COMP_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_rest_table_chan_low[DPD_REST_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_rest_table_chan_mid[DPD_REST_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_rest_table_chan_hig[DPD_REST_TABLE_CNT][MAX_PARALEN] = {0};
#endif

#if (defined(RF_SELF_CALI_FROM_NV) || defined(RF_SELF_CALI_WRITE_TO_NV))
#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
#define CMN_FLASH_OTP_REGION 0
uint32_t nv_self_cali_addr = CMN_FLASH_OTP_REGION;
#else
uint32_t nv_self_cali_addr = FLASH_NOR_OTP_NV_BASE_ADDR; // 0x30200000
#endif
FLASH_DEV cali_flash_dev  = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
    .addr_bytes = 3,
    .addr_auto = 0,
};
ls_nv_selfcali_cfg_t nv_selfcali_cfg = {0};
#endif

extern int8_t efuse_write_word(uint8_t addr, uint32_t val);
extern int8_t efuse_read_word(uint8_t addr, uint32_t *val);
extern uint32_t ls_tpc_update_tx_power_table(int8_t *power_table, int8_t chan_type, int8_t update);

static ls_nv_fixzone_body_t s_nv_fixzone_cache = {0};
static ls_nv_fixzone_reg_override_cache_t s_nv_fixzone_reg_override_cache = {0};
static uint32_t s_nv_fixzone_cache_base_addr = 0;
static int8_t s_nv_fixzone_last_parse_result = NV_FIXZONE_PARSE_NOT_TRIED;

#define NV_FIXZONE_DBG_PREFIX "[fixzone_dbg] "

static void nv_fixzone_cache_reset(void)
{
    memset(&s_nv_fixzone_cache, 0, sizeof(s_nv_fixzone_cache));
    memset(&s_nv_fixzone_reg_override_cache, 0, sizeof(s_nv_fixzone_reg_override_cache));
}

static void nv_fixzone_debug_log_header(const char *stage, uint32_t base_addr, const ls_nv_fixzone_header_t *hdr)
{
    if (!hdr)
        return;

    CLOGI(NV_FIXZONE_DBG_PREFIX "%s base=0x%08lx magic=0x%08lx len=%u version=%u crc32=0x%08lx\n",
        stage, base_addr, hdr->magic, hdr->length, hdr->version, hdr->crc32);
}

static void nv_fixzone_debug_log_state(const char *stage, const nv_fixzone_rf_load_state_t *state)
{
    if (!state)
        return;

    CLOGI(NV_FIXZONE_DBG_PREFIX "%s state xo=%u ppa_cap=%u ppa_gain=%u power_offset=%u rssi=%u target_power=%u\n",
        stage,
        state->xo_cap,
        state->wf_ppa_cap,
        state->wf_ppa_gain,
        state->wf_power_offset,
        state->wf_rssi_offset,
        state->wf_target_power);
}

static void nv_fixzone_debug_log_cache_body(const char *stage, const ls_nv_fixzone_body_t *body)
{
    if (!body)
        return;

    CLOGI(NV_FIXZONE_DBG_PREFIX "%s has xo=%u ppa_cap=%u ppa_gain=%u power_offset=%u rssi=%u target_power=%u\n",
        stage,
        body->has_xo_cap,
        body->has_wf_ppa_cap,
        body->has_wf_ppa_gain,
        body->has_wf_power_offset,
        body->has_wf_rssi_offset,
        body->has_wf_target_power);
    if (body->has_xo_cap) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "%s xo_cap=%d\n", stage, body->xo_cap);
    }
    if (body->has_wf_ppa_cap) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "%s wf_ppa_cap=%u/%u/%u\n",
            stage, body->wf_ppa_cap[0], body->wf_ppa_cap[1], body->wf_ppa_cap[2]);
    }
    if (body->has_wf_power_offset) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "%s wf_power_offset=%d/%d/%d\n",
            stage, body->wf_power_offset[0], body->wf_power_offset[1], body->wf_power_offset[2]);
    }
    if (body->has_wf_rssi_offset) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "%s wf_rssi_offset dsss=%d ofdm=%d\n",
            stage, body->wf_rssi_offset.dsss, body->wf_rssi_offset.ofdm);
    }
    if (body->has_wf_ppa_gain) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "%s wf_ppa_gain[0..9]=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
            stage,
            body->wf_ppa_gain[0], body->wf_ppa_gain[1], body->wf_ppa_gain[2], body->wf_ppa_gain[3],
            body->wf_ppa_gain[4], body->wf_ppa_gain[5], body->wf_ppa_gain[6], body->wf_ppa_gain[7],
            body->wf_ppa_gain[8], body->wf_ppa_gain[9]);
        CLOGI(NV_FIXZONE_DBG_PREFIX "%s wf_ppa_gain[10..18]=%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
            stage,
            body->wf_ppa_gain[10], body->wf_ppa_gain[11], body->wf_ppa_gain[12], body->wf_ppa_gain[13],
            body->wf_ppa_gain[14], body->wf_ppa_gain[15], body->wf_ppa_gain[16], body->wf_ppa_gain[17],
            body->wf_ppa_gain[18]);
    }
}

static void nv_fixzone_debug_log_reg_overrides(const char *stage)
{
    uint16_t i = 0;

    if (!s_nv_fixzone_reg_override_cache.count)
        return;

    CLOGI(NV_FIXZONE_DBG_PREFIX "%s reg_override count=%u bytes=%u\n",
        stage,
        s_nv_fixzone_reg_override_cache.count,
        (uint16_t)(s_nv_fixzone_reg_override_cache.count * sizeof(ls_nv_fixzone_reg_override_item_t)));
    for (i = 0; i < s_nv_fixzone_reg_override_cache.count; i++) {
        const ls_nv_fixzone_reg_override_item_t *item = &s_nv_fixzone_reg_override_cache.items[i];

        CLOGI(NV_FIXZONE_DBG_PREFIX "%s reg_override[%u] addr=0x%08lx value=0x%08lx\n",
            stage,
            i,
            item->addr,
            item->value);
    }
}

static void nv_fixzone_debug_log_runtime(const char *stage)
{
    CLOGI(NV_FIXZONE_DBG_PREFIX "%s runtime xo_reg=%d xo_force=%u wf_ppa_cap=%u/%u/%u wf_power_offset_en=%u wf_power_offset_fake=%d/%d/%d tpc_raw=%u rssi=%d/%d\n",
        stage,
        IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG,
        IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC,
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM,
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM,
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM,
        wf_power_offset_en,
        wf_power_offset_fake_reg[0],
        wf_power_offset_fake_reg[1],
        wf_power_offset_fake_reg[2],
        IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET,
        IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_DSSS_OFFSET,
        IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_OFDM_OFFSET);
    CLOGI(NV_FIXZONE_DBG_PREFIX "%s runtime wf_ppa_gain[0..9]=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
        stage,
        ls_rf_get_wf_ppa_gain(0), ls_rf_get_wf_ppa_gain(1), ls_rf_get_wf_ppa_gain(2), ls_rf_get_wf_ppa_gain(3),
        ls_rf_get_wf_ppa_gain(4), ls_rf_get_wf_ppa_gain(5), ls_rf_get_wf_ppa_gain(6), ls_rf_get_wf_ppa_gain(7),
        ls_rf_get_wf_ppa_gain(8), ls_rf_get_wf_ppa_gain(9));
    CLOGI(NV_FIXZONE_DBG_PREFIX "%s runtime wf_ppa_gain[10..18]=%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
        stage,
        ls_rf_get_wf_ppa_gain(10), ls_rf_get_wf_ppa_gain(11), ls_rf_get_wf_ppa_gain(12), ls_rf_get_wf_ppa_gain(13),
        ls_rf_get_wf_ppa_gain(14), ls_rf_get_wf_ppa_gain(15), ls_rf_get_wf_ppa_gain(16), ls_rf_get_wf_ppa_gain(17),
        ls_rf_get_wf_ppa_gain(18));
}

static void nv_fixzone_log_bad_tlv_len(uint16_t tag, uint16_t len, uint16_t expect_len)
{
    CLOGW("NV fix zone TLV tag %u length %u mismatch, expect %u, skip it!\n", tag, len, expect_len);
}

typedef struct {
    uint16_t tag;
    size_t field_offset;
    size_t field_size;
    size_t valid_offset;
} nv_fixzone_tlv_field_desc_t;

#define NV_FIXZONE_TLV_FIELD_DESC(tag_id, member, valid_member) \
    { \
        (tag_id), \
        offsetof(ls_nv_fixzone_body_t, member), \
        sizeof(((ls_nv_fixzone_body_t *)0)->member), \
        offsetof(ls_nv_fixzone_body_t, valid_member), \
    }

static const nv_fixzone_tlv_field_desc_t s_nv_fixzone_tlv_field_descs[] = {
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_CHIP_ID, chip_id, has_chip_id),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_WF_MAC, wf_mac, has_wf_mac),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_XO_CAP, xo_cap, has_xo_cap),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_WF_PPA_CAP, wf_ppa_cap, has_wf_ppa_cap),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_WF_PPA_GAIN, wf_ppa_gain, has_wf_ppa_gain),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_WF_POWER_OFFSET, wf_power_offset, has_wf_power_offset),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_WF_RSSI_OFFSET, wf_rssi_offset, has_wf_rssi_offset),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_WF_TARGET_POWER, wf_target_power, has_wf_target_power),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_BT_MAC, bt_mac, has_bt_mac),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_BT_POWER_OFFSET, bt_power_offset, has_bt_power_offset),
    NV_FIXZONE_TLV_FIELD_DESC(NV_FIXZONE_TAG_BT_TARGET_POWER, bt_target_power, has_bt_target_power),
};

static const nv_fixzone_tlv_field_desc_t *nv_fixzone_find_tlv_field_desc(uint16_t tag)
{
    uint32_t index = 0;

    for (index = 0; index < sizeof(s_nv_fixzone_tlv_field_descs) / sizeof(s_nv_fixzone_tlv_field_descs[0]); index++) {
        if (s_nv_fixzone_tlv_field_descs[index].tag == tag)
            return &s_nv_fixzone_tlv_field_descs[index];
    }

    return NULL;
}

static bool nv_fixzone_is_reg_override_addr_allowed(uint32_t addr)
{
    static const struct {
        uintptr_t base;
        uintptr_t last_word_addr;
    } allowed_ranges[] = {
        {(uintptr_t)CMN_SYS_BASE, (uintptr_t)CMN_SYS_BASE + sizeof(*IP_CMN_SYS) - sizeof(uint32_t)},
        {(uintptr_t)CMN_SYS_NODFT, (uintptr_t)CMN_SYS_NODFT + sizeof(*IP_SYSNODEF) - sizeof(uint32_t)},
        {(uintptr_t)CORE_IOMUX_BASE, (uintptr_t)CORE_IOMUX_BASE + sizeof(*IP_CMN_IOMUX) - sizeof(uint32_t)},
        {(uintptr_t)RF_IF_BASE, (uintptr_t)RF_IF_BASE + sizeof(*IP_RFIF) - sizeof(uint32_t)},
        {(uintptr_t)AON_CTRL_BASE, (uintptr_t)AON_CTRL_BASE + sizeof(*IP_AON_CTRL) - sizeof(uint32_t)},
    };
    uintptr_t raw_addr = (uintptr_t)addr;
    uint32_t index = 0;

    for (index = 0; index < sizeof(allowed_ranges) / sizeof(allowed_ranges[0]); index++) {
        if (raw_addr >= allowed_ranges[index].base && raw_addr <= allowed_ranges[index].last_word_addr)
            return true;
    }

    return false;
}

static void nv_fixzone_collect_reg_override_tlv(const uint8_t *payload, uint16_t len)
{
    ls_nv_fixzone_reg_override_item_t *item = NULL;
    uint32_t addr = 0;
    uint32_t value = 0;

    if (!payload)
        return;

    if (len != sizeof(addr) + sizeof(value)) {
        CLOGW("NV fix zone reg override TLV length %u invalid, expect %u, skip it!\n",
            len, (uint16_t)(sizeof(addr) + sizeof(value)));
        return;
    }
    if (s_nv_fixzone_reg_override_cache.count >= NV_FIXZONE_REG_OVERRIDE_MAX_CNT) {
        CLOGW("NV fix zone reg override count exceeds %u, skip remaining item!\n",
            NV_FIXZONE_REG_OVERRIDE_MAX_CNT);
        return;
    }
    memcpy(&addr, payload, sizeof(addr));
    memcpy(&value, payload + sizeof(addr), sizeof(value));
    if (addr & 0x3) {
        CLOGW("NV fix zone reg override addr 0x%08lx is not 4-byte aligned, skip it!\n", addr);
        return;
    }
    if (!nv_fixzone_is_reg_override_addr_allowed(addr)) {
        CLOGW("NV fix zone reg override addr 0x%08lx is outside allowed ranges, skip it!\n", addr);
        return;
    }
    item = &s_nv_fixzone_reg_override_cache.items[s_nv_fixzone_reg_override_cache.count];
    item->addr = addr;
    item->value = value;
    s_nv_fixzone_reg_override_cache.count++;
}

static void nv_fixzone_apply_tlv_item(ls_nv_fixzone_body_t *body, uint16_t tag, const uint8_t *payload, uint16_t len)
{
    const nv_fixzone_tlv_field_desc_t *desc = NULL;

    if (!body || !payload)
        return;

    desc = nv_fixzone_find_tlv_field_desc(tag);
    if (!desc) {
        CLOGW("NV fix zone TLV tag %u unknown, skip it!\n", tag);
        return;
    }

    if (len != desc->field_size) {
        nv_fixzone_log_bad_tlv_len(tag, len, (uint16_t)desc->field_size);
        return;
    }

    memcpy((uint8_t *)body + desc->field_offset, payload, len);
    *((uint8_t *)body + desc->valid_offset) = true;
}

static int8_t nv_fixzone_parse_tlv_stream(const uint8_t *data, uint16_t data_len, ls_nv_fixzone_body_t *body)
{
    uint16_t offset = 0;

    if (!data || !body)
        return -1;

    memset(body, 0, sizeof(*body));

    while (offset < data_len) {
        ls_nv_fixzone_tlv_hdr_t tlv_hdr = {0};
        uint16_t remain = data_len - offset;

        if (remain < sizeof(tlv_hdr)) {
            CLOGW("NV fix zone TLV truncated header, remain %u!\n", remain);
            return -1;
        }

        memcpy(&tlv_hdr, data + offset, sizeof(tlv_hdr));
        offset += sizeof(tlv_hdr);
        remain = data_len - offset;
        if (tlv_hdr.len > remain) {
            CLOGW("NV fix zone TLV tag %u truncated payload len %u remain %u!\n",
                tlv_hdr.tag, tlv_hdr.len, remain);
            return -1;
        }

        CLOGI(NV_FIXZONE_DBG_PREFIX "parse tlv tag=%u len=%u payload_offset=%u\n",
            tlv_hdr.tag, tlv_hdr.len, offset);
        if (tlv_hdr.tag == NV_FIXZONE_TAG_REG_OVERRIDE) {
            nv_fixzone_collect_reg_override_tlv(data + offset, tlv_hdr.len);
            offset += tlv_hdr.len;
            continue;
        }
        nv_fixzone_apply_tlv_item(body, tlv_hdr.tag, data + offset, tlv_hdr.len);
        offset += tlv_hdr.len;
    }

    return 0;
}

static void nv_fixzone_commit_parse_state(int8_t parse_result)
{
    ls_nv_fixzone_valid_flag = (parse_result == NV_FIXZONE_PARSE_OK);
    s_nv_fixzone_last_parse_result = parse_result;
    s_nv_fixzone_cache_base_addr = wf_conf_base_addr;
}

static void nv_fixzone_ensure_cache(void)
{
    if (s_nv_fixzone_last_parse_result == NV_FIXZONE_PARSE_NOT_TRIED ||
        s_nv_fixzone_cache_base_addr != wf_conf_base_addr) {
        #ifndef WIFI_RAM_ATE
        nv_fixzone_init();
        #endif
    }
}

static void nv_fixzone_apply_xo_cap(int8_t xo_cap)
{
    IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = xo_cap;
    IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC = 1;
}

static void nv_fixzone_apply_wf_ppa_cap(const uint8_t *ppa_cap)
{
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM = ppa_cap[0];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_DSSS = ppa_cap[0];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM = ppa_cap[1];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_DSSS = ppa_cap[1];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM = ppa_cap[2];
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_WF_2_DSSS = ppa_cap[2];
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_0 = ppa_cap[0];
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_1 = ppa_cap[1];
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_2 = ppa_cap[2];
}

static void nv_fixzone_apply_wf_ppa_gain(const uint8_t *ppa_gain)
{
    uint8_t i = 0;

    for (i = 0; i < NV_FIXZONE_WF_PPA_GAIN_DIM; i++)
        ls_rf_set_wf_ppa_gain(i, ppa_gain[i]);
}

static void nv_fixzone_apply_wf_power_offset(const int8_t *power_offset)
{
    uint8_t i = 0;

    for (i = 0; i < 3; i++)
        wf_power_offset_fake_reg[i] = power_offset[i];
    wf_power_offset_en = 1;
}

static void nv_fixzone_apply_wf_rssi_offset(int16_t dsss, int16_t ofdm)
{
    IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_DSSS_OFFSET = dsss;
    IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_OFDM_OFFSET = ofdm;
}

static void nv_fixzone_apply_wf_target_power(const ls_nv_fixzone_wf_target_power_t *target_power)
{
    pwr_table_t power_table[3] = {0};
    uint8_t i = 0;

    memcpy(power_table, target_power->value, sizeof(power_table));
    if (target_power->channel_ind == 0) {
        ls_tpc_update_tx_power_table((int8_t *)&power_table[0], target_power->channel_ind, 1);
        return;
    }

    for (i = 0; i < 3; i++)
        ls_tpc_update_tx_power_table((int8_t *)&power_table[i], i + 1, 1);
}

static void nv_fixzone_apply_reg_overrides(void)
{
    uint16_t index = 0;

    for (index = 0; index < s_nv_fixzone_reg_override_cache.count; index++) {
        const ls_nv_fixzone_reg_override_item_t *item = &s_nv_fixzone_reg_override_cache.items[index];
        volatile uint32_t *reg = (volatile uint32_t *)(uintptr_t)item->addr;

        if (!nv_fixzone_is_reg_override_addr_allowed(item->addr)) {
            CLOGW("NV fix zone reg override[%u] addr 0x%08lx is outside allowed ranges, skip apply!\n",
                index, item->addr);
            continue;
        }
        CLOGI(NV_FIXZONE_DBG_PREFIX "apply reg_override[%u] addr=0x%08lx value=0x%08lx\n",
            index, item->addr, item->value);
        *reg = item->value;
    }
}

static bool nv_fixzone_try_set_xo_cap(nv_fixzone_rf_load_state_t *state, int8_t xo_cap)
{
    if (!state || state->xo_cap)
        return false;

    nv_fixzone_apply_xo_cap(xo_cap);
    state->xo_cap = true;
    return true;
}

static bool nv_fixzone_try_set_wf_ppa_cap(nv_fixzone_rf_load_state_t *state, const uint8_t *wf_ppa_cap)
{
    if (!state || !wf_ppa_cap || state->wf_ppa_cap)
        return false;

    nv_fixzone_apply_wf_ppa_cap(wf_ppa_cap);
    state->wf_ppa_cap = true;
    return true;
}

static bool nv_fixzone_try_set_wf_ppa_gain(nv_fixzone_rf_load_state_t *state, const uint8_t *wf_ppa_gain)
{
    if (!state || !wf_ppa_gain || state->wf_ppa_gain)
        return false;

    nv_fixzone_apply_wf_ppa_gain(wf_ppa_gain);
    state->wf_ppa_gain = true;
    return true;
}

static bool nv_fixzone_try_set_wf_power_offset(nv_fixzone_rf_load_state_t *state, const int8_t *wf_power_offset)
{
    if (!state || !wf_power_offset || state->wf_power_offset)
        return false;

    nv_fixzone_apply_wf_power_offset(wf_power_offset);
    state->wf_power_offset = true;
    return true;
}

static bool nv_fixzone_try_set_wf_rssi_offset(nv_fixzone_rf_load_state_t *state, int16_t dsss, int16_t ofdm)
{
    if (!state || state->wf_rssi_offset)
        return false;

    nv_fixzone_apply_wf_rssi_offset(dsss, ofdm);
    state->wf_rssi_offset = true;
    return true;
}

static bool nv_fixzone_try_set_wf_target_power(
    nv_fixzone_rf_load_state_t *state,
    const ls_nv_fixzone_wf_target_power_t *target_power)
{
    if (!state || !target_power || state->wf_target_power)
        return false;

    nv_fixzone_apply_wf_target_power(target_power);
    state->wf_target_power = true;
    return true;
}

static void nv_fixzone_load_rf_from_cache(const ls_nv_fixzone_body_t *body, nv_fixzone_rf_load_state_t *state)
{
    if (!body || !state)
        return;

    if (body->has_xo_cap)
        nv_fixzone_try_set_xo_cap(state, body->xo_cap);
    if (body->has_wf_ppa_cap)
        nv_fixzone_try_set_wf_ppa_cap(state, body->wf_ppa_cap);
    if (body->has_wf_ppa_gain)
        nv_fixzone_try_set_wf_ppa_gain(state, body->wf_ppa_gain);
    if (body->has_wf_power_offset)
        nv_fixzone_try_set_wf_power_offset(state, body->wf_power_offset);
    if (body->has_wf_rssi_offset)
        nv_fixzone_try_set_wf_rssi_offset(state, body->wf_rssi_offset.dsss, body->wf_rssi_offset.ofdm);
    if (body->has_wf_target_power)
        nv_fixzone_try_set_wf_target_power(state, &body->wf_target_power);
}

int8_t nv_efuse_read_mac(uint8_t *mac_addr)
{
    uint32_t rd0 = 0, rd1 = 0;
    uint8_t tmp_mac[6] = {0};
    uint8_t zero_mac[6] = {0};
    const efuse_base_addrs_t *base_addrs = &g_efuse_nv_slots;

    /* Read from highest-priority base to lowest */
    for (int a = base_addrs->addr_count - 1; a >= 0; a--) {
        uint32_t base = base_addrs->addrs[a];
        EFUSE_RD32(base, &rd0);
        EFUSE_RD32((base + 1), &rd1);
        memcpy(&tmp_mac[0], &rd0, 4);
        memcpy(&tmp_mac[4], &rd1, 2);
        if (memcmp(zero_mac, tmp_mac, 6) != 0) {
            memcpy(mac_addr, tmp_mac, 6);
            return 0;
        }
    }
    //CLOGW("cannot read valid mac addr from efuse\n");
    return -1;
}

int8_t nv_efuse_sync_mac(uint8_t *mac_addr)
{
    uint8_t zero_mac[6] = {0};

    if (!memcmp(zero_mac, mac_addr, 6) || MAC_ADDR_GROUP(mac_addr))
        return -1;

    memcpy(&nv_efuse_cfg_env.mac, mac_addr, 6);
    // CLOGD("sync efuse mac addr " MACSTR "\n", MAC2STR(mac_addr));
    return 0;
}

int8_t nv_efuse_burn_mac(void)
{
    uint32_t *mac_ptr = (uint32_t *)&nv_efuse_cfg_env.mac[0];
    uint32_t rd0 = 0, rd1 = 0;
    uint8_t tmp_mac[6] = {0};
    uint8_t zero_mac[6] = {0};
    const efuse_base_addrs_t *base_addrs = &g_efuse_nv_slots;

    /* Write to the first base (lowest priority) that is still all-zero for the two words */
    for (uint8_t a = 0; a < base_addrs->addr_count; a++) {
        uint32_t base = base_addrs->addrs[a];
        EFUSE_RD32(base, &rd0);
        EFUSE_RD32((base + 1), &rd1);
        memcpy(&tmp_mac[0], &rd0, 4);
        memcpy(&tmp_mac[4], &rd1, 2);
        if (!memcmp(zero_mac, tmp_mac, 6)) {
            EFUSE_WR32(base, *mac_ptr);
            EFUSE_WR32((base + 1), (*(mac_ptr + 1) & 0x0000FFFF));
            CLOGI("burn efuse mac addr " MACSTR " success\n", MAC2STR((uint8_t *)&nv_efuse_cfg_env.mac[0]));
            return 0;
        }
    }
    CLOGW("efuse space for mac addr full used!\n");
    return -1;
}

int8_t nv_efuse_read_common_item(uint8_t *item, const efuse_cfg_t *cfg, char *fn)
{
    uint32_t tmp32 = 0;
    uint8_t i;
    const efuse_base_addrs_t *base_addrs = cfg->base_addrs;

    assert(cfg->bits_width * cfg->field_dim <= 32);

    /* Read from highest-priority base to lowest */
    for (int8_t a = base_addrs->addr_count - 1; a >= 0; a--) {
        uint32_t actual_addr = base_addrs->addrs[a] + cfg->offset_idx;
        EFUSE_RD32(actual_addr, &tmp32);
        if (tmp32 & (1u << cfg->bit_valid)) {
            for (i = 0; i < cfg->field_dim; i++) {
                item[i] = (tmp32 >> (cfg->bit_start + cfg->bits_width * i)) & cfg->bits_mask;
                if (cfg->is_signed && (item[i] & (1u << (cfg->bits_width - 1))))
                    item[i] |= ~cfg->bits_mask;
            }
            return 0;
        }
    }
    return -1;
}

int8_t nv_efuse_sync_common_item(void *env_field, uint8_t *item, const efuse_cfg_t *cfg, char *fn)
{
    uint8_t  *byte_field = (uint8_t *)env_field;
    uint16_t *word_field = (uint16_t *)env_field;
    uint32_t *dword_field = (uint32_t *)env_field;
    uint8_t i = 0;

    assert(cfg->bits_width * cfg->field_dim <= 32);
    //CLOGD("%s:field_dim=%d, bit_width=%d\n", fn, cfg->field_dim, cfg->bits_width);

    for (i = 0; i < cfg->field_dim; i++)
    {
        if (cfg->bits_width <= 8)
            byte_field[i] = *((uint8_t *)item + i);
        else if (cfg->bits_width <= 16)
            word_field[i] = *((uint16_t *)item + i);
        else
            dword_field[i] = *((uint32_t *)item + i);
    }
    return 0;
}

int8_t nv_efuse_burn_common_item(void *env_field, const efuse_cfg_t *cfg, char *fn)
{
    uint32_t tmp32 = 0;
    uint8_t  *byte_field = (uint8_t *)env_field;
    uint16_t *word_field = (uint16_t *)env_field;
    uint32_t *dword_field = (uint32_t *)env_field;
    uint8_t i;
    const efuse_base_addrs_t *base_addrs = cfg->base_addrs;

    assert(cfg->bits_width * cfg->field_dim <= 32);

    /* Write to the first base (lowest priority) where the target field is not written.
       Use the valid-bit indicator to determine if this word has already been burned. */
    for (uint8_t a = 0; a < base_addrs->addr_count; a++) {
        uint32_t actual_addr = base_addrs->addrs[a] + cfg->offset_idx;
        EFUSE_RD32(actual_addr, &tmp32);
        if (!(tmp32 & (1u << cfg->bit_valid))) {
            for (i = 0; i < cfg->field_dim; i++) {
                if (cfg->bits_width <= 8)
                    tmp32 |= (byte_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
                else if (cfg->bits_width <= 16)
                    tmp32 |= (word_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
                else
                    tmp32 |= (dword_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
            }
            tmp32 |= (1u << cfg->bit_valid);
            EFUSE_WR32(actual_addr, tmp32);
            CLOGI("burn efuse %s success\n", fn);
            return 0;
        }
    }
    CLOGW("efuse space for %s full used!\n", fn);
    return -1;
}

int8_t nv_efuse_read_wf_ppa_cap(uint8_t *cap)
{
    int8_t ret = nv_efuse_read_common_item(cap, &mfg_efuse_cfg_tb[EFUSE_ITEM_WF_PPA_CAP], "wf_ppa_cap");
    return ret;
}

int8_t nv_efuse_read_wf_power_offset(int8_t *offset)
{
    int8_t ret = nv_efuse_read_common_item((uint8_t *)offset, &mfg_efuse_cfg_tb[EFUSE_ITEM_WF_POWER_OFFSET], "wf_power_offset");
    return ret;
}

int8_t nv_efuse_read_wf_rssi_offset(int8_t *offset)
{
    int8_t ret = nv_efuse_read_common_item((uint8_t *)offset, &mfg_efuse_cfg_tb[EFUSE_ITEM_WF_RSSI_OFFSET], "wf_rssi_offset");
    return ret;
}

int8_t nv_efuse_read_bt_power_offset(int8_t *offset)
{
    int8_t ret = nv_efuse_read_common_item((uint8_t *)offset, &mfg_efuse_cfg_tb[EFUSE_ITEM_BT_POWER_OFFSET], "bt_power_offset");
    return ret;
}

int8_t nv_efuse_read_xo24m_cap(int8_t *cap)
{
    int8_t ret = nv_efuse_read_common_item((uint8_t *)cap, &mfg_efuse_cfg_tb[EFUSE_ITEM_XO24M_CAP], "xo24m_cap");
    return ret;
}

int8_t nv_fixzone_head_check(uint32_t base_addr, uint32_t magic_code)
{
    uint32_t calc_crc = 0;
    ls_nv_fixzone_header_t *hdr = (ls_nv_fixzone_header_t *)base_addr;
    uint16_t max_data_len = NV_FIXZONE_MAX_DATA_LEN;

    if(!hdr)
        return -1;

    nv_fixzone_debug_log_header("head_check", base_addr, hdr);

    if (hdr->magic != magic_code) {
        CLOGI("NV fix zone magic (%x) mismatch, skip it!\n", hdr->magic);
        return -2;
    }
    if (NV_MAGIC_PATTERN == magic_code) {
        if (hdr->version != NV_FIXZONE_VER) {
            CLOGI("NV fix zone version (%x) mismatch, expect (%x), skip it!\n", hdr->version, NV_FIXZONE_VER);
            return -4;
        }
    }
    // self cali check version
    if ((NV_MAGIC_PATTERN2 == magic_code) && (hdr->version != NV_SELF_CALI_VER)) {
        CLOGI("NV fix zone version (%x) mismatch, skip it!\n", hdr->version);
        return -4;
    }
    if (!hdr->length || hdr->length > max_data_len) {
        CLOGI("NV fix zone length (%x) invalid, max (%x), skip it!\n", hdr->length, max_data_len);
        return -5;
    }
    calc_crc = crc32_sw(calc_crc, (uint8_t *)(hdr), (sizeof(*hdr) - 4));
    calc_crc = crc32_sw(calc_crc, (uint8_t *)(hdr + 1), hdr->length);
    if (calc_crc != hdr->crc32) {
        CLOGI("NV fix zone crc (%x) check failed, expect (%x) skip it!\n", hdr->crc32, calc_crc);
        return -3;
    }

    CLOGI(NV_FIXZONE_DBG_PREFIX "head check success base=0x%08lx calc_crc=0x%08lx len=%u\n",
        base_addr, calc_crc, hdr->length);

    return 0;
}

int8_t nv_fixzone_init()
{
    int8_t ret = 0;

    nv_fixzone_cache_reset();
    nv_fixzone_commit_parse_state(NV_FIXZONE_PARSE_NOT_TRIED);

    if (!wf_conf_base_addr) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "flash base addr not configured, skip flash path\n");
        nv_fixzone_commit_parse_state(NV_FIXZONE_PARSE_NO_BASE_ADDR);
        return 0;
    }

    CLOGN("Factory partition addr=0x%08lx for RF Param\n", wf_conf_base_addr);

    ret = nv_fixzone_head_check(wf_conf_base_addr, NV_MAGIC_PATTERN);
    if (!ret) {
        const ls_nv_fixzone_header_t *hdr = (const ls_nv_fixzone_header_t *)wf_conf_base_addr;

        if (!nv_fixzone_parse_tlv_stream((const uint8_t *)(hdr + 1), hdr->length, &s_nv_fixzone_cache)) {
            nv_fixzone_commit_parse_state(NV_FIXZONE_PARSE_OK);
            nv_fixzone_debug_log_cache_body("cache_after_parse", &s_nv_fixzone_cache);
            nv_fixzone_debug_log_reg_overrides("cache_after_parse");
        } else {
            ret = NV_FIXZONE_PARSE_TLV_FAIL;
            CLOGW("NV fix zone TLV parse failed, skip it!\n");
        }
    } else {
        CLOGI(NV_FIXZONE_DBG_PREFIX "head check skipped flash path, ret=%d\n", ret);
    }

    if (ret != NV_FIXZONE_PARSE_OK)
        nv_fixzone_commit_parse_state(ret);

    return 0;
}

uint32_t nv_get_wf_mfg_conf_base_addr(void)
{
    return wf_conf_base_addr;
}

void nv_set_wf_mfg_conf_base_addr(uint32_t addr)
{
     wf_conf_base_addr = addr;
}

#if 0
static void gen_random_mac(uint8_t *mac_addr)
{
    __HAL_CRM_TRNG_CLK_ENABLE();
    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    //initialize
    HAL_TRNG_Initialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
    HAL_TRNG_Control(TRNG(), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);
    HAL_TRNG_InterruptDisable(TRNG());

    for(uint32_t cnt=0; cnt<8; cnt++)
    {
        HAL_TRNG_Enable(TRNG());
        while(HAL_TRNG_GetDataReady(TRNG()) == 0);
            mac_addr[cnt] = HAL_TRNG_GetData(TRNG());
    }

    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);
    __HAL_CRM_TRNG_CLK_DISABLE();
    mac_addr[0] = 0x00;
    mac_addr[2] = 0x75;
}

static int8_t get_mac_from_nvs(uint8_t *mac_addr)
{
#if CFG_NVS || defined(CFG_AMP_IPC_MRPC_CLIENT_NVS)
    uint8_t ret, mac[8] ={0};
    uint32_t  len = NVDS_LEN_WIFI_MAC_ADDR;

    ret = nvds_get(NVDS_TAG_WIFI_MAC_ADDR, &len, mac);
    if (ret != NVDS_OK)
    {
        gen_random_mac(mac);
        nvds_put(NVDS_TAG_WIFI_MAC_ADDR, NVDS_LEN_WIFI_MAC_ADDR, mac);
    }
    memcpy(mac_addr, mac, 6);
    return 0;
#else
    return -1;
#endif
}
#endif
int8_t nv_fixzone_get_wf_mac(uint8_t *mac_addr)
{
    nv_fixzone_ensure_cache();

    if (!mac_addr || !ls_nv_fixzone_valid_flag || !s_nv_fixzone_cache.has_wf_mac)
        return -1;

    memcpy(mac_addr, s_nv_fixzone_cache.wf_mac, sizeof(s_nv_fixzone_cache.wf_mac));
    return 0;
}
int8_t nv_fixzone_get_bt_mac(uint8_t *mac_addr)
{
    nv_fixzone_ensure_cache();

    if (!mac_addr)
        return -1;
    if (!ls_nv_fixzone_valid_flag || !s_nv_fixzone_cache.has_bt_mac) {
        if (nv_efuse_read_mac(mac_addr))
            return -1;
        mac_addr[5] = mac_addr[5] + 1;
        return 0;
    }
    memcpy(mac_addr, s_nv_fixzone_cache.bt_mac, sizeof(s_nv_fixzone_cache.bt_mac));
    return 0;
}

static void nv_fixzone_golden_val_config(nv_fixzone_rf_load_state_t *state)
{
    bool loaded = false;

    if (!state)
        return;

    nv_fixzone_debug_log_state("golden_before", state);

    if (!wf_golden_val_set) {
        CLOGI("No golden value set \n");
        return;
    }

    if (wf_xo_cap_golden_val)
        loaded |= nv_fixzone_try_set_xo_cap(state, wf_xo_cap_golden_val);
    if (!state->wf_power_offset &&
        (wf_pwr_offset_high_golden_val || wf_pwr_offset_mid_golden_val || wf_pwr_offset_low_golden_val)) {
        int8_t power_offset[3] = {
            wf_pwr_offset_low_golden_val,
            wf_pwr_offset_mid_golden_val,
            wf_pwr_offset_high_golden_val
        };

        loaded |= nv_fixzone_try_set_wf_power_offset(state, power_offset);
    }
    if (wf_rssi_offset_dsss_golden_val || wf_rssi_offset_ofdm_golden_val)
        loaded |= nv_fixzone_try_set_wf_rssi_offset(
            state,
            (int16_t)wf_rssi_offset_dsss_golden_val,
            (int16_t)wf_rssi_offset_ofdm_golden_val);

    if (loaded) {
        CLOGI("golden value: xo cap %d low/mid/high chan pwr off %d %d %d rssi dsss/ofdm offset %d %d \n",
            wf_xo_cap_golden_val, wf_pwr_offset_low_golden_val, wf_pwr_offset_mid_golden_val,
            wf_pwr_offset_high_golden_val, wf_rssi_offset_dsss_golden_val, wf_rssi_offset_ofdm_golden_val);
    }
    nv_fixzone_debug_log_state("golden_after", state);
    nv_fixzone_debug_log_runtime("golden_after");
}


static void nv_fixzone_efuse_load_rf_config(nv_fixzone_rf_load_state_t *state)
{
    uint8_t wf_ppa_cap[WF_PPA_CAP_DIM] = {0};
    int8_t wf_power_offset[WF_POWER_OFFSET_DIM] = {0};
    int8_t wf_rssi_offset[WF_RSSI_OFFSET_DIM] = {0};
    int8_t xo_cap = 0;

    if (!state)
        return;

    nv_fixzone_debug_log_state("efuse_before", state);

    if (!state->wf_ppa_cap && !nv_efuse_read_wf_ppa_cap(wf_ppa_cap) &&
        nv_fixzone_try_set_wf_ppa_cap(state, wf_ppa_cap)) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "efuse wf_ppa_cap=%u/%u/%u\n",
            wf_ppa_cap[0], wf_ppa_cap[1], wf_ppa_cap[2]);
    }
    if (!state->wf_power_offset && !nv_efuse_read_wf_power_offset(wf_power_offset) &&
        nv_fixzone_try_set_wf_power_offset(state, wf_power_offset)) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "efuse wf_power_offset=%d/%d/%d\n",
            wf_power_offset[0], wf_power_offset[1], wf_power_offset[2]);
    }
    if (!state->wf_rssi_offset && !nv_efuse_read_wf_rssi_offset(wf_rssi_offset) &&
        nv_fixzone_try_set_wf_rssi_offset(state, (int16_t)wf_rssi_offset[0], (int16_t)wf_rssi_offset[1])) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "efuse wf_rssi_offset=%d/%d\n",
            wf_rssi_offset[0], wf_rssi_offset[1]);
    }
    if (!state->xo_cap && !nv_efuse_read_xo24m_cap(&xo_cap) &&
        nv_fixzone_try_set_xo_cap(state, xo_cap)) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "efuse xo_cap=%d\n", xo_cap);
    }
    nv_fixzone_debug_log_state("efuse_after", state);
    nv_fixzone_debug_log_runtime("efuse_after");
}

/*
 * xo_cap/power offset/rssi offset load from flash factory zone, or golden value configured by customer, or value from efuse
 * the 1st priority is load from flash factory zone
 * the 2nd priority is load from golden value
 * the 3rd priority is load from efuse
 */
int8_t nv_fixzone_load_rf_config(void)
{
    nv_fixzone_rf_load_state_t state = {0};

    CLOGI(NV_FIXZONE_DBG_PREFIX "load_rf_config begin\n");
    #ifndef WIFI_RAM_ATE
    nv_fixzone_init();

    if (ls_nv_fixzone_valid_flag) {
        CLOGI(NV_FIXZONE_DBG_PREFIX "load source=flash cache valid\n");
        nv_fixzone_debug_log_cache_body("cache_before_apply", &s_nv_fixzone_cache);
        nv_fixzone_load_rf_from_cache(&s_nv_fixzone_cache, &state);
        nv_fixzone_debug_log_state("after_flash", &state);
        nv_fixzone_debug_log_runtime("after_flash");
    } else {
        CLOGI(NV_FIXZONE_DBG_PREFIX "load source=flash cache invalid\n");
    }
    #endif
    if (!state.xo_cap || !state.wf_power_offset || !state.wf_rssi_offset) {
        nv_fixzone_golden_val_config(&state);
    }
    if (!state.xo_cap || !state.wf_ppa_cap || !state.wf_power_offset || !state.wf_rssi_offset) {
        nv_fixzone_efuse_load_rf_config(&state);
    }
    if (ls_nv_fixzone_valid_flag && s_nv_fixzone_reg_override_cache.count) {
        nv_fixzone_apply_reg_overrides();
    }

    nv_fixzone_debug_log_state("load_rf_config_final", &state);
    nv_fixzone_debug_log_runtime("load_rf_config_final");

    return 0;
}

int8_t nv_reset_rf_config(void)
{
    int8_t i = 0;

    for (i = 0; i < 3; i++)
        wf_power_offset_fake_reg[i] = 0;
    wf_power_offset_en = 0;
    IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = 0;
    IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_DSSS_OFFSET = 0;
    IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_OFDM_OFFSET = 0;
    IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = 0;

    return 0;
}

void print_nv_selfcali_cfg(uint32_t *p)
{
    for (int i = 0; i < 32; i++) {
        CLOGI("%08x %08x %08x %08x\n", p[i*4 + 0], p[i*4 + 1], p[i*4 + 2], p[i*4 + 3]);
    }
}

#ifdef RF_SELF_CALI_FROM_NV
int8_t nv_selfcali_head_check(void)
{
#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
    int32_t ret = 0;
    ls_nv_fixzone_header_t *hdr = (ls_nv_fixzone_header_t *)nv_self_cali_cfg_buf;
    ls_nv_selfcali_body_t *body = (ls_nv_selfcali_body_t *)(nv_self_cali_cfg_buf + sizeof(ls_nv_fixzone_header_t));

    if (!flash_if_check_security_support()) {
        CLOGE("Flash unsupport OTP region!\n");
        goto failed;
    }
    memset(hdr, 0, sizeof(nv_self_cali_cfg_buf));
    nv_self_cali_addr = 0;
    ret = flash_if_security_read(nv_self_cali_addr, &nv_self_cali_cfg_buf, sizeof(nv_self_cali_cfg_buf));
    if (ret) {
        CLOGW("read selfcali NV config from Flash OTP region failed, ret=%d\n", ret);
        goto failed;
    } else {
        CLOGI("read selfcali NV config from Flash OTP region success\n");
    }
    CLOGI("nv_selfcali_head_check magic %x len %x version %x crc32 %x\n", hdr->magic, hdr->length, hdr->version, hdr->crc32);
    //print_nv_selfcali_cfg((uint32_t *)hdr);
#else
    ls_nv_fixzone_header_t *hdr = (ls_nv_fixzone_header_t *)nv_self_cali_addr;
    ls_nv_selfcali_body_t *body = (ls_nv_selfcali_body_t *)(nv_self_cali_addr + sizeof(ls_nv_fixzone_header_t));
#endif
    ls_nv_selfcali_valid_flag = !nv_fixzone_head_check((uint32_t)hdr, NV_MAGIC_PATTERN2);
    return !ls_nv_selfcali_valid_flag;
failed:
    ls_nv_selfcali_valid_flag = false;
    return !ls_nv_selfcali_valid_flag;
}

int8_t nv_selfcali_load_config(int8_t from_otp)
{
    uint8_t i = 0;
    uint8_t j = 0;

#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
    ls_nv_fixzone_header_t *hdr = (ls_nv_fixzone_header_t *)nv_self_cali_cfg_buf;
    ls_nv_selfcali_body_t *body = (ls_nv_selfcali_body_t *)(nv_self_cali_cfg_buf + sizeof(ls_nv_fixzone_header_t));
#else
    ls_nv_fixzone_header_t *hdr = (ls_nv_fixzone_header_t *)nv_self_cali_addr;
    ls_nv_selfcali_body_t *body = (ls_nv_selfcali_body_t *)(nv_self_cali_addr + sizeof(ls_nv_fixzone_header_t));
#endif
    P_RF_CALI_OPS cali = rf_cali.ops;
    complexint16 tx_comp_dc = {0};
    /*Note that makesure nv_selfcali_head_check at least do once */
    if (from_otp && !ls_nv_selfcali_valid_flag && nv_selfcali_head_check()) {
        return -1;
    }
    if (!from_otp)
    {
        hdr = &nv_selfcali_cfg.hdr;
        body = &nv_selfcali_cfg.bdy;
    }
    else
    {
        nv_selfcali_cfg = *((ls_nv_selfcali_cfg_t *)hdr);
    }
#if 0 //DEBUG
    //CLOGI("hdr=%p, %x; body=%p, %x\n", hdr, hdr->magic, body, body->rx_rc_cap);
    CLOGI("load rxcali config from NV, rc_cap=%d, dcoc_comp_i=%d, dcoc_comp_q=%d, iq_comp_i=%d, iq_comp_q=%d\n", body->rx_rc_cap, body->rx_dcoc_comp_i, body->rx_dcoc_comp_q, body->rx_iq_comp_i, body->rx_iq_comp_q);
    CLOGI("load txcali config from NV, dc_comp_i=%d, dc_comp_q=%d, iq_comp_i=%d, iq_comp_q=%d\n", body->tx_dc_comp_i, body->tx_dc_comp_q, body->tx_iq_comp_i, body->tx_iq_comp_q);
#endif
    cali->env_init();
    cali->set_ppa_cap(0, body->ppa_cap_0);
    cali->set_ppa_cap(1, body->ppa_cap_1);
    cali->set_ppa_cap(2, body->ppa_cap_2);
    set_sc_i(RFCALI_MODE_WF, body->rx_dcoc_sc_i);
    set_sc_q(RFCALI_MODE_WF, body->rx_dcoc_sc_q);
    cali->rxdcoc_result(&body->rx_dcoc_comp_i, &body->rx_dcoc_comp_q, 1);
    cali->rxiq_result(body->rx_iq_comp_i, body->rx_iq_comp_q);
    cali->rxrc_result(body->rx_rc_cap);
    tx_comp_dc.re = body->tx_dc_comp_i;
    tx_comp_dc.im = body->tx_dc_comp_q;
    cali->txdc_result(&tx_comp_dc, 0);
    cali->txiq_tx_result(body->tx_iq_comp_i, body->tx_iq_comp_q, RF_TXIQ_PWR_RANGE_ALL);
    cali->txdpd_remap_pred();
    for (i = 0; i < DPD_COMP_TABLE_CNT; i++)
    {
        uint8_t idx = 0;
        for (j = 0; j < MAX_PARALEN; j++)
        {
            if (j != 1 && j != 3 && j != 6 && j!= 8 && j!= 11 && j!= 13) {
                nv_tx_pred_table_chan_low[i][j].re = (int16_t)(body->tx_pred_table_chan_low[i][idx] & 0xffff);
                nv_tx_pred_table_chan_low[i][j].im = (int16_t)(body->tx_pred_table_chan_low[i][idx] >> 16);
                nv_tx_pred_table_chan_mid[i][j].re = (int16_t)(body->tx_pred_table_chan_mid[i][idx] & 0xffff);
                nv_tx_pred_table_chan_mid[i][j].im = (int16_t)(body->tx_pred_table_chan_mid[i][idx] >> 16);
                nv_tx_pred_table_chan_hig[i][j].re = (int16_t)(body->tx_pred_table_chan_hig[i][idx] & 0xffff);
                nv_tx_pred_table_chan_hig[i][j].im = (int16_t)(body->tx_pred_table_chan_hig[i][idx] >> 16);
                idx++;
            }
            if (idx >= PREDLEN)
                break;
        }
        #if 0 //DEBUG
        CLOGI("load txdpd config from NV, idx=%d detail:", i);
        rf_cali_txdpd_print_para(dpd_cfg_table[i].pred_lut_idx, dpd_cfg_table[i].tssi, &nv_tx_pred_table_chan_low[i][0]);
        rf_cali_txdpd_print_para(dpd_cfg_table[i].pred_lut_idx, dpd_cfg_table[i].tssi, &nv_tx_pred_table_chan_mid[i][0]);
        rf_cali_txdpd_print_para(dpd_cfg_table[i].pred_lut_idx, dpd_cfg_table[i].tssi, &nv_tx_pred_table_chan_hig[i][0]);
        #endif
    }
    for (i = 0; i < DPD_REST_TABLE_CNT; i++) {
        uint8_t pwr_step = 2 << i;
        int8_t rest_pwr_tssi = dpd_cfg_table[0].tssi - pwr_step;
        int8_t rest_pred_lut_idx = (int8_t)dpd_cfg_table[0].pred_lut_idx - i - 1;
        if (rest_pred_lut_idx < 0)
            break;
        rf_cali_txdpd_calc_rest_table(&nv_tx_pred_rest_table_chan_low[i][0], &nv_tx_pred_table_chan_low[0][0], pwr_step);
        rf_cali_txdpd_calc_rest_table(&nv_tx_pred_rest_table_chan_mid[i][0], &nv_tx_pred_table_chan_mid[0][0], pwr_step);
        rf_cali_txdpd_calc_rest_table(&nv_tx_pred_rest_table_chan_hig[i][0], &nv_tx_pred_table_chan_hig[0][0], pwr_step);
        #if 0 //DEBUG
        rf_cali_txdpd_print_para(rest_pred_lut_idx, rest_pwr_tssi, &nv_tx_pred_rest_table_chan_low[i][0]);
        rf_cali_txdpd_print_para(rest_pred_lut_idx, rest_pwr_tssi, &nv_tx_pred_rest_table_chan_mid[i][0]);
        rf_cali_txdpd_print_para(rest_pred_lut_idx, rest_pwr_tssi, &nv_tx_pred_rest_table_chan_hig[i][0]);
        #endif
    }
    cali->env_deinit();

    return 0;
}
#endif

#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
int8_t nv_update_selfcali_wf_rx_params(int8_t *rxcali_rc_cap, int8_t *rxcali_dc_comp_i, int8_t *rxcali_dc_comp_q,
    int8_t *rxcali_dc_sc_i, int8_t *rxcali_dc_sc_q, int16_t *rxcali_iq_comp_i, int16_t *rxcali_iq_comp_q)
{
    ls_nv_selfcali_body_t *bd = &nv_selfcali_cfg.bdy;

    if (rxcali_rc_cap)
        bd->rx_rc_cap = *rxcali_rc_cap;
    if (rxcali_dc_comp_i)
        bd->rx_dcoc_comp_i = *rxcali_dc_comp_i;
    if (rxcali_dc_comp_q)
        bd->rx_dcoc_comp_q = *rxcali_dc_comp_q;
    if (rxcali_dc_sc_i)
        bd->rx_dcoc_sc_i = *rxcali_dc_sc_i;
    if (rxcali_dc_sc_q)
        bd->rx_dcoc_sc_q = *rxcali_dc_sc_q;
    if (rxcali_iq_comp_i)
        bd->rx_iq_comp_i = *rxcali_iq_comp_i;
    if (rxcali_iq_comp_q)
        bd->rx_iq_comp_q = *rxcali_iq_comp_q;

    return 0;
}

int8_t nv_update_selfcali_wf_tx_params(int16_t *txcali_dc_comp_i, int16_t *txcali_dc_comp_q, int16_t *txcali_iq_comp_i, int16_t *txcali_iq_comp_q)
{
    ls_nv_selfcali_body_t *bd = &nv_selfcali_cfg.bdy;

    if (txcali_dc_comp_i)
        bd->tx_dc_comp_i = *txcali_dc_comp_i;
    if (txcali_dc_comp_q)
        bd->tx_dc_comp_q = *txcali_dc_comp_q;
    if (txcali_iq_comp_i)
        bd->tx_iq_comp_i = *txcali_iq_comp_i;
    if (txcali_iq_comp_q)
        bd->tx_iq_comp_q = *txcali_iq_comp_q;

    return 0;
}

int8_t nv_update_selfcali_dpd_params(uint8_t tbl_idx, uint32_t *txcali_dpd_tbl)
{
    uint8_t j = 0;
    uint8_t i = 0;
    uint8_t chan_type = IP_RFIF->REG_CTRL0.bit.WF_CHANNEL;
    ls_nv_selfcali_body_t *bd = &nv_selfcali_cfg.bdy;

    if (chan_type == 0) {
        i = 0;
        for (j = 0; j < MAX_PARALEN; j++)
        {
            if (j != 1 && j != 3 && j != 6 && j != 8 && j != 11 && j != 13) {
                bd->tx_pred_table_chan_low[tbl_idx][i] = txcali_dpd_tbl[j];
                i++;
            }
        }
    }
    else if (chan_type == 1) {
        i = 0;
        for (j = 0; j < MAX_PARALEN; j++)
        {
            if (j != 1 && j != 3 && j != 6 && j != 8 && j != 11 && j != 13) {
                bd->tx_pred_table_chan_mid[tbl_idx][i] = txcali_dpd_tbl[j];
                i++;
            }
        }
    }
    else if (chan_type == 2) {
        i = 0;
        for (j = 0; j < MAX_PARALEN; j++)
        {
            if (j != 1 && j != 3 && j != 6 && j != 8 && j != 11 && j != 13) {
                bd->tx_pred_table_chan_hig[tbl_idx][i] = txcali_dpd_tbl[j];
                i++;
            }
        }
    }
    return 0;
}

int8_t nv_update_selfcali_ppa_cap_params(uint8_t *ppa_cap_0, uint8_t *ppa_cap_1, uint8_t *ppa_cap_2)
{
    ls_nv_selfcali_body_t *bd = &nv_selfcali_cfg.bdy;

    if (ppa_cap_0)
        bd->ppa_cap_0 = *ppa_cap_0;
    if (ppa_cap_1)
        bd->ppa_cap_1 = *ppa_cap_1;
    if (ppa_cap_2)
        bd->ppa_cap_2 = *ppa_cap_2;
    return 0;
}

void nv_selfcali_init(void)
{
    ls_nv_fixzone_header_t *hdr = &nv_selfcali_cfg.hdr;

    hdr->magic = NV_MAGIC_PATTERN2;
    hdr->version = NV_SELF_CALI_VER;
    hdr->length = sizeof(ls_nv_selfcali_body_t);

#ifdef CFG_FLASH_IF
    CLOGI("nv_selfcali_init call flash_init\n");
    flash_if_init(&cali_flash_dev, 0, 0);
#endif
}

int8_t nv_selfcali_burn_config(void)
{
    ls_nv_fixzone_header_t *hdr = &nv_selfcali_cfg.hdr;
    int32_t ret = 0;
    uint32_t otp_flag = 0;

#if RF_BOARD_VER == 2
    if (nv_selfcali_get_otp_flag(&otp_flag) != 0 || otp_flag != NV_MAGIC_USR_TRIG1) {
        CLOGI("won't burn OTP since flag %x is not expect %x\n", otp_flag, NV_MAGIC_USR_TRIG1);
        return -1;
    }
#endif

#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
    ls_nv_fixzone_header_t *buf_hd = (ls_nv_fixzone_header_t *)nv_self_cali_cfg_buf;
    int16_t write_len = MAX_SEC_LEN;
    uint8_t *write_addr = &nv_self_cali_cfg_buf[0];
    int16_t read_len = MAX_SEC_LEN;
    uint8_t *read_addr = &nv_self_cali_cfg_buf[0];

    if (!flash_if_check_security_support()) {
        CLOGE("Flash unsupport OTP region!\n");
        return -1;
    }
#endif
    hdr->crc32 = 0;
    hdr->crc32 = crc32_sw(hdr->crc32, (uint8_t *)(hdr), (sizeof(*hdr) - 4));
    hdr->crc32 = crc32_sw(hdr->crc32, (uint8_t *)(hdr) + sizeof(ls_nv_fixzone_header_t), hdr->length);

    CLOGI("start write protect flash %x\n", nv_self_cali_addr);
#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
    flash_if_write_protection_set(false);
    nv_self_cali_addr = 0;
    CLOGI("start erase flash OTP %x\n", nv_self_cali_addr);
    ret = flash_if_security_erase(nv_self_cali_addr);
    if (ret) {
        CLOGW("erase flash OTP %x failed, ret=%d\n", nv_self_cali_addr, ret);
        goto write_failed;
    }
    else {
        CLOGI("erase flash OTP %x success\n", nv_self_cali_addr);
    }
    memcpy(nv_self_cali_cfg_buf, hdr, sizeof(ls_nv_selfcali_cfg_t));

    nv_self_cali_addr = 0;
    while (write_len > WR_SEC_LEN) {
        ret = flash_if_security_write(nv_self_cali_addr, (void *)write_addr, WR_SEC_LEN);
        if (ret) {
            CLOGE("write selfcali NV config to Flash OTP region failed, ret=%d\n", ret);
            goto write_failed;
        } else
            CLOGI("write selfcali NV config to Flash OTP region success\n");
        nv_self_cali_addr += WR_SEC_LEN;
        write_addr += WR_SEC_LEN;
        write_len -= WR_SEC_LEN;
    }
    ret = flash_if_security_write(nv_self_cali_addr, (void *)write_addr, write_len);
    if (ret) {
        CLOGE("write selfcali NV config to Flash OTP region tail failed, ret=%d\n", ret);
        goto write_failed;
    } else
        CLOGI("write selfcali NV config to Flash OTP region tail success\n");
    #if 1 //read back to check
    memset(nv_self_cali_cfg_buf, 0, sizeof(nv_self_cali_cfg_buf));
    nv_self_cali_addr = 0;
    ret = flash_if_security_read(nv_self_cali_addr, &nv_self_cali_cfg_buf, sizeof(nv_self_cali_cfg_buf));
    if (ret) {
        CLOGW("read selfcali NV config from Flash OTP region failed, ret=%d\n", ret);
        goto write_failed;
    } else {
        CLOGI("read selfcali NV config from Flash OTP region success\n");
    }
    CLOGI("nv_selfcali_burn_config magic %x len %x version %d crc32 %x and detail:\n", buf_hd->magic, buf_hd->length, buf_hd->version, buf_hd->crc32);
    print_nv_selfcali_cfg((uint32_t *)buf_hd);
    #endif
#else
    ret = flash_if_erase(nv_self_cali_addr, sizeof(nv_selfcali_cfg));
    if (ret) {
        CLOGE("erase flash %x failed, ret=%d\n", nv_self_cali_addr, ret);
        goto write_failed;
    }
    else {
        CLOGI("erase flash %x success\n", nv_self_cali_addr);
    }
    ret = flash_if_write(nv_self_cali_addr, (uint8_t *)&nv_selfcali_cfg, sizeof(nv_selfcali_cfg));
    if (ret) {
        CLOGE("write flash %x failed, ret=%d\n", nv_self_cali_addr, ret);
        goto write_failed;
    }
    else {
        CLOGI("write flash %x success\n", nv_self_cali_addr);
    }
#endif
    flash_if_write_protection_set(true);
    return 0;
write_failed:
    flash_if_write_protection_set(true);
    return -1;
}
#endif

int8_t nv_selfcali_get_otp_flag(uint32_t *flag)
{
    int32_t ret = 0;

    if (!flag) {
        CLOGE("nv_selfcali_get_otp_flag param flag is NULL!\n");
        return -1;
    }

#if (defined(RF_SELF_CALI_FROM_NV) || defined(RF_SELF_CALI_WRITE_TO_NV))
#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
    if (!flash_if_check_security_support()) {
        CLOGE("Flash unsupport OTP region!\n");
        return -1;
    }
#if (USE_FLASH_OTP == 1)
    ret = flash_if_security_read(nv_self_cali_addr, (void *)&g_magic_code, sizeof(uint32_t));
#else
    ret = flash_if_read(nv_self_cali_addr, (void *)&g_magic_code, sizeof(uint32_t));
#endif
    if (ret) {
        CLOGW("read selfcali NV config from Flash OTP flag failed, ret=%d\n", ret);
        return -1;
    } else {
        CLOGI("read selfcali NV config from Flash OTP flag success\n");
    }
#endif
#endif
    CLOGI("nv_selfcali_get_otp_flag 0x%08x\n", g_magic_code);
    *flag = g_magic_code;

    return 0;
}
