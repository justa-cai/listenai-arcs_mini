/**
 ****************************************************************************************
 *
 * @file rf_drv.c
 *
 * @brief functions of RF driver
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 30, 2023
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */


#include "log_print.h"
#include "systick.h"
#include "arcs_ap.h"
#include "rf_drv.h"
#include "rf_cali.h"
#include "ClockManager.h"
#include "nv_config.h"
#include "ls_misc.h"
#include "wf_soc_drv.h"

#include "nv_otp.h"

#define RFIF IP_RFIF
#define AON_CTRL IP_AON_CTRL
#define CMN_SYS IP_SYSCTRL
#define CMN_IOMUX IP_CMN_IOMUX
#define GPIOA IP_GPIOA
#define FLASH_NV_BASE_ADDR 0x301FF800

#define RF_PPA_GAIN_WF_BASE_ADDR    ((uint32_t)&RFIF->REG_TX_LOGIC4.all)
#define RF_PPA_GAIN_BT_BASE_ADDR    ((uint32_t)&RFIF->REG_TX_LOGIC1.all)
#define RF_DIG_GAIN_WF_BASE_ADDR    ((uint32_t)&IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_0.all)
#define MEM_RD32(addr)              (*(volatile uint32_t *)(addr))
#define MEM_WR32(addr, value)       (*(volatile uint32_t *)(addr)) = (value)
#define READ8_F(addr_oft) (*(volatile uint8_t *)(FLASH_NV_BASE_ADDR+addr_oft))

extern uint8_t wf_power_offset_en;

static void rf_set_wf_ppa_gain(uint8_t index, uint8_t ppa_val);

static uint8_t g_wf_ppa_gain_table[19] = {
    #include "rf_ppa_gain_cfg.h"
};

extern uint32_t  CALI_MEM_START_ADDR;
extern uint32_t  CALI_MEM_MID_ADDR;
extern uint32_t  CALI_MEM_END_ADDR;
extern uint32_t CALI_MEM_START_OFFSET;
extern uint32_t CALI_MEM_MID_OFFSET;
extern uint32_t CALI_MEM_END_OFFSET;

#if RF_BOARD_VER == 2
int8_t  wf_power_offset_reg[3] = {-4, 0, 8};
#endif

int rf_udelay(uint32_t us)
{
#if 1
    volatile int ret = 0;
    for(int i = 0; i < us; i++)
    {
        for(int j = 0; j < 100; j++)
            ret += j;
    }
    return ret;
#else
    SysTick_Delay_Us(us);
#endif
}

void wf_clk_init(void)
{
    RFIF->REG_CTRL0.bit.WF_START = 1;
    while(!IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_LOCK);
    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ADDABUF_WFCLKEN = 0x1;
#if 0
    RFIF->REG_ADDA_CLKGEN_LOGIC0.bit.REG_RXADC_CLK_SEL_DIG = 0x4;  // fetx_clk=40HMz
    RFIF->REG_ADDA_CLKGEN_LOGIC0.bit.REG_RFDAC_CLK_SEL_DIG = 0x1;  // fedac_clk=160/1=160MHz
    RFIF->REG_ADDA_CLKGEN_LOGIC0.bit.REG_RSSIADC_CLK_SEL_DIG = 0x2;  // rssiadc_clk_out=160/2=80MHz
    //RFIF->REG_ADDA_CLKGEN_LOGIC.bit.CLK_COHERENCE_FORCE  = 0x1;
    //RFIF->REG_ADDA_CLKGEN_LOGIC.bit.REG_CLK_COHERENCE    = 0x1;
#endif
}

void bt_clk_init(void)
{

    //RFIF->REG_CTRL0.bit.WF_START = 1;
    if(IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ENABLE ==0)  //set at bootclk_init
    {
        BBPLL_Init();
    }
    while(!IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_LOCK);

    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ADDABUF_BTCLKEN = 0x1;

    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_BT_HCLK = 0x1;

    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_RFIF_CLK = 0x1;

}

uint8_t rf_get_version(void)
{
    return 1;
}

