/**
 ****************************************************************************************
 *
 * @file rf_cali.c
 *
 * @brief functions of RF calibraion module
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Sep 15, 2023
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */


#include <string.h>
#include <stdio.h>
#include "log_print.h"
#include "rf_fxp.h"
#include "rf_cali.h"
#include "DpdEst.h"
#include "arcs_ap.h"
#include "FbSignal.h"
#include "TxSignal.h"
#if DPD_LUNA
#include "luna.h"
#include "DpdEstLuna.h"
#endif
#include "nv_otp.h"
#include "rf_cali_chk.h"
#include "wifi_api.h"
#include "wf_soc_drv.h"

#define __STATIC
#define NEW_DFE IP_NEW_DFE

#if defined(RFCALI_WF_EN)
extern RF_CALI_OPS wf_cali_ops;
#endif
#if defined(RFCALI_BT_EN)
extern RF_CALI_OPS bt_cali_ops;
uint8_t dcoc_diff_thred = 10;
RF_CALI_RXDCOC_WORD g_bt_rxdc_comp[3][3];
RF_CALI_RXIQ_WORD g_bt_rxiq_comp;
RF_CALI_TXDC_WORD g_bt_txdc_comp;
RF_CALI_TXIQ_WORD g_bt_txiq_comp;
uint8_t g_bt_rxrc_comp;
#endif

//#define PPA_CAP_USE_SOFT_CALC
#define FB_VEC_SIZE 4096
#define POWER_MEASURE_TIMES 32
#define POWER_MEASURE_THRESHOLD_LOW  200000
#define POWER_MEASURE_THRESHOLD_HIGH 420000
#define CALI_CHECK_RESULT
#define MEM_RD32(addr)              (*(volatile uint32_t *)(addr))
#define MEM_WR32(addr, value)       (*(volatile uint32_t *)(addr)) = (value)
#if DPD_LUNA
#define __HAL_CRM_LUNA_CLK_ENABLE()    \
do { \
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_LUNA_CLK = 0x1; \
} while(0)

//int32_t iTxSignalVecLunaRe[SEL_LEN] __attribute__((section(".cali_luna_bss")));
//int32_t iTxSignalVecLunaIm[SEL_LEN] __attribute__((section(".cali_luna_bss")));
//int32_t iFbSignalVecLunaRe[SEL_LEN] __attribute__((section(".cali_luna_bss")));
//int32_t iFbSignalVecLunaIm[SEL_LEN] __attribute__((section(".cali_luna_bss")));
#if 0
int32_t *iTxSignalVecLunaRe = (int32_t *)(0x200A0000);
int32_t *iTxSignalVecLunaIm = (int32_t *)(0x200A4000);
int32_t *iFbSignalVecLunaRe = (int32_t *)(0x200A8000);
int32_t *iFbSignalVecLunaIm = (int32_t *)(0x200AC000);
int32_t *pApiTemp32 =  (int32_t *)(0x20060000);
int64_t *pApiTemp64 =  (int64_t *)(0x20068000);
int32_t *pAmpSig = (int32_t *)(0x20070000);
#else
int32_t *iTxSignalVecLunaRe = (int32_t *)(LUNA_VECTOR_ADDR);
int32_t *iTxSignalVecLunaIm = (int32_t *)(LUNA_VECTOR_ADDR + LUNA_VECTOR_QUAD_SIZE);
int32_t *iFbSignalVecLunaRe = (int32_t *)(LUNA_VECTOR_ADDR + LUNA_VECTOR_QUAD_SIZE*2);
int32_t *iFbSignalVecLunaIm = (int32_t *)(LUNA_VECTOR_ADDR + LUNA_VECTOR_QUAD_SIZE*3);
int32_t *pApiTemp32 =  (int32_t *)(LUNA_TEMP32_ADDR);
int64_t *pApiTemp64 =  (int64_t *)(LUNA_TEMP64_ADDR);
int32_t *pAmpSig = (int32_t *)(LUNA_AMPSIG_ADDR);
#endif

#endif

volatile uint8_t wifi_cali[WIFI_CALI_BUF_SIZE] __attribute__ ((section("WIFI_CALI"), retain));

RF_CALI_ENTRY rf_cali = {
    .owner = &rf_entry,
    .params = {
        .mode = RFCALI_MODE_WF,
        .rxdcoc_disable = 0,
        .rxrc_disable = 0,
        .rxiq_disable = 0,
        .txiq_disable = 0,
        .txiq_iter_times = 10,
        .txdpd_disable = 0,
        .txdpd_tbl_idx = 0xff,
        .txdpd_iter_times = 5,
        .txdpd_fb_gain = 0xff,
    },
    .ops = NULL,
    .redo_cali = NULL,
};

uint32_t  CALI_MEM_START_ADDR;
uint32_t  CALI_MEM_MID_ADDR;
uint32_t  CALI_MEM_END_ADDR;
uint32_t CALI_MEM_START_OFFSET;
uint32_t CALI_MEM_MID_OFFSET;
uint32_t CALI_MEM_END_OFFSET;

/*===================================================================================
*
* function: add sign for compensation value
* rxdc compensation value is not a sign value, only set flag in the highest bit
*
* compensation value:  bit7     bit6    bit5    bit4    bit3    bit2    bit1    bit0
*                    |         |                                                    |
*                    |         |                                                    |
*                    |direction|                        value                       |
*
* direction:  0: positive value, 1: negative value
*
 ===================================================================================*/
#define rmv_dirc(x) ((x)>=0x80? (-(x-0x80)):(x))

#define SIGN(q, bit) (((q) >= (1 << (bit) >> 1)) ? ((q) - (1 << (bit))) : (q))
#define CALI_RUNTIME_STATE_IDLE 0
#define CALI_RUNTIME_STATE_ONGOING 1
RF_CALI_DPD_CFG dpd_base_table[DPD_COMP_TABLE_CNT] = {
    {13, 12, 2},
    {14, 14, 3},
    {15, 16, 4},
    {16, 18, 5}
};
RF_CALI_DPD_CFG dpd_cfg_table[DPD_COMP_TABLE_CNT] = {0};
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

RF_CALI_POWER_DB_TBL power_db_table[] = {
{0	, 231725},  // -0.5dbm
{1	, 291725},  // 0.5dbm
{2	, 367260},  // 1.5dbm
{3	, 462353},  // 2.5dbm
{4	, 582067},  // 3.5dbm
{5	, 732780},  // 4.5dbm
{6	, 922515},  // 5.5dbm
{7	, 1161377}, // 6.5dbm
{8	, 1462087}, // 7.5dbm
{9	, 1840659}, // 8.5dbm
{10	, 2317252}  // 9.5dbm
};

static int8_t pwr_idx_dc = 16;
static int8_t pwr_idx_iq = 14;
static volatile uint8_t cali_runtime_state = CALI_RUNTIME_STATE_IDLE;


__STATIC void rf_cali_rxdcoc_sel_sc(uint8_t mode, int16_t dc_i, int16_t dc_q)
{
    uint8_t sc_i = 1;
    uint8_t sc_q = 1;

    if (mode == RFCALI_MODE_WF)
    {
        if (abs(dc_i) < 867) // dc_i < 260mv (260/(1200/2^12)=867)
        {
            // 0-260mv
            CLOGD("set SC_I = 1");
            sc_i = 1;
        }
        else if (abs(dc_i) < 1733) // dc_i < 520mv (520/(1200/2^12)=1733)
        {
            // 260-520mv
            CLOGD("set SC_I = 2");
            sc_i = 2;
        }
        else if (abs(dc_i) < 2047) // dc_i < 780mv (780/(1200/2^12)=2047)
        {
            // 520-780mv
            CLOGD("set SC_I = 3");
            sc_i = 3;
        }
        else
        {
            // >780mv
            CLOGD("set SC_I = 4");
            sc_i = 4;
        }

        if (abs(dc_q) < 867) // dc_q < 260mv (260/(1200/2^12)=867)
        {
            // 0-260mv
            CLOGD("set SC_Q = 1");
            sc_q = 1;
        }
        else if (abs(dc_q) < 1733) // dc_q < 520mv (520/(1200/2^12)=1733)
        {
            // 260-520mv
            CLOGD("set SC_Q = 2");
            sc_q = 2;
        }
        else if (abs(dc_q) < 2047) // dc_q < 780mv (780/(1200/2^12)=2047)
        {
            // 520-780mv
            CLOGD("set SC_Q = 3");
            sc_q = 3;
        }
        else
        {
            // >780mv
            CLOGD("set SC_Q = 4");
            sc_q = 4;
        }
    }
    else
    {
        //if (abs(dc_i) < 1195) // dc_i < 350mv, 350/(1200/2^12)=1195
        if (abs(dc_i) < 1000) // dc_i < 300mv, 300/(1200/2^12)=1000
        {
            // 0-400mv
            CLOGD("set SC_I = 1");
            sc_i = 1;
        }
        else if (abs(dc_i) < 2047) // -2048 <= dc_i[0:11] <= 2047
        {
            // 0-800mv
            CLOGW("set SC_I = 2");
            sc_i = 2;
        }
        else
        {
            // 0-1200mv
            CLOGE("set SC_I = 3");
            sc_i = 3;
        }

        //if (abs(dc_q) < 1195) // dc_q < 350mv, 350/(1200/2^12)=1195
        if (abs(dc_q) < 1000) // dc_q < 300mv, 300/(1200/2^12)=1000
        {
            // 0-400mv
            CLOGD("set SC_Q = 1");
            sc_q = 1;
        }
        else if (abs(dc_q) < 2047) // -2048 <= dc_q[0:11] <= 2047
        {
            // 0-800mv
            CLOGW("set SC_Q = 2");
            sc_q = 2;
        }
        else
        {
            // 0-1200mv
            CLOGE("set SC_Q = 3");
            sc_q = 3;
        }
    }

    set_sc_i(mode, sc_i);
    set_sc_q(mode, sc_q);
#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
    nv_update_selfcali_wf_rx_params(NULL, NULL, NULL, &sc_i, &sc_q, NULL, NULL);
#endif
    return;
}

