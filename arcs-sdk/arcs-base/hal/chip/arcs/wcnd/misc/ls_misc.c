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


float tcal = 25.8+0.5; // room_temp + heat_res (25.00 Deg/W) * chip_power (0.05W)
float vptat_cal = 0.70819921875;

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
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


void ls_read_efuse_temp_para(void)
{
    int32_t vptat_val = 0;
    int32_t tcal_val = 0;
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

int32_t ls_get_cur_temp(void)
{
    float vptat = 0.0;
    float die_temp = 0.0;
    ls_read_temp_voltage(100, &vptat);
    die_temp = tcal + (tcal + 273.15) / vptat_cal * (vptat - vptat_cal);

    return (int32_t)die_temp;
}

void ls_temp_por_update(void)
{
    int32_t temp = 0;
    static int32_t last_temp = -273;
    uint32_t ref;

    if (ls_efuse_read_word(11, &ref))
    {
        CLOGE("read efuse 11 fail \r\n");
        return;
    }

    // TODO: Generate EFUSE fields from excel
    ref = (ref & 0xF000000) >> 24;

    volatile int32_t delay_count = 100000;
    while(delay_count--);

    temp = ls_get_cur_temp();
   // CLOG("temp %d \r\n", temp);

    if (abs(temp - last_temp) >= HYSTERESIS_THRESHOLD)
    {
        rf_por_temp_config(temp, ref);
        last_temp = temp;
    }
}

void ls_temp_default_por(void)
{
    int32_t temp = 26;
    uint32_t ref = 5; // refer to PA_BIASL_WF_OFDM = 5*1.3 = 7

    if (ls_efuse_read_word(11, &ref))
    {
        CLOGW("read efuse 11 fail \r\n");
    }
    ref = (ref & 0xF000000) >> 24;
    rf_por_temp_config(temp, ref);
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