void rf_por_config(uint8_t rf_ver)
{
    /* Following code piece is generated from Arcs_D0_POR.xls (RF, register/rfif_reg.h) */
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_BIASL_WF_OFDM = 9; // fkxiong@2024-11-18
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_BIASL_WF_DSSS = 10; // fkxiong@2024-11-14
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_DSSS = 15; // fkxiong@2025-01-20
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_DSSS = 12; // fkxiong@2025-01-20
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_WF_2_DSSS = 10; // fkxiong@2025-01-20
    IP_RFIF->REG_RX_LOGIC5.bit.REG_RF_RX_LNA_LG_IB_BT_1 = 8; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC5.bit.REG_RF_RX_LNA_LG_IB_BT_2 = 7; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC5.bit.REG_RF_RX_LNA_LG_IB_BT_3 = 6; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC5.bit.REG_RF_RX_LNA_LG_IB_WF_0 = 13; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC5.bit.REG_RF_RX_LNA_LG_IB_WF_1 = 15; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC6.bit.REG_RF_RX_LNA_LG_IB_WF_2 = 13; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC6.bit.REG_RF_RX_LNA_LG_IB_WF_3 = 11; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC6.bit.REG_RF_RX_LNA_LG_IB_WF_4 = 9; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC52.bit.REG_RF_RX_LNA_LG_ATT_RIN_WF_0 = 7; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC58.bit.REG_RF_RX_LNA_HG_IB_BT_5 = 4; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC58.bit.REG_RF_RX_LNA_HG_IB_BT_6 = 4; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC58.bit.REG_RF_RX_LNA_HG_IB_BT_7 = 4; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC59.bit.REG_RF_RX_LNA_HG_IB_WF_5 = 9; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC59.bit.REG_RF_RX_LNA_HG_IB_WF_6 = 9; // ltnie@2024-11-14
    IP_RFIF->REG_RX_LOGIC59.bit.REG_RF_RX_LNA_HG_IB_WF_7 = 9; // ltnie@2024-11-14
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_BIASH_WF_DSSS = 9; // fkxiong@2024-11-14
    IP_RFIF->REG_TX_REG1.bit.RF_TX_PPA_IN_ATT_RES = 2; // fkxiong@2024-11-18
    IP_RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_SRC_10U_TRIM_OFDM = 30; // fkxiong@2024-11-18
    IP_RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_SRC_10U_TRIM_DSSS = 30; // fkxiong@2024-11-18
    IP_RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_18 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_17 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_16 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_15 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_14 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_13 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_12 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_11 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_10 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_9 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_8 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_7 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_6 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_5 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_4 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_3 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_2 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_1 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_0 = 5; // leifeng@2025-01-21
    IP_RFIF->REG_TX_LOGIC8.bit.REG_RF_TX_PPA_GAIN_WF_18 = 255; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC8.bit.REG_RF_TX_PPA_GAIN_WF_17 = 222; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC8.bit.REG_RF_TX_PPA_GAIN_WF_16 = 210; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC8.bit.REG_RF_TX_PPA_GAIN_WF_15 = 155; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC7.bit.REG_RF_TX_PPA_GAIN_WF_14 = 120; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC7.bit.REG_RF_TX_PPA_GAIN_WF_13 = 95; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC7.bit.REG_RF_TX_PPA_GAIN_WF_12 = 75; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC7.bit.REG_RF_TX_PPA_GAIN_WF_11 = 56; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC6.bit.REG_RF_TX_PPA_GAIN_WF_10 = 45; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC6.bit.REG_RF_TX_PPA_GAIN_WF_9 = 36; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC6.bit.REG_RF_TX_PPA_GAIN_WF_8 = 28; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC6.bit.REG_RF_TX_PPA_GAIN_WF_7 = 23; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC5.bit.REG_RF_TX_PPA_GAIN_WF_6 = 17; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC5.bit.REG_RF_TX_PPA_GAIN_WF_5 = 15; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC5.bit.REG_RF_TX_PPA_GAIN_WF_4 = 11; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC5.bit.REG_RF_TX_PPA_GAIN_WF_3 = 8; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC4.bit.REG_RF_TX_PPA_GAIN_WF_2 = 7; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC4.bit.REG_RF_TX_PPA_GAIN_WF_1 = 5; // leifeng@2025-02-11
    IP_RFIF->REG_TX_LOGIC4.bit.REG_RF_TX_PPA_GAIN_WF_0 = 4; // leifeng@2025-02-11
    IP_RFIF->REG_SX_REG1.bit.RF_SX_PFD_VDDRES = 2; // xyshi1@2024-11-25
    IP_RFIF->REG_SX_REG0.bit.RF_SX_DOUBLER_DELAY = 1; // syhan@2024-12-03
    IP_RFIF->REG_SX_REG0.bit.RF_SX_LDO_OUT = 4; // syhan@2024-12-03
    IP_RFIF->REG_RX_REG3.bit.RF_RX_ABB_1ST_OPA_IBIAS_POW = 4; // ltnie@2024-12-11
    IP_RFIF->REG_TX_LOGIC11.bit.REG_RF_TX_PA_IPTAT_CAS_WF_OFDM = 0; // fkxiong@2024-12-12
    IP_RFIF->REG_TX_LOGIC11.bit.REG_RF_TX_PA_IPTAT_CAS_WF_DSSS = 0; // fkxiong@2024-12-12
    IP_RFIF->REG_TX_LOGIC11.bit.REG_RF_TX_PA_IPTAT_CS_WF_OFDM = 0; // fkxiong@2024-12-12
    IP_RFIF->REG_TX_LOGIC11.bit.REG_RF_TX_PA_IPTAT_CS_WF_DSSS = 0; // fkxiong@2024-12-12
    IP_RFIF->REG_TX_REG1.bit.RF_TX_PA_IPTAT_CAS_BT = 0; // fkxiong@2024-12-12
    IP_RFIF->REG_TX_REG1.bit.RF_TX_PA_IPTAT_CS_BT = 0; // fkxiong@2024-12-12
    IP_RFIF->REG_TX_REG3.bit.RF_TX_ABB_TIA_POW = 5; // fkxiong@2024-12-12
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM = 15; // fkxiong@2025-01-20
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM = 12; // fkxiong@2025-01-20
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM = 10; // fkxiong@2025-01-20
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_0 = 15; // fkxiong@2025-01-20
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_1 = 12; // fkxiong@2025-01-20
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_2 = 10; // fkxiong@2025-01-20
    /* Following code piece is generated from Arcs_D0_POR.xls (modem, register/bt_modem_reg.h) */
    /* Following code piece is generated from Arcs_D0_POR.xls (link, register/ble_reg.h) */
    /* Following code piece is generated from Arcs_D0_POR.xls (aon_ctrl, register/aon_ctrl_reg.h) */
    IP_AON_CTRL->REG_XO24M_CTRL.bit.XO24M_IBIT = 6; // xyshi1@2024-12-04
    IP_AON_CTRL->REG_XO24M_CTRL.bit.XO24M_LDO_OUT = 0; //xiaoyang @2025-04-24
    /* Following code piece is generated from Arcs_D0_POR.xls (CMN_SYSCFG, register/cmn_syscfg_reg.h) */
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RF_TX_PA_BIASL_WF = 1; // fkxiong@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RF_TX_PPA_CAP_SW_BT = 1; // fkxiong@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RF_TX_PPA_CAP_SW_WF = 1; // fkxiong@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RF_TX_PA_CAP_SW_BT = 1; // fkxiong@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RF_TX_PA_CAP_SW_WF = 1; // fkxiong@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RFDAC_SRC_10U_TRIM = 1; // fkxiong@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RF_RX_LNA_CLOAD_WF = 1; // ltnie@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_RF_RX_LNA_CLOAD_BT = 1; // ltnie@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_ADDA_LDOANA_OUT = 1; // haochen@2025-03-24
    IP_CMN_SYS->REG_SYS_EFUSE_SEL.bit.LDEFU_ADDA_LDODIG_OUT = 1; // haochen@2025-03-24
    /* Following code piece is generated from Arcs_D0_POR.xls (CMN_BUSCFG, register/cmn_buscfg_reg.h) */
    IP_SYSNODEF->REG_SYSPLL_CFG2.bit.SYSPLL_VCO_LDO_OUT = 5; // xyshi1@2024-11-14
    IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_VCO_KVCO = 7; // xyshi1@2024-11-25
    IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_PERI_DIV_SEL = 3; // xyshi1@2024-11-25
    /* Following code piece is generated from Arcs_D0_POR.xls (core_iomux, register/core_iomux_reg.h) */
    IP_CMN_IOMUX->REG_PAD_FLASHIO_00.bit.PAD_FLASHIO_00_DRV = 1; // ltnie@2025-02-05
    IP_CMN_IOMUX->REG_PAD_FLASHIO_01.bit.PAD_FLASHIO_01_DRV = 1; // ltnie@2025-02-05
    IP_CMN_IOMUX->REG_PAD_FLASHIO_02.bit.PAD_FLASHIO_02_DRV = 1; // ltnie@2025-02-05
    IP_CMN_IOMUX->REG_PAD_FLASHIO_03.bit.PAD_FLASHIO_03_DRV = 1; // ltnie@2025-02-05
    IP_CMN_IOMUX->REG_PAD_FLASHIO_04.bit.PAD_FLASHIO_04_DRV = 1; // ltnie@2025-02-05
    IP_CMN_IOMUX->REG_PAD_FLASHIO_05.bit.PAD_FLASHIO_05_DRV = 1; // ltnie@2025-02-05
}

