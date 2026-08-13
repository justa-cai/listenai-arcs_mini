/**
 ****************************************************************************************
 *
 * @file nv_config.h
 *
 * @brief definitions and declarations of NV configuraton functions
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */

#ifndef _NV_CONFIG_H_
#define _NV_CONFIG_H_

#include "ls_wifi_type.h"
#include "rf_cali.h"

#define NV_FIXZONE_VER    2
#define NV_SELF_CALI_VER  1
#define NV_MAGIC_PATTERN    0x55aa0bf4
#define NV_MAGIC_PATTERN2   0x22ff0ce5
#define NV_MAGIC_USR_TRIG1  0x11223344
#ifndef FLASH_NOR_OTP_NV_BASE_ADDR
#define FLASH_NOR_OTP_NV_BASE_ADDR (CMN_FLASH_REGION + 0x200000)
#endif
#ifndef FLASH_WF_MFG_CONF_BASE_ADDR
#define FLASH_WF_MFG_CONF_BASE_ADDR 0
#endif
#define FLASH_OTP_NV_LENGTH   512
#define NV_FIXZONE_MAC_ADDR_LEN 6
#define NV_FIXZONE_WF_PPA_GAIN_DIM 19

#define WF_PPA_CAP_BITS_MASK       0x1f
#define WF_PPA_CAP_BITS_WIDTH         5
#define WF_PPA_CAP_0_BIT_OFFSET      16
#define WF_PPA_CAP_VALID_BIT_OFFSET  31
#define WF_PPA_CAP_DIM                3
#define WF_POWER_OFFSET_BITS_MASK      0x3f
#define WF_POWER_OFFSET_BITS_WIDTH        6
#define WF_POWER_OFFSET_0_BIT_OFFSET      0
#define WF_POWER_OFFSET_VALID_BIT_OFFSET  18
#define WF_POWER_OFFSET_DIM                3
#define WF_RSSI_OFFSET_VALID_BIT_OFFSET    31
#define WF_RSSI_OFFSET_BITS_MASK         0x3f
#define WF_RSSI_OFFSET_0_BIT_OFFSET        19
#define WF_RSSI_OFFSET_BITS_WIDTH           6
#define WF_RSSI_OFFSET_DIM                  2
#define BT_POWER_OFFSET_VALID_BIT_OFFSET   24
#define BT_POWER_OFFSET_BITS_MASK        0x3f
#define BT_POWER_OFFSET_0_BIT_OFFSET        0
#define BT_POWER_OFFSET_BITS_WIDTH          6
#define BT_POWER_OFFSET_DIM                 3
#define XO24M_CAP_VALID_BIT_OFFSET    30
#define XO24M_CAP_BITS_MASK         0x1f
#define XO24M_CAP_BIT_OFFSET          25
#define XO24M_CAP_BITS_WIDTH           5
#define XO24M_CAP_DIM                  1
#define EFUSE_NV_SLOT0_ADDR    64
#define EFUSE_NV_SLOT1_ADDR    68

#ifndef WIFI_RF_SET_GOLDEN
#define WIFI_RF_SET_GOLDEN              0
#endif
#ifndef XO_CAP_GOLDEN_VAL
#define XO_CAP_GOLDEN_VAL               0
#endif
#ifndef PWR_OFFSET_LOW_GOLDEN_VAL
#define PWR_OFFSET_LOW_GOLDEN_VAL       0
#endif
#ifndef PWR_OFFSET_MID_GOLDEN_VAL
#define PWR_OFFSET_MID_GOLDEN_VAL       0
#endif
#ifndef PWR_OFFSET_HIGH_GOLDEN_VAL
#define PWR_OFFSET_HIGH_GOLDEN_VAL      0
#endif
#ifndef RSSI_OFFSET_DSSS_GOLDEN_VAL
#define RSSI_OFFSET_DSSS_GOLDEN_VAL     0
#endif
#ifndef RSSI_OFFSET_OFDM_GOLDEN_VAL
#define RSSI_OFFSET_OFDM_GOLDEN_VAL     0
#endif
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#ifndef MAC_ADDR_GROUP
#define MAC_ADDR_GROUP(mac_addr_ptr) ((*((uint8_t *)(mac_addr_ptr))) & 1)
#endif