__STATIC int8_t rf_cali_rxdcoc(uint16_t freq, uint8_t lna_gain, uint8_t sc_auto)
{
    int32_t dc_i;
    int32_t dc_q;
    int32_t dc_i_p;
    int32_t dc_q_p;
    int32_t dc_i_c;
    int32_t dc_q_c;
    int32_t dc_i_n = 0;
    int32_t dc_q_n = 0;
    int8_t word_i = 0;
    int8_t word_q = 0;
    int8_t direction_i;
    int8_t direction_q;
    uint8_t last_measure_i = 0;
    uint8_t last_measure_q = 0;
    int8_t i;
    uint8_t mode = rf_cali.params.mode;
    P_RF_CALI_OPS cali = rf_cali.ops;

    //CLOGD("rf_cali_rxdcoc\n");
    cali->rxdcoc_init(freq, lna_gain);
    if (cali->rxdcoc_measure(&dc_i, &dc_q, word_i, word_q))
        goto fail;

    CLOGD("inital freq=%d lna_gain=%d\n", freq, lna_gain);
    CLOGD("inital dc_i=%d word_i=%d\n", dc_i, word_i);
    CLOGD("inital dc_q=%d word_q=%d\n", dc_q, word_q);
    direction_i = (dc_i > 0) ? 1 : 0;
    direction_q = (dc_q > 0) ? 1 : 0;

    if (sc_auto)
    {
        rf_cali_rxdcoc_sel_sc(mode, dc_i, dc_q);
    }

    if (mode == RFCALI_MODE_WF) {
        word_i = (direction_i) ? 0x80 : 0x0;
        word_q = (direction_q) ? 0x80 : 0x0;
    } else {
        word_i = (direction_i) ? 0x0 : 0x80;
        word_q = (direction_q) ? 0x0 : 0x80;
    }

    for (i = 6; i >= 0; i--)
    {
        word_i |= (1 << i);
        word_q |= (1 << i);
        if (cali->rxdcoc_measure(&dc_i, &dc_q, word_i, word_q))
            goto fail;
        CLOGD("iter(%d) dc_i=%d word_i=%d\n", i, dc_i, word_i);
        CLOGD("iter(%d) dc_q=%d word_q=%d\n", i, dc_q, word_q);
        if (i == 0) {
            dc_i_c = dc_i;
            dc_q_c = dc_q;
            break;
        }
        if ((!direction_i && dc_i > 0) || (direction_i && dc_i < 0))
            word_i &= ~(1 << i);
        if ((!direction_q && dc_q > 0) || (direction_q && dc_q < 0))
            word_q &= ~(1 << i);
    }

    word_i &= ~0x1;
    word_q &= ~0x1;
    if (cali->rxdcoc_measure(&dc_i_p, &dc_q_p, word_i, word_q))
        goto fail;

    if (abs(dc_i_p) > abs(dc_i_c)) {
        word_i |= 0x1;
        word_i++;
        last_measure_i = 1;
    }
    if (abs(dc_q_p) > abs(dc_q_c)) {
        word_q |= 0x1;
        word_q++;
        last_measure_q = 1;
    }
    if (last_measure_i || last_measure_q) {
        if (cali->rxdcoc_measure(&dc_i_n, &dc_q_n, word_i, word_q))
            goto fail;
        if (last_measure_i && (abs(dc_i_c) < abs(dc_i_n)))
            word_i--;
        if (last_measure_q && (abs(dc_q_c) < abs(dc_q_n)))
            word_q--;
    }
    if (cali->rxdcoc_detect_interference && cali->rxdcoc_detect_interference(&word_i, &word_q)) {
        CLOGE("RXDCOC failed @freq=%d since detect interference\n", freq);
        goto fail;
    }
    if (cali->rxdcoc_result(&word_i, &word_q, 1)) {
        CLOGE("RXDCOC save result failed\n");
        goto fail;
    }

#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
    if (mode == RFCALI_MODE_WF)
        nv_update_selfcali_wf_rx_params(NULL, &word_i, &word_q, NULL, NULL, NULL, NULL);
#endif
    CLOGI("[RF][RXDCOC] dc_i_p/c/n=%d/%d/%d @word_i=%d\n", dc_i_p, dc_i_c, dc_i_n, word_i);
    CLOGI("[RF][RXDCOC] dc_q_p/c/n=%d/%d/%d @word_q=%d\n", dc_q_p, dc_q_c, dc_q_n, word_q);
    return 0;
fail:
    return -1;
}

__STATIC int8_t rf_cali_rxrc()
{
    int64_t E0_s;
    int64_t E1;
    int8_t direction = 0;
    uint64_t delta_min;
    uint64_t delta;
    uint8_t cap;
    int8_t i;
    uint8_t mode = rf_cali.params.mode;
    P_RF_CALI_OPS cali = rf_cali.ops;

    //CLOGD("rf_cali_rxrc\n");
    if (cali->rxrc_init(&E0_s, &cap))
        goto fail;
    CLOGD("RXRC E0_s=%lld!\n", E0_s);
    if (cali->rxrc_measure(&E1, NULL))
        goto fail;
    CLOGD("RXRC E1=%lld!\n", E1);
    direction = (E0_s > E1) ? 0 : 1;
    delta_min = abs(E0_s - E1);

    if (direction == 0)
    {
        while (cap > 0)
        {
            --cap;
            if (cali->rxrc_measure(&E1, &cap))
                goto fail;
            CLOGD("RXRC decr E1=%lld cap=%d!", E1, cap);
            delta = abs(E0_s - E1);
            if (delta < delta_min)
            {
                delta_min = delta;
            }
            else
            {
                ++cap;
                break;
            }
        }
    }
    else
    {
        while (cap <= 127)
        {
            ++cap;
            if (cali->rxrc_measure(&E1, &cap))
                goto fail;
            CLOGD("RXRC incr E1=%lld cap=%d!", E1, cap);
            delta = abs(E0_s - E1);
            if (delta < delta_min)
            {
                delta_min = delta;
            }
            else
            {
                --cap;
                break;
            }
        }
    }
    cali->rxrc_result(cap);
#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
    if (mode == RFCALI_MODE_WF)
        nv_update_selfcali_wf_rx_params(&cap, NULL, NULL, NULL, NULL, NULL, NULL);
#endif
    CLOGI("[RF][RXRC] E0/2=%lld E1=%lld @cap=%d\n", E0_s, E1, cap);

    return 0;
fail:
    return -1;
}

__STATIC int8_t calc_rxiq_matrix(int16_t *c21, int16_t *c22,
    int32_t Isq, int32_t Qsq, int32_t IxQ)
{
    int32_t g_fix;
    int32_t gx2_fix;
    int32_t thetaTmp_fix;
    int32_t thetaEst_fix;
    int32_t tmp_fix;

    if (!c21 || !c22)
        goto fail;
    if (Qsq == 0)
        goto fail;

    gx2_fix = (((uint64_t)Isq << 30) / Qsq); //Q30
    g_fix = fix_sqrt(gx2_fix); //Q15
    thetaTmp_fix = FIXED_MULT(Qsq, g_fix, 15); //Q15
    thetaEst_fix = FIXED_DIV(IxQ, thetaTmp_fix, 15); //Q15
    tmp_fix = FIXED_ONE(15) - FIXED_MULT(FIXED_MULT(FIXED_HALF(15), thetaEst_fix, 15), thetaEst_fix, 15); //Q15
    *c21 = -(thetaEst_fix >> 4); //Q11
    *c22 = FIXED_DIV(g_fix, tmp_fix, 11); //Q11
    return 0;
fail:
    return -1;
}