void rf_update_ppa_gain()
{
    for (int i = 0; i < 19; i++) {
        rf_set_wf_ppa_gain(i, g_wf_ppa_gain_table[i]);
    }
}

void rf_load_mfg_cali_goldden()
{
#if RF_BOARD_VER == 2 // OTA mode for 淘云DVT2
    IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = 0;
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM = 15;
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM = 12;
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM = 8;
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_DSSS = 15;
    IP_RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_DSSS = 12;
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_WF_2_DSSS = 8;
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_0 = 15;
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_1 = 12;
    IP_RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_BT_2 = 8;
    IP_RFIF->REG_TX_LOGIC11.bit.REG_RF_TX_PA_CAP_SW_WF_2_OFDM = 20;
    IP_RFIF->REG_TX_LOGIC11.bit.REG_RF_TX_PA_CAP_SW_WF_2_DSSS = 20;
    IP_RFIF->REG_TX_LOGIC10.bit.REG_RF_TX_PA_CAP_SW_BT_2 = 20;
#endif
}

void rf_set_ppa_gain_by_idx(uint32_t base_addr, uint8_t byte_pos, uint8_t index, uint8_t ppa_val)
{
    uint8_t offset = byte_pos >> 2;
    uint32_t addr = base_addr + (offset << 2);
    uint32_t rdata = MEM_RD32(addr);
    uint8_t lsf = (3 - (byte_pos & 0x3)) << 3;
    uint32_t mask = 0xff << lsf;
    uint32_t val = ppa_val << lsf;
    uint32_t wdata = (rdata & (~mask)) | val;

    MEM_WR32(addr, wdata);
}

uint8_t rf_get_ppa_gain_by_idx(uint32_t base_addr, uint8_t byte_pos, uint8_t index)
{
    uint8_t offset = byte_pos >> 2;
    uint32_t addr = base_addr + (offset << 2);
    uint32_t rdata = MEM_RD32(addr);
    uint8_t rsf = (3 - (byte_pos & 0x3)) << 3;

    return (uint8_t)(rdata >> rsf);
}

void rf_set_wf_ppa_gain_by_idx(uint32_t base_addr, uint8_t index, uint8_t ppa_val)
{
    uint8_t byte_pos = index + 1;
    rf_set_ppa_gain_by_idx(base_addr, byte_pos, index, ppa_val);
}

uint8_t rf_get_wf_ppa_gain_by_idx(uint32_t base_addr, uint8_t index)
{
    uint8_t byte_pos = index + 1;
    return (rf_get_ppa_gain_by_idx(base_addr, byte_pos, index));
}

static void rf_set_wf_ppa_gain(uint8_t index, uint8_t ppa_val)
{
    if (index >= 19)
        CLOGE("invalid wf ppa gain idx(%d)\n", index);
    else
        rf_set_wf_ppa_gain_by_idx(RF_PPA_GAIN_WF_BASE_ADDR, index, ppa_val);
}

uint8_t rf_get_wf_ppa_gain(uint8_t index)
{
    if (index >= 19) {
        CLOGE("invalid wf ppa gain idx(%d)\n", index);
        return 0;
    }
    else {
        return rf_get_wf_ppa_gain_by_idx(RF_PPA_GAIN_WF_BASE_ADDR, index);
    }
}

void rf_set_bt_ppa_gain_by_idx(uint32_t base_addr, uint8_t index, uint8_t ppa_val)
{
    uint8_t byte_pos = index + 3;
    rf_set_ppa_gain_by_idx(base_addr, byte_pos, index, ppa_val);
}

uint8_t rf_get_bt_ppa_gain_by_idx(uint32_t base_addr, uint8_t index)
{
    uint8_t byte_pos = index + 3;
    return (rf_get_ppa_gain_by_idx(base_addr, byte_pos, index));
}

void rf_set_bt_ppa_gain(uint8_t index, uint8_t ppa_val)
{
    if (index >= 8)
        CLOGE("invalid bt ppa gain idx(%d)\n", index);
    else if (index == 5)
        RFIF->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_5 = ppa_val;
    else if (index == 6)
        RFIF->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_6 = ppa_val;
    else if (index == 7)
        RFIF->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_7 = ppa_val;
    else
        rf_set_bt_ppa_gain_by_idx(RF_PPA_GAIN_BT_BASE_ADDR, index, ppa_val);
}

