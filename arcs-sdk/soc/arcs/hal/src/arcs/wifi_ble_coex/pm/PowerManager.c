/*
 * PowerManager.c
 *
 *  Created on: Feb 2, 2023
 *      Author: USER
 */

#include "PowerManager.h"

void PowerManager_CallOnce_Hook(pm_callonce_hook* __hook){
    static uint8_t call_once_flag = 0;

    if (call_once_flag == 0 && __hook){
        __hook();
    }

    // Never edit
    call_once_flag = 1;
}
