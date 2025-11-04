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
#ifdef CFG_FLASH_IF
#include "flash_if.h"
#else
#include "spiflash.h"
#endif
#include "nv_otp.h"

#include "ClockManager.h"
#include "Driver_TRNG.h"
#include "ls_misc.h"
#include "crc32_sw.h"

/*
 * DEFINES
 ****************************************************************************************
 */
#define TPC_DIG_GAIN_BASE_ADDR   (&IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_0.all)
#define EFUSE_RD32 ls_efuse_read_word//efuse_read_word
/* TODO use efuse_write_word define in bsp driver later */
//extern int efuse_write_word_simple(uint32_t addr, uint32_t val);
#define EFUSE_WR32 ls_efuse_write_word //efuse_write_word_simple
#define MEM_RD32(addr)              (*(volatile uint32_t *)(addr))
#define MEM_WR32(addr, value)       (*(volatile uint32_t *)(addr)) = (value)

#ifdef CFG_FLASH_IF
#ifndef WIFI_RAM_ATE
#define flash_init(dev, sclk_div, run_mod)                 flash_if_init(dev, sclk_div, run_mod)
#define flash_write_protection_set(dev, enable)            flash_if_write_protection_set(enable)
#define flash_write(dev, offset, data, len)                flash_if_write(offset, data, len)
#define flash_read(dev, offset, data, len)                 flash_if_read(offset, data, len)
#define flash_erase_page(dev, offset, sector_size)         flash_if_erase_page(offset, sector_size)
#define flash_security_read(dev, offset, data, len)        flash_if_security_read(offset, data, len)
#define flash_security_write(dev, offset, data, len)       flash_if_security_write(offset, data, len)
#define flash_security_erase(dev, offset)                  flash_if_security_erase(offset)
#endif
#endif

#define MAX_SEC_LEN 512
#define WR_SEC_LEN  256
uint8_t nv_self_cali_cfg_buf[MAX_SEC_LEN] = {0};
uint32_t g_magic_code = 0;

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */
typedef struct {
    uint8_t ppa_gain; //u8.0
    uint8_t abb_gain; //u3.0
    uint16_t dig_gain; //u12.9
} wf_power_table_item_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
int8_t ls_nv_fixzone_valid_flag = false;
int8_t ls_nv_selfcali_valid_flag = false;
ls_nv_fixzone_efuse_t nv_efuse_cfg_env = {0};

const efuse_cfg_t mfg_efuse_cfg_tb[] = {
    {EFUSE_NV_SLOT0_ADDR(1), EFUSE_NV_SLOT1_ADDR(1), WF_PPA_CAP_DIM, WF_PPA_CAP_VALID_BIT_OFFSET, WF_PPA_CAP_BITS_MASK, WF_PPA_CAP_0_BIT_OFFSET, WF_PPA_CAP_BITS_WIDTH, false},
    {EFUSE_NV_SLOT0_ADDR(2), EFUSE_NV_SLOT1_ADDR(2), WF_POWER_OFFSET_DIM, WF_POWER_OFFSET_VALID_BIT_OFFSET, WF_POWER_OFFSET_BITS_MASK, WF_POWER_OFFSET_0_BIT_OFFSET, WF_POWER_OFFSET_BITS_WIDTH, true},
    {EFUSE_NV_SLOT0_ADDR(2), EFUSE_NV_SLOT1_ADDR(2), WF_RSSI_OFFSET_DIM, WF_RSSI_OFFSET_VALID_BIT_OFFSET, WF_RSSI_OFFSET_BITS_MASK, WF_RSSI_OFFSET_0_BIT_OFFSET, WF_RSSI_OFFSET_BITS_WIDTH, true},
    {EFUSE_NV_SLOT0_ADDR(3), EFUSE_NV_SLOT1_ADDR(3), BT_POWER_OFFSET_DIM, BT_POWER_OFFSET_VALID_BIT_OFFSET, BT_POWER_OFFSET_BITS_MASK, BT_POWER_OFFSET_0_BIT_OFFSET, BT_POWER_OFFSET_BITS_WIDTH, true},
    {EFUSE_NV_SLOT0_ADDR(3), EFUSE_NV_SLOT1_ADDR(3), XO24M_CAP_DIM, XO24M_CAP_VALID_BIT_OFFSET, XO24M_CAP_BITS_MASK, XO24M_CAP_BIT_OFFSET, XO24M_CAP_BITS_WIDTH, true},
};