uint8_t rf_get_bt_ppa_gain(uint8_t index)
{
    if (index >= 8) {
        CLOGE("invalid bt ppa gain idx(%d)\n", index);
        return 0;
    }
    else if (index == 7) {
        return RFIF->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_7;
    }
    else if (index == 6) {
        return RFIF->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_6;
    }
    else if (index == 5) {
        return RFIF->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_5;
    }
    else {
        return rf_get_bt_ppa_gain_by_idx(RF_PPA_GAIN_BT_BASE_ADDR, index);
    }
}

void rf_set_wf_dig_gain_by_idx(uint32_t base_addr, uint8_t index, uint16_t dig_val)
{
    uint8_t offset = index >> 1;
    uint32_t addr = base_addr + (offset << 2);
    uint32_t rdata = MEM_RD32(addr);
    uint8_t lsf = (index & 0x1) << 4;
    uint32_t mask = 0xfff << lsf;
    uint32_t val = dig_val << lsf;
    uint32_t wdata = (rdata & (~mask)) | val;

    MEM_WR32(addr, wdata);
}

uint16_t rf_get_wf_dig_gain_by_idx(uint32_t base_addr, uint8_t index)
{
    uint8_t offset = index >> 1;
    uint32_t addr = base_addr + (offset << 2);
    uint32_t rdata = MEM_RD32(addr);
    uint8_t rsf = (index & 0x1) << 4;
    //CLOGD("get dig gain addr 0x%x, rdata 0x%x, rsf=%d\n", addr, rdata, rsf);
    return (uint16_t)(rdata >> rsf);
}

void rf_set_wf_dig_gain(uint8_t index, uint16_t dig_val)
{
    if (index >= 19)
        CLOGE("invalid wf dig gain idx(%d)\n", index);
    else
        rf_set_wf_dig_gain_by_idx(RF_DIG_GAIN_WF_BASE_ADDR, index, dig_val);
}

uint16_t rf_get_wf_dig_gain(uint8_t index)
{
    if (index >= 19) {
        CLOGE("invalid wf dig gain idx(%d)\n", index);
        return 0;
    }
    else {
        return rf_get_wf_dig_gain_by_idx(RF_DIG_GAIN_WF_BASE_ADDR, index);
    }
}

void rf_set_wf_abb_gain(uint8_t index, uint8_t abb_val)
{
    if (index >= 19)
        CLOGE("invalid wf abb gain idx(%d)\n", index);
    else if (abb_val >= 8)
        CLOGE("invalid wf abb value(%d)\n", abb_val);
    else if (index == 18)
        RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_18 = abb_val;
    else if (index == 17)
        RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_17 = abb_val;
    else if (index == 16)
        RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_16 = abb_val;
    else if (index == 15)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_15 = abb_val;
    else if (index == 14)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_14 = abb_val;
    else if (index == 13)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_13 = abb_val;
    else if (index == 12)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_12 = abb_val;
    else if (index == 11)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_11 = abb_val;
    else if (index == 10)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_10 = abb_val;
    else if (index == 9)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_9 = abb_val;
    else if (index == 8)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_8 = abb_val;
    else if (index == 7)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_7 = abb_val;
    else if (index == 6)
        RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_6 = abb_val;
    else if (index == 5)
        RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_5 = abb_val;
    else if (index == 4)
        RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_4 = abb_val;
    else if (index == 3)
        RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_3 = abb_val;
    else if (index == 2)
        RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_2 = abb_val;
    else if (index == 1)
        RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_1 = abb_val;
    else if (index == 0)
        RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_0 = abb_val;
}

uint8_t rf_get_wf_abb_gain(uint8_t index)
{
    if (index >= 19) {
        CLOGE("invalid wf abb gain idx(%d)\n", index);
        return -1;
    }
    else if (index == 0)
        return RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_0;
    else if (index == 1)
        return RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_1;
    else if (index == 2)
        return RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_2;
    else if (index == 3)
        return RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_3;
    else if (index == 4)
        return RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_4;
    else if (index == 5)
        return RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_5;
    else if (index == 6)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_6;
    else if (index == 7)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_7;
    else if (index == 8)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_8;
    else if (index == 9)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_9;
    else if (index == 10)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_10;
    else if (index == 11)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_11;
    else if (index == 12)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_12;
    else if (index == 13)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_13;
    else if (index == 14)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_14;
    else if (index == 15)
        return RFIF->REG_TX_LOGIC13.bit.REG_RF_TX_ABB_TIA_RFB_WF_15;
    else if (index == 16)
        return RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_16;
    else if (index == 17)
        return RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_17;
    else if (index == 18)
        return RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_TIA_RFB_WF_18;
    return -1;
}

void rf_load_nv_config(void)
{
#if 0
    volatile uint32_t *p_flash_head_flag = (volatile uint32_t *)FLASH_NV_BASE_ADDR;
    volatile uint8_t pa_bias = *(volatile uint8_t *)(FLASH_NV_BASE_ADDR+6);
    volatile uint8_t *p_ppa_cfg = (volatile uint8_t *)(FLASH_NV_BASE_ADDR+8);
    uint8_t i;

    if (*p_flash_head_flag == 0xdeccbbaa) {
        if (pa_bias != 0xff) {
            IP_RFIF->REG_TX_DAC_REG0.bit.RFDAC_SRC_10U_TRIM = READ8_F(4);
            IP_RFIF->REG_TX_REG3.bit.RF_TX_ABB_TIA_RFB_WF = READ8_F(5);
            IP_RFIF->REG_TX_REG1.bit.RF_TX_PA_BIASL_WF = READ8_F(6);
            IP_RFIF->REG_TX_REG0.bit.RF_TX_PPA_BIASL_WF = READ8_F(7);
        }
        for (i = 0; i < 24; i++) {
            rf_set_wf_ppa_gain(i, p_ppa_cfg[i]);
        }
    }
#endif
}

