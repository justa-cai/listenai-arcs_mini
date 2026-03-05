/**
 ****************************************************************************************
 *
 * @file rf_drv_cali.c
 *
 * @brief functions of RF calibraion driver
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Sep 15, 2023
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */


#include "log_print.h"
#include "systick.h"
#include "arcs_ap.h"
#include "rf_fxp.h"
#include "DpdEst.h"
#include "rf_cali.h"
#include <string.h>

//#define __STATIC static
#define __STATIC

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

#define RFIF IP_RFIF
#define AON_CTRL IP_AON_CTRL
#define SYS_NODFT IP_SYSNODEF
#define BT_MODEM IP_BT_MODEM
#define NEW_DFE IP_NEW_DFE
#define WIFI_CRM IP_WIFI_CRM
#define WIFI_MAC_CORE IP_WIFI_MAC_CORE
#define WIFI_MAC_PL IP_WIFI_MAC_PL
#define WIFI_CTRL IP_WIFI_CTRL

extern uint32_t  CALI_MEM_START_ADDR;
extern uint32_t  CALI_MEM_MID_ADDR;
extern uint32_t  CALI_MEM_END_ADDR;
extern uint32_t CALI_MEM_START_OFFSET;
extern uint32_t CALI_MEM_MID_OFFSET;
extern uint32_t CALI_MEM_END_OFFSET;


#define HW_TIMEOUT 8000
//#define REG_ADDR_CALIBR_MEM_DATA ((uint32_t)&NEW_DFE->REG_CALIBR_MEM_DATA)
#define REG_ADDR_PUMPING_MEM_DATA (&(NEW_DFE->REG_PUMPING_DATA))
#define RXDCOC_DAC_WF_STEPS 25
#define RXDCOC_DAC_BT_STEPS 12

#define RX_DCOC_CALI 3
#define RX_RC_CALI 4
#define RX_IQ_CALI 5
#define TX_SQR_CALI 6
#define TX_IQ_TTG_CALI 7
#define TX_DPD_CALI 8

#define MEM_RD32(addr)              (*(volatile uint32_t *)(addr))
#define MEM_WR32(addr, value)       (*(volatile uint32_t *)(addr)) = (value)

#define DC_SIGN_BIT                    12
#define I_SQRT_SIGN_BIT                20
#define Q_SQRT_SIGN_BIT				   20
#define I_MULT_Q_SIGN_BIT			   21
#define SIGN(q, bit) (((q) >= (1 << (bit) >> 1)) ? ((q) - (1 << (bit))) : (q))
#define SIGN24(q) SIGN((q), 24)
#define SIGN28(q) SIGN((q), 28)

#define CALI_EST_BYPASS_LEN 100

#if RFCALI_WF_EN
int8_t wf_cali_trigger_nodump(void);
#define WF_CALI_TRIGGER_NODUMP do { \
    if (wf_cali_trigger_nodump()) \
    return -1; \
} while (0)
#endif
#if RFCALI_BT_EN
int8_t bt_cali_trigger_nodump(void);
#define BT_CALI_TRIGGER_NODUMP do { \
    if (bt_cali_trigger_nodump()) \
    return -1; \
} while (0)
#endif

#if RFCALI_WF_EN
/* Ni=1 0.625MHz */
const uint32_t wf_rxrc_low_data[128] = {
    0x007f0080,
    0x007f0006,
    0x007f000d,
    0x007f0013,
    0x007e0019,
    0x007c001f,
    0x007a0025,
    0x0079002b,
    0x00760031,
    0x00740037,
    0x0071003c,
    0x006e0042,
    0x006a0047,
    0x0067004c,
    0x00630051,
    0x005f0056,
    0x005b005b,
    0x0056005f,
    0x00510063,
    0x004c0067,
    0x0047006a,
    0x0042006e,
    0x003c0071,
    0x00370074,
    0x00310076,
    0x002b0079,
    0x0025007a,
    0x001f007c,
    0x0019007e,
    0x0013007f,
    0x000d007f,
    0x0006007f,
    0x0080007f,
    0x0086007f,
    0x008d007f,
    0x0093007f,
    0x0099007e,
    0x009f007c,
    0x00a5007a,
    0x00ab0079,
    0x00b10076,
    0x00b70074,
    0x00bc0071,
    0x00c2006e,
    0x00c7006a,
    0x00cc0067,
    0x00d10063,
    0x00d6005f,
    0x00db005b,
    0x00df0056,
    0x00e30051,
    0x00e7004c,
    0x00ea0047,
    0x00ee0042,
    0x00f1003c,
    0x00f40037,
    0x00f60031,
    0x00f9002b,
    0x00fa0025,
    0x00fc001f,
    0x00fe0019,
    0x00ff0013,
    0x00ff000d,
    0x00ff0006,
    0x00ff0080,
    0x00ff0086,
    0x00ff008d,
    0x00ff0093,
    0x00fe0099,
    0x00fc009f,
    0x00fa00a5,
    0x00f900ab,
    0x00f600b1,
    0x00f400b7,
    0x00f100bc,
    0x00ee00c2,
    0x00ea00c7,
    0x00e700cc,
    0x00e300d1,
    0x00df00d6,
    0x00db00db,
    0x00d600df,
    0x00d100e3,
    0x00cc00e7,
    0x00c700ea,
    0x00c200ee,
    0x00bc00f1,
    0x00b700f4,
    0x00b100f6,
    0x00ab00f9,
    0x00a500fa,
    0x009f00fc,
    0x009900fe,
    0x009300ff,
    0x008d00ff,
    0x008600ff,
    0x008000ff,
    0x000600ff,
    0x000d00ff,
    0x001300ff,
    0x001900fe,
    0x001f00fc,
    0x002500fa,
    0x002b00f9,
    0x003100f6,
    0x003700f4,
    0x003c00f1,
    0x004200ee,
    0x004700ea,
    0x004c00e7,
    0x005100e3,
    0x005600df,
    0x005b00db,
    0x005f00d6,
    0x006300d1,
    0x006700cc,
    0x006a00c7,
    0x006e00c2,
    0x007100bc,
    0x007400b7,
    0x007600b1,
    0x007900ab,
    0x007a00a5,
    0x007c009f,
    0x007e0099,
    0x007f0093,
    0x007f008d,
    0x007f0086
};

/* Ni=18 11.25MHz */
const uint32_t wf_rxrc_high_data[128] = {
    0x007f0080,
    0x00510063,
    0x0099007e,
    0x00f1003c,
    0x00f600b1,
    0x00a500fa,
    0x004700ea,
    0x007f008d,
    0x005b005b,
    0x008d007f,
    0x00ea0047,
    0x00fa00a5,
    0x00b100f6,
    0x003c00f1,
    0x007e0099,
    0x00630051,
    0x0080007f,
    0x00e30051,
    0x00fe0099,
    0x00bc00f1,
    0x003100f6,
    0x007a00a5,
    0x006a0047,
    0x000d007f,
    0x00db005b,
    0x00ff008d,
    0x00c700ea,
    0x002500fa,
    0x007600b1,
    0x0071003c,
    0x0019007e,
    0x00d10063,
    0x00ff0080,
    0x00d100e3,
    0x001900fe,
    0x007100bc,
    0x00760031,
    0x0025007a,
    0x00c7006a,
    0x00ff000d,
    0x00db00db,
    0x000d00ff,
    0x006a00c7,
    0x007a0025,
    0x00310076,
    0x00bc0071,
    0x00fe0019,
    0x00e300d1,
    0x008000ff,
    0x006300d1,
    0x007e0019,
    0x003c0071,
    0x00b10076,
    0x00fa0025,
    0x00ea00c7,
    0x008d00ff,
    0x005b00db,
    0x007f000d,
    0x0047006a,
    0x00a5007a,
    0x00f60031,
    0x00f100bc,
    0x009900fe,
    0x005100e3,
    0x007f0080,
    0x00510063,
    0x0099007e,
    0x00f1003c,
    0x00f600b1,
    0x00a500fa,
    0x004700ea,
    0x007f008d,
    0x005b005b,
    0x008d007f,
    0x00ea0047,
    0x00fa00a5,
    0x00b100f6,
    0x003c00f1,
    0x007e0099,
    0x00630051,
    0x0080007f,
    0x00e30051,
    0x00fe0099,
    0x00bc00f1,
    0x003100f6,
    0x007a00a5,
    0x006a0047,
    0x000d007f,
    0x00db005b,
    0x00ff008d,
    0x00c700ea,
    0x002500fa,
    0x007600b1,
    0x0071003c,
    0x0019007e,
    0x00d10063,
    0x00ff0080,
    0x00d100e3,
    0x001900fe,
    0x007100bc,
    0x00760031,
    0x0025007a,
    0x00c7006a,
    0x00ff000d,
    0x00db00db,
    0x000d00ff,
    0x006a00c7,
    0x007a0025,
    0x00310076,
    0x00bc0071,
    0x00fe0019,
    0x00e300d1,
    0x008000ff,
    0x006300d1,
    0x007e0019,
    0x003c0071,
    0x00b10076,
    0x00fa0025,
    0x00ea00c7,
    0x008d00ff,
    0x005b00db,
    0x007f000d,
    0x0047006a,
    0x00a5007a,
    0x00f60031,
    0x00f100bc,
    0x009900fe,
    0x005100e3
};
const uint32_t wf_txiq_ttg_data[128] = {
    0x000001b6,
    0x00ce0182,
    0x016c00f3,
    0x01b4002b,
    0x01957f58,
    0x01167ead,
    0x00557e52,
    0x7f817e5d,
    0x7eca7eca,
    0x7e5d7f81,
    0x7e520055,
    0x7ead0116,
    0x7f580195,
    0x002b01b4,
    0x00f3016c,
    0x018200ce,
    0x01b60000,
    0x01827f32,
    0x00f37e94,
    0x002b7e4c,
    0x7f587e6b,
    0x7ead7eea,
    0x7e527fab,
    0x7e5d007f,
    0x7eca0136,
    0x7f8101a3,
    0x005501ae,
    0x01160153,
    0x019500a8,
    0x01b47fd5,
    0x016c7f0d,
    0x00ce7e7e,
    0x00007e4a,
    0x7f327e7e,
    0x7e947f0d,
    0x7e4c7fd5,
    0x7e6b00a8,
    0x7eea0153,
    0x7fab01ae,
    0x007f01a3,
    0x01360136,
    0x01a3007f,
    0x01ae7fab,
    0x01537eea,
    0x00a87e6b,
    0x7fd57e4c,
    0x7f0d7e94,
    0x7e7e7f32,
    0x7e4a0000,
    0x7e7e00ce,
    0x7f0d016c,
    0x7fd501b4,
    0x00a80195,
    0x01530116,
    0x01ae0055,
    0x01a37f81,
    0x01367eca,
    0x007f7e5d,
    0x7fab7e52,
    0x7eea7ead,
    0x7e6b7f58,
    0x7e4c002b,
    0x7e9400f3,
    0x7f320182,
    0x000001b6,
    0x00ce0182,
    0x016c00f3,
    0x01b4002b,
    0x01957f58,
    0x01167ead,
    0x00557e52,
    0x7f817e5d,
    0x7eca7eca,
    0x7e5d7f81,
    0x7e520055,
    0x7ead0116,
    0x7f580195,
    0x002b01b4,
    0x00f3016c,
    0x018200ce,
    0x01b60000,
    0x01827f32,
    0x00f37e94,
    0x002b7e4c,
    0x7f587e6b,
    0x7ead7eea,
    0x7e527fab,
    0x7e5d007f,
    0x7eca0136,
    0x7f8101a3,
    0x005501ae,
    0x01160153,
    0x019500a8,
    0x01b47fd5,
    0x016c7f0d,
    0x00ce7e7e,
    0x00007e4a,
    0x7f327e7e,
    0x7e947f0d,
    0x7e4c7fd5,
    0x7e6b00a8,
    0x7eea0153,
    0x7fab01ae,
    0x007f01a3,
    0x01360136,
    0x01a3007f,
    0x01ae7fab,
    0x01537eea,
    0x00a87e6b,
    0x7fd57e4c,
    0x7f0d7e94,
    0x7e7e7f32,
    0x7e4a0000,
    0x7e7e00ce,
    0x7f0d016c,
    0x7fd501b4,
    0x00a80195,
    0x01530116,
    0x01ae0055,
    0x01a37f81,
    0x01367eca,
    0x007f7e5d,
    0x7fab7e52,
    0x7eea7ead,
    0x7e6b7f58,
    0x7e4c002b,
    0x7e9400f3,
    0x7f320182
};
#endif

#if RFCALI_BT_EN
/* Ni=2 0.375MHz */
const uint32_t bt_rxrc_low_data[128] = {
    0x00ff0000,
    0x00ff008d,
    0x00fe0099,
    0x00fa00a5,
    0x00f600b1,
    0x00f100bc,
    0x00ea00c7,
    0x00e300d1,
    0x00db00db,
    0x00d100e3,
    0x00c700ea,
    0x00bc00f1,
    0x00b100f6,
    0x00a500fa,
    0x009900fe,
    0x008d00ff,
    0x000000ff,
    0x000d00ff,
    0x001900fe,
    0x002500fa,
    0x003100f6,
    0x003c00f1,
    0x004700ea,
    0x005100e3,
    0x005b00db,
    0x006300d1,
    0x006a00c7,
    0x007100bc,
    0x007600b1,
    0x007a00a5,
    0x007e0099,
    0x007f008d,
    0x007f0000,
    0x007f000d,
    0x007e0019,
    0x007a0025,
    0x00760031,
    0x0071003c,
    0x006a0047,
    0x00630051,
    0x005b005b,
    0x00510063,
    0x0047006a,
    0x003c0071,
    0x00310076,
    0x0025007a,
    0x0019007e,
    0x000d007f,
    0x0000007f,
    0x008d007f,
    0x0099007e,
    0x00a5007a,
    0x00b10076,
    0x00bc0071,
    0x00c7006a,
    0x00d10063,
    0x00db005b,
    0x00e30051,
    0x00ea0047,
    0x00f1003c,
    0x00f60031,
    0x00fa0025,
    0x00fe0019,
    0x00ff000d,
    0x00ff0000,
    0x00ff008d,
    0x00fe0099,
    0x00fa00a5,
    0x00f600b1,
    0x00f100bc,
    0x00ea00c7,
    0x00e300d1,
    0x00db00db,
    0x00d100e3,
    0x00c700ea,
    0x00bc00f1,
    0x00b100f6,
    0x00a500fa,
    0x009900fe,
    0x008d00ff,
    0x000000ff,
    0x000d00ff,
    0x001900fe,
    0x002500fa,
    0x003100f6,
    0x003c00f1,
    0x004700ea,
    0x005100e3,
    0x005b00db,
    0x006300d1,
    0x006a00c7,
    0x007100bc,
    0x007600b1,
    0x007a00a5,
    0x007e0099,
    0x007f008d,
    0x007f0000,
    0x007f000d,
    0x007e0019,
    0x007a0025,
    0x00760031,
    0x0071003c,
    0x006a0047,
    0x00630051,
    0x005b005b,
    0x00510063,
    0x0047006a,
    0x003c0071,
    0x00310076,
    0x0025007a,
    0x0019007e,
    0x000d007f,
    0x0000007f,
    0x008d007f,
    0x0099007e,
    0x00a5007a,
    0x00b10076,
    0x00bc0071,
    0x00c7006a,
    0x00d10063,
    0x00db005b,
    0x00e30051,
    0x00ea0047,
    0x00f1003c,
    0x00f60031,
    0x00fa0025,
    0x00fe0019,
    0x00ff000d
};

