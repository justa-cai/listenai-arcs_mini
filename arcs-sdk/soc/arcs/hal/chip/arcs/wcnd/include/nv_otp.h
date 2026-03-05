/**
 ****************************************************************************************
 *
 * @file nv_otp.h
 *
 * @brief definitions and declarations of rf calibration parameter save to or load from NV
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */

#ifndef _NV_OTP_H_
#define _NV_OTP_H_

#include "DpdEst.h"
#include "nv_config.h"

#ifdef RF_SELF_CALI_FROM_NV
extern complexint16 nv_tx_pred_table_chan_low[DPD_COMP_TABLE_CNT][MAX_PARALEN];
extern complexint16 nv_tx_pred_table_chan_mid[DPD_COMP_TABLE_CNT][MAX_PARALEN];
extern complexint16 nv_tx_pred_table_chan_hig[DPD_COMP_TABLE_CNT][MAX_PARALEN];
extern complexint16 nv_tx_pred_rest_table_chan_low[DPD_REST_TABLE_CNT][MAX_PARALEN];
extern complexint16 nv_tx_pred_rest_table_chan_mid[DPD_REST_TABLE_CNT][MAX_PARALEN];
extern complexint16 nv_tx_pred_rest_table_chan_hig[DPD_REST_TABLE_CNT][MAX_PARALEN];
extern int8_t ls_nv_selfcali_valid_flag;
extern int8_t nv_selfcali_head_check(void);
extern int8_t nv_selfcali_load_config(int8_t from_otp);
#endif

#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
extern ls_nv_selfcali_cfg_t nv_selfcali_cfg;
extern int8_t nv_update_selfcali_wf_rx_params(int8_t *rxcali_rc_cap, int8_t *rxcali_dc_comp_i, int8_t *rxcali_dc_comp_q,
    int8_t *rxcali_dc_sc_i, int8_t *rxcali_dc_sc_q, int16_t *rxcali_iq_comp_i, int16_t *rxcali_iq_comp_q);
extern int8_t nv_update_selfcali_wf_tx_params(int16_t *txcali_dc_comp_i, int16_t *txcali_dc_comp_q, int16_t *txcali_iq_comp_i, int16_t *txcali_iq_comp_q);
extern int8_t nv_update_selfcali_dpd_params(uint8_t tbl_idx, uint32_t *txcali_dpd_tbl);
extern int8_t nv_update_selfcali_ppa_cap_params(uint8_t *ppa_cap_0, uint8_t *ppa_cap_1, uint8_t *ppa_cap_2);
extern void nv_selfcali_init();
extern int8_t nv_selfcali_burn_config();
#endif

#endif
