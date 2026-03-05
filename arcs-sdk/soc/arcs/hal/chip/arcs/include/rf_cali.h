/**
 ****************************************************************************************
 *
 * @file rf_cali.h
 *
 * @brief Definitions and declarations of RF calibraion module
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Sep 15, 2023
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */


#ifndef _RF_CALI_H_
#define _RF_CALI_H_

#include "stdint.h"
#include "rf_drv.h"

enum {
    RFCALI_MODE_WF = 0,
    RFCALI_MODE_BT = 1,
};

typedef struct rf_cali_params {
    uint8_t mode;
    uint8_t rxdcoc_disable;
    uint8_t rxrc_disable;
    uint8_t rxiq_disable;
    uint8_t txiq_disable;
    uint8_t txiq_iter_times;
    uint8_t txdpd_disable;
    uint8_t txdpd_tbl_idx;
    uint8_t txdpd_iter_times;
    uint8_t txdpd_fb_gain;
} RF_CALI_PARAMS, *P_RF_CALI_PARAMS;

typedef struct rf_cali_dpd_cfg_table {
    uint8_t pwr_idx;
    int8_t tssi;
    uint8_t pred_lut_idx;
} RF_CALI_DPD_CFG, *P_RF_CALI_DPD_CFG;

typedef const struct rf_cali_ops {
    void (*env_init)(void);
    void (*env_deinit)(void);
    int8_t (*rxdcoc_init)(uint16_t freq, uint8_t lna_gain);
    int8_t (*rxdcoc_measure)(int32_t *dc_i, int32_t *dc_q, int8_t word_i, int8_t word_q);
    int8_t (*rxdcoc_detect_interference)(int8_t *word_i, int8_t *word_q);
    int8_t (*rxdcoc_result)(int8_t *word_i, int8_t *word_q, uint8_t len);
    int8_t (*rxrc_init)(int64_t *half_E0_q, uint8_t *cap);
    int8_t (*rxrc_measure)(int64_t *E1_q, uint8_t *cap);
    int8_t (*rxrc_result)(int8_t cap);
    int8_t (*rxiq_init)(uint16_t freq);
    int8_t (*rxiq_measure)(int32_t *Isq, int32_t *Qsq, int32_t *IxQ);
    int8_t (*rxiq_result)(int16_t c21, int16_t c22);
    int8_t (*txiq_fb_init)(uint8_t dbg, uint8_t abb_gain);
    int8_t (*txiq_fb_measure)(int32_t *Isq, int32_t *Qsq, int32_t *IxQ);
    int8_t (*txiq_fb_result)(int16_t c21, int16_t c22);
    int8_t (*txiq_tx_init)(void);
    int8_t (*txiq_tx_measure)(void);
    int8_t (*txiq_tx_result)(int16_t c21, int16_t c22);
    int8_t (*txiq_fb_deinit)(void);
    int8_t (*txiq_dump_data)(int32_t *Isq, int32_t *Qsq, int32_t *IxQ);
    int8_t (*txiq_restore_rxiq_result)(void);
    int8_t (*txdpd_remap_pred)(void);
    int8_t (*txdpd_init)(uint8_t fb_delay);
    int8_t (*txdcdpd_toggle_mixen)(uint8_t en);
    int8_t (*txdpd_adjust_gain)(int8_t pwr_delta);
    int8_t (*txdpd_measure)(int8_t pwr_idx);
    int8_t (*txdpd_deinit)(void);
    int8_t (*txdpd_result)(uint8_t tbl_idx, void *tbl_src);
    int8_t (*txdpd_get_result)(uint8_t tbl_idx, void *tbl_dst);
    int8_t (*txdc_result)(void *comp_dc, uint8_t range);
    int8_t (*set_ppa_cap)(uint8_t idx, uint8_t val);
    int8_t (*get_ppa_cap)(uint8_t idx);
    int8_t (*send_ttg_start)(uint8_t tx_pwr, int8_t dir, uint16_t lo_freq);
    int8_t (*send_ttg_stop)(void);
} RF_CALI_OPS, *P_RF_CALI_OPS;

