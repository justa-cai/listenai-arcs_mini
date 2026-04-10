/**
 ****************************************************************************************
 *
 * @file ls_misc.c
 *
 * @brief wifi temp/mac... functions implement.
 *
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */
#include "log_print.h"
#include "arcs_ap.h"
#include "rf_drv.h"
#include "ls_misc.h"
#include "ls_utils.h"
#include "nv_config.h"
#include <stdlib.h>

/*
 * DEFINES
 ****************************************************************************************
 */
#define XO_LOW_TEMP  0
#define XO_HIGH_TEMP 1
#define XO_HYSTERSIS_THRESHOLD 95
#define XO_HYSTERSIS_DELTA 3
#define XO_HIGH_THR (XO_HYSTERSIS_THRESHOLD + XO_HYSTERSIS_DELTA)
#define XO_LOW_THR (XO_HYSTERSIS_THRESHOLD - XO_HYSTERSIS_DELTA)


float tcal = 25.8+0.5; // room_temp + heat_res (25.00 Deg/W) * chip_power (0.05W)
float vptat_cal = 0.70819921875;
uint32_t pa_bias_ref = 0;

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
static uint32_t hysteresis_threshold = TEMP_THRESHOLD;
uint8_t last_xo_cali_status = XO_LOW_TEMP;
 #if 0
static float GPADC_read_temp_voltage(int count)
{
    float vptat_sum = 0.0, vptat, vfs = 1.2;
    uint32_t code = 0x00;
    static uint8_t init = 0;
    volatile int32_t delay_count = 10000;

    if (!init)
    {
        HAL_GPADC_Initialize(GPADC());
        init = 1;
    }
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_TEMP | CSK_GPADC_CHANNEL_SEL_VBAT) | \
                                CSK_GPADC_DMA_ENABLE(0));

    HAL_GPADC_SetTriggerNum(GPADC(), 1);
    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2

    HAL_GPADC_Start(GPADC());
    HAL_GPADC_PollForConversion(GPADC(), 0);

    while(delay_count--) ;


    for(int i = 0; i < count; i++)
    {
        code = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_TEMP);
        vptat = code / 1024.0 * vfs;
        vptat_sum += vptat;
    }
    return (vptat_sum / count);

}

static float GPADC_read_temp_voltage(int count)
{
    uint32_t  reg_value;
    uint8_t adc_complete = 0;
    volatile int32_t delay_count = 100000;

    IP_GPADC->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR = 0x1;   //1 bits

    IP_CMN_SYS->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_LD = 0x0; // 1 bits
    IP_CMN_SYS->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_M = 0xC; // 10 bits
    IP_CMN_SYS->REG_PERI_CLK_CFG5.bit.ENA_GPADC_CLK = 0x1; // 1 bits

    IP_GPADC->REG_ADC_CONFIG.bit.LDO_EN = 0x1; // 1 bits
    IP_GPADC->REG_ADC_CONFIG.bit.CLK_MODE = 0x0; // 1 bits
    IP_GPADC->REG_ADC_CONFIG.bit.VREF_SEL = 0x0;  // 2 bits
    IP_GPADC->REG_ADC_CONFIG.bit.VIN_BUF_EN_FORCE = 0x1;  // 1 bits
    IP_GPADC->REG_ADC_CONFIG.bit.LDO_TUNE = 0x0; // 3 bits
    IP_GPADC->REG_ADC_CONFIG.bit.VIN_BUF_EN = 0x1; // 1 bits

    IP_GPADC->REG_ADC_CH_SEL.bit.DMA_CH_EN = 0x0;  // 8 bits
    IP_GPADC->REG_ADC_CH_SEL.bit.ADC_CH_SEL = 0x8;	// 8 bits

    IP_GPADC->REG_ADC_CTRL0.bit.ADC_TRIG_NUM = 120; // 8 bits
    IP_GPADC->REG_ADC_CTRL0.bit.TSENSOR_COEF = 0x4; // 3 bits
    IP_GPADC->REG_ADC_CTRL0.bit.SOFT_TRIG = 0x1; // 1 bits

    while(delay_count-- && !adc_complete)
    {
        reg_value = IP_GPADC->REG_ADC_IRSR0.all;
        if (reg_value & (1<<3))
        {
            IP_GPADC->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR = 0x1;   //1 bits
            adc_complete = 1;
        }
    };
    //IP_GPADC->REG_ADC_CTRL0.bit.SOFT_TRIG = 0x1; // 1 bits
    float vptat_sum = 0.0, vptat, vfs = 1.2;
    uint32_t code = 0x00;

    for(int i = 0; i < count; i++) {
        code = IP_GPADC->REG_ADC_RDR3.bit.READ_DATA_CH3;
        vptat = code / 1024.0 * vfs;
        vptat_sum += vptat;
    }
    return (vptat_sum / count);
}
#endif

static void ls_get_efuse_pa_bias(void)
{
    uint32_t ref = 0;

    if (ls_efuse_read_word(11, &ref))
    {
        CLOGW("read efuse pa bias fail \r\n");
    }
    ref = (ref & 0xF000000) >> 24;
    pa_bias_ref = ref;
}

