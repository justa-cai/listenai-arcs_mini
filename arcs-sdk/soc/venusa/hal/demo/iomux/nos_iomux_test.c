/*
 * nos_iomux_test.c
 *
 *
 */
#include "log_print.h"

#include "IOMuxManager.h"
#include "venusa_ap.h"

#include <stdio.h>
#include <stdlib.h>

typedef void (*function)(void);

// IOMux Pad
#define IOMUX_IOA_TEST_PIN              5
#define IOMUX_IOB_TEST_PIN              4
#define IOMUX_IOC_TEST_PIN				3

/// Normal IOMUX
static void IOMUX_FuncSelect_PinA_Cfg(void);
static void IOMUX_FuncSelect_PinB_Cfg(void);
static void IOMUX_FuncSelect_PinC_Cfg(void);
static void IOMUX_FuncSelect_SDIO_Cfg(void);
static void IOMUX_FuncSelect_FLASHIO_Cfg(void);

/// Analog IOMUX
static void ANA_IOMUX_FuncSelect_PinA_Cfg(void);
static void ANA_IOMUX_FuncSelect_PinB_Cfg(void);
static void ANA_IOMUX_FuncSelect_PinC_Cfg(void);

/// Pin mode configure
static void IOMUX_Mode_PinA_Cfg(void);
static void IOMUX_Mode_PinB_Cfg(void);
static void IOMUX_Mode_PinC_Cfg(void);

/// Pin force set
static void IOMUX_ForceSet_PinA_Cfg(void);
static void IOMUX_ForceSet_PinB_Cfg(void);
static void IOMUX_ForceSet_PinC_Cfg(void);
static void IOMUX_ForceSet_SDIO_Cfg(void);
static void IOMUX_ForceSet_FLASHIO_Cfg(void);


/// Always domain
static void AON_IOMUX_FuncSelect_PinB_Cfg(void);
static void AON_IOMUX_Mode_PinB_Cfg(void);
static void AON_IOMUX_ForceSet_PinB_Cfg(void);

static function test_function_array[] = {
    IOMUX_FuncSelect_PinA_Cfg,
//    IOMUX_FuncSelect_PinB_Cfg,
//	  IOMUX_FuncSelect_PinC_Cfg,
//    ANA_IOMUX_FuncSelect_PinA_Cfg,
//    ANA_IOMUX_FuncSelect_PinB_Cfg,
//	  ANA_IOMUX_FuncSelect_PinC_Cfg,
//    IOMUX_Mode_PinA_Cfg,
//    IOMUX_Mode_PinB_Cfg,
//	  IOMUX_Mode_PinC_Cfg,
//    IOMUX_ForceSet_PinA_Cfg,
//    IOMUX_ForceSet_PinB_Cfg,
//	  IOMUX_ForceSet_PinC_Cfg,
//	  IOMUX_ForceSet_SDIO_Cfg,
//	  IOMUX_ForceSet_FLASHIO_Cfg,
//	  AON_IOMUX_FuncSelect_PinB_Cfg,
//	  AON_IOMUX_Mode_PinB_Cfg,
//	  AON_IOMUX_ForceSet_PinB_Cfg,
};

/// Normal IOMUX
static void IOMUX_FuncSelect_PinA_Cfg(void){
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_ALTER13);
}

static void IOMUX_FuncSelect_PinB_Cfg(void){
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_ALTER13);
}

static void IOMUX_FuncSelect_PinC_Cfg(void) {
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, CSK_IOMUX_FUNC_ALTER13);
}

static void IOMUX_FuncSelect_SDIO_Cfg(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_SDIO, 0, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_SDIO, 1, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_SDIO, 2, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_SDIO, 3, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_SDIO, 4, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_SDIO, 5, CSK_IOMUX_FUNC_ALTER1);

}

static void IOMUX_FuncSelect_FLASHIO_Cfg(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_FLASHIO, 0, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_FLASHIO, 1, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_FLASHIO, 2, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_FLASHIO, 3, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_FLASHIO, 4, CSK_IOMUX_FUNC_ALTER1);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_FLASHIO, 5, CSK_IOMUX_FUNC_ALTER1);
}

/// Analog IOMUX
static void ANA_IOMUX_FuncSelect_PinA_Cfg(void){
	ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_ANA_IOMUX_FUNC_DEFAULT);
	ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 27, CSK_ANA_IOMUX_FUNC_DEFAULT);
}

static void ANA_IOMUX_FuncSelect_PinB_Cfg(void){
	ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, CSK_ANA_IOMUX_FUNC_ALTER1);
	ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, CSK_ANA_IOMUX_FUNC_ALTER1);
}

static void ANA_IOMUX_FuncSelect_PinC_Cfg(void){
	ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 0, CSK_ANA_IOMUX_FUNC_DEFAULT);
	ANA_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 1, CSK_ANA_IOMUX_FUNC_DEFAULT);
}

/// Pin mode configure
static void IOMUX_Mode_PinA_Cfg(void){
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_PULLUP_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_PULLDOWN_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_NONE_MODE);
}

static void IOMUX_Mode_PinB_Cfg(void){
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_PULLUP_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_PULLDOWN_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_NONE_MODE);
}

static void IOMUX_Mode_PinC_Cfg(void){
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, HAL_IOMUX_PULLUP_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, HAL_IOMUX_PULLDOWN_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, HAL_IOMUX_NONE_MODE);
}

/// Pin force set
static void IOMUX_ForceSet_PinA_Cfg(void){
	IOMuxManager_PinForce(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_FORCE_OUT_HIGH);
}

static void IOMUX_ForceSet_PinB_Cfg(void){
	IOMuxManager_PinForce(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_FORCE_OUT_HIGH);
}

static void IOMUX_ForceSet_PinC_Cfg(void){
	IOMuxManager_PinForce(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_C, IOMUX_IOC_TEST_PIN, HAL_IOMUX_FORCE_OUT_HIGH);
}

static void IOMUX_ForceSet_SDIO_Cfg(void){
	IOMuxManager_PinForce(CSK_IOMUX_PAD_SDIO, 0, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_SDIO, 1, HAL_IOMUX_FORCE_OUT_HIGH);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_SDIO, 2, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_SDIO, 3, HAL_IOMUX_FORCE_OUT_HIGH);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_SDIO, 4, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_SDIO, 5, HAL_IOMUX_FORCE_OUT_HIGH);
}

static void IOMUX_ForceSet_FLASHIO_Cfg(void){
	IOMuxManager_PinForce(CSK_IOMUX_PAD_FLASHIO, 0, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_FLASHIO, 1, HAL_IOMUX_FORCE_OUT_HIGH);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_FLASHIO, 2, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_FLASHIO, 3, HAL_IOMUX_FORCE_OUT_HIGH);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_FLASHIO, 4, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_FLASHIO, 5, HAL_IOMUX_FORCE_OUT_HIGH);
}

/// Always domain
static void AON_IOMUX_FuncSelect_PinB_Cfg(void){
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_AON_IOMUX_FUNC_ALTER3);
}

static void AON_IOMUX_Mode_PinB_Cfg(void) {
	AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_PULLUP_MODE);
	AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_PULLDOWN_MODE);
	AON_IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_NONE_MODE);
}

static void AON_IOMUX_ForceSet_PinB_Cfg(void) {
	AON_IOMuxManager_PinForce(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_FORCE_OUT_LOW);
	AON_IOMuxManager_PinForce(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_FORCE_OUT_HIGH);
}

int main(){
    uint32_t times;

    logInit(0, 115200);
    CLOGD("IOMux None OS Demo");

    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}
