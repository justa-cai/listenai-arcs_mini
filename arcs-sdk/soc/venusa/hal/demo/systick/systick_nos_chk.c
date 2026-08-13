/*
 * systick_nos_chk.c
 *
 *  Created on: Jul 5, 2023
 *      Author: USER
 */

#include "systick.h"

#include "log_print.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 100000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static void SYSTICK_MS_Validation(void);
static void SYSTICK_US_Validation(void);
static void SYSTICK_Time_Validation(void);

static function test_function_array[] = {
    SYSTICK_MS_Validation,
    SYSTICK_US_Validation,
    SYSTICK_Time_Validation,
};

static void SYSTICK_MS_Validation(void){
    CLOGD("[SYSTICK] Ms validation start");

    CLOGD("[SYSTICK] 0 Ms trigger!!!");

    SysTick_Delay_Ms(2000);

    CLOGD("[SYSTICK] 2000 Ms trigger!!!");

    SysTick_Delay_Ms(500);

    CLOGD("[SYSTICK] 2500 Ms trigger!!!");

    SysTick_Delay_Ms(250);

    CLOGD("[SYSTICK] 2750 Ms trigger!!!");

    SysTick_Delay_Ms(10);

    CLOGD("[SYSTICK] 2760 Ms trigger!!!");

    SysTick_Delay_Ms(1);

    CLOGD("[SYSTICK] 2761 Ms trigger!!!");

    CLOGD("[SYSTICK] Ms validation end");
}

static void SYSTICK_US_Validation(void){
    CLOGD("[SYSTICK] Us validation start");

    CLOGD("[SYSTICK] 0 Us trigger!!!");

    SysTick_Delay_Us(1000);

    CLOGD("[SYSTICK] 1000 Us trigger!!!");

    SysTick_Delay_Us(500);

    CLOGD("[SYSTICK] 1500 Us trigger!!!");

    SysTick_Delay_Us(250);

    CLOGD("[SYSTICK] 1750 Us trigger!!!");

    SysTick_Delay_Us(10);

    CLOGD("[SYSTICK] 1760 Us trigger!!!");

    SysTick_Delay_Us(1);

    CLOGD("[SYSTICK] 1761 Us trigger!!!");

    CLOGD("[SYSTICK] Us validation end");
}

// optimize O0
__attribute__((optimize("O0"))) static void SYSTICK_Time_Validation(void){
    uint32_t systick_0 = 0, systick_1 = 0, systick_diff;

    SysTick_Open(SYSTICK_MAX_INT);
    systick_0 = SysTick_Time();

    uint32_t i,j;

    for(i = 0; i < 1000; i++){
        for(j = 0; j < 1000; j++);
    }


    systick_1 = SysTick_Time();
    systick_diff = systick_1 - systick_0;
    CLOGD("Systick0 -> %u", systick_0);
    CLOGD("Systick1 -> %u", systick_1);
    CLOGD("Systick diff -> %u", systick_diff);
}

int main(){
    uint32_t times;
    logInit(0, 115200);
    CLOGD("SYSTICK VALIDATION");
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}
