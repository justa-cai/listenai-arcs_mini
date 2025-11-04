/**
 ****************************************************************************************
 *
 * @file rf_cali_chk.h
 *
 * @brief definitions and declarations of rf calibration Check results
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */
#ifndef _RF_CALI_CHK_H_
#define _RF_CALI_CHK_H_

#include "DpdEst.h"

#define RX_DC_THD 100
#define RX_IQ_C21_THD 100
#define RX_IQ_C22_THD 250
#define TX_DC_THD 50
#define TX_IQ_C21_THD 190
#define TX_IQ_C22_THD 140
#define PPA_CAP_UP_THD 22
#define PPA_CAP_DOWN_THD 5


extern int8_t rf_cali_check_dc_result(int8_t dc_i, int8_t dc_q, int8_t dc_i_thd, int8_t dc_q_thd);
extern int8_t rf_cali_check_iq_result(int16_t *c21, int16_t *c22, int16_t c21_thd, int16_t c22_thd);
extern int8_t rf_cali_check_dpd_result(complexint16* cParaEst);

#endif