__STATIC int8_t calc_txiq_matrix(int16_t *c21, int16_t *c22,
    int32_t Isq, int32_t Qsq, int32_t IxQ)
{
    return calc_rxiq_matrix(c21, c22, Isq, Qsq, IxQ);
}

__STATIC int8_t rf_cali_rxiq_freq(uint16_t freq, int16_t *c21, int16_t *c22)
{
    P_RF_CALI_OPS cali = rf_cali.ops;
    int32_t Isq;
    int32_t Qsq;
    int32_t IxQ;

    cali->rxiq_init(freq);
    if (cali->rxiq_measure(&Isq, &Qsq, &IxQ))
        goto fail;

    CLOGD("RXIQ Isq=%d, Qsq=%d, IxQ=%d", Isq, Qsq, IxQ);
    calc_rxiq_matrix(c21, c22, Isq, Qsq, IxQ);
    CLOGD("RXIQ c21=%d c22=%d\n", *c21, *c22);
fail:
    return -1;
}

__STATIC int8_t rf_cali_rxiq_wf()
{
    int16_t c21_l = 0, c21_h = 0;
    int16_t c22_l = 2048, c22_h = 2048;
    P_RF_CALI_OPS cali = rf_cali.ops;

    /* Use the average results from -6M and +6M */
    rf_cali_rxiq_freq(2434, &c21_l, &c22_l);
    rf_cali_rxiq_freq(2446, &c21_h, &c22_h);
    c21_l = (c21_l + c21_h) / 2;
    c22_l = (c22_l + c22_h) / 2;
#if defined(CALI_CHECK_RESULT)
    rf_cali_check_iq_result(&c21_l, &c22_l, RX_IQ_C21_THD, RX_IQ_C22_THD);
#endif
    cali->rxiq_result(c21_l, c22_l);
#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
    nv_update_selfcali_wf_rx_params(NULL, NULL, NULL, NULL, NULL, &c21_l, &c22_l);
#endif
    CLOGI("[RF][RXIQ] @c21=%d\n", c21_l);
    CLOGI("[RF][RXIQ] @c22=%d\n", c22_l);
    return 0;
}

__STATIC int8_t rf_cali_rxiq_bt()
{
    int16_t c21 = 0;
    int16_t c22 = 2048;
    P_RF_CALI_OPS cali = rf_cali.ops;

    /* calib at -2M : 2440M(ttg_pll) - 2442M(freq) */
    rf_cali_rxiq_freq(2442, &c21, &c22);

    cali->rxiq_result(c21, c22);
    g_bt_rxiq_comp.c21 = c21;
    g_bt_rxiq_comp.c22 = c22;
    CLOGI("BT RXIQ @c21=%d, @c22=%d\n", c21, c22);
    return 0;
fail:
    return -1;
}

__STATIC int8_t rf_cali_txiq(uint8_t fb_gain)
{
    int32_t Isq;
    int32_t Qsq;
    int32_t IxQ;
    int16_t c21 = 0;
    int16_t c22 = 2048;
    int16_t phase_err = 0;
    int16_t amp_err = 0;
    int16_t legacy_phase = 0;
    int16_t legacy_amp = 0;
    uint8_t iter_times = rf_cali.params.txiq_iter_times;
    P_RF_CALI_OPS cali = rf_cali.ops;
    uint8_t t;

    CLOGD("TXIQ start with fb_bq_gain=%d", fb_gain);
    cali->txiq_fb_init(0, fb_gain);
    if (cali->txiq_fb_measure(&Isq, &Qsq, &IxQ))
        goto fail;

    CLOGD("TXIQ feedback 1 Isq=%ld, Qsq=%ld, IxQ=%ld", Isq, Qsq, IxQ);
    calc_rxiq_matrix(&c21, &c22, Isq, Qsq, IxQ);
#if defined(CALI_CHECK_RESULT)
    rf_cali_check_iq_result(&c21, &c22, RX_IQ_C21_THD, RX_IQ_C22_THD);
#endif
    cali->txiq_fb_result(c21, c22);
    CLOGI("TXIQ feedback 1 comp c21=%d, c22=%d", c21, c22);
    cali->txiq_fb_deinit();
    return 0;
fail:
    cali->txiq_fb_deinit();
    return -1;
}

int8_t rf_cali_calc_delta_db(uint32_t uPower)
{
    int8_t delta_db = -1;

    for (int i = sizeof(power_db_table) / sizeof(RF_CALI_POWER_DB_TBL) - 1; i >= 0; i--) {
        if (uPower >= power_db_table[i].pwr) {
            delta_db = power_db_table[i].db;
            break;
        }
    }
    return delta_db;
}

int8_t rf_cali_self_adj_gain(int8_t pwr_idx)
{
    P_RF_CALI_OPS cali = rf_cali.ops;
    complexint16 *iFbSignalVec = (complexint16 *)CALI_MEM_START_ADDR;
    uint8_t dump_finished = 0;
    uint8_t meas_time = 0;
    uint8_t power_violation = 0;
    uint32_t uPower = 0;
    int8_t delta_db = 0;

    memset(iFbSignalVec, 0, 4096*4);
    cali->txdpd_measure(pwr_idx);
    cali->txdcdpd_toggle_mixen(1);
    while(!dump_finished || power_violation) {
        cali->txdpd_measure(pwr_idx);
        //CLOGI("last feedback sample 0x%x\n", iFbSignalVec[4095]);
        dump_finished = (((uint32_t *)iFbSignalVec)[4095] >> 31) & 0x1;
        if (dump_finished) {
            uPower = wf_cali_read_hw_power();
            power_violation = (uPower > POWER_MEASURE_THRESHOLD_HIGH) || (uPower < POWER_MEASURE_THRESHOLD_LOW) ? 1 : 0;
            if (!power_violation)
                break;
            delta_db = rf_cali_calc_delta_db(uPower);
            //CLOGI("pwr_idx=%d, delta_db=%d, uPower=%d\n", pwr_idx, delta_db, uPower);
            cali->txdpd_adjust_gain(-delta_db);
        }
        meas_time++;
        if (meas_time > POWER_MEASURE_TIMES)
            break;
    }
    if (power_violation || meas_time > POWER_MEASURE_TIMES) {
        CLOGW("target %d measure(%d) power(%u) violation from range [%u, %u]\n", pwr_idx, meas_time, uPower, POWER_MEASURE_THRESHOLD_LOW, POWER_MEASURE_THRESHOLD_HIGH);
        return -1;
    }
    CLOGI("final pwr_idx=%d, power=%d, meas_time=%d\n", pwr_idx, uPower, meas_time);
    return 0;
}

__STATIC void rf_cali_txdpd_print_para(uint8_t tbl_idx, int8_t pwr_idx, complexint16 *cParaEst)
{
        char msg[200], *p_msg=&msg[0];
        uint32_t msg_len = sprintf(p_msg, "[RF][DPD] table %d pwr %d @dpd_param={", tbl_idx, pwr_idx);
        p_msg += msg_len;
        for (uint32_t i = 0; i < MAX_PARALEN; i++)
        {
            msg_len = sprintf(p_msg, "{%d,%d},", cParaEst[i].re, cParaEst[i].im);
            p_msg += msg_len;
        }
        p_msg--;
        sprintf(p_msg, "}\n");
        CLOGI("%s", msg);
}