/* Ni=12 2.25MHz */
const uint32_t bt_rxrc_high_data[128] = {
    0x00ff0000,
    0x00ea00c7,
    0x00b100f6,
    0x001900fe,
    0x005b00db,
    0x007e0099,
    0x00760031,
    0x0047006a,
    0x0000007f,
    0x00c7006a,
    0x00f60031,
    0x00fe0099,
    0x00db00db,
    0x009900fe,
    0x003100f6,
    0x006a00c7,
    0x007f0000,
    0x006a0047,
    0x00310076,
    0x0099007e,
    0x00db005b,
    0x00fe0019,
    0x00f600b1,
    0x00c700ea,
    0x000000ff,
    0x004700ea,
    0x007600b1,
    0x007e0019,
    0x005b005b,
    0x0019007e,
    0x00b10076,
    0x00ea0047,
    0x00ff0000,
    0x00ea00c7,
    0x00b100f6,
    0x001900fe,
    0x005b00db,
    0x007e0099,
    0x00760031,
    0x0047006a,
    0x0000007f,
    0x00c7006a,
    0x00f60031,
    0x00fe0099,
    0x00db00db,
    0x009900fe,
    0x003100f6,
    0x006a00c7,
    0x007f0000,
    0x006a0047,
    0x00310076,
    0x0099007e,
    0x00db005b,
    0x00fe0019,
    0x00f600b1,
    0x00c700ea,
    0x000000ff,
    0x004700ea,
    0x007600b1,
    0x007e0019,
    0x005b005b,
    0x0019007e,
    0x00b10076,
    0x00ea0047,
    0x00ff0000,
    0x00ea00c7,
    0x00b100f6,
    0x001900fe,
    0x005b00db,
    0x007e0099,
    0x00760031,
    0x0047006a,
    0x0000007f,
    0x00c7006a,
    0x00f60031,
    0x00fe0099,
    0x00db00db,
    0x009900fe,
    0x003100f6,
    0x006a00c7,
    0x007f0000,
    0x006a0047,
    0x00310076,
    0x0099007e,
    0x00db005b,
    0x00fe0019,
    0x00f600b1,
    0x00c700ea,
    0x000000ff,
    0x004700ea,
    0x007600b1,
    0x007e0019,
    0x005b005b,
    0x0019007e,
    0x00b10076,
    0x00ea0047,
    0x00ff0000,
    0x00ea00c7,
    0x00b100f6,
    0x001900fe,
    0x005b00db,
    0x007e0099,
    0x00760031,
    0x0047006a,
    0x0000007f,
    0x00c7006a,
    0x00f60031,
    0x00fe0099,
    0x00db00db,
    0x009900fe,
    0x003100f6,
    0x006a00c7,
    0x007f0000,
    0x006a0047,
    0x00310076,
    0x0099007e,
    0x00db005b,
    0x00fe0019,
    0x00f600b1,
    0x00c700ea,
    0x000000ff,
    0x004700ea,
    0x007600b1,
    0x007e0019,
    0x005b005b,
    0x0019007e,
    0x00b10076,
    0x00ea0047
};
#endif

struct _restore_regs {
    volatile uint32_t *addr;
    volatile uint32_t value;
    volatile uint32_t mask;
};

__STATIC struct _restore_regs cali_restore_reg_list[] = {
    {&RFIF->REG_SX_LOGIC0.all,          0x00000087,    0x000fffff},
    {&RFIF->REG_SX_LOGIC1.all,          0x00066665,    0x01fffffc},
    {&RFIF->REG_RX_LOGIC0.all,          0x00000000,    0xffffffff},
    {&RFIF->REG_RX_LOGIC1.all,          0x00000000,    0xffffffff},
    {&RFIF->REG_RX_REG1.all,            0x1a112490,    0xffffffff},
    {&RFIF->REG_RSV_REG0.all,           0x00000000,    0x00000020},
    {&RFIF->REG_RX_LOGIC2.all,          0x00000008,    0xffffffff},
    {&RFIF->REG_RX_LOGIC3.all,          0x0000000a,    0xffffffff},
    {&RFIF->REG_RX_LOGIC4.all,          0x0000000a,    0xffffffff},
    {&RFIF->REG_TX_LOGIC0.all,          0x00000000,    (RFIF_TX_LOGIC0_RF_TX_PPA_EN_FORCE_Msk|RFIF_TX_LOGIC0_REG_RF_TX_PPA_EN_Msk)},
    {&RFIF->REG_TX_LOGIC9.all,          0x00000000,    0x3fff0000},
    {&NEW_DFE->REG_NEW_DFE_NOTCH_BT.all,             0x00000000,    0x00000003},
    {&NEW_DFE->REG_RX_DOWNSAMPLE_EN.all,             0x00000000,    NEW_DFE_RX_DOWNSAMPLE_EN_REG_RX_DOWNSAMPLE_EN_Msk},
    {&NEW_DFE->REG_RX_FDIQ_COMP_EN.all,              0x00000000,    NEW_DFE_RX_FDIQ_COMP_EN_REG_RX_FDIQ_COMP_EN_Msk},
    {&NEW_DFE->REG_RX_CALIBR_FREQSHIFT.all,          0x00000000,    NEW_DFE_RX_CALIBR_FREQSHIFT_RX_CALIBR_FO_BYPASS_Msk},
    {&NEW_DFE->REG_DFE_SPUR_CANCEL_CTRL1.all,        0x00000000,    NEW_DFE_DFE_SPUR_CANCEL_CTRL1_CFG_WIFI_RX_NOTCH_FILTER_EN_FORCE_EN_Msk},
    {&NEW_DFE->REG_DFE_SPUR_CANCEL_CTRL0.all,        0x00000000,    NEW_DFE_DFE_SPUR_CANCEL_CTRL0_CFG_SPUR_CHANNEL_EN_Msk},
    {&NEW_DFE->REG_NEW_DFE_HPF1_PART1.all,           0x23f2853f,    NEW_DFE_NEW_DFE_HPF1_PART1_REG_HPF_EN_Msk},
    {&NEW_DFE->REG_AGC_BO_CFG0.all,                  0x00000000,    NEW_DFE_AGC_BO_CFG0_CFG_OVERLOAD_DET_EN_Msk},
    {&WIFI_CTRL->REG_WIFI_CALIB_RAMIF_DUMP_EN.all,    0x00000000,   WIFI_CTRL_WIFI_CALIB_RAMIF_DUMP_EN_CFG_CALIB_RAMIF_DUMP_EN_Msk},
    {&NEW_DFE->REG_DFE_EST_CTRL_EN.all,              0x00000000,    NEW_DFE_DFE_EST_CTRL_EN_REG_DFE_EST_RESULT_SHIFT_EN_Msk},
    {&NEW_DFE->REG_TPC_CTRL_COMMON.all,              0x00000000,    NEW_DFE_TPC_CTRL_COMMON_CFG_TPC_PWR_OFFSET_Msk},
    {&NEW_DFE->REG_NEW_DFE_TX_FILT_GAIN.all,         0x00000000,    NEW_DFE_NEW_DFE_TX_FILT_GAIN_CFG_NEW_TXMASK_FLT_EN_Msk},
    {&NEW_DFE->REG_TX_CFR_COMMON_13.all,             0x00000000,    NEW_DFE_TX_CFR_COMMON_13_CFGHCEN_Msk},
    {&NEW_DFE->REG_TX_CFR0.all,                      0x00000000,    NEW_DFE_TX_CFR0_CFGCFR0EN_Msk},
    {&NEW_DFE->REG_TX_CFR1.all,                      0x00000000,    NEW_DFE_TX_CFR1_CFGCFR1EN_Msk},
    {&NEW_DFE->REG_TPC_CTRL_CFREN0.all,              0x00000000,    0x3fffffff},
    {&NEW_DFE->REG_TPC_CTRL_CFREN1.all,              0x00000000,    0x07ffffff},
    {&NEW_DFE->REG_CFR_POST_DIG_GAIN_8.all,          0x00000000,    0xffffffff},
    {&NEW_DFE->REG_CFR_POST_DIG_GAIN_9.all,          0x00000000,    0xffffffff},
    {(volatile uint32_t *)0x4B800854, 0x00000000}, //scramble seed
};

static int16_t reg_rxiq_cfg0 = 0;
static int16_t reg_rxiq_cfg1 = 2048;
static uint16_t reg_sx_intg = 0x87;
static uint32_t reg_sx_flac = 0;
static uint8_t reg_wf_channel = 0;

__STATIC void save_reg_config(void)
{
    uint16_t i = 0;

    for (i = 0; i < ARRAY_SIZE(cali_restore_reg_list); i++)
    {
        cali_restore_reg_list[i].value = MEM_RD32(cali_restore_reg_list[i].addr);
    }
}

__STATIC void restore_reg_config(void)
{
    uint16_t i = 0;
    uint32_t new_value = 0;

    for (i = 0; i < ARRAY_SIZE(cali_restore_reg_list); i++)
    {
        new_value = MEM_RD32(cali_restore_reg_list[i].addr);
        new_value &= ~(cali_restore_reg_list[i].mask);
        new_value |= cali_restore_reg_list[i].value & cali_restore_reg_list[i].mask;
        MEM_WR32(cali_restore_reg_list[i].addr, new_value);
        rf_udelay(10);
    }
}

__STATIC void wf_phy_sw_reset()
{
    uint8_t i = 0;
    /*Note: workaround solution 10 times phy_sw_reset to makesure PHY reset valid for ArcsC0 */
    for (i = 0; i < 1; i++)
    {
        uint32_t phy_sw_reset_addr = (uint32_t) &(WIFI_CRM->REG_RSTCTRL);
        MEM_WR32(phy_sw_reset_addr, 0x111);
        rf_udelay(10);
        MEM_WR32(phy_sw_reset_addr, 0x0);
    }
    rf_udelay(10);
}

__STATIC void wf_cali_work_en(uint8_t enb)
{
    NEW_DFE->REG_CALIBR_TOP_0.bit.CALIBR_WORK_EN = enb;
    rf_udelay(10);
}
__STATIC void set_dcoc_force(uint8_t enable)
{
    RFIF->REG_RX_LOGIC7.bit.RF_RX_ABB_DCOC_DACI_FORCE = enable;
    RFIF->REG_RX_LOGIC29.bit.RF_RX_ABB_DCOC_DACQ_FORCE = enable;
}

__STATIC void set_dcoc_dac(uint32_t base_addr, uint8_t byte_pos, uint8_t dac_v)
{
    uint8_t offset = byte_pos >> 2;
    uint32_t addr = base_addr + (offset << 2);
    uint32_t rdata = MEM_RD32(addr);
    uint8_t lsf = (3 - (byte_pos & 0x3)) << 3;
    uint32_t mask = 0xff << lsf;
    uint32_t val = dac_v << lsf;
    uint32_t wdata = (rdata & (~mask)) | val;

    MEM_WR32(addr, wdata);
}

#if RFCALI_WF_EN
__STATIC void set_dcoc_daci_wf_bank0(uint8_t index, uint8_t dac_i)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC16;
    uint8_t byte_pos = index + 1;

    set_dcoc_dac(base_addr, byte_pos, dac_i);
}

__STATIC void set_dcoc_daci_wf_bank1(uint8_t index, uint8_t dac_i)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC22;
    uint8_t byte_pos = index + 2;

    set_dcoc_dac(base_addr, byte_pos, dac_i);
}

__STATIC void set_dcoc_dacq_wf_bank0(uint8_t index, uint8_t dac_q)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC38;
    uint8_t byte_pos = index + 1;

    set_dcoc_dac(base_addr, byte_pos, dac_q);
}

__STATIC void set_dcoc_dacq_wf_bank1(uint8_t index, uint8_t dac_q)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC44;
    uint8_t byte_pos = index + 2;

    set_dcoc_dac(base_addr, byte_pos, dac_q);
}

__STATIC void wf_set_dcoc_daci(uint8_t index, uint8_t dac_i)
{
    set_dcoc_daci_wf_bank0(index, dac_i);
    set_dcoc_daci_wf_bank1(index, dac_i);
}

__STATIC void wf_set_dcoc_dacq(uint8_t index, uint8_t dac_q)
{
    set_dcoc_dacq_wf_bank0(index, dac_q);
    set_dcoc_dacq_wf_bank1(index, dac_q);
}
#endif

#if RFCALI_BT_EN
__STATIC void bt_set_dcoc_daci_low(uint8_t index, uint8_t dac_i)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC7;
    uint8_t byte_pos = index + 1;
    set_dcoc_dac(base_addr, byte_pos, dac_i);
}

__STATIC void bt_set_dcoc_daci_mid(uint8_t index, uint8_t dac_i)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC10;
    uint8_t byte_pos = index + 1;
    set_dcoc_dac(base_addr, byte_pos, dac_i);
}

__STATIC void bt_set_dcoc_daci_high(uint8_t index, uint8_t dac_i)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC13;
    uint8_t byte_pos = index + 1;
    set_dcoc_dac(base_addr, byte_pos, dac_i);
}

__STATIC void bt_set_dcoc_daci_gen(uint8_t base, uint8_t index, uint8_t dac_i)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC7;
    uint8_t byte_pos = index + 1;

    if (base == 1)
    {
        base_addr = (uint32_t)&RFIF->REG_RX_LOGIC10;
    }
    else if (base == 2)
    {
        base_addr = (uint32_t)&RFIF->REG_RX_LOGIC13;
    }

    set_dcoc_dac(base_addr, byte_pos, dac_i);
}

__STATIC void bt_set_dcoc_dacq_low(uint8_t index, uint8_t dac_q)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC29;
    uint8_t byte_pos = index + 1;
    set_dcoc_dac(base_addr, byte_pos, dac_q);
}

__STATIC void bt_set_dcoc_dacq_mid(uint8_t index, uint8_t dac_q)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC32;
    uint8_t byte_pos = index + 1;
    set_dcoc_dac(base_addr, byte_pos, dac_q);
}

__STATIC void bt_set_dcoc_dacq_high(uint8_t index, uint8_t dac_q)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC35;
    uint8_t byte_pos = index + 1;
    set_dcoc_dac(base_addr, byte_pos, dac_q);
}

__STATIC void bt_set_dcoc_dacq_gen(uint8_t base, uint8_t index, uint8_t dac_q)
{
    uint32_t base_addr = (uint32_t)&RFIF->REG_RX_LOGIC29;
    uint8_t byte_pos = index + 1;

    if (base == 1)
    {
        base_addr = (uint32_t)&RFIF->REG_RX_LOGIC32;
    }
    else if (base == 2)
    {
        base_addr = (uint32_t)&RFIF->REG_RX_LOGIC35;
    }

    set_dcoc_dac(base_addr, byte_pos, dac_q);
}

__STATIC void bt_set_dcoc_daci(uint8_t index, uint8_t dac_i)
{
    bt_set_dcoc_daci_low(index, dac_i);
    bt_set_dcoc_daci_mid(index, dac_i);
    bt_set_dcoc_daci_high(index, dac_i);
}

__STATIC void bt_set_dcoc_dacq(uint8_t index, uint8_t dac_q)
{
    bt_set_dcoc_dacq_low(index, dac_q);
    bt_set_dcoc_dacq_mid(index, dac_q);
    bt_set_dcoc_dacq_high(index, dac_q);
}

void bt_get_dcoc_word(RF_CALI_RXDCOC_WORD *dcoc_word)
{
    dcoc_word->dac_sc_i = IP_RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_ABB_DCOC_DAC_SC_1;
    dcoc_word->dac_sc_q = IP_RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_ABB_DCOC_DAC_SCQ_1;
    dcoc_word->dac_i = RFIF->REG_RX_LOGIC7.bit.REG_RF_RX_ABB_DCOC_DACI_BT_LOW_00;
    dcoc_word->dac_q = RFIF->REG_RX_LOGIC29.bit.REG_RF_RX_ABB_DCOC_DACQ_BT_LOW_00;
}

void bt_set_dcoc_word(RF_CALI_RXDCOC_WORD (*dcoc_word_arr)[3], uint8_t row, uint8_t col)
{
    set_sc_i(RFCALI_MODE_BT, dcoc_word_arr[0][0].dac_sc_i);
    set_sc_q(RFCALI_MODE_BT, dcoc_word_arr[0][0].dac_sc_q);
    for (uint8_t i=0; i<row; i++)
    {
        for (uint8_t j=0; j<col; j++)
        {
            for (uint8_t k=0; k<4; k++)
            {
                bt_set_dcoc_daci_gen(i, j*4+k, dcoc_word_arr[i][j].dac_i);
                bt_set_dcoc_dacq_gen(i, j*4+k, dcoc_word_arr[i][j].dac_q);
            }
        }
    }
}

#endif

__STATIC void set_abb_cap_wf(uint8_t cap_val)
{
    RFIF->REG_RX_LOGIC54.bit.REG_RF_RX_ABB_CAP_WF = cap_val;
}

void set_abb_cap_bt(uint8_t cap_val)
{
    RFIF->REG_RX_LOGIC54.bit.REG_RF_RX_ABB_CAP_BT = cap_val;
}

__STATIC void set_abb_cap(uint8_t cap_val)
{
    set_abb_cap_wf(cap_val);
#if RFCALI_BT_EN
    set_abb_cap_bt(cap_val + 5);
    g_bt_rxrc_comp = cap_val + 5;
#endif
}
__STATIC uint8_t get_abb_cap_wf(void)
{
    return(RFIF->REG_RX_LOGIC54.bit.REG_RF_RX_ABB_CAP_WF);
}

__STATIC uint8_t get_abb_cap_bt(void)
{
    return(RFIF->REG_RX_LOGIC54.bit.REG_RF_RX_ABB_CAP_BT);
}

__STATIC uint8_t get_abb_cap(void)
{
    return get_abb_cap_wf();
}

__STATIC void set_lna_gain(uint8_t index)
{
    RFIF->REG_RX_LOGIC2.bit.REG_RF_RX_LNA_GC = index;
    RFIF->REG_RX_LOGIC2.bit.RF_RX_LNA_GC_FORCE = 1;
}