uint8_t wf_power_offset_en = 0;
int8_t  wf_power_offset_fake_reg[3] = {0};
#ifdef RF_SELF_CALI_FROM_NV
complexint16 nv_tx_pred_table_chan_low[DPD_COMP_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_table_chan_mid[DPD_COMP_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_table_chan_hig[DPD_COMP_TABLE_CNT][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_table_update_chan_low[DPD_COMP_TABLE_CNT_UPDATE][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_table_update_chan_mid[DPD_COMP_TABLE_CNT_UPDATE][MAX_PARALEN] = {0};
complexint16 nv_tx_pred_table_update_chan_hig[DPD_COMP_TABLE_CNT_UPDATE][MAX_PARALEN] = {0};
#endif

#if (defined(RF_SELF_CALI_FROM_NV) || defined(RF_SELF_CALI_WRITE_TO_NV))
#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
#define CMN_FLASH_OTP_REGION 0
uint32_t nv_self_cali_addr = CMN_FLASH_OTP_REGION;
#else
uint32_t nv_self_cali_addr = CMN_FLASH_REGION + 0x200000;
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


static void set_tpc_dig_gain(uint8_t idx, uint16_t dgain)
{
    uint32_t *base_addr = (uint32_t *)TPC_DIG_GAIN_BASE_ADDR;
    uint8_t offset = idx >> 1;
    uint32_t *addr = base_addr + offset;
    uint32_t rdata = MEM_RD32(addr);
    uint8_t lsf = (idx & 0x1) << 4;
    uint32_t mask = 0xfff << lsf;
    uint32_t rval = (rdata >> lsf) & 0xfff;
    uint32_t val = rval * dgain >> 9 << lsf;
    uint32_t wdata = (rdata & (~mask)) | val;

    MEM_WR32(addr, wdata);
}

int8_t nv_efuse_read_mac(uint8_t *mac_addr)
{
    uint32_t read_val = 0;
    uint8_t tmp_mac[6] = {0};
    uint8_t zero_mac[6] = {0};

    EFUSE_RD32(EFUSE_NV_SLOT1_ADDR(0), &read_val);
    memcpy(&tmp_mac[0], &read_val, 4);
    EFUSE_RD32(EFUSE_NV_SLOT1_ADDR(1), &read_val);
    memcpy(&tmp_mac[4], &read_val, 2);
    if (memcmp(zero_mac, tmp_mac, 6)) {
        memcpy(mac_addr, tmp_mac, 6);
        goto rd_ok;
    }
    EFUSE_RD32(EFUSE_NV_SLOT0_ADDR(0), &read_val);
    memcpy(&tmp_mac[0], &read_val, 4);
    EFUSE_RD32(EFUSE_NV_SLOT0_ADDR(1), &read_val);
    memcpy(&tmp_mac[4], &read_val, 2);
    if (memcmp(zero_mac, tmp_mac, 6)) {
        memcpy(mac_addr, tmp_mac, 6);
        goto rd_ok;
    }
    CLOGW("cannot read valid mac addr from efuse\n");
    return -1;
rd_ok:
    // CLOGD("read efuse mac addr " MACSTR "\n", MAC2STR(mac_addr));
    return 0;

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
    uint32_t tmp32 = 0;
    uint8_t tmp_mac[6] = {0};
    uint8_t zero_mac[6] = {0};

    EFUSE_RD32(EFUSE_NV_SLOT0_ADDR(0), &tmp32);
    memcpy(&tmp_mac[0], &tmp32, 4);
    EFUSE_RD32(EFUSE_NV_SLOT0_ADDR(1), &tmp32);
    memcpy(&tmp_mac[4], &tmp32, 2);
    if (!memcmp(zero_mac, tmp_mac, 6)) {
        EFUSE_WR32(EFUSE_NV_SLOT0_ADDR(0), *mac_ptr);
        mac_ptr++;
        EFUSE_WR32(EFUSE_NV_SLOT0_ADDR(1), (*mac_ptr & 0x0000FFFF));
        goto burn_ok;
    }
    EFUSE_RD32(EFUSE_NV_SLOT1_ADDR(0), &tmp32);
    memcpy(&tmp_mac[0], &tmp32, 4);
    EFUSE_RD32(EFUSE_NV_SLOT1_ADDR(1), &tmp32);
    memcpy(&tmp_mac[4], &tmp32, 2);
    if (!memcmp(zero_mac, tmp_mac, 6)) {
        EFUSE_WR32(EFUSE_NV_SLOT1_ADDR(0), *mac_ptr);
        mac_ptr++;
        EFUSE_WR32(EFUSE_NV_SLOT1_ADDR(1), (*mac_ptr & 0x0000FFFF));
        goto burn_ok;
    }
    else {
        CLOGW("efuse space for mac addr full used!\n");
    }
burn_ok:
    CLOGI("burn efuse mac addr " MACSTR "success\n", MAC2STR((uint8_t *)&nv_efuse_cfg_env.mac[0]));
    return 0;
}

int8_t nv_efuse_read_common_item(uint8_t *item, const efuse_cfg_t *cfg, char *fn)
{
    uint32_t tmp32 = 0;

    assert(cfg->bits_width * cfg->field_dim <= 32);

    EFUSE_RD32(cfg->addr1, &tmp32);
    if (tmp32 & (1 << cfg->bit_valid)) //valid == 1
    {
        for (uint8_t i = 0; i < cfg->field_dim; i++) {
            item[i] = (tmp32 >> (cfg->bit_start + cfg->bits_width * i)) & cfg->bits_mask;
            if (cfg->is_signed && (item[i] & (1 << (cfg->bits_width - 1))))
                item[i] |= ~cfg->bits_mask;
        }
        goto rd_ok;
    }
    EFUSE_RD32(cfg->addr0, &tmp32);
    if (tmp32 & (1 << cfg->bit_valid)) //valid == 1
    {
        for (uint8_t i = 0; i < cfg->field_dim; i++) {
            item[i] = (tmp32 >> (cfg->bit_start + cfg->bits_width * i)) & cfg->bits_mask;
            if (cfg->is_signed && (item[i] & (1 << (cfg->bits_width - 1))))
                item[i] |= ~cfg->bits_mask;
        }
        goto rd_ok;
    }
    CLOGW("cannot read valid %s from efuse\n", fn);
    return -1;
rd_ok:
    return 0;
}

int8_t nv_efuse_sync_common_item(void *env_field, uint8_t *item, const efuse_cfg_t *cfg, char *fn)
{
    uint8_t  *byte_field = (uint8_t *)env_field;
    uint16_t *word_field = (uint16_t *)env_field;
    uint32_t *dword_field = (uint32_t *)env_field;
    uint8_t i = 0;

    assert(cfg->bits_width * cfg->field_dim <= 32);
    CLOGD("%s:field_dim=%d, bit_width=%d\n", fn, cfg->field_dim, cfg->bits_width);

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

    assert(cfg->bits_width * cfg->field_dim <= 32);

    EFUSE_RD32(cfg->addr0, &tmp32);
    if (!(tmp32 & (1 << cfg->bit_valid))) //valid == 0
    {
        for (uint8_t i = 0; i < cfg->field_dim; i++)
        {
            if (cfg->bits_width <= 8)
                tmp32 |= (byte_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
            else if (cfg->bits_width <= 16)
                tmp32 |= (word_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
            else
                tmp32 |= (dword_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
        }
        tmp32 |= (1 << cfg->bit_valid); //force valid = 1
        EFUSE_WR32(cfg->addr0, tmp32);
        goto burn_ok;
    }
    EFUSE_RD32(cfg->addr1, &tmp32);
    if (!(tmp32 & (1 << cfg->bit_valid))) //valid == 0
    {
        for (uint8_t i = 0; i < cfg->field_dim; i++)
        {
            if (cfg->bits_width <= 8)
                tmp32 |= (byte_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
            else if (cfg->bits_width <= 16)
                tmp32 |= (word_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
            else
                tmp32 |= (dword_field[i] & cfg->bits_mask) << (cfg->bit_start + cfg->bits_width * i);
        }
        tmp32 |= (1 << cfg->bit_valid); //force valid = 1
        EFUSE_WR32(cfg->addr1, tmp32);
        goto burn_ok;
    }
burn_ok:
    CLOGI("burn efuse %s success\n", fn);
    return 0;
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

    if(!hdr)
        return -1;


    if (hdr->magic != magic_code) {
        CLOGI("NV fix zone magic (%x) mismatch, skip it!\n", hdr->magic);
        return -2;
    }
    calc_crc = crc32_sw(calc_crc, (uint8_t *)(hdr), (sizeof(*hdr) - 4));
    calc_crc = crc32_sw(calc_crc, (uint8_t *)(hdr + 1), hdr->length);
    if (calc_crc != hdr->crc32) {
        CLOGI("NV fix zone crc (%x) check failed, expect (%x) skip it!\n", hdr->crc32, calc_crc);
        return -3;
    }
    // self cali check version
    if ((NV_MAGIC_PATTERN2 == magic_code) && (hdr->version != cal_ver || cal_ver == 0xffff)) {
        CLOGI("NV fix zone version (%x) mismatch, skip it!\n", hdr->version);
        return -4;
    }

    return 0;
}

int8_t nv_fixzone_init()
{
    ls_nv_fixzone_valid_flag = !nv_fixzone_head_check(FIXZONE_NV_BASE_ADDR, NV_MAGIC_PATTERN);
    return 0;
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
    ls_nv_fixzone_body_t *body = (ls_nv_fixzone_body_t *)(FIXZONE_NV_BASE_ADDR+sizeof(ls_nv_fixzone_header_t));

    if (!mac_addr || !ls_nv_fixzone_valid_flag)
        return -1;

    memcpy(mac_addr, body->wf_mac, 6);
    return 0;
}
int8_t nv_fixzone_get_bt_mac(uint8_t *mac_addr)
{
    ls_nv_fixzone_body_t *body = (ls_nv_fixzone_body_t *)(FIXZONE_NV_BASE_ADDR+sizeof(ls_nv_fixzone_header_t));

    if (!mac_addr)
        return -1;
    if (!ls_nv_fixzone_valid_flag) {
        if (nv_efuse_read_mac(mac_addr))
            return -1;
        mac_addr[5] = mac_addr[5] + 1;
        return 0;
    }
    memcpy(mac_addr, body->wf_mac, 6);
    return 0;
}

int8_t nv_fixzone_efuse_load_rf_config(void)
{
#if 1
    int8_t tmp8[4] = {0};
    uint8_t i = 0;

    if (!nv_efuse_read_wf_ppa_cap((uint8_t *)&tmp8[0])) {
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM = tmp8[0];
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_DSSS = tmp8[0];
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM = tmp8[1];
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_DSSS = tmp8[1];
        IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM = tmp8[2];
        IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_WF_2_DSSS = tmp8[2];
    }
    if (!nv_efuse_read_wf_power_offset(&tmp8[0])) {
        for (i = 0; i < 3; i++)
            wf_power_offset_fake_reg[i] = tmp8[i];
        wf_power_offset_en = 1;
    }
    if (!nv_efuse_read_wf_rssi_offset(&tmp8[0])) {
        IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_DSSS_OFFSET = (int16_t)tmp8[0];
        IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_OFDM_OFFSET = (int16_t)tmp8[1];
    }
    if (!nv_efuse_read_xo24m_cap(&tmp8[0])) {
        IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = tmp8[0];
        if (tmp8[0]!= 0)
            IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC = 1;
    }
#endif
    return 0;
}

int8_t nv_fixzone_load_rf_config(void)
{
#if 1
    uint8_t i = 0;
    ls_nv_fixzone_body_t *body = (ls_nv_fixzone_body_t *)(FIXZONE_NV_BASE_ADDR+sizeof(ls_nv_fixzone_header_t));

    nv_fixzone_init();

    if (!ls_nv_fixzone_valid_flag) {
        nv_fixzone_efuse_load_rf_config();
        return 0;
    }

    IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = body->xo_cap;
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM = body->wf_ppa_cap[0];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_DSSS = body->wf_ppa_cap[0];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM = body->wf_ppa_cap[1];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_DSSS = body->wf_ppa_cap[1];
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM = body->wf_ppa_cap[2];
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_WF_2_DSSS = body->wf_ppa_cap[2];
    for (i = 0; i < 19; i++)
    {
        wf_power_table_item_t *p_item = (wf_power_table_item_t *)(&(body->wf_power_table[i]));
        ls_rf_set_wf_ppa_gain(i, p_item->ppa_gain);
        ls_rf_set_wf_abb_gain(i, p_item->abb_gain);
        ls_rf_set_wf_dig_gain(i, p_item->dig_gain);
    }
    for (i = 0; i < 3; i++)
        wf_power_offset_fake_reg[i] = body->wf_power_offset[i];
    wf_power_offset_en = 1;
    IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_DSSS_OFFSET = body->wf_rssi_offset_dsss;
    IP_WIFI_CTRL->REG_WIFI_RSSI_OFFSET.bit.CFG_RSSI_OFDM_OFFSET = body->wf_rssi_offset_ofdm;
    if (body->wf_target_power_channel_ind == 0) {
        ls_tpc_update_tx_power_table((int8_t *)&body->wf_target_power[0], body->wf_target_power_channel_ind, 1);
    }
    else {
        for (i = 0; i < 3; i++)
            ls_tpc_update_tx_power_table((int8_t *)&body->wf_target_power[i], i+1, 1);
    }
#endif
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
    ret = flash_security_read(&cali_flash_dev, nv_self_cali_addr, &nv_self_cali_cfg_buf, sizeof(nv_self_cali_cfg_buf));
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
#if 0 //DEBUG
    //CLOGI("hdr=%p, %x; body=%p, %x\n", hdr, hdr->magic, body, body->rx_rc_cap);
    CLOGI("load rxcali config from NV, rc_cap=%d, dcoc_comp_i=%d, dcoc_comp_q=%d, iq_comp_i=%d, iq_comp_q=%d\n", body->rx_rc_cap, body->rx_dcoc_comp_i, body->rx_dcoc_comp_q, body->rx_iq_comp_i, body->rx_iq_comp_q);
    CLOGI("load txcali config from NV, dc_comp_i=%d, dc_comp_q=%d, iq_comp_i=%d, iq_comp_q=%d\n", body->tx_dc_comp_i, body->tx_dc_comp_q, body->tx_iq_comp_i, body->tx_iq_comp_q);
    for (i = 0; i < DPD_COMP_TABLE_CNT; i++)
    {
        CLOGI("load txdpd config from NV, idx=%d detail:\n", i);
        for (j = 0; j < PREDLEN; j++)
            CLOGI("{%d, %d}", (int16_t)(body->tx_pred_table_chan_low[i][j] & 0xffff), (int16_t)(body->tx_pred_table_chan_low[i][j] >> 16));
        for (j = 0; j < PREDLEN; j++)
            CLOGI("{%d, %d}", (int16_t)(body->tx_pred_table_chan_mid[i][j] & 0xffff), (int16_t)(body->tx_pred_table_chan_mid[i][j] >> 16));
        for (j = 0; j < PREDLEN; j++)
            CLOGI("{%d, %d}", (int16_t)(body->tx_pred_table_chan_hig[i][j] & 0xffff), (int16_t)(body->tx_pred_table_chan_hig[i][j] >> 16));
    }
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
    cali->txiq_tx_result(body->tx_iq_comp_i, body->tx_iq_comp_q);

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
        //Add pwr offset
        if (dpd_cfg_table[i].tssi == dpd_tr_pwr) {
            for (uint8_t i_update = 0; i_update < DPD_COMP_TABLE_CNT_UPDATE; i_update++) {
                uint8_t pwr_step = dpd_tr_pwr - dpd_cfg_table_update[i_update].tssi;
                rf_cali_txdpd_calc_rest_table(&nv_tx_pred_table_update_chan_low[i_update][0], &nv_tx_pred_table_chan_low[i][0], pwr_step);
                rf_cali_txdpd_calc_rest_table(&nv_tx_pred_table_update_chan_mid[i_update][0], &nv_tx_pred_table_chan_mid[i][0], pwr_step);
                rf_cali_txdpd_calc_rest_table(&nv_tx_pred_table_update_chan_hig[i_update][0], &nv_tx_pred_table_chan_hig[i][0], pwr_step);
                #if 0 //DEBUG
                rf_cali_txdpd_print_para(dpd_cfg_table_update[i_update].pred_lut_idx, dpd_cfg_table_update[i_update].tssi, &nv_tx_pred_table_update_chan_low[i_update][0]);
                rf_cali_txdpd_print_para(dpd_cfg_table_update[i_update].pred_lut_idx, dpd_cfg_table_update[i_update].tssi, &nv_tx_pred_table_update_chan_mid[i_update][0]);
                rf_cali_txdpd_print_para(dpd_cfg_table_update[i_update].pred_lut_idx, dpd_cfg_table_update[i_update].tssi, &nv_tx_pred_table_update_chan_hig[i_update][0]);
                #endif
            }
        }
        #if 0 //DEBUG
        CLOGI("load txdpd config from NV, idx=%d detail:", i);
        for (j = 0; j < MAX_PARALEN; j++)
            CLOGI("{%d,%d},", nv_tx_pred_table_chan_low[i][j].re, nv_tx_pred_table_chan_low[i][j].im);
        CLOGI("\n");
        for (j = 0; j < MAX_PARALEN; j++)
            CLOGI("{%d,%d},", nv_tx_pred_table_chan_mid[i][j].re, nv_tx_pred_table_chan_mid[i][j].im);
        CLOGI("\n");
        for (j = 0; j < MAX_PARALEN; j++)
            CLOGI("{%d,%d},", nv_tx_pred_table_chan_hig[i][j].re, nv_tx_pred_table_chan_hig[i][j].im);
        CLOGI("\n");
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
    hdr->length = sizeof(ls_nv_selfcali_body_t);
    CLOGI("nv_selfcali_init call flash_init\n");
    flash_init(&cali_flash_dev, 0, 0);
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
    hdr->version = cal_ver;
    hdr->crc32 = crc32_sw(hdr->crc32, (uint8_t *)(hdr), (sizeof(*hdr) - 4));
    hdr->crc32 = crc32_sw(hdr->crc32, (uint8_t *)(hdr) + sizeof(ls_nv_fixzone_header_t), hdr->length);

    CLOGI("start write protect flash %x\n", nv_self_cali_addr);
    flash_write_protection_set(&cali_flash_dev, false);
#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
    nv_self_cali_addr = 0;
    CLOGI("start erase flash OTP %x\n", nv_self_cali_addr);
    ret = flash_security_erase(&cali_flash_dev, nv_self_cali_addr);
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
        ret = flash_security_write(&cali_flash_dev, nv_self_cali_addr, (void *)write_addr, WR_SEC_LEN);
        if (ret) {
            CLOGE("write selfcali NV config to Flash OTP region failed, ret=%d\n", ret);
            goto write_failed;
        } else
            CLOGI("write selfcali NV config to Flash OTP region success\n");
        nv_self_cali_addr += WR_SEC_LEN;
        write_addr += WR_SEC_LEN;
        write_len -= WR_SEC_LEN;
    }
    ret = flash_security_write(&cali_flash_dev, nv_self_cali_addr, (void *)write_addr, write_len);
    if (ret) {
        CLOGE("write selfcali NV config to Flash OTP region tail failed, ret=%d\n", ret);
        goto write_failed;
    } else
        CLOGI("write selfcali NV config to Flash OTP region tail success\n");
    #if 1 //read back to check
    memset(nv_self_cali_cfg_buf, 0, sizeof(nv_self_cali_cfg_buf));
    nv_self_cali_addr = 0;
    ret = flash_security_read(&cali_flash_dev, nv_self_cali_addr, &nv_self_cali_cfg_buf, sizeof(nv_self_cali_cfg_buf));
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
    ret = flash_erase(&cali_flash_dev, nv_self_cali_addr, sizeof(nv_selfcali_cfg));
    if (ret) {
        CLOGE("erase flash %x failed, ret=%d\n", nv_self_cali_addr, ret);
        goto write_failed;
    }
    else {
        CLOGI("erase flash %x success\n", nv_self_cali_addr);
    }
    ret = flash_write(&cali_flash_dev, nv_self_cali_addr, (uint8_t *)&nv_selfcali_cfg, sizeof(nv_selfcali_cfg));
    if (ret) {
        CLOGE("write flash %x failed, ret=%d\n", nv_self_cali_addr, ret);
        goto write_failed;
    }
    else {
        CLOGI("write flash %x success\n", nv_self_cali_addr);
    }
#endif
    flash_write_protection_set(&cali_flash_dev, true);
    return 0;
write_failed:
    flash_write_protection_set(&cali_flash_dev, true);
    return -1;
}
#endif

int8_t nv_selfcali_erase_otp(void)
{
   int8_t ret = 0;
   
#if (defined(RF_SELF_CALI_FROM_NV) || defined(RF_SELF_CALI_WRITE_TO_NV))
#if defined(CFG_FLASH_IF) && (USE_FLASH_OTP == 1)
    if (!flash_if_check_security_support()) {
        CLOGE("Flash unsupport OTP region!\n");
        return -1;
    }
    nv_self_cali_addr = 0;
    flash_write_protection_set(&cali_flash_dev, false);
    CLOGI("start erase flash OTP %x\n", nv_self_cali_addr);
    ret = flash_security_erase(&cali_flash_dev, nv_self_cali_addr);
    flash_write_protection_set(&cali_flash_dev, true);
    if (ret) {
        CLOGW("erase flash OTP %x failed, ret=%d\n", nv_self_cali_addr, ret);
        return -1;
    }
    else {
        CLOGI("erase flash OTP %x success\n", nv_self_cali_addr);
    }
#endif
#endif
    return ret;
}

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
    nv_self_cali_addr = 0;
    ret = flash_security_read(&cali_flash_dev, nv_self_cali_addr, (void *)&g_magic_code, sizeof(uint32_t));
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
