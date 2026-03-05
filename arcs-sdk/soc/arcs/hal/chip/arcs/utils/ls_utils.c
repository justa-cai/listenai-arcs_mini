/**
 ****************************************************************************************
 *
 * @file ls_utils.c
 *
 * @brief utils functions implement.
 *
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */
#include <stdlib.h>
#include <string.h>

#include "log_print.h"
#include "arcs_ap.h"
#include "nvs.h"
#include "nvds_tag_def.h"
#include "ls_utils.h"
#include "Driver_GPADC.h"
#include "ClockManager.h"
#include "Driver_TRNG.h"

/*
 * DEFINES
 ****************************************************************************************
 */




/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/**
 * @brief     This func read temperatue voltage from GPADC TEMP related channel
 *
 * @attention it is for wifi get temperature, if wifi and application not in same core, IPC/MRPC utils should be used 
 *
 * @params
 *
 * @return
 *    - 0 : succeed
 *    - others: other errors.
 */
__attribute__((weak)) int ls_read_temp_voltage(float *vout)
{
    // 不能运行到这个weak函数，应该用外部的实现
    __builtin_trap();

    float vptat_sum = 0.0, vptat, vfs = 1.2;
    uint32_t code = 0x00;
    static uint8_t init = 0;
//    volatile int32_t delay_count = 10000;
    int ret = 0;

#if (CONFIG_PM == 0)
    if (!init)
#endif
    {
        HAL_GPADC_Initialize(GPADC());
#if (CONFIG_PM == 0)
        init = 1;
#endif
    }
    HAL_GPADC_Control(GPADC(), (CSK_GPADC_CHANNEL_SEL_TEMP | CSK_GPADC_CHANNEL_SEL_VBAT) | \
                                CSK_GPADC_DMA_ENABLE(0));

    HAL_GPADC_SetTriggerNum(GPADC(), 1);
    HAL_GPADC_SetVrefSel(GPADC(), 0); //VBG1/2

    HAL_GPADC_Start(GPADC());
    HAL_GPADC_PollForConversion(GPADC(), 0);

//    while(delay_count--) ;

    code = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_TEMP);
    vptat = code / 1024.0 * vfs;
    //vptat_sum += vptat;

    *vout = vptat;
    return ret;
}

static void gen_random_mac(uint8_t *mac_addr)
{
    __HAL_CRM_TRNG_CLK_ENABLE();
    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    //initialize
    HAL_TRNG_Initialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
    HAL_TRNG_Control(TRNG(), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);
    HAL_TRNG_InterruptDisable(TRNG());

    for(uint32_t cnt=0; cnt<6; cnt++)
    {
        HAL_TRNG_Enable(TRNG());
        while(HAL_TRNG_GetDataReady(TRNG()) == 0);
            mac_addr[cnt] = HAL_TRNG_GetData(TRNG());
    }

    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);
    __HAL_CRM_TRNG_CLK_DISABLE();
    mac_addr[0] = 0x00;
    mac_addr[2] = 0x75;
}

/**
 * @brief     This func get mac address from NVS for wifi
 *
 * @attention it is to get mac from nvs, if wifi and application not in same core, IPC/MRPC utils should be used 
 *
 * @params
 *
 * @return
 *    - 0: succeed
 *    - others: other errors.
 */
int8_t ls_get_mac_from_nvs(uint8_t mac_addr[6])
{
#if CFG_NVS
    uint8_t ret, mac[8] ={0};
    size_t len = NVDS_LEN_WIFI_MAC_ADDR;

    ret = nvds_get(NVDS_TAG_WIFI_MAC_ADDR, &len, mac);
    if (ret != NVDS_OK)
    {
        gen_random_mac(mac);
        nvds_put(NVDS_TAG_WIFI_MAC_ADDR, NVDS_LEN_WIFI_MAC_ADDR, mac);
    }
    memcpy(mac_addr, mac, 6);
    return 0;
#else
    return -1;
#endif
}

/**
 * @brief     This func was reserved for mac address customized
 *
 * @attention if wifi and application not in same core, IPC/MRPC utils should be used
 *
 * @params
 *
 * @return
 *    - 0: succeed
 *    - others: other errors.
 */
__attribute__((weak)) int8_t ls_get_mac_customized(uint8_t mac_addr[6])
{
     /// TODO
}