__STATIC void set_abb_bq_gain(uint8_t index)
{
    RFIF->REG_RX_LOGIC3.bit.REG_RF_RX_ABB_BQ_GC = index;
    RFIF->REG_RX_LOGIC3.bit.RF_RX_ABB_BQ_GC_FORCE = 1;
}

__STATIC void set_abb_buf_gain(uint8_t index)
{
    RFIF->REG_RX_LOGIC4.bit.REG_RF_RX_ABB_BUF_GC = index;
    RFIF->REG_RX_LOGIC4.bit.RF_RX_ABB_BUF_GC_FORCE = 1;
}

__STATIC void set_abb_1st_gain(uint8_t index)
{
    RFIF->REG_RX_LOGIC52.bit.REG_RF_RX_ABB_1ST_GC_WF_0 = index;
    RFIF->REG_RX_LOGIC52.bit.RF_RX_ABB_1ST_GC_WF_FORCE = 1;
}

__STATIC void set_rx_gain(uint8_t lna, uint8_t abb_bq, uint8_t abb_buf)
{
    set_lna_gain(lna);
    set_abb_bq_gain(abb_bq);
    set_abb_buf_gain(abb_buf);
}

__STATIC uint8_t get_abb_bq_gain()
{
    return RFIF->REG_RX_LOGIC3.bit.REG_RF_RX_ABB_BQ_GC;
}

__STATIC uint8_t get_abb_buf_gain()
{
    return RFIF->REG_RX_LOGIC4.bit.REG_RF_RX_ABB_BUF_GC;
}

__STATIC uint8_t get_abb_1st_gain()
{
    return RFIF->REG_RX_LOGIC52.bit.REG_RF_RX_ABB_1ST_GC_WF_0;
}

__STATIC void set_tx_ppa_gain(uint8_t gain_idx)
{

}

__STATIC void enb_ttgpll(void)
{
    SYS_NODFT->REG_BBPLL_CFG2.bit.BBPLL_TO_TTG_CLK_EN = 1;
    SYS_NODFT->REG_PLL_CTRL0.bit.MPLL_TTG_ENABLE = 1;
}

__STATIC void dis_ttgpll(void)
{
    SYS_NODFT->REG_BBPLL_CFG2.bit.BBPLL_TO_TTG_CLK_EN = 0;
    SYS_NODFT->REG_PLL_CTRL0.bit.MPLL_TTG_ENABLE = 0;
}

void set_sc_i(uint8_t mode, uint8_t sc_i)
{
    if (mode == RFCALI_MODE_WF)
        IP_RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_ABB_DCOC_DAC_SC_0 = sc_i;
    else
        IP_RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_ABB_DCOC_DAC_SC_1 = sc_i;
}

void set_sc_q(uint8_t mode, uint8_t sc_q)
{
    if (mode == RFCALI_MODE_WF)
        IP_RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_ABB_DCOC_DAC_SCQ_0 = sc_q;
    else
        IP_RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_ABB_DCOC_DAC_SCQ_1 = sc_q;
}

#if RFCALI_WF_EN
void wf_cali_set_channel(uint16_t freq)
{
    rf_set_channel_sx(freq);
}

__STATIC void wf_cali_cmn_rf_init(uint8_t cali_mode, uint16_t freq, uint8_t lna_gain, uint8_t abb_bq, uint8_t abb_buf)
{
    wf_cali_work_en(0);
    RFIF->REG_CTRL0.bit.SW_RST = 1;
    rf_udelay(10);
    RFIF->REG_CTRL0.bit.SW_RST = 0;
    rf_udelay(10);
    NEW_DFE->REG_CALIBR_TOP_0.bit.CALIBR_MODE = cali_mode;
    NEW_DFE->REG_CALIBR_TOP_0.bit.CALIBR_WORK_EN = 1;
    NEW_DFE->REG_NEW_DFE_NOTCH_BT.bit.CALIB_AGC_ON_EN_FORCE = 1;
    NEW_DFE->REG_NEW_DFE_NOTCH_BT.bit.CALIB_AGC_ON_EN = 1;
    set_rx_gain(lna_gain, abb_bq, abb_buf);
    wf_cali_set_channel(freq);
    /* Note: calibration should has this channel settle initial time */
    rf_udelay(100);
    //CLOGD("cali_cmn_rf_init\n");
}

__STATIC void wf_m1_est_init(uint8_t src_sel, uint8_t len_pow)
{
    NEW_DFE->REG_M1_DUMP_AND_BYPASS_LEN.bit.REG_DFE_M1_BYPASS_LEN = 1000;
    NEW_DFE->REG_M1_DUMP_AND_BYPASS_LEN.bit.REG_DFE_M1_DUMP_TAIL_LEN_POWER = len_pow;
    NEW_DFE->REG_DFE_EST_LEN.bit.REG_EST_BYPASS_LEN = CALI_EST_BYPASS_LEN;
    NEW_DFE->REG_DFE_EST_LEN.bit.REG_EST_LEN_POWER = len_pow;
    NEW_DFE->REG_M0_M1_DUMP_AND_TRIG_SEL.bit.REG_M1_DUMP_SEL = src_sel;
    NEW_DFE->REG_M0_M1_DUMP_AND_TRIG_SEL.bit.REG_M1_DUMP_TRIG_SEL = 4;
    NEW_DFE->REG_DFE_EST_TRIG_SEL.bit.REG_EST_TRIG_SEL = 2;
    NEW_DFE->REG_DFE_SHAREMEM_LEN_0.bit.REG_DFE_SHAREMEM_LENGTH_0 = 0;
    NEW_DFE->REG_DFE_SHAREMEM_LEN_1.bit.REG_DFE_SHAREMEM_LENGTH_1 = 0;
    return;
}

#endif

#if RFCALI_BT_EN
__STATIC void bt_cali_cmn_rf_init(uint8_t cali_mode, uint16_t freq, uint8_t lna_gain, uint8_t abb_bq, uint8_t abb_buf)
{
    RFIF->REG_CTRL0.bit.SW_RST = 1;
    rf_udelay(10);
    RFIF->REG_CTRL0.bit.SW_RST = 0;
    rf_udelay(10);

    BT_MODEM->REG_TOP_CFG0.bit.TOP_CALIBR_WORK_EN = 0;
    BT_MODEM->REG_TOP_CFG0.bit.TOP_CALIBR_MODE = cali_mode;
    BT_MODEM->REG_TOP_CFG0.bit.TOP_CALIBR_WORK_EN = 1;
    BT_MODEM->REG_BT_CALIB_LEN.bit.RX_SW_AGC_GAIN_INV = 2048;
    BT_MODEM->REG_BT_RX_GLB_CFG.bit.RX_CALIB_EN = 1;

    /* set 4096 samples */
    BT_MODEM->REG_BT_CALIB_LEN.bit.RX_DC_EST_LEN = 0x4;
    BT_MODEM->REG_BT_CALIB_LEN.bit.RX_IQ_EST_LEN = 0x3;
    BT_MODEM->REG_CMM_CFG0.bit.CMM_MEMLEN_0 = 0;
    BT_MODEM->REG_CMM_CFG0.bit.CMM_MEMLEN_1 = 127;
    BT_MODEM->REG_CALIB_CFG0.all = 1;
    set_rx_gain(lna_gain, abb_bq, abb_buf);
    bt_rf_set_channel(freq);
    /*Note: RF initial time */
    rf_udelay(50);
    //CLOGD("cali_cmn_rf_init\n");
}
#endif

#if RFCALI_WF_EN
int8_t wf_cali_trigger_nodump(void)
{
    volatile int8_t read_val = NEW_DFE->REG_DFE_EST_DUMP_TRIG.bit.REG_DFE_EST_DUMP_TRIG;
    NEW_DFE->REG_DFE_EST_DUMP_TRIG.bit.REG_DFE_EST_DUMP_TRIG = ~read_val;
    return 0;
}

__STATIC int8_t wf_cali_wait_iq_done(void)
{
    uint32_t timeout = 0;

    NEW_DFE->REG_DFE_EST_DUMP_EN.bit.REG_DFE_EST_DUMP_EN = 1;
    rf_udelay(10); //FixMe: important delay need here
    wf_cali_trigger_nodump();
    /*
      * WAR: To avoid the previous calibration DFE_DCI_DCQ_RESULT_VLD_RPT value affect the next
      * calibration, it should add some delay here.
      */
    rf_udelay(CALI_EST_BYPASS_LEN / 80);
    do {
        timeout++;
        rf_udelay(10);

        if (timeout > HW_TIMEOUT) {
            CLOGI("wait calibration complete timeout!\n");
            goto cali_fail;
        }
    } while (NEW_DFE->REG_DUMP_EST_RESULT_VLD_RPT.bit.DFE_DCI_DCQ_RESULT_VLD_RPT != 1);
    CLOGD("wait calibration complete(%d)\n", timeout);
    NEW_DFE->REG_DFE_EST_DUMP_EN.bit.REG_DFE_EST_DUMP_EN = 0;
    return 0;
cali_fail:
    return -1;
}

__STATIC int8_t wf_cali_wait_i2_q2_done(void)
{
    uint32_t timeout = 0;

    NEW_DFE->REG_DFE_EST_DUMP_EN.bit.REG_DFE_EST_DUMP_EN = 1;
    rf_udelay(10); //FixMe: important delay need here
    wf_cali_trigger_nodump();
    /*
      * WAR: To avoid the previous calibration DFE_DCI_DCQ_RESULT_VLD_RPT value affect the next
      * calibration, it should add some delay here.
      */
    rf_udelay(CALI_EST_BYPASS_LEN / 80);
    do {
        timeout++;
        rf_udelay(10);
        //CLOGD("wait calibration complete...\n");
        if (timeout > HW_TIMEOUT) {
            CLOGI("wait calibration complete timeout!\n");
            goto cali_fail;
        }
    } while (NEW_DFE->REG_DUMP_EST_RESULT_VLD_RPT.bit.DFE_I2_Q2_IQ_RESULT_VLD_RPT != 1);
    CLOGD("calibration completed(%d)!\n", timeout);
    NEW_DFE->REG_DFE_EST_DUMP_EN.bit.REG_DFE_EST_DUMP_EN = 0;
    return 0;
cali_fail:
    return -1;
}
#endif

#if RFCALI_BT_EN
int8_t bt_cali_trigger_nodump(void)
{
    uint32_t timeout = 0;

    BT_MODEM->REG_CALIB_CFG0.all = 1;
    do {
        timeout++;
        rf_udelay(1);
        //CLOGD("wait calibration complete...\n");
        if (timeout > HW_TIMEOUT) {
            CLOGD("wait calibration complete timeout!\n");
            goto cali_fail;
        }

    } while (!BT_MODEM->REG_CALIB_CFG0.bit.CALIBR_DONE);
    //CLOGD("calibration completed(%d)!\n", timeout);
    return 0;
cali_fail:
    return -1;
}
#endif

#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxdcoc_init(uint16_t freq, uint8_t lna_gain)
{
    wf_cali_cmn_rf_init(RX_DCOC_CALI/*mode*/, freq/*freq*/, 5/*lna*/, 13/*abb_bq*/, 10/*abb_buf*/);
    wf_m1_est_init(4, 14);
    set_sc_i(RFCALI_MODE_WF, 2);
    set_sc_q(RFCALI_MODE_WF, 2);
    set_dcoc_force(1);
    wf_set_dcoc_daci(0, 0);
    wf_set_dcoc_dacq(0, 0);
    return 0;
}
#endif

#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxdcoc_init(uint16_t freq, uint8_t lna_gain)
{
    bt_cali_cmn_rf_init(RX_DCOC_CALI/*mode*/, freq/*channel*/, lna_gain/*lna*/, 11/*abb_bq*/, 0/*None*/);
    set_dcoc_force(1);
    bt_set_dcoc_daci(0, 0);
    bt_set_dcoc_dacq(0, 0);
    return 0;
}
#endif

#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxdcoc_measure(int32_t *dc_i, int32_t *dc_q, int8_t word_i, int8_t word_q)
{
    wf_set_dcoc_daci(0, word_i);
    wf_set_dcoc_dacq(0, word_q);
    wf_cali_wait_iq_done();
    if (dc_i)
        *dc_i = SIGN28(NEW_DFE->REG_EST_RESULT_I_RPT.bit.DFE_EST_I_RPT);
    if (dc_q)
        *dc_q = SIGN28(NEW_DFE->REG_EST_RESULT_Q_RPT.bit.DFE_EST_Q_RPT);
    //CLOGD("dc_i=%d\n", *dc_i);
    //CLOGD("dc_q=%d\n", *dc_q);

    return 0;
}
#endif

#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxdcoc_measure(int32_t *dc_i, int32_t *dc_q, int8_t word_i, int8_t word_q)
{
    bt_set_dcoc_daci(0, word_i);
    bt_set_dcoc_dacq(0, word_q);
    BT_CALI_TRIGGER_NODUMP;
    if (dc_i)
        *dc_i = SIGN(BT_MODEM->REG_BT_RX_GLB_CFG0.bit.RX_DC_EST2SOFT_I, DC_SIGN_BIT);
    if (dc_q)
        *dc_q = SIGN(BT_MODEM->REG_BT_RX_GLB_CFG0.bit.RX_DC_EST2SOFT_Q, DC_SIGN_BIT);
    //CLOGD("dc_i=%d\n", *dc_i);
    //CLOGD("dc_q=%d\n", *dc_q);
    return 0;
}
#endif

#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxdcoc_detect_interference(int8_t *word_i, int8_t *word_q)
{
    uint32_t read_val_l, read_val_h;
    int64_t read_val_i, read_val_q;

    /* Check if the interference is strong. */
    wf_set_dcoc_daci(0, *word_i);
    wf_set_dcoc_dacq(0, *word_q);
    wf_cali_wait_i2_q2_done();
    read_val_l = NEW_DFE->REG_EST_RESULT_I2_L_RPT.bit.DFE_EST_I2_L_RPT;
    read_val_h = NEW_DFE->REG_EST_RESULT_I2_H_RPT.bit.DFE_EST_I2_H_RPT;
    read_val_i = ((int64_t)read_val_h << 32) + read_val_l;
    read_val_l = NEW_DFE->REG_EST_RESULT_Q2_L_RPT.bit.DFE_EST_Q2_L_RPT;
    read_val_h = NEW_DFE->REG_EST_RESULT_Q2_H_RPT.bit.DFE_EST_Q2_H_RPT;
    read_val_q = ((int64_t)read_val_h << 32) + read_val_l;
    // Set the threshold to 300000, it's about -20dBm for RF signal.
    if (read_val_i > 300000 || read_val_q > 300000) {
        //If the interference is strong, it should discard the results.
        CLOGI("RXDCOC cali failed: %lld %lld \n", read_val_i, read_val_q);
        return -1;
    }
    return 0;
}

__STATIC int8_t wf_cali_rxdcoc_result(int8_t *word_i, int8_t *word_q, uint8_t len)
{
    uint8_t i = 0;
    set_dcoc_force(0);
    if (len < RXDCOC_DAC_WF_STEPS)  {
        for (i = 0; i < RXDCOC_DAC_WF_STEPS; i++)
        {
            wf_set_dcoc_daci(i, *word_i);
            wf_set_dcoc_dacq(i, *word_q);
        }
    } else {
        for (i = 0; i < RXDCOC_DAC_WF_STEPS; i++)
        {
            wf_set_dcoc_daci(i, word_i[i]);
            wf_set_dcoc_dacq(i, word_q[i]);
        }
    }
    return 0;
}
#endif

#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxdcoc_result(int8_t *word_i, int8_t *word_q, uint8_t len)
{
    uint8_t i = 0;

    set_dcoc_force(0);
    if (len < RXDCOC_DAC_BT_STEPS)  {
        for (i = 0; i < RXDCOC_DAC_BT_STEPS; i++)
        {
            bt_set_dcoc_daci(i, *word_i);
            bt_set_dcoc_dacq(i, *word_q);
        }
    } else {
        for (i = 0; i < RXDCOC_DAC_BT_STEPS; i++)
        {
            bt_set_dcoc_daci(i, word_i[i]);
            bt_set_dcoc_dacq(i, word_q[i]);
        }
    }
    return 0;
}
#endif