void rf_delay_config(uint32_t multi)
{
    IP_RFIF->REG_DELAY_CTRL0.bit.DELAY1 = RFIF_DELAY1_DEF * multi;

    IP_RFIF->REG_DELAY_CTRL1.bit.DELAY2 = RFIF_DELAY2_DEF * multi;
    IP_RFIF->REG_DELAY_CTRL1.bit.DELAY3 = RFIF_DELAY3_DEF * multi;
    IP_RFIF->REG_DELAY_CTRL1.bit.DELAY4 = RFIF_DELAY4_DEF * multi;
    IP_RFIF->REG_DELAY_CTRL1.bit.DELAY5 = RFIF_DELAY5_DEF * multi;

    IP_RFIF->REG_DELAY_CTRL2.bit.DELAY7 = RFIF_DELAY7_DEF * multi;
    IP_RFIF->REG_DELAY_CTRL2.bit.DELAY8 = RFIF_DELAY8_FINE_TUNE;
    IP_RFIF->REG_DELAY_CTRL2.bit.DELAY9 = RFIF_DELAY9_FINE_TUNE;
}

void rf_war_config(void)
{

    IP_RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BUF_RX_EN = 0x1;  // 1 bits
    IP_RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BUF_RX_EN_FORCE = 0x1;  // 1 bits
#if 0 //Note: only use in RF Tx Performance test
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = 0x1; // 1 bits
    IP_RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0x1;  // 1 bits
#endif
}

const static uint32_t temp_reg_map[TEMP_INTV_NUM][TEMP_REG_NUM] =
{
    /* (-oo, -35) */ {30, 7},
    /* [-35, -25) */ {30, 7},
    /* [-25, -15) */ {30, 6},
    /* [-15, -05) */ {30, 6},
    /* [-05, +05) */ {30, 4},
    /* [+05, +15) */ {0, 4},
    /* [+15, +25) */ {30, 3},
    /* [+25, +35) */ {30, 2},
    /* [+35, +45) */ {30, 1},
    /* [+45, +55) */ {0, 0},
    /* [+55, +65) */ {4, 0},
    /* [+65, +75) */ {8, 0},
    /* [+75, +85) */ {12, 0},
    /* [+85, +95) */ {15, 0},
    /* [+95, +oo) */ {15, 0},
};

// Calculate bias register values based on temperature and reference
void calculate_bias_values(int temp, int ref, int *hd, int *ho, int *ld, int *lo) {
    if (temp == TEMP_NORMAL) {
        *lo = ref;
        *ho = 7;
        *ld = FIXED_POINT_ROUND(1.3 * ref);  // 1.3 * ref, fixed-point rounding
        *hd = *ho + 2;
    } else if (temp == TEMP_LOW) {
        *lo = FIXED_POINT_ROUND(1.3 * ref);  // 1.3 * ref, fixed-point rounding
        *ho = 9;
        *ld = FIXED_POINT_ROUND(1.2 * (*lo));  // 1.2 * lo, fixed-point rounding
        *hd = *ho + 2;
    } else if (temp == TEMP_HIGH) {
        *lo = FIXED_POINT_ROUND(0.9 * ref);  // 0.9 * ref, fixed-point rounding
        *ho = 7;
        *ld = *lo;
        *hd = *ho;
    }
}

// Linear interpolation function
int linear_interpolate(int temp, int temp1, int temp2, int value1, int value2) {
    return value1 + (value2 - value1) * (temp - temp1) / (temp2 - temp1);
}

// Main function to calculate interpolated register values based on temperature
void calculate_interpolated_values(int32_t temp, int32_t ref, int32_t *hd, int32_t *ho, int32_t *ld, int32_t *lo) {
    int hd_n, ho_n, ld_n, lo_n;
    int hd_h, ho_h, ld_h, lo_h;
    int hd_l, ho_l, ld_l, lo_l;

    // Calculate base values for 25°C, -40°C, and 85°C
    calculate_bias_values(TEMP_NORMAL, ref, &hd_n, &ho_n, &ld_n, &lo_n);
    calculate_bias_values(TEMP_LOW, ref, &hd_l, &ho_l, &ld_l, &lo_l);
    calculate_bias_values(TEMP_HIGH, ref, &hd_h, &ho_h, &ld_h, &lo_h);

    CLOGI("25' - hd:%d, ho:%d, ld:%d, lo:%d\n", hd_n, ho_n, ld_n, lo_n);
    CLOGI("-40' - hd:%d, ho:%d, ld:%d, lo:%d\n", hd_l, ho_l, ld_l, lo_l);
    CLOGI("85' - hd:%d, ho:%d, ld:%d, lo:%d\n", hd_h, ho_h, ld_h, lo_h);

    // Perform linear interpolation
    if (temp >= TEMP_NORMAL) {
        temp = (temp > TEMP_HIGH) ? TEMP_HIGH : temp;
        *lo = linear_interpolate(temp, TEMP_NORMAL, TEMP_HIGH, lo_n, lo_h);
        *ho = linear_interpolate(temp, TEMP_NORMAL, TEMP_HIGH, ho_n, ho_h);
        *ld = linear_interpolate(temp, TEMP_NORMAL, TEMP_HIGH, ld_n, ld_h);
        *hd = linear_interpolate(temp, TEMP_NORMAL, TEMP_HIGH, hd_n, hd_h);
    } else if (temp < TEMP_NORMAL) {
        temp = (temp < TEMP_LOW) ? TEMP_LOW : temp;
        *lo = linear_interpolate(temp, TEMP_LOW, TEMP_NORMAL, lo_l, lo_n);
        *ho = linear_interpolate(temp, TEMP_LOW, TEMP_NORMAL, ho_l, ho_n);
        *ld = linear_interpolate(temp, TEMP_LOW, TEMP_NORMAL, ld_l, ld_n);
        *hd = linear_interpolate(temp, TEMP_LOW, TEMP_NORMAL, hd_l, hd_n);
    }

    *lo = (*lo < 0) ? 0 : (*lo > 15 ? 15 : *lo);
    *ho = (*ho < 0) ? 0 : (*ho > 15 ? 15 : *ho);
    *ld = (*ld < 0) ? 0 : (*ld > 15 ? 15 : *ld);
    *hd = (*hd < 0) ? 0 : (*hd > 15 ? 15 : *hd);
}