__STATIC int8_t rf_cali_txdpd_one(uint8_t tbl_idx, int8_t pwr_idx, uint8_t iter_times, complexint16 *mix_off_dc, int8_t pwr_idx_dc, int8_t pwr_idx_iq, complexint16* dpd_para_est)
{
    uint8_t t;
    uint8_t uTimeEstStartTx = 0;
    uint8_t uTimeEstStartFb = 0;
    uint16_t uTimeEstWinLen = 4000;
    uint8_t uTimeEstLen = 9;
    int16_t iIntDelay = 0;
    int16_t iFracDelay = 0;
    complexint16 cFbGain = {0};
    complexint16 mix_on_dc = {0};
    complexint16 pre_comp_dc = {SIGN(NEW_DFE->REG_TPC_CTRL_DCCOMP_I0.bit.CFG_TPC_DCCOMP_I_0, 12), SIGN(NEW_DFE->REG_TPC_CTRL_DCCOMP_Q0.bit.CFG_TPC_DCCOMP_Q_0, 12)};
    complexint16 cur_comp_dc = {0};
    uint16_t uGainOffset = 2048;//2368;
    int16_t uGainOffsetDiv = 2048;//1771;
    complexint16 cParaEst[MAX_PARALEN] = {0};
    complexint16 cLegcyPara[MAX_PARALEN] = {0};
    P_RF_CALI_OPS cali = rf_cali.ops;
    complexint16 *iTxSignalVec = (complexint16 *)(&ro_iTxSignalVec[0]);
    complexint16 *iFbSignalVec = (complexint16 *)CALI_MEM_START_ADDR;
    int16_t c21 = 0;
    int16_t c22 = 2048;
    int16_t leg_c21 = SIGN(NEW_DFE->REG_TPC_CTRL_IQCOMP_I0.bit.CFG_TPC_IQCOMP_I_0, 12);
    int16_t leg_c22 = NEW_DFE->REG_TPC_CTRL_IQCOMP_Q0.bit.CFG_TPC_IQCOMP_Q_0;
    uint8_t dump_finished = 0;
    uint8_t meas_time = 0;
    uint8_t power_violation = 0;
    complexint32 mix_on_dc_luna = {0};
    int8_t dpd_res;
    uint8_t dpd_itr = 0;

#if 0 //TODO: DPD cLegcyPara from registers
    cali->txdpd_get_result(tbl_idx, (void *)&cLegcyPara[0]);
    for (uint8_t i = 0; i < MAX_PARALEN; i++) {
        cLegcyPara[i].re = SIGN(cLegcyPara[i].re, 14);
        cLegcyPara[i].im = SIGN(cLegcyPara[i].im, 14);
    }
    rf_cali_txdpd_print_para(tbl_idx, pwr_idx, cLegcyPara);
#endif
    if (pwr_idx_dc == pwr_idx) {
        for (t = 0; t < 4; t++) {
            complexint16 mix_off_tmpdc = {0};

            cali->txdcdpd_toggle_mixen(0);
            cali->txdpd_measure(pwr_idx_dc);
            /* hardware 128 samples estimate */
            mix_off_dc->re += SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_I, 12);
            mix_off_dc->im += SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_Q, 12);
        }
        mix_off_dc->re = mix_off_dc->re >> 2;
        mix_off_dc->im = mix_off_dc->im >> 2;
        //CLOGD("mix_off_dc re=%d, im=%d", mix_off_dc->re, mix_off_dc->im);
    }
    //CLOGI("TXDPD cali table %d pwr %d =========================\n", tbl_idx, pwr_idx);
    cali->txdcdpd_toggle_mixen(1);
    for (t = 0; t < iter_times; t++) {
    #if DPD_LUNA
        //iTxSignalVec = &ro_iTxSignalVec[0];
        for(int i = 0; i < 4096; i++)
        {
            iTxSignalVecLunaRe[i] = iTxSignalVec[i].re;
            iTxSignalVecLunaIm[i] = iTxSignalVec[i].im;
        }
    #endif
        dump_finished = 0;
        meas_time = 0;
        while(!dump_finished) {
            cali->txdpd_measure(pwr_idx);
            //CLOGD("last feedback sample 0x%x\n", iFbSignalVec[4095]);
            dump_finished = (((uint32_t *)iFbSignalVec)[4095] >> 31) & 0x1;
            if (dump_finished) {
                for (int i = 0; i < 4096; i++)
                {
                #if DPD_LUNA
                    iFbSignalVecLunaRe[i] = SIGN(iFbSignalVec[i].re & 0x0FFF, 12);
                    iFbSignalVecLunaIm[i] = SIGN(iFbSignalVec[i].im & 0x0FFF, 12);
                #if DPD_LUNA_DEBUG
                    iFbSignalVec[i].re = SIGN(iFbSignalVec[i].re & 0x0FFF, 12);
                    iFbSignalVec[i].im = SIGN(iFbSignalVec[i].im & 0x0FFF, 12);
                #endif
                #else
                    iFbSignalVec[i].re = SIGN(iFbSignalVec[i].re & 0x0FFF, 12);
                    iFbSignalVec[i].im = SIGN(iFbSignalVec[i].im & 0x0FFF, 12);
                #endif
                }
            #if DPD_LUNA
                CalculateDcEstLuna(iFbSignalVecLunaRe, iFbSignalVecLunaIm, &mix_on_dc_luna);
                mix_on_dc.re = (int16_t)mix_on_dc_luna.re;
                mix_on_dc.im = (int16_t)mix_on_dc_luna.im;
            #if DPD_LUNA_DEBUG
                CalculateDcEst(iFbSignalVec, &mix_on_dc);
            #endif
            #else
                CalculateDcEst(iFbSignalVec, &mix_on_dc);
            #endif
                break;
            }
            meas_time++;
            if (meas_time > 10)
                goto fail_measure;
        }
    #if DPD_LUNA
        CalculateTimeEstLuna(iTxSignalVecLunaRe, iTxSignalVecLunaIm, iFbSignalVecLunaRe, iFbSignalVecLunaIm, uTimeEstStartTx, uTimeEstStartFb, uTimeEstWinLen, uTimeEstLen, &iIntDelay, &iFracDelay);
    #if DPD_LUNA_DEBUG
        int16_t iIntDelayR;
        int16_t iFracDelayR;
        CalculateTimeEst(iTxSignalVec, iFbSignalVec, uTimeEstStartTx, uTimeEstStartFb, uTimeEstWinLen, uTimeEstLen, &iIntDelayR, &iFracDelayR);
        CLOGI("iIntDelay=%d, iFracDelay=%d\n", iIntDelay, iFracDelay);
        CLOGI("iIntDelayR=%d, iFracDelayR=%d\n", iIntDelayR, iFracDelayR);
    #endif
    #else
        CalculateTimeEst(iTxSignalVec, iFbSignalVec, uTimeEstStartTx, uTimeEstStartFb, uTimeEstWinLen, uTimeEstLen, &iIntDelay, &iFracDelay);
    #endif
    #if DPD_LUNA
        FbCompTimeLuna(iFbSignalVecLunaRe, iFbSignalVecLunaIm, iFracDelay);
    #if DPD_LUNA_DEBUG
        FbCompTime(iFbSignalVec, iFracDelay);
        CLOGI("iFbSignalVecLunaRe[0]=%d, iFbSignalVecLunaIm=%d\n", iFbSignalVecLunaRe[0], iFbSignalVecLunaIm[0]);
        CLOGI("iFbSignalVec[0]=%d, iFbSignalVe[0]c=%d\n", iFbSignalVec[0].re, iFbSignalVec[0].im);
    #endif
    #else
        FbCompTime(iFbSignalVec, iFracDelay);
    #endif
    #if DPD_LUNA
        CalculateGainEstLuna(iTxSignalVecLunaRe, iTxSignalVecLunaIm, iFbSignalVecLunaRe + iIntDelay, iFbSignalVecLunaIm + iIntDelay, 0, 4000, &cFbGain);
    #if DPD_LUNA_DEBUG
        complexint16 cFbGainR;
        CalculateGainEst(iTxSignalVec, iFbSignalVec + iIntDelay, 0 , 4000, &cFbGainR);
        CLOGI("cFbGain[0]=%d, cFbGain=%d\n", cFbGain.re, cFbGain.im);
        CLOGI("cFbGainR[0]=%d, cFbGainR[0]=%d\n", cFbGainR.re, cFbGainR.im);
    #endif
    #else
        CalculateGainEst(iTxSignalVec, iFbSignalVec + iIntDelay, 0 , 4000, &cFbGain);
    #endif
        CLOGD("[iter %d]rest dc.re=%d,im=%d iIntDelay=%d, iFracDelay=%d, cFbGain re=%d im=%d\n",
            t, mix_on_dc.re, mix_on_dc.im, iIntDelay, iFracDelay, cFbGain.re, cFbGain.im);
    #if DPD_LUNA
        FbCompGainLuna(iFbSignalVecLunaRe, iFbSignalVecLunaIm, cFbGain);
    #if DPD_LUNA_DEBUG
        FbCompGain(iFbSignalVec, cFbGainR);
        CLOGI("iFbSignalVecLunaRe[0]=%d, iFbSignalVecLunaIm=%d\n", iFbSignalVecLunaRe[0], iFbSignalVecLunaIm[0]);
        CLOGI("iFbSignalVec[0]=%d, iFbSignalVec[0]=%d\n", iFbSignalVec[0].re, iFbSignalVec[0].im);
    #endif
    #else
        FbCompGain(iFbSignalVec, cFbGain);
    #endif
        if (pwr_idx_dc == pwr_idx) {
            /* hardware 128 samples estimate */
            mix_on_dc.re = SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_I, 12);
            mix_on_dc.im = SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_Q, 12);
            CalculateTxDcEst(&mix_on_dc, &pre_comp_dc, mix_off_dc, &cFbGain, &cur_comp_dc, t);
            //CLOGD("mix_off_dc->re=%d, mix_off_dc->im=%d, mix_on_dc.re=%d, mix_on_dc.im=%d, cur_comp_dc.re=%d, cur_comp_dc.im=%d\n",
            //    mix_off_dc->re, mix_off_dc->im, mix_on_dc.re, mix_on_dc.im, cur_comp_dc.re, cur_comp_dc.im);
        #if defined(CALI_CHECK_RESULT)
            if (t == iter_times - 1) {
                if (rf_cali_check_dc_result(cur_comp_dc.re, cur_comp_dc.im, TX_DC_THD, TX_DC_THD)) {
                    CLOGW("TXDC abort since cur_comp_dc.re(%d) cur_comp_dc.im(%d) out of range\n", cur_comp_dc.re, cur_comp_dc.im);
                    cur_comp_dc.re = 0;
                    cur_comp_dc.im = 0;
                }
            }
        #endif
            cali->txdc_result(&cur_comp_dc, 0);
        #if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
            nv_update_selfcali_wf_tx_params(&cur_comp_dc.re, &cur_comp_dc.im, NULL, NULL);
        #endif
            CLOGI("[RF][TXDC] @dc_i=%d\n", cur_comp_dc.re);
            CLOGI("[RF][TXDC] @dc_q=%d\n", cur_comp_dc.im);
            memcpy(&pre_comp_dc, &cur_comp_dc, sizeof(complexint16));
        }
        if (pwr_idx_iq == pwr_idx) {
        #if DPD_LUNA
            TxIqEstLuna(iFbSignalVecLunaRe, iFbSignalVecLunaIm, iIntDelay, &c21, &c22);
        #else
            TxIqEst(iFbSignalVec, iIntDelay, &c21, &c22);
        #endif
            c21 = (int16_t)(((int32_t)c21 * (int32_t)uDcLamda[0]) >> 11) + (leg_c21>>1);
            c22 = (int16_t)(((int32_t)c22 * (int32_t)leg_c22) >> 11);
        #if defined(CALI_CHECK_RESULT)
            if (t == iter_times - 1) {
                rf_cali_check_iq_result(&c21, &c22, TX_IQ_C21_THD, TX_IQ_C22_THD);
            }
        #endif
            cali->txiq_tx_result(c21, c22);
        #if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
            nv_update_selfcali_wf_tx_params(NULL, NULL, &c21, &c22);
        #endif
            leg_c21 = c21;
            leg_c22 = c22;
            CLOGI("[RF][TXIQ] @c21=%d\n", c21);
            CLOGI("[RF][TXIQ] @c22=%d\n", c22);
        }
    #if DPD_LUNA
        CalculateDpdParaLuna(iTxSignalVecLunaRe, iTxSignalVecLunaIm, iFbSignalVecLunaRe + iIntDelay, iFbSignalVecLunaIm + iIntDelay, MAX_ORDER, MAX_MEMORY, cLegcyPara, cParaEst, 4080, dpd_itr, uGainOffsetDiv);
    #if DPD_LUNA_DEBUG
        complexint16 cParaEstR[MAX_PARALEN] = {0};
        CalculateDpdPara(iTxSignalVec, iFbSignalVec + iIntDelay, MAX_ORDER, MAX_MEMORY, cLegcyPara, cParaEstR, 4080, dpd_itr, uGainOffsetDiv);
        rf_cali_txdpd_print_para(tbl_idx, pwr_idx, cParaEstR);
    #endif
    #else
        CalculateDpdPara(iTxSignalVec, iFbSignalVec + iIntDelay, MAX_ORDER, MAX_MEMORY, cLegcyPara, cParaEst, 4080, dpd_itr, uGainOffsetDiv);
    #endif
    #if defined(CALI_CHECK_RESULT)
        dpd_res = rf_cali_check_dpd_result(cParaEst);
        if (dpd_res) {
            CLOGW("TXDPD abort since cParaEst out of range\n");
            memcpy(cParaEst, cLegcyPara, MAX_PARALEN*sizeof(int32_t));
        }
        else {
            dpd_itr++;
            memcpy(cLegcyPara, cParaEst, MAX_PARALEN*sizeof(int32_t));
        }
    #endif
        cali->txdpd_result(tbl_idx, (void *)&cParaEst[0]);
        rf_cali_txdpd_print_para(tbl_idx, pwr_idx, cParaEst);
        CLOGI("dpd_itr=%d\n", dpd_itr);
    }
    if (dpd_para_est)
        memcpy(dpd_para_est, cParaEst, 15*sizeof(int32_t));
    return 0;