#if RFCALI_WF_EN
__STATIC void wf_forge_tone(const uint32_t *stimu_data, int8_t dir, uint8_t scale)
{
    uint8_t i, j;

    NEW_DFE->REG_DFE_ARB_DATA_SEL.bit.REG_DFE_ARB_DATA_SEL = 1;
    NEW_DFE->REG_DFE_ARB_EN.bit.REG_DFE_ARB_EN = 1;
    NEW_DFE->REG_PUMPING_DATA_START_ADDR.bit.REG_PUMPING_DATA_START_ADDR = 0;
    rf_udelay(10);
    for (i = 0; i < 128; i++)
    {
        if (dir >= 0) {
            if (scale == 1)
                MEM_WR32(REG_ADDR_PUMPING_MEM_DATA, stimu_data[i]<<1);
            else
                MEM_WR32(REG_ADDR_PUMPING_MEM_DATA, stimu_data[i]);
        }
        else if (dir < 0) {
            int16_t q_inv = -(stimu_data[i] >> 16);
            uint32_t tmp = (q_inv << 16) | (stimu_data[i] & 0x0000ffff);
            if (scale == 1)
                MEM_WR32(REG_ADDR_PUMPING_MEM_DATA, tmp<<1);
            else
                MEM_WR32(REG_ADDR_PUMPING_MEM_DATA, tmp);
        }
        /*
          * Add some delay here, normally 40 loops works, for safety
          * We set t0 60 loops.
          */
        for (j = 0; j < 60; j++) {
            __NOP();
            __NOP();
            __NOP();
            __NOP();
            __NOP();
        }
    }
    NEW_DFE->REG_ARB_SAMPL_SEND_LEN.bit.REG_ARB_SAMPL_SEND_LENGTH = 1;
    NEW_DFE->REG_ARB_SAMPL_SEND_NUM.bit.REG_ARB_SEND_NUM = 0;
    NEW_DFE->REG_DFE_ARB_SEND_TRIG.bit.REG_DFE_ARB_SEND_TRIG = ~NEW_DFE->REG_DFE_ARB_SEND_TRIG.bit.REG_DFE_ARB_SEND_TRIG;

}
#endif

#if RFCALI_BT_EN
__STATIC void bt_forge_tone(const uint32_t *stimu_data, uint8_t len)
{
    uint8_t i = 0;

    for (i = 0; i < len; i++) {
        BT_MODEM->REG_CALIB_CFG3.all = (stimu_data[i]);
    }
}

#endif


#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxrc_init(int64_t *half_E0_q, uint8_t *cap)
{
    int32_t read_val_l;
    int8_t read_val_h;
    int64_t read_val;

    if (cap)
        *cap = get_abb_cap();
    wf_cali_cmn_rf_init(RX_RC_CALI/*mode*/, 2442/*freq*/, 3/*lna*/, 7/*abb_bq*/, 5/*abb_buf*/);
    NEW_DFE->REG_CALIBR_TX_0.bit.CALIBR_START_POS = 0;
    NEW_DFE->REG_CALIBR_TX_0.bit.CALIBR_END_POS = 127;
    wf_m1_est_init(4, 12);
    set_sc_i(RFCALI_MODE_WF, 6);
    set_sc_q(RFCALI_MODE_WF, 6);
    wf_forge_tone(wf_rxrc_low_data, 0, 0);
    rf_udelay(10);
    wf_cali_wait_i2_q2_done();
    read_val_l = NEW_DFE->REG_EST_RESULT_I2_L_RPT.bit.DFE_EST_I2_L_RPT;
    read_val_h = NEW_DFE->REG_EST_RESULT_I2_H_RPT.bit.DFE_EST_I2_H_RPT;
    read_val = ((int64_t)read_val_h << 32) + read_val_l;
    if (half_E0_q)
        *half_E0_q = read_val >> 1;
    wf_forge_tone(wf_rxrc_high_data, 0, 0);
    rf_udelay(10);
    return 0;
}
#endif

#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxrc_init(int64_t *half_E0_q, uint8_t *cap)
{
    int32_t read_val;

    if (cap)
        *cap = get_abb_cap();
    bt_cali_cmn_rf_init(RX_RC_CALI/*mode*/, 2440/*freq*/, 8/*lna*/, 10/*abb_bq*/, 0/*abb_buf*/);
    bt_forge_tone(bt_rxrc_low_data, 128);
    rf_udelay(10);
    BT_CALI_TRIGGER_NODUMP;
    read_val = BT_MODEM->REG_BT_RX_GLB_CFG2.bit.RX_IQ_EST_Q;
    if (half_E0_q)
        *half_E0_q = (int64_t)(read_val >> 1);
    bt_forge_tone(bt_rxrc_high_data, 128);
    rf_udelay(10);
    return 0;
}
#endif

#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxrc_measure(int64_t *E1_q, uint8_t *cap)
{
    int32_t read_val_l;
    int8_t read_val_h;
    int64_t read_val;

    if (cap)
        set_abb_cap(*cap);
    wf_cali_wait_i2_q2_done();
    read_val_l = NEW_DFE->REG_EST_RESULT_I2_L_RPT.bit.DFE_EST_I2_L_RPT;
    read_val_h = NEW_DFE->REG_EST_RESULT_I2_H_RPT.bit.DFE_EST_I2_H_RPT;
    read_val = ((int64_t)read_val_h << 32) + read_val_l;
    if (E1_q)
        *E1_q = read_val;

    return 0;
}
#endif

#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxrc_measure(int64_t *E1_q, uint8_t *cap)
{
    int32_t read_val;

    if (cap)
        set_abb_cap(*cap);
    BT_CALI_TRIGGER_NODUMP;
    read_val = BT_MODEM->REG_BT_RX_GLB_CFG2.bit.RX_IQ_EST_Q;
    if (E1_q)
        *E1_q = (int64_t)read_val;
    return 0;
}
#endif

#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxrc_result(int8_t cap)
{
    set_abb_cap(cap);
    return 0;
}
#endif

#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxrc_result(int8_t cap)
{
    NEW_DFE->REG_DFE_ARB_DATA_SEL.bit.REG_DFE_ARB_DATA_SEL = 0;
    NEW_DFE->REG_DFE_ARB_EN.bit.REG_DFE_ARB_EN = 0;

    set_abb_cap(cap);
    return 0;
}
#endif


#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxiq_init(uint16_t freq)
{
    /* makesure the IQ comp disabled */
    NEW_DFE->REG_COMPS_CFG0.bit.RX_IQ_COMPS_EN = 0;
    RFIF->REG_RSV_REG0.bit.RF_D2A_RSV |= 1 << 5;
    wf_cali_cmn_rf_init(RX_IQ_CALI/*mode*/, freq/*freq*/, 3/*lna*/, 7/*abb_bq*/, 5/*abb_buf*/);
    wf_m1_est_init(4, 12);

    /* enable RF test tone 2440MHz */
    RFIF->REG_RX_LOGIC1.bit.RF_RX_RXIQCAL_TTG_EN_FORCE = 0;
    enb_ttgpll();
    rf_udelay(10);
    RFIF->REG_RX_LOGIC51.bit.RF_RX_LNA_DCOC_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_LNA_DCOC_EN_WF_0 = 1;
    SYS_NODFT->REG_PLL_CTRL1.bit.REG_MPLL_TTG_FCEN = 0;
    rf_udelay(10);
    return 0;
}
#endif
#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxiq_init(uint16_t freq)
{
    /* makesure the IQ comp disabled */
    BT_MODEM->REG_BT_CALIB_VALUE_IQ.bit.RX_IQ_COMP_EN = 0;
    RFIF->REG_RSV_REG0.bit.RF_D2A_RSV |= 1<<5;
    bt_cali_cmn_rf_init(RX_IQ_CALI/*mode*/, freq/*freq*/, 3/*lna*/, 3/*abb_bq*/, 0/*abb_buf*/);

    /* enable RF test tone 2440MHz */
    RFIF->REG_RX_LOGIC1.bit.RF_RX_RXIQCAL_TTG_EN_FORCE = 0;
    enb_ttgpll();
    rf_udelay(10);
    RFIF->REG_RX_LOGIC51.bit.RF_RX_LNA_DCOC_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_LNA_DCOC_EN_WF_0 = 1;
    SYS_NODFT->REG_PLL_CTRL1.bit.REG_MPLL_TTG_FCEN = 0;
    rf_udelay(10);
    return 0;
}
#endif

#if RFCALI_WF_EN
void wf_cali_read_est_value(int32_t *I, int32_t *Q, int32_t *I2, int32_t *Q2, int32_t *IQ)
{
    *I = SIGN28(NEW_DFE->REG_EST_RESULT_I_RPT.bit.DFE_EST_I_RPT);
    *Q = SIGN28(NEW_DFE->REG_EST_RESULT_Q_RPT.bit.DFE_EST_Q_RPT);
    *I2 = NEW_DFE->REG_EST_RESULT_I2_L_RPT.bit.DFE_EST_I2_L_RPT;
    *Q2 = NEW_DFE->REG_EST_RESULT_Q2_L_RPT.bit.DFE_EST_Q2_L_RPT;
    *IQ = (int32_t)(NEW_DFE->REG_EST_RESULT_IQ_L_RPT.bit.DFE_EST_IQ_L_RPT);
    CLOGD("est I=%ld\n", *I);
    CLOGD("est Q=%ld\n", *Q);
    CLOGD("est I2=%ld\n", *I2);
    CLOGD("est Q2=%ld\n", *Q2);
    CLOGD("est IQ=%ld\n", *IQ);
    return;
}

void wf_est_remove_dc(int32_t *Isq, int32_t *Qsq, int32_t *IxQ, int32_t *I, int32_t *Q, int32_t *I2, int32_t *Q2, int32_t *IQ)
{
    *Isq = *I2 - *I * *I;
    *Qsq = *Q2 - *Q * *Q;
    *IxQ = *IQ - *I * *Q;
    CLOGD("Isq=%ld\n", *Isq);
    CLOGD("Qsq=%ld\n", *Qsq);
    CLOGD("IxQ=%ld\n", *IxQ);
    return;
}

__STATIC int8_t wf_cali_rxiq_measure(int32_t *Isq, int32_t *Qsq, int32_t *IxQ)
{
    int32_t i, q;
    int32_t i2, q2, iq;

    if (!Isq || !Qsq || !IxQ)
        return -1;

    wf_cali_wait_i2_q2_done();
    wf_cali_read_est_value(&i, &q, &i2, &q2, &iq);
    wf_est_remove_dc(Isq, Qsq, IxQ, &i, &q, &i2, &q2, &iq);

    RFIF->REG_RSV_REG0.bit.RF_D2A_RSV &= ~(1 << 5);

    RFIF->REG_RX_LOGIC51.bit.RF_RX_LNA_DCOC_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_LNA_DCOC_EN_WF_0 = 0;
    return 0;
}

int32_t wf_cali_read_hw_power()
{
    int32_t i2, q2;

    i2 = NEW_DFE->REG_EST_RESULT_I2_L_RPT.bit.DFE_EST_I2_L_RPT;
    q2 = NEW_DFE->REG_EST_RESULT_Q2_L_RPT.bit.DFE_EST_Q2_L_RPT;
    CLOGD("power=%ld\n", i2+q2);
    return (i2 + q2);
}

int32_t wf_cali_read_hw_power_without_dc()
{
    int32_t i2, q2, i, q;
    int32_t power;

    i2 = NEW_DFE->REG_EST_RESULT_I2_L_RPT.bit.DFE_EST_I2_L_RPT;
    q2 = NEW_DFE->REG_EST_RESULT_Q2_L_RPT.bit.DFE_EST_Q2_L_RPT;
    i = SIGN28(NEW_DFE->REG_EST_RESULT_I_RPT.bit.DFE_EST_I_RPT);
    q = SIGN28(NEW_DFE->REG_EST_RESULT_Q_RPT.bit.DFE_EST_Q_RPT);
    i2 = i2 - i * i;
    q2 = q2 - q * q;
    power = i2 + q2;
    CLOGD("power=%ld\n", power);
    return (power);
}
#endif

#if RFCALI_BT_EN
__STATIC int8_t bt_cali_rxiq_measure(int32_t *Isq, int32_t *Qsq, int32_t *IxQ)
{
    if (!Isq || !Qsq || !IxQ)
        return -1;
    BT_CALI_TRIGGER_NODUMP;
    *Isq = BT_MODEM->REG_BT_RX_GLB_CFG1.bit.RX_IQ_EST_I;
    *Qsq = BT_MODEM->REG_BT_RX_GLB_CFG2.bit.RX_IQ_EST_Q;
    *IxQ = SIGN(BT_MODEM->REG_BT_RX_GLB_CFG3.bit.RX_IQ_EST_IQ, I_MULT_Q_SIGN_BIT);
    // recover RF_D2A_RSV
    RFIF->REG_RSV_REG0.bit.RF_D2A_RSV &= ~(1 << 5);
    RFIF->REG_RX_LOGIC51.bit.RF_RX_LNA_DCOC_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_LNA_DCOC_EN_WF_0 = 0;
    return 0;
}
#endif
#if RFCALI_WF_EN
__STATIC int8_t wf_cali_rxiq_result(int16_t c21, int16_t c22)
{

    NEW_DFE->REG_RXIQ_BANK0_CFG0.bit.RX_IQ_COMPS_DATA_I_BANK0_HIGH = c21;
    NEW_DFE->REG_RXIQ_BANK0_CFG0.bit.RX_IQ_COMPS_DATA_Q_BANK0_HIGH = c22;
    NEW_DFE->REG_RXIQ_BANK0_CFG1.bit.RX_IQ_COMPS_DATA_I_BANK0_MID = c21;
    NEW_DFE->REG_RXIQ_BANK0_CFG1.bit.RX_IQ_COMPS_DATA_Q_BANK0_MID = c22;
    NEW_DFE->REG_RXIQ_BANK0_CFG2.bit.RX_IQ_COMPS_DATA_I_BANK0_LOW = c21;
    NEW_DFE->REG_RXIQ_BANK0_CFG2.bit.RX_IQ_COMPS_DATA_Q_BANK0_LOW = c22;
    NEW_DFE->REG_RXIQ_BANK1_CFG0.bit.RX_IQ_COMPS_DATA_I_BANK1_HIGH = c21;
    NEW_DFE->REG_RXIQ_BANK1_CFG0.bit.RX_IQ_COMPS_DATA_Q_BANK1_HIGH = c22;
    NEW_DFE->REG_RXIQ_BANK1_CFG1.bit.RX_IQ_COMPS_DATA_I_BANK1_MID = c21;
    NEW_DFE->REG_RXIQ_BANK1_CFG1.bit.RX_IQ_COMPS_DATA_Q_BANK1_MID = c22;
    NEW_DFE->REG_RXIQ_BANK1_CFG2.bit.RX_IQ_COMPS_DATA_I_BANK1_LOW = c21;
    NEW_DFE->REG_RXIQ_BANK1_CFG2.bit.RX_IQ_COMPS_DATA_Q_BANK1_LOW = c22;
    NEW_DFE->REG_COMPS_CFG0.bit.RX_IQ_COMPS_EN = 1;
    return 0;
}
#endif

#if RFCALI_BT_EN
int8_t bt_cali_rxiq_result(int16_t c21, int16_t c22)
{
    BT_MODEM->REG_BT_CALIB_VALUE_IQ.bit.RX_IQ_COMP_DATA_I = c21;
    BT_MODEM->REG_BT_CALIB_VALUE_IQ.bit.RX_IQ_COMP_DATA_Q = c22;
    BT_MODEM->REG_BT_CALIB_VALUE_IQ.bit.RX_IQ_COMP_EN = 1;
    return 0;
}
#endif

#if RFCALI_BT_EN
int8_t bt_cali_txiq_set(int16_t c21, int16_t c22)
{
    BT_MODEM->REG_TX_CFG1.bit.TX_C_MATRIX_1_2 = c21;
    BT_MODEM->REG_TX_CFG2.bit.TX_C_MATRIX_2_2 = c22;
    BT_MODEM->REG_TX_CFG3.bit.TX_TXIQCOMPEN   = 1;

    return 0;
}

int8_t bt_cali_txdc_set(uint16_t dac_i, uint16_t dac_q)
{
    BT_MODEM->REG_TX_CFG5.bit.TX_DCVALUE_I = dac_i;
    BT_MODEM->REG_TX_CFG5.bit.TX_DCVALUE_Q = dac_q;

    return 0;
}
#endif


#if RFCALI_WF_EN
__STATIC void wf_cali_txiq_tia_fb_path(void)
{
    RFIF->REG_RX_LOGIC1.bit.RF_RX_RXIQCAL_TTG_EN_FORCE = 1;
    //RFIF->REG_RX_LOGIC1.bit.RF_RX_RSSI_EN_FORCE = 0;
    RFIF->REG_RSV_REG0.bit.RF_D2A_RSV |= 1 << 5;;
    RFIF->REG_RX_REG1.bit.RF_RX_LO_BUF = 1;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_LNA_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_DIV_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_MXR_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_RFFE_LDO_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_RFFE_LDO_VB_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_DIVIDER_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_MXR_TIA_EN_FORCE= 1;
    RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_DIVIDER_EN = 1;
    RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_MXR_TIA_EN = 1;
}

__STATIC void wf_cali_txiq_tia_fb_path_dis(void)
{
    RFIF->REG_RX_LOGIC1.bit.RF_RX_RXIQCAL_TTG_EN_FORCE = 0;
    RFIF->REG_RSV_REG0.bit.RF_D2A_RSV &= ~(1<<5);
    RFIF->REG_RX_REG1.bit.RF_RX_LO_BUF = 0;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_LNA_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_DIV_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_MXR_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_RFFE_LDO_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_RFFE_LDO_VB_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_DIVIDER_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_MXR_TIA_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_DIVIDER_EN = 0;
    RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_MXR_TIA_EN = 0;
}