typedef int32_t (*rf_cali_runtime)(uint32_t log_level);

typedef struct rf_cali_entry {
     P_RF_ENTRY owner;
     RF_CALI_PARAMS params;
     P_RF_CALI_OPS ops;
     rf_cali_runtime redo_cali;
} RF_CALI_ENTRY, *P_RF_CALI_ENTRY;

typedef struct {
    uint8_t dac_sc_i;
    uint8_t dac_sc_q;
    uint8_t dac_i;
    uint8_t dac_q;
}RF_CALI_RXDCOC_WORD;

typedef struct {
    uint16_t c21;
    uint16_t c22;
}RF_CALI_RXIQ_WORD;

typedef struct {
    uint16_t dac_i;
    uint16_t dac_q;
}RF_CALI_TXDC_WORD;

typedef struct {
    uint16_t c21;
    uint16_t c22;
}RF_CALI_TXIQ_WORD;

typedef struct rf_cali_power_db_table {
    int8_t db;
    int32_t pwr;
} RF_CALI_POWER_DB_TBL, *P_RF_CALI_POWER_DB_TBL;

extern RF_CALI_DPD_CFG dpd_cfg_table[];
extern RF_CALI_DPD_CFG dpd_base_table[];
#define DPD_COMP_TABLE_CNT  4
#define DPD_REST_TABLE_CNT  2
extern RF_CALI_OPS cali_ops;
extern RF_CALI_ENTRY rf_cali;
extern RF_CALI_RXDCOC_WORD g_bt_rxdc_comp[3][3];
extern RF_CALI_RXIQ_WORD g_bt_rxiq_comp;
extern RF_CALI_TXDC_WORD g_bt_txdc_comp;
extern RF_CALI_TXIQ_WORD g_bt_txiq_comp;
extern uint8_t g_bt_rxrc_comp;

int ls_rf_cali_redo(int8_t ppa_cap);

extern int32_t ls_rf_cali_probe(rf_cali_runtime *do_rfcali, void *params);
extern int32_t ls_rf_cali_proc(void);
extern void set_sc_i(uint8_t mode, uint8_t sc_i);
extern void set_sc_q(uint8_t mode, uint8_t sc_q);
extern void bt_rf_set_channel(uint16_t freq);
extern void bt_get_dcoc_word(RF_CALI_RXDCOC_WORD *dcoc_word);
extern void bt_set_dcoc_word(RF_CALI_RXDCOC_WORD (*dcoc_word_arr)[3], uint8_t row, uint8_t col);

extern uint8_t _sshram[], _eshram[];
extern uint8_t _scali[], _ecali[];

#define SET_CHANNEL(freq) do { (rf_cali.owner->ops->set_channel(freq)); } while(0)
#define RF_SW_RESET() do { (rf_cali.owner->ops->sw_reset()); } while(0)
#define WIFI_CALI_BUF_SIZE 72
#if CALI_BUF
#define MEM_DUMP_START_ADDR (_scali)
#define MEM_DUMP_MID_ADDR (_scali + ((uint32_t)(_ecali - _scali) >> 1))
#define MEM_DUMP_END_ADDR (_ecali)
#else
#define MEM_DUMP_START_ADDR (_sshram)
#define MEM_DUMP_MID_ADDR (_sshram +  0x2000)
#define MEM_DUMP_END_ADDR (_sshram + 0x4000)
#endif
#define MEM_DUMP_LEN 4096

//#define DPD_LUNA 1
#define DPD_LUNA_DEBUG 0
#define DPD_TS_DEBUG 0
#define LUNA_VECTOR_ADDR 0x200A0000
#define LUNA_VECTOR_SIZE 0x10000
#define LUNA_VECTOR_QUAD_SIZE 0x4000
#define LUNA_TEMP32_ADDR 0x2009C010
#define LUNA_TEMP64_ADDR 0x2009C018
#define LUNA_AMPSIG_ADDR 0x2009C020

#endif