fail_measure:
    return -1;
}

__STATIC int8_t rf_cali_txdc(int8_t pwr_idx_dc, uint8_t fb_gain, uint8_t iter_times, complexint16 *mix_off_dc)
{
    uint8_t t;
    uint8_t uTimeEstStartTx = 0;
    uint8_t uTimeEstStartFb = 0;
    uint16_t uTimeEstWinLen = 4000;
    uint8_t uTimeEstLen = 9;
    int16_t iIntDelay = 0;
    int16_t iFracDelay = 0;
    complexint16 cFbGain = {0};
    complexint16 mix_on_dc = {0};
    complexint16 pre_comp_dc = {0};
    complexint16 cur_comp_dc = {0};
    P_RF_CALI_OPS cali = rf_cali.ops;
    complexint16 *iTxSignalVec = (complexint16 *)CALI_MEM_START_ADDR;
    complexint16 *iFbSignalVec = (complexint16 *)CALI_MEM_MID_ADDR;

    CLOGI("TXDC cali pwr %d start===================================================\n", pwr_idx_dc);
    cali->txdpd_init(6);
    cali->txdpd_measure(pwr_idx_dc);
    for (t = 0; t < iter_times; t++) {
        cali->txdcdpd_toggle_mixen(0);
        cali->txdpd_measure(pwr_idx_dc);
        mix_off_dc->re = SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_I, 12);
        mix_off_dc->im = SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_Q, 12);
        cali->txdcdpd_toggle_mixen(1);
        cali->txdpd_measure(pwr_idx_dc);
        for (int i = 0; i < 4096; i++)
        {
            iTxSignalVec[i].re = SIGN(iTxSignalVec[i].re & 0x0FFF, 12);
            iTxSignalVec[i].im = SIGN(iTxSignalVec[i].im & 0x0FFF, 12);
            iFbSignalVec[i].re = SIGN(iFbSignalVec[i].re & 0x0FFF, 12);
            iFbSignalVec[i].im = SIGN(iFbSignalVec[i].im & 0x0FFF, 12);
        }
        CalculateDcEst(iFbSignalVec, &mix_on_dc);
        CalculateTimeEst(iTxSignalVec, iFbSignalVec, uTimeEstStartTx, uTimeEstStartFb, uTimeEstWinLen, uTimeEstLen, &iIntDelay, &iFracDelay);
        FbCompTime(iFbSignalVec, iFracDelay);
        CalculateGainEst(iTxSignalVec, iFbSignalVec + iIntDelay, 0 , 4000, &cFbGain);
        CLOGD("[iter %d]rest dc.re=%d,im=%d iIntDelay=%d, iFracDelay=%d, cFbGain re=%d im=%d\n",
           t, mix_on_dc.re, mix_on_dc.im, iIntDelay, iFracDelay, cFbGain.re, cFbGain.im);
        FbCompGain(iFbSignalVec, cFbGain);
        mix_on_dc.re = SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_I, 12);
        mix_on_dc.im = SIGN(NEW_DFE->REG_RXDC_CFG.bit.RX_DC_EST_DATA_Q, 12);
        CalculateTxDcEst(&mix_on_dc, &pre_comp_dc, mix_off_dc, &cFbGain, &cur_comp_dc, t);
        CLOGD("mix_off_dc->re=%d, mix_off_dc->im=%d, mix_on_dc.re=%d, mix_on_dc.im=%d, cur_comp_dc.re=%d, cur_comp_dc.im=%d\n",
            mix_off_dc->re, mix_off_dc->im, mix_on_dc.re, mix_on_dc.im, cur_comp_dc.re, cur_comp_dc.im);
        cali->txdc_result(&cur_comp_dc, 2);
        memcpy(&pre_comp_dc, &cur_comp_dc, sizeof(complexint16));
    }
    CLOGI("TXDC cali pwr %d end  ===================================================\n", pwr_idx_dc);

    return 0;
}

void rf_cali_txdpd_calc_rest_table(complexint16* dpd_para_est_update, complexint16* dpd_para_est_tr, uint8_t pwr_step)
{
    P_RF_CALI_OPS cali = rf_cali.ops;
    uint16_t gain_off_n2db[4] = {13014,10338,8211,6523};

    if (pwr_step == 2) {
        gain_off_n2db[0] = 13014;
        gain_off_n2db[1] = 10338;
        gain_off_n2db[2] = 8211;
        gain_off_n2db[3] = 6523;
    }
    else if (pwr_step == 4) {
        gain_off_n2db[0] = 10338;
        gain_off_n2db[1] = 6523;
        gain_off_n2db[2] = 4115;
        gain_off_n2db[3] = 2597;
    }
    else {
        gain_off_n2db[0] = 16384;
        gain_off_n2db[1] = 16384;
        gain_off_n2db[2] = 16384;
        gain_off_n2db[3] = 16384;
    }
    for (uint8_t para_idx = 0; para_idx < MAX_PARALEN; para_idx++) {
        if(para_idx%5 != 0) {
            dpd_para_est_update[para_idx] = ComplexMultiReal16(dpd_para_est_tr+para_idx,gain_off_n2db[para_idx%5-1],14);
        }
        else {
            dpd_para_est_update[para_idx] = dpd_para_est_tr[para_idx];
        }
    }
}