__STATIC int8_t wf_cali_txiq_tx_result(int16_t c21, int16_t c22)
{
    //wf_cali_set_txsqr_iqdc_param(2, 0, 0, phase_err, amp_err);
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I0.bit.CFG_TPC_IQCOMP_I_0 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I0.bit.CFG_TPC_IQCOMP_I_1 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I1.bit.CFG_TPC_IQCOMP_I_2 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I1.bit.CFG_TPC_IQCOMP_I_3 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I2.bit.CFG_TPC_IQCOMP_I_4 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I2.bit.CFG_TPC_IQCOMP_I_5 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I3.bit.CFG_TPC_IQCOMP_I_6 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I3.bit.CFG_TPC_IQCOMP_I_7 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I4.bit.CFG_TPC_IQCOMP_I_8 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I4.bit.CFG_TPC_IQCOMP_I_9 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I5.bit.CFG_TPC_IQCOMP_I_10 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I5.bit.CFG_TPC_IQCOMP_I_11 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I6.bit.CFG_TPC_IQCOMP_I_12 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I6.bit.CFG_TPC_IQCOMP_I_13 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I7.bit.CFG_TPC_IQCOMP_I_14 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I7.bit.CFG_TPC_IQCOMP_I_15 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I8.bit.CFG_TPC_IQCOMP_I_16 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I8.bit.CFG_TPC_IQCOMP_I_17 = c21;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_I9.bit.CFG_TPC_IQCOMP_I_18 = c21;

    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q0.bit.CFG_TPC_IQCOMP_Q_0 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q0.bit.CFG_TPC_IQCOMP_Q_1 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q1.bit.CFG_TPC_IQCOMP_Q_2 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q1.bit.CFG_TPC_IQCOMP_Q_3 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q2.bit.CFG_TPC_IQCOMP_Q_4 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q2.bit.CFG_TPC_IQCOMP_Q_5 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q3.bit.CFG_TPC_IQCOMP_Q_6 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q3.bit.CFG_TPC_IQCOMP_Q_7 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q4.bit.CFG_TPC_IQCOMP_Q_8 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q4.bit.CFG_TPC_IQCOMP_Q_9 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q5.bit.CFG_TPC_IQCOMP_Q_10 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q5.bit.CFG_TPC_IQCOMP_Q_11 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q6.bit.CFG_TPC_IQCOMP_Q_12 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q6.bit.CFG_TPC_IQCOMP_Q_13 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q7.bit.CFG_TPC_IQCOMP_Q_14 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q7.bit.CFG_TPC_IQCOMP_Q_15 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q8.bit.CFG_TPC_IQCOMP_Q_16 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q8.bit.CFG_TPC_IQCOMP_Q_17 = c22;
    NEW_DFE->REG_TPC_CTRL_IQCOMP_Q9.bit.CFG_TPC_IQCOMP_Q_18 = c22;

    #if RFCALI_BT_EN
    BT_MODEM->REG_TX_CFG1.bit.TX_C_MATRIX_1_2 = c21<<3;
    BT_MODEM->REG_TX_CFG2.bit.TX_C_MATRIX_2_2 = c22<<3;
    BT_MODEM->REG_TX_CFG3.bit.TX_TXIQCOMPEN   = 1;
    g_bt_txiq_comp.c21 = BT_MODEM->REG_TX_CFG1.bit.TX_C_MATRIX_1_2;
    g_bt_txiq_comp.c22 = BT_MODEM->REG_TX_CFG2.bit.TX_C_MATRIX_2_2;
    #endif
    return 0;
}

__STATIC int8_t wf_cali_txiq_fb_init(uint8_t dbg, uint8_t abb_gain)
{
    reg_sx_intg = RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_DIVN_INTEG;
    reg_sx_flac = RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIVN_FRAC;
    reg_wf_channel = RFIF->REG_CTRL0.bit.WF_CHANNEL;
    /* makesure the IQ comp disabled */
    //reg_rxiq_comps_en = NEW_DFE->REG_COMPS_CFG0.bit.RX_IQ_COMPS_EN;
    if (dbg) {
        NEW_DFE->REG_COMPS_CFG0.bit.RX_IQ_COMPS_EN = 1;
        NEW_DFE->REG_CALIBR_FORCE_MODULE_EN.bit.REG_CALIBR_FORCE_MODULE_EN = 1;
    }
    else
        NEW_DFE->REG_COMPS_CFG0.bit.RX_IQ_COMPS_EN = 0;
    RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_DCOCDAC_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_DCOCDAC_EN = 0;
    wf_cali_txiq_tx_result(0, 2048);
    wf_cali_txiq_tia_fb_path();
    wf_cali_cmn_rf_init(RX_IQ_CALI/*mode*/, 2434/*freq*/, 4/*lna*/, abb_gain/*abb_bq*/, 10/*abb_buf*/);
    wf_m1_est_init(4, 12);
    /* enable RF test tone 2440MHz */
    RFIF->REG_RX_LOGIC1.bit.RF_RX_RXIQCAL_TTG_EN_FORCE = 0;
    enb_ttgpll();
    RFIF->REG_RX_LOGIC51.bit.RF_RX_LNA_DCOC_EN_FORCE = 1;
    RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_LNA_DCOC_EN_WF_0 = 1;
    SYS_NODFT->REG_PLL_CTRL1.bit.REG_MPLL_TTG_FCEN = 0;
    //CLOGD("wf_cali_txiq_fb_init\n");
    return 0;
}

__STATIC int8_t wf_cali_txiq_fb_measure(int32_t *Isq, int32_t *Qsq, int32_t *IxQ)
{
    wf_cali_rxiq_measure(Isq, Qsq, IxQ);
    return 0;
}

__STATIC int8_t wf_cali_txiq_fb_result(int16_t c21, int16_t c22)
{
    reg_rxiq_cfg0 = NEW_DFE->REG_RXIQ_BANK0_CFG1.bit.RX_IQ_COMPS_DATA_I_BANK0_MID;
    reg_rxiq_cfg1 = NEW_DFE->REG_RXIQ_BANK0_CFG1.bit.RX_IQ_COMPS_DATA_Q_BANK0_MID;
    wf_cali_rxiq_result(c21, c22);
    return 0;
}

__STATIC int8_t wf_cali_txiq_fb_deinit(void)
{
    RFIF->REG_RX_LOGIC51.bit.RF_RX_LNA_DCOC_EN_FORCE = 0;
    RFIF->REG_RX_LOGIC51.bit.REG_RF_RX_LNA_DCOC_EN_WF_0 = 0;
    dis_ttgpll();
    wf_cali_txiq_tia_fb_path_dis();
    RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_DCOCDAC_EN_FORCE = 0;
    //RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_DCOCDAC_EN = 0;
    RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_DIVN_INTEG = reg_sx_intg;
    RFIF->REG_SX_LOGIC0.bit.RF_SX_DIVN_INTEG_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIVN_FRAC = reg_sx_flac;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIVN_FRAC_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIG_START_FORCE = 1;
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIG_START = 0;
    rf_udelay(10);
    RFIF->REG_SX_LOGIC1.bit.REG_RF_SX_DIG_START = 1;
    RFIF->REG_SX_LOGIC0.bit.RF_SX_DIVN_INTEG_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIVN_FRAC_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIG_START_FORCE = 0;
    RFIF->REG_CTRL0.bit.WF_CHANNEL = reg_wf_channel;
    return 0;
}

__STATIC void wf_cali_set_txsqr_iqdc_param(uint8_t en,
    uint32_t dc_i, uint32_t dc_q, uint32_t phase_err, uint32_t amp_err)
{
#if 0
    NEW_DFE->REG_TXIQ_CFG1.bit.TX_IQ_IMBALANCE_DC_EST_I = dc_i & 0x7fff;
    NEW_DFE->REG_TXIQ_CFG1.bit.TX_IQ_IMBALANCE_DC_EST_Q = dc_q & 0x7fff;
    NEW_DFE->REG_TXIQ_CFG2.bit.TX_IQ_IMBALANCE_PHASE_ERR = phase_err & 0x7fff;
    NEW_DFE->REG_TXIQ_CFG2.bit.TX_IQ_IMBALANCE_AMP_ERR = amp_err & 0x7fff;
    NEW_DFE->REG_COMPS_CFG0.bit.TX_TXIQCOMPEN = (!en) ? 0 : 2;
    NEW_DFE->REG_COMPS_CFG0.bit.TX_COMPS_BYPASS = 0;
#endif
}

__STATIC void wf_cali_txiq_ttg_fb_enable(void)
{
#if 0
    RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BUF_RX_EN_FORCE = 0x1;  // 1 bits
    RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BUF_RX_EN = 0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = 0x1; // 1 bits
#endif
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_TTG_EN_FORCE = 0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_TTG_EN = 0x1;  ////0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_DPD_EN_FORCE = 0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_DPD_EN = 0x1;  // 1 bits
}

__STATIC void wf_cali_txiq_ttg_fb_disable(void)
{
    RFIF->REG_TX_LOGIC9.bit.RF_TX_UPC_LO_EN_FORCE = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_TTG_EN_FORCE = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_DPD_EN_FORCE = 0x0;  // 1 bits
}

__STATIC void wf_cali_dpd_fb_enable(void)
{
    #if 1
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_TTG_EN_FORCE = 0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_TTG_EN = 0x1;  ////0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_DPD_EN_FORCE = 0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_DPD_EN = 0x1;  // 1 bits
    //RFIF->REG_RX_REG0.bit.RF_RX_ABB_CAP_WF = 40;
    RFIF->REG_RX_REG5.bit.RF_RX_ABB_DPD_SW = 8;
    //RFIF->REG_RX_LOGIC52.bit.REG_RF_RX_ABB_1ST_GC_WF_0 = 8;
    //RFIF->REG_RX_LOGIC52.bit.RF_RX_ABB_1ST_GC_WF_FORCE = 1;
    #endif
#if 0
    //		RFIF->REG_LOGEN_LOGIC0.all;
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_LDO_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_LDO_FC_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BIAS_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BUF_RX_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BUF_TX_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_LDO_EN = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_LDO_FC_EN = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BIAS_EN = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BUF_RX_EN = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BUF_TX_EN = 0x1;  // 1 bits
#if 0
//	RFIF->REG_LOGEN_REG0.all;
	// RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_BUF_TEST_EN = 0x1;  // 1 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_LDO_OUT = 0x2;  // 3 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_LO_BIAS = 0x4;  // 3 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_RF_BIAS = 0x3;  // 3 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_CBANK = 0x9;  // 5 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_CBANK_SEL = 0x0;  // 1 bits
#endif
//	RFIF->REG_TX_LOGIC9.all;
	RFIF->REG_TX_LOGIC9.bit.RF_TX_UPC_LO_EN_FORCE = 0x01;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_UPC_LO_EN = 0x1; // 1 bits
	RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = 0x1; // 1 bits
	RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_TTG_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_TTG_EN = 0x1;  ////0x1;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_DPD_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_DPD_EN = 0x1;  // 1 bits
	// RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_X2_EN_FORCE = 0x01;  // 1 bits
	// RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_X2_EN = 0x0  ////0x1;  // 1 bits

//	RFIF->REG_TX_LOGIC0.all;
	RFIF->REG_TX_LOGIC0.bit.RF_TX_PPA_EN_FORCE = 0x1;  // 1 bits Key Setting
	RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_EN = 0x1; // 1 bits Key Setting

//	RFIF->REG_TX_LOGIC14.all;
	RFIF->REG_TX_LOGIC14.bit.RF_TX_MODE_WF_BT_FORCE = 0x1;  // 1 bits
	RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_MODE_WF_BT = 0x1;  // 2 bits
	RFIF->REG_TX_LOGIC14.bit.RF_TX_ABB_EN_FORCE = 0x1; // 1 bits Key Setting
	RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_EN = 0x1; // 1 bits Key Setting

//	RFIF->REG_RX_LOGIC1.all;
	RFIF->REG_RX_LOGIC1.bit.RF_RX_MXR_TXIQCAL_EN_FORCE = 1;
	RFIF->REG_RX_LOGIC1.bit.REG_RF_RX_MXR_TXIQCAL_EN = 1;

//	RFIF->REG_RX_LOGIC62.all;
	RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_DIVIDER_EN_FORCE = 1;
	RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_DIVIDER_EN = 1;
	RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_MXR_TIA_EN_FORCE = 1;
	RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_MXR_TIA_EN = 1;

//	RFIF->REG_RX_LOGIC0.all;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_VCMGEN_EN_FORCE = 1;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_1ST_EN_FORCE = 1;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_BQ_EN_FORCE = 1;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_BUF_EN_FORCE = 1;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_VCMGEN_EN = 1;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_1ST_EN = 1;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_BQ_EN = 1;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_BUF_EN = 1;

//	RFIF->REG_TRXSW_LOGIC0.all;
	RFIF->REG_TRXSW_LOGIC0.bit.RF_TRX_SW_FORCE = 0x1;
	RFIF->REG_TRXSW_LOGIC0.bit.REG_RF_TRX_SW = 0x0;
#endif
}

__STATIC void wf_cali_dpd_fb_disable(void)
{
    #if 1
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_TTG_EN = 0x0;  ////0x1;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_TTG_EN_FORCE = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_DPD_EN = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_DPD_EN_FORCE = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_UPC_LO_EN = 0x0; // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_UPC_LO_EN_FORCE = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = 0x0; // 1 bits
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0x0;  // 1 bits
    RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_EN = 0;
    RFIF->REG_TX_LOGIC0.bit.RF_TX_PPA_EN_FORCE = 0x0;
    RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_DACIN_I = 0;
    RFIF->REG_TX_DAC_LOGIC0.bit.RFDAC_DACIN_I_FORCE = 0;
    RFIF->REG_TX_DAC_LOGIC1.bit.REG_RFDAC_DACIN_Q = 0;
    RFIF->REG_TX_DAC_LOGIC1.bit.RFDAC_DACIN_Q_FORCE = 0;
    //RFIF->REG_RX_REG0.bit.RF_RX_ABB_CAP_WF = 55;
    RFIF->REG_RX_REG5.bit.RF_RX_ABB_DPD_SW = 0;
    RFIF->REG_RX_LOGIC52.bit.REG_RF_RX_ABB_1ST_GC_WF_0 = 0xa;
    RFIF->REG_RX_LOGIC52.bit.RF_RX_ABB_1ST_GC_WF_FORCE = 0;
    #endif
#if 0
    RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_LDO_EN_FORCE = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_LDO_FC_EN_FORCE = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BIAS_EN_FORCE = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BUF_RX_EN_FORCE = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.RF_LOGEN_BUF_TX_EN_FORCE = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_LDO_EN = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_LDO_FC_EN = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BIAS_EN = 0x0;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BUF_RX_EN = 0x1;  // 1 bits
	RFIF->REG_LOGEN_LOGIC0.bit.REG_RF_LOGEN_BUF_TX_EN = 0x0;  // 1 bits
#if 0
//	RFIF->REG_LOGEN_REG0.all;
	// RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_BUF_TEST_EN = 0x1;  // 1 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_LDO_OUT = 0x2;  // 3 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_LO_BIAS = 0x4;  // 3 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_RF_BIAS = 0x3;  // 3 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_CBANK = 0x9;  // 5 bits
	RFIF->REG_LOGEN_REG0.bit.RF_LOGEN_CBANK_SEL = 0x0;  // 1 bits
#endif
//	RFIF->REG_TX_LOGIC9.all;
	RFIF->REG_TX_LOGIC9.bit.RF_TX_UPC_LO_EN_FORCE = 0x00;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_UPC_LO_EN = 0x0; // 1 bits
	RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0x0;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = 0x0; // 1 bits
	RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_TTG_EN_FORCE = 0x0;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_TTG_EN = 0x0;  ////0x1;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_DPD_EN_FORCE = 0x0;  // 1 bits
	RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_DPD_EN = 0x0;  // 1 bits
	// RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_X2_EN_FORCE = 0x01;  // 1 bits
	// RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_X2_EN = 0x0  ////0x1;  // 1 bits

//	RFIF->REG_TX_LOGIC14.all;
	RFIF->REG_TX_LOGIC14.bit.RF_TX_MODE_WF_BT_FORCE = 0x0;  // 1 bits
	RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_MODE_WF_BT = 0x0;  // 2 bits
	RFIF->REG_TX_LOGIC14.bit.RF_TX_ABB_EN_FORCE = 0x0; // 1 bits Key Setting
	RFIF->REG_TX_LOGIC14.bit.REG_RF_TX_ABB_EN = 0x0; // 1 bits Key Setting

//	RFIF->REG_RX_LOGIC1.all;
	RFIF->REG_RX_LOGIC1.bit.RF_RX_MXR_TXIQCAL_EN_FORCE = 0;
	RFIF->REG_RX_LOGIC1.bit.REG_RF_RX_MXR_TXIQCAL_EN = 0;

//	RFIF->REG_RX_LOGIC62.all;
	RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_DIVIDER_EN_FORCE = 0;
	RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_DIVIDER_EN = 0;
	RFIF->REG_RX_LOGIC62.bit.RF_RX_DPD_MXR_TIA_EN_FORCE = 0;
	RFIF->REG_RX_LOGIC62.bit.REG_RF_RX_DPD_MXR_TIA_EN = 0;

//	RFIF->REG_RX_LOGIC0.all;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_VCMGEN_EN_FORCE = 0;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_1ST_EN_FORCE = 0;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_BQ_EN_FORCE = 0;
	RFIF->REG_RX_LOGIC0.bit.RF_RX_ABB_BUF_EN_FORCE = 0;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_VCMGEN_EN = 0;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_1ST_EN = 0;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_BQ_EN = 0;
	RFIF->REG_RX_LOGIC0.bit.REG_RF_RX_ABB_BUF_EN = 0;

//	RFIF->REG_TRXSW_LOGIC0.all;
	RFIF->REG_TRXSW_LOGIC0.bit.RF_TRX_SW_FORCE = 0x0;
	RFIF->REG_TRXSW_LOGIC0.bit.REG_RF_TRX_SW = 0x0;
#endif
}