enum {
    EFUSE_ITEM_WF_PPA_CAP = 0,
    EFUSE_ITEM_WF_POWER_OFFSET = 1,
    EFUSE_ITEM_WF_RSSI_OFFSET = 2,
    EFUSE_ITEM_BT_POWER_OFFSET = 3,
    EFUSE_ITEM_XO24M_CAP = 4,
    EFUSE_ITEM_MAX,
};

typedef struct {
    uint8_t mac[6];
    int8_t xo_cap;
    uint8_t wf_ppa_cap[3];
    int8_t wf_power_offset[3];
    int8_t bt_power_offset[3];
    int8_t wf_rssi_offset[2];
} ls_nv_fixzone_efuse_t;

/* Separate base address configuration (reusable) */
typedef struct efuse_base_addrs_s {
    uint8_t addr_count;    /* number of base addresses */
    const uint8_t *addrs;  /* pointer to base addresses array */
} efuse_base_addrs_t;

/* efuse configuration table entry: holds base addresses pointer and field layout
 * Base addresses are treated as base word addresses; actual word to access is
 * base + layout.offset_idx. Entries are ordered such that addrs[0] is the
 * lower-priority base and addrs[addr_count-1] is the highest-priority base.
 * Members are ordered by size (descending) to optimize memory alignment.
 */
typedef struct efuse_cfg_s
{
    const efuse_base_addrs_t *base_addrs; /* pointer to base addresses configuration (4/8 bytes) */
    uint8_t offset_idx;                   /* offset (word index) relative to base address */
    uint8_t field_dim;                    /* array dimension */
    uint8_t bit_valid;                    /* valid bit position in the efuse word */
    uint8_t bit_start;                    /* bit start position for the first item */
    uint32_t bits_mask;                   /* bits mask for a single item (4 bytes) */
    uint8_t bits_width;                   /* bits width for a single item */
    uint8_t is_signed;                    /* signed flag for the item */
    /* 2 bytes padding to align to 4-byte boundary */
} efuse_cfg_t;

typedef struct {
    uint32_t magic;
    uint16_t length;
    uint16_t version;
    uint32_t crc32;
} ls_nv_fixzone_header_t;

#define NV_FIXZONE_MAX_DATA_LEN (FLASH_OTP_NV_LENGTH - sizeof(ls_nv_fixzone_header_t))

typedef struct {
    uint16_t tag;
    uint16_t len;
} ls_nv_fixzone_tlv_hdr_t;

#define NV_FIXZONE_REG_OVERRIDE_MAX_CNT \
    (NV_FIXZONE_MAX_DATA_LEN / (sizeof(ls_nv_fixzone_tlv_hdr_t) + sizeof(uint32_t) * 2))

enum {
    NV_FIXZONE_TAG_CHIP_ID = 1,
    NV_FIXZONE_TAG_WF_MAC,
    NV_FIXZONE_TAG_XO_CAP,
    NV_FIXZONE_TAG_WF_PPA_CAP,
    NV_FIXZONE_TAG_WF_PPA_GAIN,
    NV_FIXZONE_TAG_WF_POWER_OFFSET,
    NV_FIXZONE_TAG_WF_RSSI_OFFSET,
    NV_FIXZONE_TAG_WF_TARGET_POWER,
    NV_FIXZONE_TAG_BT_MAC,
    NV_FIXZONE_TAG_BT_POWER_OFFSET,
    NV_FIXZONE_TAG_BT_TARGET_POWER,
    NV_FIXZONE_TAG_REG_OVERRIDE = 0xFFFE,
};

typedef struct {
    int16_t dsss;
    int16_t ofdm;
} ls_nv_fixzone_wf_rssi_offset_t;

typedef struct {
    uint8_t channel_ind;
    pwr_table_t value[3];
} ls_nv_fixzone_wf_target_power_t;

/* Runtime cache populated from the on-flash TLV stream. */
typedef struct {
    uint8_t has_chip_id;
    uint8_t has_wf_mac;
    uint8_t has_xo_cap;
    uint8_t has_wf_ppa_cap;
    uint8_t has_wf_ppa_gain;
    uint8_t has_wf_power_offset;
    uint8_t has_wf_rssi_offset;
    uint8_t has_wf_target_power;
    uint8_t has_bt_mac;
    uint8_t has_bt_power_offset;
    uint8_t has_bt_target_power;
    uint32_t chip_id;
    uint8_t wf_mac[NV_FIXZONE_MAC_ADDR_LEN];
    int8_t xo_cap;
    uint8_t wf_ppa_cap[WF_PPA_CAP_DIM];
    uint8_t wf_ppa_gain[NV_FIXZONE_WF_PPA_GAIN_DIM];
    int8_t wf_power_offset[WF_POWER_OFFSET_DIM];
    ls_nv_fixzone_wf_rssi_offset_t wf_rssi_offset;
    ls_nv_fixzone_wf_target_power_t wf_target_power;
    uint8_t bt_mac[NV_FIXZONE_MAC_ADDR_LEN];
    int8_t bt_power_offset[BT_POWER_OFFSET_DIM];
    int32_t bt_target_power;
} ls_nv_fixzone_body_t;