static void ls_read_efuse_temp_para(void)
{
    uint32_t vptat_val = 0;
    uint32_t tcal_val = 0;
    int8_t ret = 0;

    ret = ls_efuse_read_word(14, &tcal_val);
    if (ret)
    {
	CLOGE("read efuse fail \r\n");
        return;
    }
    ret = ls_efuse_read_word(15, &vptat_val);
    if (ret)
    {
        CLOGE("read efuse fail \r\n");
        return;
    }

    if (tcal_val != 0 && vptat_val != 0) {
        CLOG("\r\n Efuse: tcal %x vptat %x \r\n", tcal_val, vptat_val);
        vptat_cal =((float)vptat_val)/0x80000000;
        tcal = ((float)tcal_val)/0x1000000 - 80;
    }
}

void ls_get_efuse_para(void)
{
    ls_read_efuse_temp_para();
    ls_get_efuse_pa_bias();
}

void ls_set_temp_thr(uint32_t thr)
{
    hysteresis_threshold = thr;
}

int32_t ls_get_cur_temp(void)
{
    float vptat = 0.0;
    float die_temp = 0.0;
    ls_read_temp_voltage(&vptat);
    die_temp = tcal + (tcal + 273.15) / vptat_cal * (vptat - vptat_cal);

    return (int32_t)die_temp;
}

int32_t ls_calc_temp(float vptat)
{
    float die_temp = 0.0;

    die_temp = tcal + (tcal + 273.15) / vptat_cal * (vptat - vptat_cal);

    return (int32_t)die_temp;
}

void ls_adj_xo(int32_t temp)
{
    if (temp >= XO_HIGH_THR && last_xo_cali_status == XO_LOW_TEMP) {
        int8_t xo_cap = IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG;
        IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = xo_cap + 3;
        last_xo_cali_status = XO_HIGH_TEMP;
        CLOGI("Update xo_cap:%d since high temp:%d\n", IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG, temp);
    }
    else if (temp < XO_LOW_THR && last_xo_cali_status == XO_HIGH_TEMP) {
        int8_t xo_cap = IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG;
        IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG = xo_cap - 3;
        last_xo_cali_status = XO_LOW_TEMP;
        CLOGI("Update xo_cap:%d since low temp:%d\n", IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.XO24M_CAP_FRC_REG, temp);
    }
}

#if RF_BOARD_VER == 2 //Taoyun
bool ls_temp_por_update(void)
{
    int32_t temp = 0;
    static int32_t last_temp = -273;
    uint32_t ref = pa_bias_ref;
    bool need_cali = false;

    temp = ls_get_cur_temp();
    //CLOGD("temp %d\n", temp);

    if (abs(temp - last_temp) >= TEMP_THRESHOLD)
    {
        need_cali = rf_por_temp_config(temp, ref);
        last_temp = temp;
    }

    return need_cali;
}

void ls_temp_default_por(void)
{
    int32_t temp = 36; // default chip die temp = envionment temp 26 + 10 degree
    uint32_t ref = pa_bias_ref;

    rf_por_temp_config(temp, ref);
}

#else
bool ls_temp_por_update(void)
{
    int32_t temp = 0;
    static int32_t last_temp = -273;
    bool need_cali = false;

    temp = ls_get_cur_temp();
    //CLOGD("temp %d\n", temp);

    if (abs(temp - last_temp) >= hysteresis_threshold)
    {
        rf_por_temp_config(temp);
        last_temp = temp;
        need_cali = true;
    }
    ls_adj_xo(temp);
    return need_cali;
}

void ls_temp_default_por(void)
{
    uint32_t ref = pa_bias_ref;

    rf_pa_bias_config(ref, TEMP_NORMAL);
    rf_por_temp_config(TEMP_NORMAL+10); //chip die temperature = environmental temperature + 10℃
}
#endif

bool ls_temp_rf_por_config(int32_t temp, bool realtime)
{
    static int32_t last_temp = TEMP_NORMAL;
    uint32_t ref = pa_bias_ref;
    bool need_cali = false;

    if (realtime) {
        #if RF_BOARD_VER == 2
        rf_por_temp_config(temp, ref);
        #else
        rf_por_temp_config(temp);
        #endif
    } else {
        if (abs(temp - last_temp) >= hysteresis_threshold)
        {
            #if RF_BOARD_VER == 2
            need_cali = rf_por_temp_config(temp, ref);
            #else
            need_cali = rf_por_temp_config(temp);
            #endif
            last_temp = temp;
            if (hysteresis_threshold >= 30)
                need_cali = true;
        }
    }

    #if RF_BOARD_VER != 2
    ls_adj_xo(temp);
    #endif

    return need_cali;
}

int8_t ls_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;
    /* #1 get mac from flash fixzone */
    if (!nv_fixzone_get_wf_mac(mac_addr))
        return 0;
    /* #2  get mac from efuse */
    if (!nv_efuse_read_mac(mac_addr))
        return 0;
    /* #3 get mac from or generate mac to nvs */
    ret = ls_get_mac_from_nvs(mac_addr);
    return ret;
}