/* Reserve API here for temp PoR configuration */
void rf_por_temp_config(int32_t temp, uint32_t ref)
{
#if 1
    CLOGI("Current die temperature :%d, ref = %d\n", temp, ref);
    int32_t hd, ho, ld, lo;
    IP_RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_SRC_10U_TRIM_DSSS = temp_reg_map[temp2idx(temp)][0];
    IP_RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_SRC_10U_TRIM_OFDM = temp_reg_map[temp2idx(temp)][0];
    IP_RFIF->REG_TX_REG1.bit.RF_TX_PPA_IN_ATT_RES = temp_reg_map[temp2idx(temp)][1];

    if (!ref) {
        CLOGW("BIAS REF not programmed in efuse :%d\n", ref);
        ref = DEF_BIASL_WF;
    }

    calculate_interpolated_values(temp, ref, &hd, &ho, &ld, &lo);

    CLOGI("HD:%d, HO:%d, LD:%d, LO:%d\n", hd, ho, ld, lo);
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_BIASL_WF_DSSS = ld;
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_BIASL_WF_OFDM = ld;
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_BIASH_WF_DSSS = hd;
    IP_RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_BIASH_WF_OFDM = hd;
    IP_RFIF->REG_TX_REG1.bit.RF_TX_PA_BIASL_BT = lo;
    IP_RFIF->REG_TX_REG1.bit.RF_TX_PA_BIASH_BT = ho;
    if(temp < TEMP_LDO_THRESH) {
        IP_RFIF->REG_SX_REG0.bit.RF_SX_LDO_OUT = 5;
    } else {
        IP_RFIF->REG_SX_REG0.bit.RF_SX_LDO_OUT = 4;
    }
#endif
}

#define BT_CRM_CLKGATEPHYFCTRL0_ADDR   0x4B400010
#define BT_MACBYP_CLKEN_ADDR           0x4B90000C

/// Macro to read a platform register
#define REG_PL_RD(addr)              (*(volatile uint32_t *)(addr))

/// Macro to write a platform register
#define REG_PL_WR(addr, value)       (*(volatile uint32_t *)(addr)) = (value)


void wf_macbyp_clken_set(uint32_t value)
{
    REG_PL_WR(BT_MACBYP_CLKEN_ADDR, value);
}

void wf_crm_rcclkforce_setf(uint8_t rcclkforce)
{
    //ASSERT_ERR((((uint32_t)rcclkforce << 27) & ~((uint32_t)0x08000000)) == 0);
    REG_PL_WR(BT_CRM_CLKGATEPHYFCTRL0_ADDR, (REG_PL_RD(BT_CRM_CLKGATEPHYFCTRL0_ADDR) & ~((uint32_t)0x08000000)) | ((uint32_t)rcclkforce << 27));
}

void rf_init()
{
    CLOGD("==== rf_init start ==========\n");
    CALI_MEM_START_ADDR = (uint32_t)MEM_DUMP_START_ADDR;
    CALI_MEM_MID_ADDR = (uint32_t)MEM_DUMP_MID_ADDR;
    CALI_MEM_END_ADDR = (uint32_t)MEM_DUMP_END_ADDR;
    CALI_MEM_START_OFFSET = ((uint32_t)MEM_DUMP_START_ADDR & ~0x20000000);
    CALI_MEM_MID_OFFSET = ((uint32_t)MEM_DUMP_MID_ADDR & ~0x20000000);
    CALI_MEM_END_OFFSET = ((uint32_t)MEM_DUMP_END_ADDR & ~0x20000000);

    rf_entry.params.version = rf_get_version();
    rf_por_config(rf_entry.params.version);
    rf_update_ppa_gain();
    rf_load_mfg_cali_goldden();
    rf_load_nv_config();
    rf_delay_config(CRM_GetCmn_peri_pclkFreq() / CRM_GetSrcFreq(CRM_IpSrcXtalClk));
    rf_war_config();
    #if defined(WCN_TYPE_WF)
    //update efuse for temperature cali
    ls_read_efuse_temp_para();
    ls_temp_default_por(); //default 26 temp por
    #endif
    wf_clk_init();
    wf_soc_init();

#if defined(WCN_TYPE_WF)
    /* update efuse calibrate data */
    nv_fixzone_load_rf_config();
#endif
#if defined(WCN_TYPE_BT)
    bt_clk_init();
#endif
}

