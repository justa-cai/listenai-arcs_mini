/*
 * aon_iomux_nos_chk.c
 *
 *  Created on: 2023年3月22日
 *      Author: USER
 */
#include "arcs_ap.h"
#include "log_print.h"
#include "systick.h"
#include "IOMuxManager.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define CSK_AON_IOMUX_PAD_B 	CSK_IOMUX_PAD_B

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

static void GPIOB_PIN0_AON_MODE_CONFIGURE(void);
static void GPIOB_PIN1_AON_MODE_CONFIGURE(void);
static void GPIOB_PIN2_AON_MODE_CONFIGURE(void);
static void GPIOB_PIN3_AON_MODE_CONFIGURE(void);
static void GPIOB_PIN4_AON_MODE_CONFIGURE(void);
static void GPIOB_PIN5_AON_MODE_CONFIGURE(void);
static void GPIOB_PIN6_AON_MODE_CONFIGURE(void);
static void GPIOB_PIN7_AON_MODE_CONFIGURE(void);

static function test_function_array[] = {
    GPIOB_PIN0_AON_MODE_CONFIGURE,
    GPIOB_PIN1_AON_MODE_CONFIGURE,
    GPIOB_PIN2_AON_MODE_CONFIGURE,
    GPIOB_PIN3_AON_MODE_CONFIGURE,
    GPIOB_PIN4_AON_MODE_CONFIGURE,
    GPIOB_PIN5_AON_MODE_CONFIGURE,
	GPIOB_PIN6_AON_MODE_CONFIGURE,
	GPIOB_PIN7_AON_MODE_CONFIGURE

};

static void GPIOB_PIN0_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 0, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 0, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 0, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

static void GPIOB_PIN1_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 1, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 1, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 1, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

static void GPIOB_PIN2_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 2, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 2, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 2, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

static void GPIOB_PIN3_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 3, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 3, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 3, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

static void GPIOB_PIN4_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 4, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 4, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 4, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

static void GPIOB_PIN5_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 5, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 5, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 5, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

static void GPIOB_PIN6_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 6, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 6, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 6, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

static void GPIOB_PIN7_AON_MODE_CONFIGURE(void){
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 7, HAL_IOMUX_NONE_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 7, HAL_IOMUX_PULLUP_MODE);
    SysTick_Delay_Ms(1000);
    AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, 7, HAL_IOMUX_PULLDOWN_MODE);
    FAKE_WHILE();
}

int main(){
    uint32_t times;
    logInit(0, 115200);
    CLOGD("[TEST] AON IOMUX VALIDATION");

    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}
