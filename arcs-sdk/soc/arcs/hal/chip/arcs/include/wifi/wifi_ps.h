/****************************************************************************************
 *
 * @file psm.h
 *
 * @brief WiFi power save management
 *
 * Copyright (C) ListenAI 2023
 *
 * Created on: Dec 7, 2023
 *
 *
 ****************************************************************************************
 */
#ifndef _WIFI_PS_H_
#define _WIFI_PS_H_

#include "arcs_ap.h"


#define PSM_LOCK_BIT_APP           0x00000001
#define PSM_LOCK_BIT_FHOST_CNTRL   0x00000002
#define PSM_LOCK_BIT_FHOST_RX      0x00000004
#define PSM_LOCK_BIT_LWIP          0x00000008

#define WIFI_PS_WAKEUP_BY_MAC
//#define WIFI_PS_WAKEUP_BY_AON
//#define WIFI_PS_WAKEUP_BY_TIMER

struct wifi_ps_ops {
    /*Before entering sleep, it is necessary to set the clock, power domains,
     * and other operations that depend on the hardware of the system platform*/
    int32_t (*sys_wifi_suspend)(uint32_t sleep_time, bool suspend);
    /*Operations to wakeup wifi hardware*/
    int32_t (*sys_wifi_resume)(bool suspend);
};


#endif