__STATIC int8_t rf_cali_txdpd_process_rest_tables(complexint16* dpd_para_est_update, complexint16* dpd_para_est_tr, P_RF_CALI_DPD_CFG dpd_tr_table)
{
    int8_t dpd_tr_tssi = dpd_tr_table->tssi;
    uint8_t dpd_tr_lut_idx = dpd_tr_table->pred_lut_idx;
    P_RF_CALI_OPS cali = rf_cali.ops;

    for (uint8_t i = 0; i < DPD_REST_TABLE_CNT; i++) {
        uint8_t pwr_step = 2 << i;
        int8_t rest_pwr_tssi = dpd_tr_tssi - pwr_step;
        int8_t rest_pred_lut_idx = (int8_t)dpd_tr_lut_idx - i - 1;
        if (rest_pred_lut_idx < 0)
            break;
        rf_cali_txdpd_calc_rest_table(dpd_para_est_update, dpd_para_est_tr, pwr_step);
        cali->txdpd_result((uint8_t)rest_pred_lut_idx, dpd_para_est_update);
        rf_cali_txdpd_print_para((uint8_t)rest_pred_lut_idx, rest_pwr_tssi, dpd_para_est_update);
    }
    return 0;
}

__STATIC int8_t rf_cali_txdpd(void)
{
    P_RF_CALI_PARAMS params = &rf_cali.params;
    P_RF_CALI_OPS cali = rf_cali.ops;
    uint8_t tbl_idx = params->txdpd_tbl_idx;
    uint8_t iter_times = params->txdpd_iter_times;
    complexint16 mix_off_dc = {0};
    complexint16 dpd_para_est_tr[MAX_PARALEN] = {0};
    complexint16 dpd_para_est_update[MAX_PARALEN] = {0};
#if DPD_LUNA
    __HAL_CRM_LUNA_CLK_ENABLE();
    enable_GINT();
    luna_init();
#endif
    CLOGI("TXDPD start==========================================================================\n");
    cali->txdpd_init(0);
    if (tbl_idx < DPD_COMP_TABLE_CNT) {
        if (rf_cali_self_adj_gain(dpd_cfg_table[tbl_idx].tssi)) {
            goto cali_dpd_done;
        }
        rf_cali_txdpd_one(dpd_cfg_table[tbl_idx].pred_lut_idx, dpd_cfg_table[tbl_idx].tssi, iter_times, &mix_off_dc, dpd_cfg_table[tbl_idx].tssi, dpd_cfg_table[tbl_idx].tssi, dpd_para_est_tr);
        rf_cali_txdpd_process_rest_tables(dpd_para_est_update, dpd_para_est_tr, &dpd_cfg_table[tbl_idx]);
    } else {
        for (uint8_t i = 0; i < DPD_COMP_TABLE_CNT; i++)
        {
            if (rf_cali_self_adj_gain(dpd_cfg_table[i].tssi)) {
                continue;
            }
            rf_cali_txdpd_one(dpd_cfg_table[i].pred_lut_idx, dpd_cfg_table[i].tssi, iter_times, &mix_off_dc, pwr_idx_dc, pwr_idx_iq, dpd_para_est_tr);
        #if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
            nv_update_selfcali_dpd_params(i, (uint32_t *)dpd_para_est_tr);
        #endif
            if (i == 0)
                rf_cali_txdpd_process_rest_tables(dpd_para_est_update, dpd_para_est_tr, &dpd_cfg_table[0]);
        }
    }
#if 0
    rf_cali_txdc(5, 10, iter_times, &mix_off_dc);
#endif
cali_dpd_done:
    cali->txdpd_deinit();
    cali->txiq_restore_rxiq_result();
    CLOGI("TXDPD end============================================================================\n");
    return 0;
}

__STATIC int8_t rf_cali_ppacap_measure_power(P_RF_CALI_OPS cali, int cap_idx, uint8_t ppa_cap, int8_t pwr_idx, complexint16 *iFbSignalVec, uint32_t *power) {
    uint8_t dump_finished = 0, meas_time = 0;
    cali->set_ppa_cap(cap_idx, ppa_cap);
    memset(iFbSignalVec, 0, FB_VEC_SIZE * sizeof(complexint16));
    while (!dump_finished) {
        cali->txdpd_measure(pwr_idx);
        dump_finished = (((uint32_t *)iFbSignalVec)[FB_VEC_SIZE - 1] >> 31) & 0x1;
        if (++meas_time >= POWER_MEASURE_TIMES)
            return -1;
    }
#if defined(PPA_CAP_USE_SOFT_CALC)
    for (int i = 0; i < FB_VEC_SIZE; i++) {
        iFbSignalVec[i].re = SIGN(iFbSignalVec[i].re & 0x0FFF, 12);
        iFbSignalVec[i].im = SIGN(iFbSignalVec[i].im & 0x0FFF, 12);
    }
    CalculatePowerEst(iFbSignalVec, power);
#else
    *power = wf_cali_read_hw_power_without_dc();
#endif
    return 0;
}

__STATIC int8_t rf_cali_ppacap(void)
{
    uint32_t power = 0, tmp_pwr = 0, target_pwr = 0, rec_target_pwr = 0, min_delta_pwr = 0xffffffff;
    uint8_t ppa_cap = 0, max_pos = 0, tar_pos = 0, tar_pos_mid = 0, tar_pos_low = 0, tar_pos_high = 0;
    P_RF_CALI_OPS cali = rf_cali.ops;
    complexint16 *iFbSignalVec = (complexint16 *)CALI_MEM_START_ADDR;
    uint8_t rec_cap_0 = cali->get_ppa_cap(0);
    uint8_t rec_cap_1 = cali->get_ppa_cap(1);
    uint8_t rec_cap_2 = cali->get_ppa_cap(2);
    int8_t tx_power = 12;

    cali->txdpd_init(0);
    cali->txdcdpd_toggle_mixen(1);
    rf_cali.owner->ops->set_channel(2442);
    rf_udelay(100);

    /* 1. 找最大功率点 */
    for (ppa_cap = 0; ppa_cap < 32; ppa_cap++) {
        if (rf_cali_ppacap_measure_power(cali, 1, ppa_cap, tx_power, iFbSignalVec, &power)) {
            CLOGW("2442M measure ppa cap timeout\n");
            goto fail;
        }
        if (ppa_cap && power < tmp_pwr) {
            max_pos = ppa_cap - 1;
            break;
        }
        tmp_pwr = power;
    }
    if (max_pos > 11) {
        CLOGE("Chip violation on PPA cap calibration, max power position > 11\n");
        goto fail;
    }

    /* 2. 2442M目标功率 */
    tar_pos_mid = max_pos + 12;
    if (rf_cali_ppacap_measure_power(cali, 1, tar_pos_mid, tx_power, iFbSignalVec, &target_pwr)) {
        CLOGE("2412M measure ppa cap timeout\n");
        goto fail;
    }
    CLOGI("[RF][PPA_CAP][chan2442] target_pwr=%u max_pos=%d @tar_pos=%d ", target_pwr, max_pos, tar_pos_mid);

    /* 3. 2417M寻找最接近目标功率点 */
    rf_cali.owner->ops->set_channel(2417);
    rf_udelay(100);
    min_delta_pwr = 0xffffffff;
    for (ppa_cap = tar_pos_mid; ppa_cap < 32; ppa_cap++) {
        if (rf_cali_ppacap_measure_power(cali, 0, ppa_cap, tx_power, iFbSignalVec, &power)) {
            CLOGE("2417M measure ppa cap timeout\n");
            goto fail;
        }
        tmp_pwr = abs((int32_t)power - (int32_t)target_pwr);
        if (tmp_pwr < min_delta_pwr) {
            min_delta_pwr = tmp_pwr;
            tar_pos = ppa_cap;
            rec_target_pwr = power;
        } else {
            break;
        }
    }
    tar_pos_low = tar_pos;
    if (tar_pos_low > PPA_CAP_UP_THD) {
        CLOGW("Abort since ppa_cap_0(%d) > UP_THRESHOLD(%d)", tar_pos_low, PPA_CAP_UP_THD);
        goto fail;
    }
    cali->set_ppa_cap(0, tar_pos_low);
    CLOGI("[RF][PPA_CAP][chan2417] low_target_pwr=%u @tar_pos=%d", rec_target_pwr, tar_pos_low);

    /* 4. 2462M寻找最接近目标功率点 */
    rf_cali.owner->ops->set_channel(2462);
    rf_udelay(100);
    min_delta_pwr = 0xffffffff;
    for (ppa_cap = tar_pos_mid; ppa_cap > 0; ppa_cap--) {
        if (rf_cali_ppacap_measure_power(cali, 2, ppa_cap, tx_power, iFbSignalVec, &power)) {
            CLOGI("2462M measure ppa cap timeout\n");
            goto fail;
        }
        tmp_pwr = abs((int32_t)power - (int32_t)target_pwr);
        if (tmp_pwr < min_delta_pwr) {
            min_delta_pwr = tmp_pwr;
            tar_pos = ppa_cap;
            rec_target_pwr = power;
        } else {
            break;
        }
    }
    tar_pos_high = tar_pos;
    if (tar_pos_high < PPA_CAP_DOWN_THD) {
        CLOGW("Abort since ppa_cap_2(%d) < DOWN_THRESHOLD(%d)", tar_pos_high, PPA_CAP_DOWN_THD);
        goto fail;
    }
    cali->set_ppa_cap(2, tar_pos_high);
    CLOGI("[RF][PPA_CAP][chan2462] high_target_pwr=%u @tar_pos=%d", rec_target_pwr, tar_pos_high);
    #if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
        nv_update_selfcali_ppa_cap_params(&tar_pos_low, &tar_pos_mid, &tar_pos_high);
    #endif
    cali->txdpd_deinit();
    return 0;

fail:
    cali->set_ppa_cap(0, rec_cap_0);
    cali->set_ppa_cap(1, rec_cap_1);
    cali->set_ppa_cap(2, rec_cap_2);
    CLOGI("[RF][PPA_CAP] Abort and restore: cap0=%d, cap1=%d, cap2=%d\n", rec_cap_0, rec_cap_1, rec_cap_2);
    #if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
        nv_update_selfcali_ppa_cap_params(&rec_cap_0, &rec_cap_1, &rec_cap_2);
    #endif
    cali->txdpd_deinit();
    return -1;
}

