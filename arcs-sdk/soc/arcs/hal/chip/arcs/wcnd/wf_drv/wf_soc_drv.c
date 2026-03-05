/**
 ****************************************************************************************
 *
 * @file wf_soc_drv.c
 *
 * @brief functions of WiFi SOC driver
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

static void newriu_rx_overload_prot_en(int rssi)
{
    /* strong RSSI protection for rx */
    IP_NEW_DFE->REG_NEW_DFE_INTERRUPT.bit.IRQ_WB_OVERLOAD_EN = 1;
    IP_NEW_DFE->REG_AGC_TOP_CFG0.bit.REG_MIN_LNA_CODE = 0;
    IP_NEW_DFE->REG_AGC_BO_CFG0.bit.CFG_OVERLOAD_DET_EN = 1;
    IP_NEW_DFE->REG_AGC_BO_CFG0.bit.CFG_OVERLOAD_TH = rssi;
}

void newriu_init(void)
{
    newriu_rx_overload_prot_en(-17);  // real signal strength = -17 +8.5+8 = -0.5dbm
    IP_NEW_DFE->REG_AGC_STAG_CFG6.bit.REG_ABORT_PINTH = 0xab;
    IP_WIFI_CTRL->REG_WIFI_CTRL_AGC_CLK_SEL.bit.CFG_ERSU_LLR_COMBINE = 1;
    IP_WIFI_CTRL->REG_WIFI_CTRL_RXFD_CTRL_0.bit.CFGMUMIMOCHEICEN = 1;
    IP_NEW_DFE->REG_NEW_DFE_CCA4.bit.INBDCCA20PPOWMINDBM = 0x100;

    IP_NEW_DFE->REG_RX_FDIQ_COMP_0.bit.REG_RX_FDIQ_COMP_0 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_0.bit.REG_RX_FDIQ_COMP_1 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_1.bit.REG_RX_FDIQ_COMP_2 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_1.bit.REG_RX_FDIQ_COMP_3 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_2.bit.REG_RX_FDIQ_COMP_4 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_2.bit.REG_RX_FDIQ_COMP_5 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_3.bit.REG_RX_FDIQ_COMP_6 = 0x7ff;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_3.bit.REG_RX_FDIQ_COMP_7 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_4.bit.REG_RX_FDIQ_COMP_8 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_4.bit.REG_RX_FDIQ_COMP_9 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_5.bit.REG_RX_FDIQ_COMP_10 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_5.bit.REG_RX_FDIQ_COMP_11 = 0;
    IP_NEW_DFE->REG_RX_FDIQ_COMP_6.bit.REG_RX_FDIQ_COMP_12 = 0;

    IP_NEW_DFE->REG_AGC_STAG_CFG12.bit.REG_HIUNLOCK_LNA_DELTA = 3;
    IP_NEW_DFE->REG_NEW_DFE_TX_FILT_GAIN.bit.CFG_TXDIGGAINLINDSSS = 0x34;
    /* CFR function */
    IP_NEW_DFE->REG_TX_CFR_TPC0_1_PARA0.bit.REG_CFR_TPC0_THRPOWERLOG2 = 30387; //finetune by haolin@2024-12-30
    IP_NEW_DFE->REG_TX_CFR_TPC0_PARA1_2.bit.REG_CFR_TPC0_THRAMPLINEARSINGLE = 647; //finetune by haolin@2024-12-30
    IP_NEW_DFE->REG_TX_CFR_TPC0_PARA1_2.bit.REG_CFR_TPC0_THRPOWERLINEAR = 409; //finetune by haolin@2024-12-30
    IP_NEW_DFE->REG_TX_CFR_TPC_EN.bit.REG_CFR_TPC_EN = 0; //finetune by haolin@2024-12-30
    IP_NEW_DFE->REG_TX_CFR_COMMON_13.bit.CFGHCEN = 1;
    IP_NEW_DFE->REG_TX_CFR0.bit.CFGCFR0EN = 1;
    IP_NEW_DFE->REG_TX_CFR1.bit.CFGCFR1EN = 1;

    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_0 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_1 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_2 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_3 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_4 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_5 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_6 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_7 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_8 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_9 = 0x7;

    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_10 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_11 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_12 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_13 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_14 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_15 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_16 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_17 = 0x7;
    IP_NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_18 = 0x7;

    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_0 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_1 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_2 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_3 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_4 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_5 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_6 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_7 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_8 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_9 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_10 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_11 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_12 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_13 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_14 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX0.bit.CFG_TPC_CFRIDX_15 = 0x3;
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX1.bit.CFG_TPC_CFRIDX_16 = 0x0; //finetune by haolin@2024-12-30
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX1.bit.CFG_TPC_CFRIDX_17 = 0x0; //finetune by haolin@2024-12-30
    IP_NEW_DFE->REG_TPC_CTRL_CFRIDX1.bit.CFG_TPC_CFRIDX_18 = 0x0; //finetune by haolin@2024-12-30


    //adjust agc start time
    //As delay 9 set max delay for rf start, which also delay start time
    //Some AP send ack (6M/24M) a little earlier, and arcs agc may start late , which cause ack was not received well
    //so start agc a little earlier
    IP_NEW_DFE->REG_AGC_STAG_CFG13.bit.REG_CNT_AT0ST_TH = 2; // by mingwei@2025-11-10

}


