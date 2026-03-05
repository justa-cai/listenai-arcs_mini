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

#define NV_SELF_CALI_VER  1
#define NV_MAGIC_PATTERN    0x55aa0bf4
#define NV_MAGIC_PATTERN2   0x22ff0ce5
#define NV_MAGIC_USR_TRIG1  0x11223344
#ifndef FLASH_NOR_OTP_NV_BASE_ADDR
#define FLASH_NOR_OTP_NV_BASE_ADDR (CMN_FLASH_REGION + 0x200000)
#endif
#define FLASH_OTP_NV_LENGTH   512
#define FIXZONE_NV_BASE_ADDR  0x301FF000
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
#define EFUSE_NV_SLOT0_ADDR(idx)    (64 + idx)
#define EFUSE_NV_SLOT1_ADDR(idx)    (68 + idx)
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

typedef struct efuse_cfg_s
{
    uint8_t  addr0;            /*efuse slot0 word address*/
    uint8_t  addr1;            /*efuse slot1 word address*/
    uint8_t  field_dim;        /*if field_dim > 1 this means field type is array, array's dim == field_dim*/
    uint8_t  bit_valid;        /*field bit valid position in the efuse word*/
    uint32_t bits_mask;        /*field bits mask*/
    uint8_t  bit_start;        /*field bit start position in the efuse word*/
    uint8_t  bits_width;       /*field single item bits width*/
    uint8_t  is_signed;        /*field item's highest bit is signed or not*/
} efuse_cfg_t;

typedef struct {
    uint32_t magic;
    uint16_t length;
    uint16_t version;
    uint32_t crc32;
} ls_nv_fixzone_header_t;

typedef struct {
    uint32_t chip_id;
    uint8_t wf_mac[6];
    uint8_t bt_mac[6];
    int8_t xo_cap;
    uint8_t wf_ppa_cap[3];
    uint32_t wf_power_table[19];
    int8_t wf_power_offset[3];
    int8_t bt_power_offset[3];
    int16_t wf_rssi_offset_dsss;
    int16_t wf_rssi_offset_ofdm;
    uint16_t reserve0;
    int32_t bt_target_power;
    uint8_t wf_target_power_channel_ind;
    uint8_t reserve1;
    pwr_table_t wf_target_power[3];
} ls_nv_fixzone_body_t;

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
extern int8_t nv_selfcali_erase_otp(void);
extern int8_t nv_selfcali_get_otp_flag(uint32_t *flag);
#endif//_NV_CONFIG_H_