__STATIC int32_t rf_cali_bootup_proc_wf(void)
{
    P_RF_CALI_PARAMS params = &rf_cali.params;
    uint8_t i = 0;

    rf_cali.ops->env_init();
    if(!params->rxrc_disable)
    {
        rf_cali_rxrc();
    }

    if(!params->rxdcoc_disable)
    {
        while(rf_cali_rxdcoc(2442, 8, 1))
        {
            i++;
            if (i > 2) {
                /* If 3 times of calibration are all failed on 2442, then do it on 2399. */
                rf_cali_rxdcoc(2399, 8, 1);
                break;
            }
        }
    }

    if(!params->rxiq_disable)
    {
        rf_cali_rxiq_wf();
    }
    rf_cali.ops->env_deinit();

    return 0;
}

#if defined(RFCALI_BT_EN)
void rf_cali_rxdcoc_bt()
{
    uint16_t freq[3] = {2414, 2440, 2467};
    uint8_t lna_gain[3] = {2, 6, 8};
    RF_CALI_RXDCOC_WORD word_ref[2];
    RF_CALI_RXDCOC_WORD word_dft;
    RF_CALI_RXDCOC_WORD word_tmp[2];

    // first trriger bt rxdcoc calibrate, auto set dcoc_sc
    rf_cali_rxdcoc(2399, 8, 1);
    bt_get_dcoc_word(&word_ref[0]);
    rf_cali_rxdcoc(2483, 8, 0);
    bt_get_dcoc_word(&word_ref[1]);
    CLOGD("BT RXDCOC 2399 ref, dci:%d,dcq:%d,sc_i:%d,sc_q:%d\n", word_ref[0].dac_i, word_ref[0].dac_q, word_ref[0].dac_sc_i, word_ref[0].dac_sc_q);
    CLOGD("BT RXDCOC 2483 ref, dci:%d,dcq:%d,sc_i:%d,sc_q:%d\n", word_ref[1].dac_i, word_ref[1].dac_q, word_ref[1].dac_sc_i, word_ref[1].dac_sc_q);

    if ((word_ref[0].dac_i & 0x80) == (word_ref[1].dac_i & 0x80))
    {
        word_dft.dac_i = ((uint16_t)word_ref[0].dac_i + (uint16_t)word_ref[1].dac_i)/2;
    }
    else
    {
        word_dft.dac_i = word_ref[0].dac_i;
    }
    if ((word_ref[0].dac_q & 0x80) == (word_ref[1].dac_q & 0x80))
    {
        word_dft.dac_q = ((uint16_t)word_ref[0].dac_q + (uint16_t)word_ref[1].dac_q)/2;
    }
    else
    {
        word_dft.dac_q = word_ref[0].dac_q;
    }
    word_dft.dac_sc_i = word_ref[0].dac_sc_i;
    word_dft.dac_sc_q = word_ref[0].dac_sc_q;
    CLOGI("BT RXDCOC default, dci:%d,dcq:%d,sc_i:%d,sc_q:%d\n", word_dft.dac_i, word_dft.dac_q, word_dft.dac_sc_i, word_dft.dac_sc_q);

    for (uint8_t i=0; i<3; i++)
    {
        for (uint8_t j=0; j<3; j++)
        {
            rf_cali_rxdcoc(freq[i], lna_gain[j], 0);
            bt_get_dcoc_word(&word_tmp[0]);
            rf_cali_rxdcoc(freq[i], lna_gain[j], 0);
            bt_get_dcoc_word(&word_tmp[1]);

            if ((abs(rmv_dirc(word_dft.dac_i) - rmv_dirc(word_tmp[1].dac_i)) > (dcoc_diff_thred+5))
                || (abs(rmv_dirc(word_dft.dac_q) - rmv_dirc(word_tmp[1].dac_q)) > (dcoc_diff_thred+5))
                || (abs(rmv_dirc(word_tmp[0].dac_i) - rmv_dirc(word_tmp[1].dac_i)) > dcoc_diff_thred)
                || (abs(rmv_dirc(word_tmp[0].dac_q) - rmv_dirc(word_tmp[1].dac_q)) > dcoc_diff_thred))
            {
                g_bt_rxdc_comp[i][j].dac_i = word_dft.dac_i;
                g_bt_rxdc_comp[i][j].dac_q = word_dft.dac_q;
                g_bt_rxdc_comp[i][j].dac_sc_i = word_dft.dac_sc_i;
                g_bt_rxdc_comp[i][j].dac_sc_q = word_dft.dac_sc_q;
                CLOGW("BT RXDCOC diff greater then diff_thred, i:%d,j:%d,dci_diff:%d,dcq_diff:%d,dft_dci_diff:%d,dft_dcq_diff:%d\n", i, j,\
                        (abs(rmv_dirc(word_tmp[0].dac_i) - rmv_dirc(word_tmp[1].dac_i))),\
                        (abs(rmv_dirc(word_tmp[0].dac_q) - rmv_dirc(word_tmp[1].dac_q))),\
                        (abs(rmv_dirc(word_dft.dac_i) - rmv_dirc(word_tmp[1].dac_i))),\
                        (abs(rmv_dirc(word_dft.dac_q) - rmv_dirc(word_tmp[1].dac_q))));

                CLOGW("BT RXDCOC use default, i:%d,j:%d,dci:%d,dcq:%d,sc_i:%d,sc_q:%d\n", i, j, g_bt_rxdc_comp[i][j].dac_i, g_bt_rxdc_comp[i][j].dac_q, g_bt_rxdc_comp[i][j].dac_sc_i, g_bt_rxdc_comp[i][j].dac_sc_q);
            }
            else
            {
                bt_get_dcoc_word(&g_bt_rxdc_comp[i][j]);
            }

        }
    }

    for (uint8_t i=0; i<3; i++)
    {
        for (uint8_t j=0; j<3; j++)
        {
            CLOGI("BT RXDCOC %d,dci:%d,dcq:%d,sc_i:%d,sc_q:%d\n", 3*i+j, g_bt_rxdc_comp[i][j].dac_i, g_bt_rxdc_comp[i][j].dac_q, g_bt_rxdc_comp[i][j].dac_sc_i, g_bt_rxdc_comp[i][j].dac_sc_q);
        }
    }

    bt_set_dcoc_word(g_bt_rxdc_comp, 3, 3);

}

__STATIC int32_t rf_cali_bootup_proc_bt(void)
{
    P_RF_CALI_PARAMS params = &rf_cali.params;
    uint8_t i = 0;

    rf_cali.ops->env_init();

    //rf_cali_rxrc();
    if(!params->rxdcoc_disable)
    {
        rf_cali_rxdcoc_bt();
    }

    if(!params->rxiq_disable)
    {
        rf_cali_rxiq_bt();
    }

    rf_cali.ops->env_deinit();
    return 0;
}
#endif

