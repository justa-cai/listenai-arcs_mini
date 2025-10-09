/**
 ****************************************************************************************
 *
 * @file rf_drv.h
 *
 * @brief definitions and declarations of RF driver
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 30, 2023
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */

#ifndef _RF_DRV_H_
#define _RF_DRV_H_

#define RFIF_DELAY1_DEF (0x77)
#define RFIF_DELAY2_DEF (0x77)
#define RFIF_DELAY3_DEF (0x2)
#define RFIF_DELAY4_DEF (0x2)
#define RFIF_DELAY5_DEF (0x3bf)
#define RFIF_DELAY7_DEF (0x77)
#define RFIF_DELAY8_DEF (0x77)
#define RFIF_DELAY9_DEF (0x2)

#define RFIF_DELAY8_FINE_TUNE (320)
#define RFIF_DELAY9_FINE_TUNE (127)

#include <stdint.h>

typedef struct rf_params {
    uint8_t version;
    uint8_t init_done;
} RF_PARAMS, *P_RF_PARAMS;

enum {
    RF_MODE_WIFI,
    RF_MODE_BT
};

typedef const struct rf_ops {
    void (*init)(void);
    void (*set_channel)(uint16_t freq);
    void (*sw_reset)(void);
    uint8_t (*get_version)(void);
    void (*set_wf_ppa_gain)(uint8_t index, uint8_t ppa_val);
    uint8_t (*get_wf_ppa_gain)(uint8_t index);
    void (*set_bt_ppa_gain)(uint8_t index, uint8_t ppa_val);
    uint8_t (*get_bt_ppa_gain)(uint8_t index);
    void (*set_wf_abb_gain)(uint8_t index, uint8_t abb_val);
    uint8_t (*get_wf_abb_gain)(uint8_t index);
    void (*set_wf_dig_gain)(uint8_t index, uint16_t dig_val);
    uint16_t (*get_wf_dig_gain)(uint8_t index);
    void (*suspend)(int32_t rf_mode);
    void (*resume)(int32_t rf_mode);
    void (*update_cal_addr)(uint32_t start, uint32_t end);
} RF_OPS, *P_RF_OPS;

typedef struct rf_entry {
    RF_PARAMS params;
    P_RF_OPS ops;
} RF_ENTRY, *P_RF_ENTRY;

extern RF_ENTRY rf_entry;

#define TEMP_BOTTOM (-35)
#define TEMP_STEP (10)
#define TEMP_TOP (95)
#define TEMP_INTV_NUM ((TEMP_TOP - TEMP_BOTTOM)/TEMP_STEP + 2)
#define TEMP_REG_NUM 2
#define TEMP_LDO_THRESH (-20)

static inline uint32_t temp2idx(int32_t temp)
{
    if(temp < TEMP_BOTTOM) {
        return 0;
    } else if (temp >= TEMP_TOP ) {
        return TEMP_INTV_NUM - 1;
    } else {
        return ((temp - TEMP_BOTTOM)/10 + 1);
    }
}

/*
 * FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern int rf_udelay(uint32_t us);
extern void ls_rf_probe(void);
extern void ls_rf_sw_reset(void);
extern void ls_rf_set_channel(uint16_t freq);
extern uint8_t ls_rf_get_version(void);
extern void ls_rf_set_wf_ppa_gain(uint8_t index, uint8_t ppa_val);
extern uint8_t ls_rf_get_wf_ppa_gain(uint8_t index);
extern void ls_rf_set_bt_ppa_gain(uint8_t index, uint8_t ppa_val);
extern uint8_t ls_rf_get_bt_ppa_gain(uint8_t index);
extern void ls_rf_set_wf_abb_gain(uint8_t index, uint8_t abb_val);
extern void ls_rf_set_wf_dig_gain(uint8_t index, uint8_t dig_val);
void rf_por_temp_config(int32_t temp, uint32_t ref);
int32_t ls_rf_suspend(int32_t rf_mode);
int32_t ls_rf_resume(int32_t rf_mode);
void wf_crm_rcclkforce_setf(uint8_t rcclkforce);
void wf_macbyp_clken_set(uint32_t value);


#endif//_RF_DRV_H_
