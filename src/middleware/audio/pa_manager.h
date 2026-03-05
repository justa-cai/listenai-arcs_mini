/**
 * @brief       PA Manager
 * @version     0.1
 * @date        2022-11-04
 * @author      mokee
 * 
 * Copyright (C) 2022 ANHUI LISTENAI Co., LTD All Rights Reserved
 */

#ifndef __LISTENAI_PA_MANAGER_H__
#define __LISTENAI_PA_MANAGER_H__

#include <stdint.h>

// PA delay off time
#define LS_PA_BASE_TIME (30 * 1000)
// Forever
#define LS_PA_FOREVER (0xffffffffUL)

typedef enum PA_MGR_STATE {
    // PA OFF
    PA_MGR_OFF = 0,
    // PA ON
    PA_MGR_ON = 1,

    PA_MGR_NONE = 0xFF,
} PA_MGR_STATE;

#define PA_PRINT_STATE(s) \
    (s == PA_MGR_ON)? "ON": \
    (s == PA_MGR_OFF)? "OFF": \
    (s == PA_MGR_NONE)? "NONE":"UNKNOW"

/**
 * @brief  PA GPIO Init
 */
void pa_manager_pre_init();

/**
 * @brief  PA OnOFF
 * @param  onoff    0: OFF, other: ON
 */
int pa_manager_onoff(int onoff);

/**
 * @brief   Init PA Manager
 * @param   init_state  Init State
 */
void pa_manager_init(PA_MGR_STATE init_state);

/**
 * @brief   Refresh PA state
 * @param   next_state  Next state of PA
 * @param   duration    Time(ms) of switching to off after change PA state
 *                      default: LS_PA_BASE_TIME
 *                      
 * @param   by_which    Caller
 */
void pa_manager_refresh(PA_MGR_STATE next_state, uint32_t duration, const char *const by_which);

/**
 * @brief   重置 PA state
 * @param   by_which    Caller
 */
void pa_manager_reset_state(const char *const by_which);

/**
 * @brief   获取 PA state
 */
PA_MGR_STATE pa_manager_get_state();

#endif