__STATIC void macbypass_tx_stop(void)
{
    MEM_WR32(WIFI_MACBYPASS_BASE + 0x0000, 0x0); //macbyp_ctrl_set(0);
    MEM_WR32(WIFI_MACBYPASS_BASE + 0x0000, 0x100); //macbyp_ctrl_set(0x100);
    return;
}

__STATIC void macbypass_tx_start(uint8_t tx_pwr, uint8_t mcs)
{
    MEM_WR32(0x4B800854, 0xff); //fix scramble seed
    MEM_WR32((WIFI_MACBYPASS_BASE+0x000C), 0x1); // Open reg clk
    //MEM_WR32((WIFI_MACBYPASS_BASE+0x0000), 0x1); //macbyp_ctrl_set(0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0000), 0x0); //macbyp_ctrl_set(0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0004), 0x10000); //tx payload incr byte payload with fcs;
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0008), 0x12); //macbyp_trigger_set(0x12);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0048), 0x300); //macbyp_interframe_delay_set(0x300);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0200), 0x5); //macbyp_txv0_set(0x5) HE_SU, 20MHz
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0204), 0x1); //macbyp_txv1_set(0x1) AntennaSet
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0208), tx_pwr<<2); //macbyp_txv2_set(0xa) pwrlevel
    MEM_WR32((WIFI_MACBYPASS_BASE+0x020C), 0x20); //macbyp_txv3_set(0x20) ntx = 0 = 1 Transmitchain, timeofdef, cont tx
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0210), 0xff); //macbyp_txv4_set(0xff) default leglength = 0xff
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0214), 0xbf); //macbyp_txv5_set(0x0) leg length, txv5[7:4] = legrate
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0218), 0x0); //macbyp_txv6_set(0x0) service
    MEM_WR32((WIFI_MACBYPASS_BASE+0x021C), 0x0); //macbyp_txv7_set(0x0) service
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0220), 0x0); //macbyp_txv8_set(0x0);//GI
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0224), 0x9); //macbyp_txv9_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0228), 0xaa); //macbyp_txv10_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x022C), 0x0); //macbyp_txv11_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0230), 0x0); //macbyp_txv12_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0234), 0x0); //macbyp_txv13_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0238), mcs); //macbyp_txv14_set(0x9);//MCS
    MEM_WR32((WIFI_MACBYPASS_BASE+0x023C), 0xe8); //macbyp_txv15_set(100);//LENGTH[7:0]
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0240), 0x4);  //macbyp_txv16_set(0x9);//LENGTH[15:8]
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0244), 0x0);  //macbyp_txv14_set(0x9);//LENGTH[19:16]
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0000), 0x301); //macbyp_ctrl_set(0x301);
    return;
}

__STATIC void macbypass_tx_one_frame(uint8_t tx_pwr, uint8_t mcs)
{
    int i = 0;

    MEM_WR32(0x4B800854, 0xff); //fix scramble seed
    //MEM_WR32((WIFI_MACBYPASS_BASE+0x000C), 0x1); // Open reg clk
    //MEM_WR32((WIFI_MACBYPASS_BASE+0x0000), 0x1); //macbyp_ctrl_set(0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0000), 0x0); //macbyp_ctrl_set(0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0004), 0x10000); //tx payload incr byte payload with fcs;
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0048), 0x300); //macbyp_interframe_delay_set(0x300);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0200), 0x5); //macbyp_txv0_set(0x5) HE_SU, 20MHz
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0204), 0x1); //macbyp_txv1_set(0x1) AntennaSet
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0208), tx_pwr<<2); //macbyp_txv2_set(0xa) pwrlevel
    MEM_WR32((WIFI_MACBYPASS_BASE+0x020C), 0x20); //macbyp_txv3_set(0x20) ntx = 0 = 1 Transmitchain, timeofdef, cont tx
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0210), 0xff); //macbyp_txv4_set(0xff) default leglength = 0xff
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0214), 0xbf); //macbyp_txv5_set(0x0) leg length, txv5[7:4] = legrate
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0218), 0x0); //macbyp_txv6_set(0x0) service
    MEM_WR32((WIFI_MACBYPASS_BASE+0x021C), 0x0); //macbyp_txv7_set(0x0) service
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0220), 0x0); //macbyp_txv8_set(0x0);//GI
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0224), 0x9); //macbyp_txv9_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0228), 0xaa); //macbyp_txv10_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x022C), 0x0); //macbyp_txv11_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0230), 0x0); //macbyp_txv12_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0234), 0x0); //macbyp_txv13_set(0x0);
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0238), mcs); //macbyp_txv14_set(0x9);//MCS
    MEM_WR32((WIFI_MACBYPASS_BASE+0x023C), 0xe8); //macbyp_txv15_set(100);//LENGTH[7:0]
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0240), 0x4);  //macbyp_txv16_set(0x9);//LENGTH[15:8]
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0244), 0x0);  //macbyp_txv14_set(0x9);//LENGTH[19:16]
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0000), 0x201); //macbyp_ctrl_set(0x201);
    for (i = 0; i < 500; i++) {
        rf_udelay(1);
        if ((MEM_RD32(WIFI_MACBYPASS_BASE+0x0000) & 0x80000000)) {
            break;
        }
    }
    MEM_WR32((WIFI_MACBYPASS_BASE+0x0000), 0x100); //macbyp_ctrl_set(0x100);
    //CLOGD("macbypass_tx_one_frame (%d)done\n", i);
    return;
}

__STATIC void dpd_tx_packet(uint8_t tx_pwr, uint8_t mcs, uint32_t tx_mem_addr, uint32_t rx_mem_addr)
{
    macbypass_tx_stop();
    //NEW_DFE->REG_COMPS_CFG0.bit.DPD_TXDPDAMEN = 0;
    //NEW_DFE->REG_COMPS_CFG0.bit.DPD_TXDPDPMEN = 0;
    NEW_DFE->REG_COMPS_CFG0.bit.RX_COMPS_BYPASS = 0;
    NEW_DFE->REG_COMPS_CFG0.bit.TX_COMPS_BYPASS = 0;

    macbypass_tx_start(tx_pwr, mcs);
    return;
}

__STATIC int8_t wf_cali_txiq_send_testtone(uint8_t tx_pwr, int8_t dir)
{
    wf_forge_tone(wf_txiq_ttg_data, dir, 1);
    return 0;
}

__STATIC int8_t wf_cali_send_ttg_start(uint8_t tx_pwr, int8_t dir, uint16_t lo_freq)
{
    save_reg_config();
    RFIF->REG_CTRL0.bit.WF_END = 1;
    /* Makesure RF in WiFi mode */
    RFIF->REG_CTRL0.bit.REG_BT_WF = 0;
    RFIF->REG_CTRL0.bit.BT_WF_FORCE = 1;
    /* WAR: Makesure mpif_clk enabled when do RF calibration */
    WIFI_CTRL->REG_WIFI_CTRL_MACCG_OPT_BYPASS.bit.CFG_WIFI_MACCG_OPT_BYPASS = 1;
    /* crm_rcclkforce_setf(1) and crm_rcclkforce_setf(1) */
    WIFI_CRM->REG_CLKGATEPHYFCTRL0.bit.FECLKFORCE = 1;
    WIFI_CRM->REG_CLKGATEPHYFCTRL0.bit.RCCLKFORCE = 1;
    /* nxmac_next_state_setf(HW_ACTIVE) */
    //WIFI_MAC_CORE->REG_STATECNTRLREG.bit.NEXTSTATE = 3;

    wf_cali_cmn_rf_init(TX_IQ_TTG_CALI/*mode*/, lo_freq/*freq*/, 8/*lna*/, 6/*abb_bq*/, 6/*abb_buf*/);
    wf_forge_tone(wf_txiq_ttg_data, dir, 0);
    dpd_tx_packet(tx_pwr, 0, CALI_MEM_START_OFFSET, CALI_MEM_START_OFFSET);
    rf_udelay(100); //Very important delay for workaround here don't modify
    NEW_DFE->REG_DFE_CALIBR_SET.all = 1;
    NEW_DFE->REG_CALIBR_TX_0.bit.CALIBR_DUMP_MODE = 1;
    return 0;
}

__STATIC int8_t wf_cali_send_ttg_stop()
{
    macbypass_tx_stop();
    wf_cali_work_en(0);
    wf_phy_sw_reset();
    rf_udelay(100);
    WIFI_CRM->REG_CLKGATEPHYFCTRL0.bit.FECLKFORCE = 0;
    RFIF->REG_CTRL0.bit.WF_START = 1;
    restore_reg_config();
    return 0;
}
__STATIC int8_t wf_cali_txiq_tx_init(void)
{
    /* disable Tx_Comps */
    wf_cali_txiq_tx_result(0, 2048);
    wf_cali_txiq_ttg_fb_enable();

    //CLOGD("wf_cali_txiq_tx_init\n");
    return 0;
}

__STATIC int8_t wf_cali_txiq_tx_measure(void)
{
    wf_cali_cmn_rf_init(TX_IQ_TTG_CALI/*mode*/, 2434/*freq*/, 8/*lna*/, 6/*abb_bq*/, 6/*abb_buf*/);
    wf_cali_txiq_send_testtone(19, 0);
    wf_cali_work_en(0);
    //wf_cali_txiq_ttg_fb_disable();
    NEW_DFE->REG_CALIBR_TX_0.bit.CALIBR_DUMP_MODE = 0;
    NEW_DFE->REG_CALIBR_TX_0.bit.CALIBR_EST_MODE = 0;
    //CLOGD("wf_cali_txiq_tx_measure\n");
    return 0;
}

__STATIC int8_t wf_cali_txiq_restore_rxiq_result()
{
    wf_cali_rxiq_result(reg_rxiq_cfg0, reg_rxiq_cfg1);
    return 0;
}

__STATIC int8_t wf_cali_txiq_dump_data(int32_t *Isq, int32_t *Qsq, int32_t *IxQ)
{
    uint32_t i = 0;
    int32_t I_sum = 0;
    int32_t Q_sum = 0;
    int16_t I_mean = 0;
    int16_t Q_mean = 0;
    int64_t Isq_sum = 0;
    int64_t Qsq_sum = 0;
    int64_t IxQ_sum = 0;
    int32_t sum_bits = 0;
    int32_t bit_shift = 0;
    uint32_t *code = (uint32_t *)CALI_MEM_START_ADDR;

    if (!Isq || !Qsq || !IxQ)
        goto bail;

    for (i = 1024; i < MEM_DUMP_LEN-1024; i++)
    {
        uint32_t tmp_code = code[i];
        int16_t tmp_I = (tmp_code & 0xFFFF) >> 4;
        int16_t tmp_Q = ((tmp_code >> 16) & 0xFFFF) >> 4;
        int16_t code_I = SIGN(tmp_I, 12);
        int16_t code_Q = SIGN(tmp_Q, 12);

        I_sum += code_I;
        Q_sum += code_Q;
    }
    I_mean = I_sum / (MEM_DUMP_LEN - 2048);
    Q_mean = Q_sum / (MEM_DUMP_LEN - 2048);

    CLOGD("I_mean=%d Q_mean=%d\n", I_mean, Q_mean);
    for (i = 1024; i < MEM_DUMP_LEN-1024; i++)
    {
        uint32_t tmp_code = code[i];
        int16_t tmp_I = (tmp_code & 0xFFFF) >> 4;
        int16_t tmp_Q = ((tmp_code >> 16) & 0xFFFF) >> 4;
        int16_t code_I = SIGN(tmp_I, 12) - I_mean;
        int16_t code_Q = SIGN(tmp_Q, 12) - Q_mean;

        Isq_sum += (int32_t)code_I * (int32_t)code_I;
        Qsq_sum += (int32_t)code_Q * (int32_t)code_Q;
        IxQ_sum += (int32_t)code_I * (int32_t)code_Q;
    }

    if (Isq_sum == 0 || Qsq_sum == 0) {
        CLOGD("dump abort since Isq=%d and Qsq=%d\n!", Isq_sum, Qsq_sum);
        goto bail;
    }
    sum_bits = max(flsll(Isq_sum), flsll(Qsq_sum));
    CLOGD("sum_bits=%d\n", sum_bits);
    if (sum_bits <= 32)
        bit_shift = 0;
    else
        bit_shift = sum_bits - 32;
    *Isq = Isq_sum >> bit_shift; // /(MEM_DUMP_LEN - 100);
    *Qsq = Qsq_sum >> bit_shift; // /(MEM_DUMP_LEN - 100);
    *IxQ = IxQ_sum >> bit_shift; // /(MEM_DUMP_LEN - 100);

    return 0;
bail:
    return -1;
}

__STATIC int8_t wf_cali_txdcdpd_toggle_mixen(uint8_t en)
{
    RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_EN = en;
    RFIF->REG_TX_LOGIC0.bit.RF_TX_PPA_EN_FORCE = 0x1;
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = en;
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0x1;
    if (en) {
        RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_DACIN_I = 0;
        RFIF->REG_TX_DAC_LOGIC0.bit.RFDAC_DACIN_I_FORCE = 0;
        RFIF->REG_TX_DAC_LOGIC1.bit.REG_RFDAC_DACIN_Q = 0;
        RFIF->REG_TX_DAC_LOGIC1.bit.RFDAC_DACIN_Q_FORCE = 0;
    } else {
        RFIF->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_DACIN_I = 2048;
        RFIF->REG_TX_DAC_LOGIC0.bit.RFDAC_DACIN_I_FORCE = 1;
        RFIF->REG_TX_DAC_LOGIC1.bit.REG_RFDAC_DACIN_Q = 2048;
        RFIF->REG_TX_DAC_LOGIC1.bit.RFDAC_DACIN_Q_FORCE = 1;
    }
    return 0;
}

static int32_t wf_cali_txdpd_map_pred_lut(uint32_t pwr_idx, uint32_t lut_idx)
{
    #if 1
    uint32_t reg, idx;
    volatile uint32_t val, *ptr = &NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.all;

    reg = pwr_idx >> 2;
    idx = (pwr_idx & 0x3) << 3;
    val = *(ptr + reg);
    val &= ~(0xFF << idx);
    val |= ((lut_idx & 0xF) | (1<<4)) << idx;
    *(ptr + reg) = val;
    #else
     switch(pwr_idx) {
        case 0:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_LUT_IDX_0 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_EN_0 = 1;
            break;
        case 1:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_LUT_IDX_1 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_EN_1 = 1;
            break;
        case 2:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_LUT_IDX_2 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_EN_2 = 1;
            break;
        case 3:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_LUT_IDX_3 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.bit.REG_PRE_D_EN_3 = 1;
            break;
        case 4:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_LUT_IDX_4 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_EN_4 = 1;
            break;
        case 5:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_LUT_IDX_5 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_EN_5 = 1;
            break;
        case 6:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_LUT_IDX_6 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_EN_6 = 1;
            break;
        case 7:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_LUT_IDX_7 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.bit.REG_PRE_D_EN_7 = 1;
            break;
        case 8:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_LUT_IDX_8 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_EN_8 = 1;
            break;
        case 9:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_LUT_IDX_9 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_EN_9 = 1;
            break;
        case 10:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_LUT_IDX_10 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_EN_10 = 1;
            break;
        case 11:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_LUT_IDX_11 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.bit.REG_PRE_D_EN_11 = 1;
            break;
        case 12:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_LUT_IDX_12 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_EN_12 = 1;
            break;
        case 13:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_LUT_IDX_13 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_EN_13 = 1;
            break;
        case 14:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_LUT_IDX_14 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_EN_14 = 1;
            break;
        case 15:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_LUT_IDX_15 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.bit.REG_PRE_D_EN_15 = 1;
            break;
        case 16:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_4.bit.REG_PRE_D_LUT_IDX_16 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_4.bit.REG_PRE_D_EN_16 = 1;
            break;
        case 17:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_4.bit.REG_PRE_D_LUT_IDX_17 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_4.bit.REG_PRE_D_EN_17 = 1;
            break;
        case 18:
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_4.bit.REG_PRE_D_LUT_IDX_18 = lut_idx;
            NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_4.bit.REG_PRE_D_EN_18 = 1;
            break;
        default:
            break;
     }
    #endif
    return 0;
}

