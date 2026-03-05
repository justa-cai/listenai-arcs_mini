/**
****************************************************************************************
*
* @file bt_drv_if.c
*
* @brief BT Driver Interface define
*
* Copyright (C)
*
* $Rev: $
*
****************************************************************************************
*/
//#include <stdio.h>
//#include <string.h>         // for memcpy
//#include <stdlib.h>         // standard lib functions
//#include <stddef.h>         // standard definitions
//#include <stdint.h>         // standard integer definition
//#include <stdbool.h>        // boolean definition

#include "bt_drv.h"

extern uint8_t bt_sleep_api_init(void **api);
extern uint8_t  bt_rf_api_init(void *api);
extern uint8_t ls_dma_api_init(void *api);

uint8_t external_api_init(struct lsip_external_api_str *api)
{
    if (NULL == api)
    {
        return 1;
    }

    api->bt_drv_init = bt_drv_reg_init;
    api->bt_sleep_api_init = bt_sleep_api_init;
    api->rf_api_init = bt_rf_api_init;
    api->bt_dma_api_init = ls_dma_api_init;
    api->bt_sleep_wakeup_reg_init = sleep_wakeup_reg_init;

    return 0;
}