__STATIC int8_t rf_cali_ppacap_proc(void)
{
    CLOGI("rf_cali_ppacap_proc\n");
    rf_cali.ops->env_init();
    rf_cali_ppacap();
    rf_cali.ops->env_deinit();
    return 0;
}

__STATIC int8_t rf_ppacap_update_nv(void)
{
    P_RF_CALI_OPS cali = rf_cali.ops;
    uint8_t rec_cap_0 = cali->get_ppa_cap(0);
    uint8_t rec_cap_1 = cali->get_ppa_cap(1);
    uint8_t rec_cap_2 = cali->get_ppa_cap(2);

    nv_update_selfcali_ppa_cap_params(&rec_cap_0, &rec_cap_1, &rec_cap_2);
    return 0;
}

__STATIC int32_t rf_cali_runtime_proc(uint32_t log_level)
{
    uint32_t saved_cloglvl = cloglvl;
#define RUNTIME_TS_DEBUG 0
#if RUNTIME_TS_DEBUG
    uint32_t t_s = MEM_RD32(0x4B700120);
    uint32_t t_e = 0;
#endif
    P_RF_CALI_PARAMS params = &rf_cali.params;

    if (log_level != -1)
        cloglvl = log_level;
    if (IP_WIFI_MAC_CORE->REG_STATECNTRLREG.bit.CURRENTSTATE) //Check MAC_STATE is IDLE
    {
        CLOGE("rf_cali_runtime_proc MAC_STATE is not IDLE, skip calibration!\n");
        return -1;
    }
    if (cali_runtime_state == CALI_RUNTIME_STATE_ONGOING)
        return -1;
    cali_runtime_state = CALI_RUNTIME_STATE_ONGOING;

    rf_cali.ops->env_init();
    if (!params->txdpd_disable) {
        rf_cali_txiq(11);
        rf_cali.ops->env_deinit();
    }
    if (!params->txdpd_disable) {
        rf_cali.ops->env_init();
        rf_cali_txdpd();
    }
    rf_cali.ops->env_deinit();
    cali_runtime_state = CALI_RUNTIME_STATE_IDLE;
#if CALI_BUF==0
    memset((void *)CALI_MEM_START_ADDR, 0, (CALI_MEM_END_ADDR - CALI_MEM_START_ADDR));
#endif
#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
    rf_ppacap_update_nv();
#endif
#if defined(RF_SELF_CALI_WRITE_TO_NV)
    nv_selfcali_burn_config();
#endif
#if defined(RF_SELF_CALI_FROM_NV)
    nv_selfcali_load_config(0);
#endif
#if RUNTIME_TS_DEBUG
    t_e = MEM_RD32(0x4B700120);
    CLOGW("rf_cali_runtime_proc time cost=%d\n", t_e - t_s);
#endif
    if (log_level != -1)
        cloglvl = saved_cloglvl;
    return 0;
}

int32_t ls_rf_cali_probe(rf_cali_runtime *do_rfcali, void *params)
{
    uint8_t mode;
    if (params && (&rf_cali.params != params))
        memcpy(&rf_cali.params, params, sizeof(rf_cali.params));
    mode = rf_cali.params.mode;
    CLOGI("==== rf_cali_probe mode_%s start ==========\n", mode?"bt":"wf");

    if ((mode != RFCALI_MODE_WF) && (mode != RFCALI_MODE_BT))
    {
        CLOGE("rf_cali_probe don't support mode(%d)!\n", mode);
        goto bail;
    }

#if defined(RFCALI_BT_EN)
    if (mode == RFCALI_MODE_BT)
    {
        rf_cali.ops = &bt_cali_ops;
        rf_cali_bootup_proc_bt();
    }
#endif

#if defined(RFCALI_WF_EN)
    if (mode == RFCALI_MODE_WF)
    {
        rf_cali.ops = &wf_cali_ops;
        rf_cali_bootup_proc_wf();
    }
#endif

    if (rf_cali.ops == NULL)
    {
        CLOGE("rf_cali_probe macro restrict to support mode(%s)!\n", mode?"bt":"wf");
        goto bail;
    }

#if defined(RFCALI_WF_EN)
    rf_cali.ops = &wf_cali_ops;
    rf_cali.redo_cali = rf_cali_runtime_proc;// wifi tx dpd cali

    if (do_rfcali)
    {
        *do_rfcali = rf_cali_runtime_proc;
    }
#endif

    CLOGI("==== rf_cali_probe mode_%s done! ==========\n", mode?"bt":"wf");
    return 0;
bail:
    return -1;
}

static void set_rf_params(P_RF_CALI_PARAMS p, uint8_t mode)
{
    p->mode = mode;
    p->rxdcoc_disable = 0;
    p->rxrc_disable = (mode == RFCALI_MODE_BT);
    p->rxiq_disable = 0;
    p->txiq_disable = (mode == RFCALI_MODE_BT);
    p->txdpd_disable = (mode == RFCALI_MODE_BT);

    if (mode == RFCALI_MODE_WF) {
        p->txiq_iter_times = 10;
        p->txdpd_tbl_idx = DPD_COMP_TABLE_CNT - 1;
        p->txdpd_iter_times = 5;
        p->txdpd_fb_gain = 0xff;
    }
}

int8_t rf_selfcali_one_time_proc(int8_t ppa_cali)
{
    int8_t ret = 0;

#if defined(RFCALI_WF_EN)
    uint16_t chan[3] = {2412, 2462, 2442};

    CLOGI("[ATTENTION]!! start to do selfcali process !! cali addr %x end addr %x\n", CALI_MEM_START_ADDR, CALI_MEM_END_ADDR);
    rf_cali.ops = &wf_cali_ops;
    rf_cali_bootup_proc_wf();
    if (ppa_cali) {
        rf_cali_ppacap_proc();
    }
    else {
        rf_ppacap_update_nv();
    }
    for (int i = 0; i < 3; i++) {
        ls_rf_set_channel(chan[i]);
        rf_udelay(100);
        rf_cali_runtime_proc(-1);
    }
#if defined(RF_SELF_CALI_FROM_NV)
    ret = nv_selfcali_burn_config();
#endif
    CLOGI("[ATTENTION]!! selfcali process done !!\n");
#endif
    return ret;
}

#if RF_BOARD_VER == 2
extern int8_t  wf_power_offset_reg[3];
#endif
int ls_rf_cali_redo(int8_t ppa_cap)
{
    int res = 0;

#if RF_BOARD_VER == 2
    if (ppa_cap)
        memset(wf_power_offset_reg, 0, sizeof(wf_power_offset_reg));
#endif

    res = wifi_free_rx_buff();
    if (res) {
        CLOGI("free rx buff fail \n");
        return res;
    }
#if defined(RF_SELF_CALI_FROM_NV)
    res = rf_selfcali_one_time_proc(ppa_cap);
    nv_selfcali_load_config(!res);
#else
    rf_cali_runtime_proc(-1);
#endif
    wifi_reinit_rx_buff();
    return res;
}

int32_t ls_rf_cali_proc(void)
{
    int32_t status = 0;
#if defined(RF_SELF_CALI_FROM_NV)
    int8_t res = 0;
#endif
#if RFCALI_WF_EN == 1 || RFCALI_BT_EN == 1
    P_RF_CALI_PARAMS p = &rf_cali.params;
#endif
    ls_rf_probe();

#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
    nv_selfcali_init();
#endif

#if RFCALI_WF_EN == 1
    set_rf_params(p, RFCALI_MODE_WF);
    ls_rf_cali_probe(NULL, p);
#endif

#if (RFCALI_WF_EN == 1) || (RFCALI_BT_EN == 1)
    wf_macbyp_clken_set(1);
    wf_crm_rcclkforce_setf(1);
    newriu_init();
#if !defined(RF_SELF_CALI_FROM_NV)
  #if !defined(WIFI_RAM_ATE) && !defined(RF_SELF_CALI_WRITE_TO_NV)
    ls_rf_set_channel(2442);
    rf_udelay(100);
    rf_cali_runtime_proc(-1);
  #endif
#else
#if RF_BOARD_VER == 2
    goto selfcali_bail;
#endif
    if (!nv_selfcali_head_check()) {
        goto selfcali_bail;
    }
    res = rf_selfcali_one_time_proc(1);
selfcali_bail:
#endif
#endif

#if RFCALI_BT_EN == 1
    set_rf_params(p, RFCALI_MODE_BT);
    ls_rf_cali_probe(NULL, p);
#endif
#if RFCALI_WF_EN == 1
    set_rf_params(p, RFCALI_MODE_WF);
#endif

#if defined(RF_SELF_CALI_FROM_NV)
    nv_selfcali_load_config(1);
#endif
    return status;
}