void rf_set_channel(uint16_t freq)
{
    uint16_t intg = freq / 18;
    uint32_t frac = (freq % 18 << 20) / 18;
#ifdef RF_SELF_CALI_FROM_NV
    P_RF_CALI_OPS cali = rf_cali.ops;
#endif
    /* Set LO freq */
    RFIF->REG_SX_REG1.bit.RF_SX_SDM_VDDRES = 0;
    RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_DIVN_INTEG = intg;
    RFIF->REG_SX_LOGIC0.bit.RF_SX_DIVN_INTEG_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIVN_FRAC = frac;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIVN_FRAC_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIG_START_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIG_START = 0;
    rf_udelay(10);
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIG_START = 1;
    RFIF->REG_SX_LOGIC0.bit.RF_SX_DIVN_INTEG_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIVN_FRAC_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIG_START_FORCE = 0;

#if defined(WCN_TYPE_WF)
    /* Set wf_channel level for PA balun cap */
    /* PA_CAP_SW_0 [2412M, 2435M]
     * PA_CAP_SW_1 [2436M, 2459M]
     * PA_CAP_SW_2 [2460M, 2484M]
     * */
#define FREQ_LOW  2430
#define FREQ_HIGH 2455
    if (freq > FREQ_HIGH) {
        RFIF->REG_CTRL0.bit.WF_CHANNEL = 2;
        #if RF_BOARD_VER == 2
        IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = wf_power_offset_reg[2];
        #endif
	if (wf_power_offset_en)
            IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = wf_power_offset_fake_reg[2];
    #ifdef RF_SELF_CALI_FROM_NV
        if (nv_tx_pred_table_chan_hig[0][0].re != 0) {
            for (int i = 0; i < DPD_COMP_TABLE_CNT; i++)
                cali->txdpd_result(dpd_cfg_table[i].pred_lut_idx, (void *)&nv_tx_pred_table_chan_hig[i][0]);
            for (int i = 0; i < DPD_COMP_TABLE_CNT_UPDATE; i++)
                cali->txdpd_result(dpd_cfg_table_update[i].pred_lut_idx, (void *)&nv_tx_pred_table_update_chan_hig[i][0]);
        }
    #endif
    }
    else if (freq > FREQ_LOW) {
        RFIF->REG_CTRL0.bit.WF_CHANNEL = 1;
        #if RF_BOARD_VER == 2
        IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = wf_power_offset_reg[1];
        #endif
        if (wf_power_offset_en)
            IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = wf_power_offset_fake_reg[1];
    #ifdef RF_SELF_CALI_FROM_NV
        if (nv_tx_pred_table_chan_mid[0][0].re != 0) {
            for (int i = 0; i < DPD_COMP_TABLE_CNT; i++)
                cali->txdpd_result(dpd_cfg_table[i].pred_lut_idx, (void *)&nv_tx_pred_table_chan_mid[i][0]);
            for (int i = 0; i < DPD_COMP_TABLE_CNT_UPDATE; i++)
                cali->txdpd_result(dpd_cfg_table_update[i].pred_lut_idx, (void *)&nv_tx_pred_table_update_chan_mid[i][0]);
        }
    #endif
    }
    else {
        RFIF->REG_CTRL0.bit.WF_CHANNEL = 0;
        #if RF_BOARD_VER == 2
        IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = wf_power_offset_reg[0];
        #endif
        if (wf_power_offset_en)
            IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = wf_power_offset_fake_reg[0];
    #ifdef RF_SELF_CALI_FROM_NV
        if (nv_tx_pred_table_chan_low[0][0].re != 0) {
            for (int i = 0; i < DPD_COMP_TABLE_CNT; i++)
                cali->txdpd_result(dpd_cfg_table[i].pred_lut_idx, (void *)&nv_tx_pred_table_chan_low[i][0]);
            for (int i = 0; i < DPD_COMP_TABLE_CNT_UPDATE; i++)
                cali->txdpd_result(dpd_cfg_table_update[i].pred_lut_idx, (void *)&nv_tx_pred_table_update_chan_low[i][0]);
        }
    #endif
    }
#endif
    //CLOGD("rf_set_channel=%d intg=%d frac=%d\n", freq, intg, frac);
}

void bt_rf_set_channel(uint16_t freq)
{
    uint16_t intg = freq / 18;
    uint32_t frac = (freq % 18 << 20) / 18;

    /* Set LO freq */
    RFIF->REG_SX_REG1.bit.RF_SX_SDM_VDDRES = 0;
    RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_DIVN_INTEG = intg;
    RFIF->REG_SX_LOGIC0.bit.RF_SX_DIVN_INTEG_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIVN_FRAC = frac;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIVN_FRAC_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIG_START_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIG_START = 0;
    rf_udelay(10);
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIG_START = 1;

    CLOGD("rf_set_channel=%d intg=%d frac=%d\n", freq, intg, frac);
}

void rf_start_test_tone(uint16_t channel, uint8_t power)
{
    // BT_WF force free, auto end, wifi end
    RFIF->REG_CTRL0.bit.BT_WF_FORCE = 0;
    RFIF->REG_CTRL0.bit.REG_BT_WF = 0;

    RFIF->REG_CTRL0.bit.AUTO_END = 1;
    RFIF->REG_CTRL0.bit.WF_END = 1;

    rf_udelay(100);

    // enable bbpll
    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ENABLE = 1;

    // start auto mode, wifi start
    RFIF->REG_CTRL0.bit.AUTO_EN = 1;
    RFIF->REG_CTRL0.bit.AUTO_MODE = 1;
    RFIF->REG_CTRL0.bit.WF_START = 1;
    RFIF->REG_CTRL0.bit.AUTO_START = 1;

    // set power
    RFIF->REG_TX_LOGIC4.bit.RF_TX_PPA_GAIN_WF_FORCE = 1;
    RFIF->REG_TX_LOGIC4.bit.REG_RF_TX_PPA_GAIN_WF_0 = 10;

    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 1;
    //RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = 1

    RFIF->REG_TX_LOGIC12.bit.RF_TX_ABB_TIA_RFB_WF_FORCE = 1;
    RFIF->REG_TX_LOGIC12.bit.REG_RF_TX_ABB_TIA_RFB_WF_0 = 5;

    RFIF->REG_TX_LOGIC10.bit.RF_TX_PA_CAP_SW_WF_FORCE = 1;
    RFIF->REG_TX_LOGIC10.bit.REG_RF_TX_PA_CAP_SW_WF_0_OFDM = 20;

    RFIF->REG_TX_LOGIC0.bit.RF_TX_PPA_CAP_SW_WF_FORCE = 1;
    RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_DSSS = 10;

    // set channel
    bt_rf_set_channel(channel);

    // force dac value
    RFIF->REG_TX_DAC_LOGIC0.bit.RFDAC_DACIN_I_FORCE = 1;
    RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_DACIN_I = 0;

    RFIF->REG_TX_DAC_LOGIC1.bit.RFDAC_DACIN_Q_FORCE = 1;
    RFIF->REG_TX_DAC_LOGIC1.bit.REG_RFDAC_DACIN_Q = 2048;
}