static int8_t wf_cali_calc_table_shift(int8_t power_offset)
{
    int8_t shift = 0;
    if (power_offset > 0) {
        shift = power_offset >> 3;
        int8_t power_remainder = power_offset & 0x7;
        if (power_remainder > 0)
            shift ++;
    }
    else if (power_offset < 0) {
        shift = -((-power_offset) >> 3);
    }
    return shift;
}

static void wf_cali_remap_dpd_cfg(P_RF_CALI_DPD_CFG entry, int8_t shift_table)
{
    entry->pwr_idx += shift_table;
    entry->tssi += (shift_table << 1);
    if (entry->pwr_idx > 18)
        entry->pwr_idx = 18;
    if (entry->tssi > 22)
        entry->tssi = 22;
    wf_cali_txdpd_map_pred_lut((uint32_t)entry->pwr_idx, (uint32_t)entry->pred_lut_idx);
    //CLOGI("DPD TSSI=%d, PWR_IDX=%d, LUT_IDX=%d\n", entry->tssi, entry->pwr_idx, entry->pred_lut_idx);
}

static int8_t wf_cali_txdpd_remap_table(P_RF_CALI_DPD_CFG dst, P_RF_CALI_DPD_CFG src, uint8_t cnt, int8_t shift_table)
{
    P_RF_CALI_PARAMS params = &rf_cali.params;
    uint32_t tbl_idx = (params->txdpd_tbl_idx < cnt) ? params->txdpd_tbl_idx : 0;
    uint32_t dpd_tr_lut_idx, dpd_tr_pwr_idx;

    for (uint32_t i = 0; i < cnt; i++) {
        dst[i] = src[i];
        wf_cali_remap_dpd_cfg(&dst[i], shift_table);
    }
    dpd_tr_lut_idx = (uint32_t)src[tbl_idx].pred_lut_idx;
    dpd_tr_pwr_idx = (uint32_t)src[tbl_idx].pwr_idx;
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t rest_pwr_idx = dpd_tr_pwr_idx - i - 1 + shift_table;
        uint32_t rest_pred_lut_idx = dpd_tr_lut_idx - i - 1;
        if ((int32_t)rest_pred_lut_idx < 0)
            break;
        wf_cali_txdpd_map_pred_lut(rest_pwr_idx, rest_pred_lut_idx);
        //CLOGI("DPD rest PWR_IDX=%d, LUT_IDX=%d\n", rest_pwr_idx, rest_pred_lut_idx);
    }
    return 0;
}

__STATIC int8_t wf_cali_txdpd_remap_pred(void)
{
    int8_t power_offset = SIGN(NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET, 6);
    int8_t shift_table = wf_cali_calc_table_shift(power_offset);

    NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_0.all = 0;
    NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_1.all = 0;
    NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_2.all = 0;
    NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_3.all = 0;
    NEW_DFE->REG_PRE_D_EN_AND_LUT_IDX_4.all = 0;
    memset(dpd_cfg_table, 0, sizeof(RF_CALI_DPD_CFG)*DPD_COMP_TABLE_CNT);
    wf_cali_txdpd_remap_table(dpd_cfg_table, dpd_base_table, DPD_COMP_TABLE_CNT, shift_table);
    return 0;
}

#if 0
static uint8_t saved_cur_mac_state = 0;
static uint32_t saved_idle_interrupt_mask = 0;

__STATIC void wf_cali_save_mac_state_and_force_idle(void)
{
    saved_cur_mac_state = WIFI_MAC_CORE->REG_STATECNTRLREG.bit.CURRENTSTATE;
    saved_idle_interrupt_mask = WIFI_MAC_PL->REG_GENINTENABLEREG.all;
    WIFI_MAC_PL->REG_GENINTENABLEREG.bit.IDLEINTERRUPT = 0;
    WIFI_MAC_CORE->REG_STATECNTRLREG.bit.NEXTSTATE = 0;
}

__STATIC void wf_cali_restore_mac_state(void)
{
    WIFI_MAC_CORE->REG_STATECNTRLREG.bit.NEXTSTATE = saved_cur_mac_state;
    CLOGD("saved_cur_mac_state=%d, restored WIFI_MAC_CORE->REG_STATECNTRLREG=0x%x\n", saved_cur_mac_state, WIFI_MAC_CORE->REG_STATECNTRLREG.all);
    WIFI_MAC_PL->REG_GENINTACKREG.bit.IDLEINTERRUPT = 1;
    WIFI_MAC_PL->REG_GENINTENABLEREG.all = saved_idle_interrupt_mask;
}
#endif

__STATIC int8_t wf_cali_txdpd_init(uint8_t fb_delay)
{
    uint32_t misc_val = MEM_RD32(0x4B1000e0);

    //wf_cali_save_mac_state_and_force_idle();
    MEM_WR32(0x4B1000e0, misc_val & (~0x100));
    set_rx_gain(4/*lna*/, 10/*abb_bq*/, 10/*abb_buf*/);
    wf_cali_dpd_fb_enable();
    NEW_DFE->REG_CALIBR_TOP_0.bit.CALIBR_MODE = TX_IQ_TTG_CALI;
    NEW_DFE->REG_CALIBR_TOP_0.bit.CALIBR_WORK_EN = 1;
    NEW_DFE->REG_CFR_POST_DIG_GAIN_EN.bit.REG_CFR_POST_DIG_GAIN_EN = 1;
    /* Reset power offset which should be restored after DPD cali*/
    NEW_DFE->REG_TPC_CTRL_COMMON.bit.CFG_TPC_PWR_OFFSET = 0;
    NEW_DFE->REG_CFR_POST_DIG_GAIN_8.bit.REG_CFR_POST_DIG_GAIN_17 = 512;
    NEW_DFE->REG_CFR_POST_DIG_GAIN_9.bit.REG_CFR_POST_DIG_GAIN_18 = 512;
    //NEW_DFE->REG_DFE_SHAREMEM_START_ADDR_0.bit.REG_DFE_SHAREMEM_START_ADDR_0 = 0;
    NEW_DFE->REG_DFE_SHAREMEM_START_ADDR_1.bit.REG_DFE_SHAREMEM_START_ADDR_1 = CALI_MEM_START_OFFSET;
    //NEW_DFE->REG_M0_DUMP_AND_BYPASS_LEN.bit.REG_DFE_M0_BYPASS_LEN = 6400;
    //NEW_DFE->REG_M0_DUMP_AND_BYPASS_LEN.bit.REG_DFE_M0_DUMP_TAIL_LEN_POWER = 12;
    NEW_DFE->REG_DFE_SHAREMEM_LEN_0.bit.REG_DFE_SHAREMEM_LENGTH_0 = 0;
    NEW_DFE->REG_M0_M1_DUMP_AND_TRIG_SEL.bit.REG_M0_DUMP_SEL = 0;
    NEW_DFE->REG_M1_DUMP_AND_BYPASS_LEN.bit.REG_DFE_M1_BYPASS_LEN = 6417 - fb_delay;
    NEW_DFE->REG_M1_DUMP_AND_BYPASS_LEN.bit.REG_DFE_M1_DUMP_TAIL_LEN_POWER = 12;
    NEW_DFE->REG_DFE_SHAREMEM_LEN_1.bit.REG_DFE_SHAREMEM_LENGTH_1 = 4096;
    NEW_DFE->REG_M0_M1_DUMP_AND_TRIG_SEL.bit.REG_M1_DUMP_SEL = 4;
    NEW_DFE->REG_MP_DPD_TX2FB_DLY.bit.REG_MP_DPD_TX2FB_DELAY = 25;
    rf_udelay(100);
    //CLOGD("wf_cali_txdpd_init\n");
    return 0;
}

__STATIC int8_t wf_cali_txdpd_adjust_gain(int8_t pwr_delta)
{
    uint8_t abb_1st_val = get_abb_1st_gain();
    uint8_t abb_bq_val = get_abb_bq_gain();
    uint8_t abb_buf_val = get_abb_buf_gain();
    int8_t rest_pwr_delta = 0;

    if (pwr_delta > 0 && abb_bq_val == 13 && abb_buf_val == 10) {
        if ((int8_t)abb_1st_val - ((pwr_delta+1)>>1) < 0) {
            abb_1st_val = 0;
        }
        else {
            abb_1st_val = (uint8_t)((int8_t)abb_1st_val-((pwr_delta+1)>>1));
        }
        set_abb_1st_gain(abb_1st_val);
        //CLOGI("set abb_1st_gain=%d\n", abb_1st_val);
    }
    else {
        if (((int8_t)abb_bq_val+(pwr_delta>>1)) < 0) {
            rest_pwr_delta = pwr_delta - (abb_bq_val<<1);
            abb_bq_val = 0;
            if (((int8_t)abb_buf_val+(rest_pwr_delta>>1)) < 0)
                abb_buf_val = 0;
            else
                abb_buf_val = (uint8_t)((int8_t)abb_buf_val+rest_pwr_delta);
        }
        else if (((int8_t)abb_bq_val+(pwr_delta>>1)) > 13) {
            rest_pwr_delta = pwr_delta - ((13-abb_bq_val)<<1);
            abb_bq_val = 13;
            if (((int8_t)abb_buf_val+(rest_pwr_delta>>1)) > 10)
                abb_buf_val = 10;
            else
                abb_buf_val = (uint8_t)((int8_t)abb_buf_val+rest_pwr_delta);
        }
        else {
            abb_bq_val = (uint8_t)((int8_t)get_abb_bq_gain()+((pwr_delta)>>1));
            abb_buf_val = (uint8_t)((int8_t)get_abb_buf_gain()+(pwr_delta%2));
            if (abb_buf_val > 10) {
                if (abb_bq_val < 13) {
                    abb_bq_val ++;
                    abb_buf_val -= 2;
                }
                else {
                    abb_buf_val = 10;
                }
            }
            if (abb_buf_val < 0) {
                if (abb_bq_val > 0) {
                    abb_bq_val --;
                    abb_buf_val += 2;
                }
                else {
                    abb_buf_val = 0;
                }
            }
        }
        set_abb_bq_gain(abb_bq_val);
        set_abb_buf_gain(abb_buf_val);
        //CLOGI("set abb_bq_gain=%d\n", abb_bq_val);
        //CLOGI("set abb_buf_gain=%d\n", abb_buf_val);
    }
    return 0;
}

__STATIC int8_t wf_cali_txdpd_measure(int8_t pwr_idx)
{
    uint8_t capture_cnt = 0;

#define TXDPD_MAX_CAP_CNY 4
    while(capture_cnt++ < TXDPD_MAX_CAP_CNY)
    {
        if (NEW_DFE->REG_DUMP_EST_RESULT_VLD_RPT.bit.DFE_DUMP_AFIFO_FULL_CNT_RPT != 0) {
            NEW_DFE->REG_DUMP_AFIFO_FULL_CNT_RPT_CLR.bit.REG_DUMP_AFIFO_FULL_CNT_RPT_CLR = 0;
            rf_udelay(10);
            NEW_DFE->REG_DUMP_AFIFO_FULL_CNT_RPT_CLR.bit.REG_DUMP_AFIFO_FULL_CNT_RPT_CLR = 1;
            rf_udelay(10);
            NEW_DFE->REG_DUMP_AFIFO_FULL_CNT_RPT_CLR.bit.REG_DUMP_AFIFO_FULL_CNT_RPT_CLR = 0;
            rf_udelay(10);
        }
        macbypass_tx_stop();
        macbypass_tx_one_frame(pwr_idx, 9);
        if (NEW_DFE->REG_DUMP_EST_RESULT_VLD_RPT.bit.DFE_DUMP_AFIFO_FULL_CNT_RPT == 0)
            break;
        else
            CLOGI("wf_cali_txdpd_measure failed since afifo full\n");
    }
    if (capture_cnt >= TXDPD_MAX_CAP_CNY) {
        CLOGW("[FinalFault]wf_cali_txdpd_measure failed since afifo full\n");
        goto fail;
    }

    //CLOGD("wf_cali_txdpd_measure\n");
    return 0;
fail:
    return -1;
}

__STATIC int8_t wf_cali_txdpd_deinit(void)
{
    uint32_t misc_val = MEM_RD32(0x4B1000e0);

    wf_cali_dpd_fb_disable();
    macbypass_tx_stop();
    NEW_DFE->REG_CALIBR_TOP_0.bit.CALIBR_MODE = 0;
    NEW_DFE->REG_CALIBR_TOP_0.bit.CALIBR_WORK_EN = 0;
    //wf_cali_restore_mac_state();
    MEM_WR32(0x4B1000e0, misc_val | 0x100);
    return 0;
}

static uint32_t *get_table_start_addr(uint8_t tbl_idx)
{
    volatile uint32_t *table_start_addr;

    switch(tbl_idx) {
        case 0:
        table_start_addr = &NEW_DFE->REG_PRE_D_PARA_A00_0.all;
        break;
        case 1:
        table_start_addr = &NEW_DFE->REG_PRE_D_PARA_A00_1.all;
        break;
        case 2:
        table_start_addr = &NEW_DFE->REG_PRE_D_PARA_A00_2.all;
        break;
        case 3:
        table_start_addr = &NEW_DFE->REG_PRE_D_PARA_A00_3.all;
        break;
        case 4:
        table_start_addr = &NEW_DFE->REG_PRE_D_PARA_A00_4.all;
        break;
        case 5:
        table_start_addr = &NEW_DFE->REG_PRE_D_PARA_A00_5.all;
        break;
        default:
        table_start_addr = &NEW_DFE->REG_PRE_D_PARA_A00_0.all;
        break;
    }
    return (uint32_t *)table_start_addr;
}

__STATIC int8_t wf_cali_txdpd_result(uint8_t tbl_idx, void *tbl_src)
{
    uint32_t *table_start_addr = get_table_start_addr(tbl_idx);

    if (tbl_src == NULL)
        return -1;

    if (tbl_idx == 2) {
        uint32_t *tmp_src = (uint32_t *)tbl_src + 10;

        memcpy((void *)table_start_addr, (void *)tbl_src, 10 * sizeof(uint32_t));
        memcpy((void *)(table_start_addr+10+1), (void*)tmp_src, 5 * sizeof(uint32_t));
    } else {
        memcpy((void *)table_start_addr, (void *)tbl_src, 15 * sizeof(uint32_t));
    }
    return 0;
}

__STATIC int8_t wf_cali_txdpd_get_result(uint8_t tbl_idx, void *tbl_dst)
{
    uint32_t *table_start_addr = get_table_start_addr(tbl_idx);

    if (tbl_dst == NULL)
        return -1;

    if (tbl_idx == 2) {
        uint32_t *tmp_dst = (uint32_t *)tbl_dst + 10;

        memcpy((void *)tbl_dst, (void *)table_start_addr, 10 * sizeof(uint32_t));
        memcpy((void *)tmp_dst, (void *)(table_start_addr+10+1), 5 * sizeof(uint32_t));
    } else {
        memcpy((void *)tbl_dst, (void *)table_start_addr, 15 * sizeof(uint32_t));
    }
    return 0;
}