typedef struct {
    uint32_t addr;
    uint32_t value;
} ls_nv_fixzone_reg_override_item_t;

typedef struct {
    uint16_t count;
    uint16_t reserve;
    ls_nv_fixzone_reg_override_item_t items[NV_FIXZONE_REG_OVERRIDE_MAX_CNT];
} ls_nv_fixzone_reg_override_cache_t;

_Static_assert(sizeof(ls_nv_fixzone_header_t) == 12, "ls_nv_fixzone_header_t size mismatch");
_Static_assert(sizeof(ls_nv_fixzone_tlv_hdr_t) == 4, "ls_nv_fixzone_tlv_hdr_t size mismatch");
_Static_assert(sizeof(ls_nv_fixzone_wf_rssi_offset_t) == 4, "ls_nv_fixzone_wf_rssi_offset_t size mismatch");
_Static_assert(sizeof(ls_nv_fixzone_wf_target_power_t) == (sizeof(pwr_table_t) * 3 + 1),
    "ls_nv_fixzone_wf_target_power_t size mismatch");

typedef struct {
    uint32_t tx_pred_table_chan_low[DPD_COMP_TABLE_CNT][9];
    uint32_t tx_pred_table_chan_mid[DPD_COMP_TABLE_CNT][9];
    uint32_t tx_pred_table_chan_hig[DPD_COMP_TABLE_CNT][9];
    int16_t tx_dc_comp_i;
    int16_t tx_dc_comp_q;
    int16_t tx_iq_comp_i;
    int16_t tx_iq_comp_q;
    int8_t rx_dcoc_comp_i;
    int8_t rx_dcoc_comp_q;
    int8_t rx_dcoc_sc_i;
    int8_t rx_dcoc_sc_q;
    int8_t rx_rc_cap;
    int16_t rx_iq_comp_i;
    int16_t rx_iq_comp_q;
    uint8_t ppa_cap_0;
    uint8_t ppa_cap_1;
    uint8_t ppa_cap_2;
} ls_nv_selfcali_body_t;

typedef struct {
    ls_nv_fixzone_header_t hdr;
    ls_nv_selfcali_body_t  bdy;
} ls_nv_selfcali_cfg_t;

extern int8_t ls_nv_fixzone_valid_flag;
extern ls_nv_fixzone_efuse_t nv_efuse_cfg_env;
extern const efuse_cfg_t mfg_efuse_cfg_tb[];
extern int8_t wf_power_offset_fake_reg[3];
extern int8_t nv_efuse_read_mac(uint8_t *mac_addr);
extern int8_t nv_efuse_sync_mac(uint8_t *mac_addr);
extern int8_t nv_efuse_burn_mac(void);
extern int8_t nv_efuse_read_common_item(uint8_t *item, const efuse_cfg_t *cfg, char *fn);
extern int8_t nv_efuse_sync_common_item(void *env_field, uint8_t *item, const efuse_cfg_t *cfg, char *fn);
extern int8_t nv_efuse_burn_common_item(void *env_field, const efuse_cfg_t *cfg, char *fn);
extern int8_t nv_fixzone_init();
extern int8_t nv_fixzone_get_wf_mac(uint8_t *mac_addr);
extern int8_t nv_fixzone_get_bt_mac(uint8_t *mac_addr);
extern int8_t nv_fixzone_load_rf_config(void);
extern int8_t nv_reset_rf_config(void);
extern int8_t nv_selfcali_erase_otp(void);
extern int8_t nv_selfcali_get_otp_flag(uint32_t *flag);

uint32_t nv_get_wf_mfg_conf_base_addr(void);
/* To set wifi factory partition address for wifi rf parameter configuration */
void nv_set_wf_mfg_conf_base_addr(uint32_t addr);

#endif//_NV_CONFIG_H_