void wf_soc_init(void)
{
#if IC_BOARD == 0
    volatile int delay_count = 10000;

    IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_ENABLE = 0x1;
    //printf("Start polling SYSPLL lock...\n");
    while(!IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_LOCK);

    // TODO: Set SYSPLL to proper value with regarding to different platform
    IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_SYSTEM_DIV_SEL = 0x3; // 0:400M, 2:240M, 3:300M

    IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.DIS_SYS_PLL_UNLOCK = 0x1;
    IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 0x1;

    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_RFIF_CLK = 0x1;

    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ENABLE = 0x1;
    IP_SYSNODEF->REG_PLL_CTRL0.bit.MPLL_TTG_ENABLE = 0x1;
    while(--delay_count>0);
    IP_RFIF->REG_CTRL0.bit.WF_START = 1;
    //printf("Start polling BBPLL lock...\n");
    while(!IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_LOCK);

    IP_RFIF->REG_ADDA_CLKGEN_LOGIC0.bit.REG_RXADC_CLK_SEL_DIG_WF = 0x4;
    IP_RFIF->REG_ADDA_CLKGEN_LOGIC0.bit.REG_RSSIADC_CLK_SEL_DIG_WF = 0x2;
    IP_RFIF->REG_ADDA_CLKGEN_LOGIC1.bit.REG_RFDAC_CLK_SEL_DIG_WF = 0x2;

    //    IP_NEW_DFE->REG_NEW_DFE_ADC_LPF1_SFO.bit.NEW_RXIQDELPATHI0 = 0x0;
    //    IP_NEW_DFE->REG_NEW_DFE_ADC_LPF1_SFO.bit.NEW_RXIQDELPATHQ0 = 0x0;
    //    IP_NEW_DFE->REG_NEW_DFE_WB_FULL_RSSI.bit.NEW_RSSIRXIQDELPATHI0 = 0x0;
    //    IP_NEW_DFE->REG_NEW_DFE_WB_FULL_RSSI.bit.NEW_RSSIRXIQDELPATHQ0 = 0x0;
    //    IP_NEW_DFE->REG_NEW_DFE_TX_DAC.bit.NEW_TXIQDELPATHI0 = 0x0;
    //    IP_NEW_DFE->REG_NEW_DFE_TX_DAC.bit.NEW_TXIQDELPATHQ0 = 0x0;
#else
    /* Following code piece is generated from Arcs_D0_POR.xls (new_dfe, register/new_dfe_reg.h) */
    IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_5.bit.REG_CFR_POST_DIG_GAIN_10 = 512; // haolin@2024-11-14
    IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_6.bit.REG_CFR_POST_DIG_GAIN_12 = 512; // haolin@2024-11-14
    IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_7.bit.REG_CFR_POST_DIG_GAIN_14 = 512; // haolin@2024-11-14
    IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_8.bit.REG_CFR_POST_DIG_GAIN_16 = 512; // haolin@2024-11-14
    IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_8.bit.REG_CFR_POST_DIG_GAIN_17 = 645; // haolin@2024-11-26
    IP_NEW_DFE->REG_CFR_POST_DIG_GAIN_9.bit.REG_CFR_POST_DIG_GAIN_18 = 723; // haolin@2024-11-26
    IP_NEW_DFE->REG_TX_CFR_COMMON_13.bit.REG_CFR_HC_MCS8_EN = 0x1; // leifeng@2025-10-20
    IP_NEW_DFE->REG_TX_CFR_COMMON_13.bit.REG_CFR_HC_MCS9_EN = 0x1; // leifeng@2025-10-20
    IP_NEW_DFE->REG_TX_CFR0.bit.REG_CFR0_MCS8_EN = 0x1; // leifeng@2025-10-20
    IP_NEW_DFE->REG_TX_CFR0.bit.REG_CFR0_MCS9_EN = 0x1; // leifeng@2025-10-20
    IP_NEW_DFE->REG_TX_CFR1.bit.REG_CFR1_MCS8_EN = 0x1; // leifeng@2025-10-20
    IP_NEW_DFE->REG_TX_CFR1.bit.REG_CFR1_MCS9_EN = 0x1; // leifeng@2025-10-20
    IP_NEW_DFE->REG_AGC_TOP_CFG1.bit.REG_PATHLOSS = 44; // mingwei@2024-11-28
    IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_MAX_POWER = 88; // haolin@2025-01-20
    IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_MIN_POWER = 200; // haolin@2025-01-20
    IP_NEW_DFE->REG_AGC_STAG_CFG13.bit.REG_CNT_AT0ST_TH = 5; // mingwei@2025-02-15
    IP_NEW_DFE->REG_AGC_STAG_CFG0.bit.REG_LNA_DELTA_HI = 3; // mingwei@2025-02-15
    IP_NEW_DFE->REG_DAC_PRE_AND_POST_DC_COMP.bit.DSSS_AHEAD_TX_DC_FORCE_EN = 1; // haolin@2025-02-18
    IP_NEW_DFE->REG_DAC_PRE_AND_POST_DC_COMP.bit.OFDM_AHEAD_TX_DC_FORCE_EN = 1; // haolin@2025-02-18
    IP_NEW_DFE->REG_AGC_STAG_CFG2.bit.REG_NOPKT_2ST_TH = 169; // mingwei@2025-03-09
    IP_NEW_DFE->REG_AGC_STAG_CFG2.bit.REG_FULLSAT_TH = 3; // mingwei@2025-03-09
    IP_NEW_DFE->REG_AGC_STAG_CFG1.bit.REG_VGA_CODE_LOWEST = 34; // mingwei@2025-03-09
    IP_NEW_DFE->REG_AGC_STAG_CFG3.bit.REG_FULL_LOTH = 83; // mingwei@2025-03-09
    /* Following code piece is generated from Arcs_D0_POR.xls (WIFI_CTRL, register/wifi_ctrl_reg.h) */
    IP_WIFI_CTRL->REG_WIFI_CTRL_MACCG_OPT_BYPASS.bit.CFG_WIFI_TBTXPWR_OFFSET_EN = 0; // linhao@2025-03-10
    /* Following code piece is generated from Arcs_D0_POR.xls (WIFI_MAC, register/wifi_mac_reg.h) */
    /* Some ceva register does not support byte read/write, thus keep it inside wifi lib */
    // IP_WIFI_MAC_CORE->REG_MAXPOWERLEVELREG.bit.OFDMMINPWRLEVEL = 216; // linhao@2025-03-10
    /* TBD: Move following registers to PoR
     * Ajust TPC range from [-10dBm, +23dBm] to [-11dBm, +22dBm] related to DPD
     * calibration table 14dBm, 16dBm, 18dBm power levels */
    //IP_NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_ADDR_OFFSET = -1;
    IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC = 1;
    //IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = 5;
#endif
}