void rf_stop_test_tone()
{
    //  auto end, wifi end
    RFIF->REG_CTRL0.bit.AUTO_END = 1;
    RFIF->REG_CTRL0.bit.WF_END = 1;
    // disable auto_en, auto_mode
    RFIF->REG_CTRL0.bit.AUTO_EN = 0;
    RFIF->REG_CTRL0.bit.AUTO_MODE = 0;

    // free power force
    RFIF->REG_TX_LOGIC4.bit.RF_TX_PPA_GAIN_WF_FORCE = 0;

    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0;

    RFIF->REG_TX_LOGIC12.bit.RF_TX_ABB_TIA_RFB_WF_FORCE = 0;
    RFIF->REG_TX_LOGIC10.bit.RF_TX_PA_CAP_SW_WF_FORCE = 0;
    RFIF->REG_TX_LOGIC0.bit.RF_TX_PPA_CAP_SW_WF_FORCE = 0;

    // free channel
    RFIF->REG_SX_LOGIC0.bit.RF_SX_DIVN_INTEG_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIVN_FRAC_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIG_START_FORCE = 0;

    // dac value free
    RFIF->REG_TX_DAC_LOGIC0.bit.RFDAC_DACIN_I_FORCE = 0;
    RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_DACIN_I = 0;
    RFIF->REG_TX_DAC_LOGIC1.bit.RFDAC_DACIN_Q_FORCE = 0;
    RFIF->REG_TX_DAC_LOGIC1.bit.REG_RFDAC_DACIN_Q = 0;
}

void rf_sw_reset(void)
{
    RFIF->REG_CTRL0.bit.WF_END = 1;
    rf_udelay(10);
    RFIF->REG_CTRL0.bit.WF_START = 1;
    rf_udelay(10);
    //CLOGD("rf_sw_reset done\n");
}

static void rf_suspend(int32_t rf_mode)
{
    RFIF->REG_CTRL0.bit.WF_END = 1;
}

static void rf_resume(int32_t rf_mode)
{
    RFIF->REG_CTRL0.bit.WF_START = 1;
}

static void rf_update_cal_addr(uint32_t start, uint32_t end)
{
    if (end < start + MEM_DUMP_LEN)
    {
         CLOGE("Not enough mem buffer , not update cali addr\n");
         return;
    }

    CALI_MEM_START_ADDR = (uint32_t)start;
    CALI_MEM_MID_ADDR = (uint32_t)start + 0x2000;
    CALI_MEM_END_ADDR = (uint32_t)start + 0x4000;
    CALI_MEM_START_OFFSET = ((uint32_t)CALI_MEM_START_ADDR & ~0x20000000);
    CALI_MEM_MID_OFFSET = ((uint32_t)CALI_MEM_MID_ADDR & ~0x20000000);
    CALI_MEM_END_OFFSET = ((uint32_t)CALI_MEM_END_ADDR & ~0x20000000);

    CLOGI("start addr %x, mid addr %x end addr %x start offset %x mid offset %x end offset %x \n",CALI_MEM_START_ADDR,CALI_MEM_MID_ADDR,CALI_MEM_END_ADDR,CALI_MEM_START_OFFSET,CALI_MEM_MID_OFFSET,CALI_MEM_END_OFFSET);
}

RF_OPS rf_ops = {
    .init = rf_init,
    .set_channel = rf_set_channel,
    .sw_reset = rf_sw_reset,
    .get_version = rf_get_version,
    .set_wf_ppa_gain = rf_set_wf_ppa_gain,
    .get_wf_ppa_gain = rf_get_wf_ppa_gain,
    .set_bt_ppa_gain = rf_set_bt_ppa_gain,
    .get_bt_ppa_gain = rf_get_bt_ppa_gain,
    .set_wf_abb_gain = rf_set_wf_abb_gain,
    .get_wf_abb_gain = rf_get_wf_abb_gain,
    .set_wf_dig_gain = rf_set_wf_dig_gain,
    .get_wf_dig_gain = rf_get_wf_dig_gain,
    .suspend = rf_suspend,
    .resume  = rf_resume,
    .update_cal_addr = rf_update_cal_addr,
};

RF_ENTRY rf_entry = {
    .params = {
        .version = 0,
        .init_done = 0,
    },
    .ops = &rf_ops,
};

void ls_rf_probe(void)
{

    if (!rf_entry.params.init_done) {
        #if defined(WCN_TYPE_WF)
        extern void wifi_rf_register_cb(RF_OPS *ops);
        wifi_rf_register_cb(&rf_ops);
        #endif
        rf_entry.ops->init();
        rf_entry.params.init_done = 1;
    }
}

void ls_rf_sw_reset(void)
{
    rf_entry.ops->sw_reset();
}

void ls_rf_set_channel(uint16_t freq)
{
    rf_entry.ops->set_channel(freq);
}

uint8_t ls_rf_get_version(void)
{
    return (rf_entry.ops->get_version());
}

void ls_rf_set_wf_ppa_gain(uint8_t index, uint8_t ppa_val)
{
    rf_entry.ops->set_wf_ppa_gain(index, ppa_val);
}

uint8_t ls_rf_get_wf_ppa_gain(uint8_t index)
{
    return (rf_entry.ops->get_wf_ppa_gain(index));
}

void ls_rf_set_wf_abb_gain(uint8_t index, uint8_t abb_val)
{
    rf_entry.ops->set_wf_abb_gain(index, abb_val);
}

uint8_t ls_rf_get_wf_abb_gain(uint8_t index)
{
    return (rf_entry.ops->get_wf_abb_gain(index));
}

void ls_rf_set_wf_dig_gain(uint8_t index, uint8_t dig_val)
{
    rf_entry.ops->set_wf_dig_gain(index, dig_val);
}

uint16_t ls_rf_get_wf_dig_gain(uint8_t index)
{
    return (rf_entry.ops->get_wf_dig_gain(index));
}

void ls_rf_set_bt_ppa_gain(uint8_t index, uint8_t ppa_val)
{
    rf_entry.ops->set_bt_ppa_gain(index, ppa_val);
}

uint8_t ls_rf_get_bt_ppa_gain(uint8_t index)
{
    return (rf_entry.ops->get_bt_ppa_gain(index));
}

int32_t ls_rf_suspend(int32_t rf_mode)
{
    rf_suspend(rf_mode);

    return 0;
}

int32_t ls_rf_resume(int32_t rf_mode)
{
    rf_resume(rf_mode);

    return 0;
}