int8_t wf_cali_txdc_result(void *comp_dc, uint8_t range)
{
    complexint16 *p_dc_comp = (complexint16 *)comp_dc;

    if (range == 0) {
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I0.bit.CFG_TPC_DCCOMP_I_0 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I0.bit.CFG_TPC_DCCOMP_I_1 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I1.bit.CFG_TPC_DCCOMP_I_2 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I1.bit.CFG_TPC_DCCOMP_I_3 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I2.bit.CFG_TPC_DCCOMP_I_4 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I2.bit.CFG_TPC_DCCOMP_I_5 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I3.bit.CFG_TPC_DCCOMP_I_6 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I3.bit.CFG_TPC_DCCOMP_I_7 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I4.bit.CFG_TPC_DCCOMP_I_8 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I4.bit.CFG_TPC_DCCOMP_I_9 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I5.bit.CFG_TPC_DCCOMP_I_10 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I5.bit.CFG_TPC_DCCOMP_I_11 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I6.bit.CFG_TPC_DCCOMP_I_12 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I6.bit.CFG_TPC_DCCOMP_I_13 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I7.bit.CFG_TPC_DCCOMP_I_14 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I7.bit.CFG_TPC_DCCOMP_I_15 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I8.bit.CFG_TPC_DCCOMP_I_16 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I8.bit.CFG_TPC_DCCOMP_I_17 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I9.bit.CFG_TPC_DCCOMP_I_18 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q0.bit.CFG_TPC_DCCOMP_Q_0  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q0.bit.CFG_TPC_DCCOMP_Q_1  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q1.bit.CFG_TPC_DCCOMP_Q_2  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q1.bit.CFG_TPC_DCCOMP_Q_3  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q2.bit.CFG_TPC_DCCOMP_Q_4  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q2.bit.CFG_TPC_DCCOMP_Q_5  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q3.bit.CFG_TPC_DCCOMP_Q_6  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q3.bit.CFG_TPC_DCCOMP_Q_7  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q4.bit.CFG_TPC_DCCOMP_Q_8  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q4.bit.CFG_TPC_DCCOMP_Q_9  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q5.bit.CFG_TPC_DCCOMP_Q_10 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q5.bit.CFG_TPC_DCCOMP_Q_11 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q6.bit.CFG_TPC_DCCOMP_Q_12 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q6.bit.CFG_TPC_DCCOMP_Q_13 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q7.bit.CFG_TPC_DCCOMP_Q_14 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q7.bit.CFG_TPC_DCCOMP_Q_15 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q8.bit.CFG_TPC_DCCOMP_Q_16 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q8.bit.CFG_TPC_DCCOMP_Q_17 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q9.bit.CFG_TPC_DCCOMP_Q_18 = p_dc_comp->im & 0x0fff;
    }
    else if (range == 1) {
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I5.bit.CFG_TPC_DCCOMP_I_10 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I5.bit.CFG_TPC_DCCOMP_I_11 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I6.bit.CFG_TPC_DCCOMP_I_12 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I6.bit.CFG_TPC_DCCOMP_I_13 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I7.bit.CFG_TPC_DCCOMP_I_14 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I7.bit.CFG_TPC_DCCOMP_I_15 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I8.bit.CFG_TPC_DCCOMP_I_16 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I8.bit.CFG_TPC_DCCOMP_I_17 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I9.bit.CFG_TPC_DCCOMP_I_18 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q5.bit.CFG_TPC_DCCOMP_Q_10 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q5.bit.CFG_TPC_DCCOMP_Q_11 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q6.bit.CFG_TPC_DCCOMP_Q_12 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q6.bit.CFG_TPC_DCCOMP_Q_13 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q7.bit.CFG_TPC_DCCOMP_Q_14 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q7.bit.CFG_TPC_DCCOMP_Q_15 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q8.bit.CFG_TPC_DCCOMP_Q_16 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q8.bit.CFG_TPC_DCCOMP_Q_17 = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q9.bit.CFG_TPC_DCCOMP_Q_18 = p_dc_comp->im & 0x0fff;
    }
    else if (range == 2) {
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I0.bit.CFG_TPC_DCCOMP_I_0 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I0.bit.CFG_TPC_DCCOMP_I_1 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I1.bit.CFG_TPC_DCCOMP_I_2 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I1.bit.CFG_TPC_DCCOMP_I_3 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I2.bit.CFG_TPC_DCCOMP_I_4 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I2.bit.CFG_TPC_DCCOMP_I_5 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I3.bit.CFG_TPC_DCCOMP_I_6 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I3.bit.CFG_TPC_DCCOMP_I_7 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I4.bit.CFG_TPC_DCCOMP_I_8 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_I4.bit.CFG_TPC_DCCOMP_I_9 = p_dc_comp->re & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q0.bit.CFG_TPC_DCCOMP_Q_0  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q0.bit.CFG_TPC_DCCOMP_Q_1  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q1.bit.CFG_TPC_DCCOMP_Q_2  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q1.bit.CFG_TPC_DCCOMP_Q_3  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q2.bit.CFG_TPC_DCCOMP_Q_4  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q2.bit.CFG_TPC_DCCOMP_Q_5  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q3.bit.CFG_TPC_DCCOMP_Q_6  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q3.bit.CFG_TPC_DCCOMP_Q_7  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q4.bit.CFG_TPC_DCCOMP_Q_8  = p_dc_comp->im & 0x0fff;
        NEW_DFE->REG_TPC_CTRL_DCCOMP_Q4.bit.CFG_TPC_DCCOMP_Q_9  = p_dc_comp->im & 0x0fff;
    }
    NEW_DFE->REG_COMPS_CFG0.bit.TX_TXDCCOMPEN = 1;

    #if RFCALI_BT_EN
    BT_MODEM->REG_TX_CFG5.bit.TX_DCVALUE_I = (p_dc_comp->re & 0x0fff)<<3;
    BT_MODEM->REG_TX_CFG5.bit.TX_DCVALUE_Q = (p_dc_comp->im & 0x0fff)<<3;
    g_bt_txdc_comp.dac_i = BT_MODEM->REG_TX_CFG5.bit.TX_DCVALUE_I;
    g_bt_txdc_comp.dac_q = BT_MODEM->REG_TX_CFG5.bit.TX_DCVALUE_Q;
    #endif
    return 0;
}

__STATIC int8_t wf_cali_set_ppa_cap(uint8_t idx, uint8_t val)
{
    switch(idx) {
        case 0:
        RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM = val;
        RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_DSSS = val;
        break;
        case 1:
        RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM = val;
        RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_DSSS = val;
        break;
        case 2:
        RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM = val;
        RFIF->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_CAP_SW_WF_2_DSSS = val;
        break;
        default:
        break;
    }
    return 0;
}

__STATIC int8_t wf_cali_get_ppa_cap(uint8_t idx)
{
    uint8_t ret = 0;

    switch(idx) {
        case 0:
        ret = RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_0_OFDM;
        break;
        case 1:
        ret = RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_1_OFDM;
        break;
        case 2:
        ret = RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_CAP_SW_WF_2_OFDM;
        break;
        default:
        break;
    }
    return ret;
}

#endif

#if RFCALI_WF_EN

void sx_force_init(void)
{
#if 0
    SYS_NODFT->REG_RFBG_LOGIC0.bit.REG_RFBG_EN = 1;
    SYS_NODFT->REG_RFBG_LOGIC0.bit.RFBG_EN_FORCE = 1;

    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_LDO_EN_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_VCO_LDO_EN_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_PFD_EN_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_CP_EN_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_VCO_EN_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_FBDIV_EN_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_POSTDIV_SEL_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.BBPLL_ADDABUF_EN_FORCE = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_LDO_EN = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_VCO_LDO_EN = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_PFD_EN = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_CP_EN = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_VCO_EN = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_FBDIV_EN = 0x1; // 1 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_POSTDIV_SEL = 0x1; // 2 bits
    SYS_NODFT->REG_BBPLL_LOGIC0.bit.REG_BBPLL_ADDABUF_EN = 0x1; // 1 bits

    RFIF->REG_SX_LOGIC0.bit.RF_SX_LDO_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC0.bit.RF_SX_PFD_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC0.bit.RF_SX_CP_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC0.bit.RF_SX_FBDIV_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_LDO_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_PFD_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_CP_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC0.bit.REG_RF_SX_FBDIV_EN = 0x1; // 1 bits

    RFIF->REG_SX_LOGIC2.bit.RF_SX_VCO_HLDO_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC2.bit.REG_RF_SX_VCO_HLDO_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC2.bit.RF_SX_VCO_LLDO_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC2.bit.REG_RF_SX_VCO_LLDO_EN = 0x1; // 1 bits

    RFIF->REG_SX_LOGIC3.bit.RF_SX_VCO_IREF_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.REG_RF_SX_VCO_IREF_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.RF_SX_VCO_VARBIAS_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.REG_RF_SX_VCO_VARBIAS_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.RF_SX_VCO_CORE_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.REG_RF_SX_VCO_CORE_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.RF_SX_VCO_BUF_DIV_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.REG_RF_SX_VCO_BUF_DIV_EN = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.RF_SX_VCO_BUF_LOG_EN_FORCE = 0x1; // 1 bits
    RFIF->REG_SX_LOGIC3.bit.REG_RF_SX_VCO_BUF_LOG_EN = 0x1; // 1 bits
#endif
}

__STATIC void wf_cali_env_init(void)
{
    save_reg_config();

    //SYS_NODFT->REG_BBPLL_CFG0.bit.BBPLL_ENABLE = 0x1;
    //SYS_NODFT->REG_BBPLL_CFG0.bit.BBPLL_ADDABUF_WFCLKEN = 0x1;

    // WF MODE
    RFIF->REG_CTRL0.bit.REG_BT_WF = 0; // 0:wifi 1:bt
    RFIF->REG_CTRL0.bit.BT_WF_FORCE = 1;

    RFIF->REG_CTRL0.bit.WF_END = 1;
    RFIF->REG_CTRL0.bit.CLK_FORCE_ON_WF = 1;

    RFIF->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_EN = 0;
    RFIF->REG_TX_LOGIC0.bit.RF_TX_PPA_EN_FORCE = 0;
    RFIF->REG_TX_LOGIC9.bit.REG_RF_TX_PA_EN = 0;
    RFIF->REG_TX_LOGIC9.bit.RF_TX_PA_EN_FORCE = 0;

    NEW_DFE->REG_RX_DOWNSAMPLE_EN.bit.REG_RX_DOWNSAMPLE_EN = 0;
    NEW_DFE->REG_RX_FDIQ_COMP_EN.bit.REG_RX_FDIQ_COMP_EN = 0;
    NEW_DFE->REG_RX_CALIBR_FREQSHIFT.bit.RX_CALIBR_FO_BYPASS = 1;
    NEW_DFE->REG_DFE_SPUR_CANCEL_CTRL1.bit.CFG_WIFI_RX_NOTCH_FILTER_EN_FORCE_EN = 0;
    NEW_DFE->REG_DFE_SPUR_CANCEL_CTRL0.bit.CFG_SPUR_CHANNEL_EN = 0;
    NEW_DFE->REG_NEW_DFE_HPF1_PART1.bit.REG_HPF_EN = 0;
    NEW_DFE->REG_AGC_BO_CFG0.bit.CFG_OVERLOAD_DET_EN = 0;
    WIFI_CTRL->REG_WIFI_CALIB_RAMIF_DUMP_EN.bit.CFG_CALIB_RAMIF_DUMP_EN = 0;
    NEW_DFE->REG_DFE_EST_CTRL_EN.bit.REG_DFE_EST_RESULT_SHIFT_EN = 1;
    NEW_DFE->REG_TX_CFR_COMMON_13.bit.CFGHCEN = 0;
    NEW_DFE->REG_TX_CFR0.bit.CFGCFR0EN = 0;
    NEW_DFE->REG_TX_CFR1.bit.CFGCFR1EN = 0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_0 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_1 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_2 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_3 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_4 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_5 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_6 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_7 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_8 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN0.bit.CFG_TPC_CFREN_9 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_10 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_11 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_12 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_13 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_14 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_15 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_16 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_17 = 0x0;
    NEW_DFE->REG_TPC_CTRL_CFREN1.bit.CFG_TPC_CFREN_18 = 0x0;

    NEW_DFE->REG_NEW_DFE_TX_FILT_GAIN.bit.CFG_NEW_TXMASK_FLT_EN = 0;

    //CLOGD("wf_cali_env_init\n");
}
#endif

#if RFCALI_BT_EN
__STATIC void bt_cali_env_init(void)
{
    ls_rf_probe();
    save_reg_config();

#if defined(WCN_TYPE_WF)
    RFIF->REG_CTRL0.bit.WF_END = 1;
#endif

    /* Makesure RF in BT mode */
    RFIF->REG_CTRL0.bit.REG_BT_WF = 1;
    RFIF->REG_CTRL0.bit.BT_WF_FORCE = 1;
    /* set BBPLL clk 96M */
    BT_MODEM->REG_TOP_CFG0.bit.TOP_NORMAL_WORK_EN = 0;
    //CLOGD("bt_cali_env_init\n");
}
#endif

#if RFCALI_WF_EN
__STATIC void wf_cali_env_deinit(void)
{
    wf_cali_work_en(0);

    //wf_phy_sw_reset();
    dis_ttgpll();
    rf_udelay(100);

    RFIF->REG_CTRL0.bit.BT_WF_FORCE = 0;
    RFIF->REG_CTRL0.bit.CLK_FORCE_ON_WF = 0;
    RFIF->REG_CTRL0.bit.WF_START = 1;

    NEW_DFE->REG_DFE_ARB_DATA_SEL.bit.REG_DFE_ARB_DATA_SEL = 0;
    NEW_DFE->REG_DFE_ARB_EN.bit.REG_DFE_ARB_EN = 0;
    restore_reg_config();
    //rwnxl_reset_evt(0);
    //CLOGD("wf_cali_env_deinit\n");
}
#endif
#if RFCALI_BT_EN
__STATIC void bt_cali_env_deinit(void)
{
    dis_ttgpll();
    BT_MODEM->REG_TOP_CFG0.bit.TOP_CALIBR_WORK_EN = 0;
    BT_MODEM->REG_BT_RX_GLB_CFG.bit.RX_CALIB_EN = 0;
    IP_BT_CTRL->REG_BT_CTRL_MODEM_CALIB.bit.CALIB_ACCESS_EM = 0;
    RFIF->REG_SX_LOGIC0.bit.RF_SX_DIVN_INTEG_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIVN_FRAC_FORCE = 0;
    RFIF->REG_SX_LOGIC1.bit.RF_SX_DIG_START_FORCE = 0;
    RFIF->REG_CTRL0.bit.REG_BT_WF = 0;
    RFIF->REG_CTRL0.bit.BT_WF_FORCE = 0;
#if defined(WCN_TYPE_WF)
    RFIF->REG_CTRL0.bit.WF_START = 1;
#endif
    restore_reg_config();
    //CLOGD("bt_cali_env_deinit\n");
}
#endif


#if RFCALI_WF_EN
RF_CALI_OPS wf_cali_ops = {
    .env_init = wf_cali_env_init,
    .env_deinit = wf_cali_env_deinit,
    .rxdcoc_init = wf_cali_rxdcoc_init,
    .rxdcoc_measure = wf_cali_rxdcoc_measure,
    .rxdcoc_detect_interference = wf_cali_rxdcoc_detect_interference,
    .rxdcoc_result = wf_cali_rxdcoc_result,
    .rxrc_init = wf_cali_rxrc_init,
    .rxrc_measure = wf_cali_rxrc_measure,
    .rxrc_result = wf_cali_rxrc_result,
    .rxiq_init = wf_cali_rxiq_init,
    .rxiq_measure = wf_cali_rxiq_measure,
    .rxiq_result = wf_cali_rxiq_result,
    .txiq_fb_init = wf_cali_txiq_fb_init,
    .txiq_fb_measure = wf_cali_txiq_fb_measure,
    .txiq_fb_result = wf_cali_txiq_fb_result,
    .txiq_fb_deinit = wf_cali_txiq_fb_deinit,
    .txiq_tx_init = wf_cali_txiq_tx_init,
    .txiq_tx_measure = wf_cali_txiq_tx_measure,
    .txiq_tx_result = wf_cali_txiq_tx_result,
    .txiq_dump_data = wf_cali_txiq_dump_data,
    .txiq_restore_rxiq_result = wf_cali_txiq_restore_rxiq_result,
    .txdpd_remap_pred = wf_cali_txdpd_remap_pred,
    .txdpd_init = wf_cali_txdpd_init,
    .txdcdpd_toggle_mixen = wf_cali_txdcdpd_toggle_mixen,
    .txdpd_adjust_gain = wf_cali_txdpd_adjust_gain,
    .txdpd_measure = wf_cali_txdpd_measure,
    .txdpd_result = wf_cali_txdpd_result,
    .txdpd_get_result = wf_cali_txdpd_get_result,
    .txdpd_deinit = wf_cali_txdpd_deinit,
    .txdc_result = wf_cali_txdc_result,
    .set_ppa_cap = wf_cali_set_ppa_cap,
    .get_ppa_cap = wf_cali_get_ppa_cap,
    .send_ttg_start = wf_cali_send_ttg_start,
    .send_ttg_stop = wf_cali_send_ttg_stop,
};
#endif

#if RFCALI_BT_EN
RF_CALI_OPS bt_cali_ops = {
    .env_init = bt_cali_env_init,
    .env_deinit = bt_cali_env_deinit,
    .rxdcoc_init = bt_cali_rxdcoc_init,
    .rxdcoc_measure = bt_cali_rxdcoc_measure,
    .rxdcoc_result = bt_cali_rxdcoc_result,
    .rxrc_init = bt_cali_rxrc_init,
    .rxrc_measure = bt_cali_rxrc_measure,
    .rxrc_result = bt_cali_rxrc_result,
    .rxiq_init = bt_cali_rxiq_init,
    .rxiq_measure = bt_cali_rxiq_measure,
    .rxiq_result = bt_cali_rxiq_result,
};
#endif
