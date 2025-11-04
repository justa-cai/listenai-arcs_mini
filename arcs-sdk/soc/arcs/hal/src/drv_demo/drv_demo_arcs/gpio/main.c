/* Standard includes. */
#include <stdio.h>
#include <string.h>
#include "log_print.h"
#include "chip.h"

#include "IOMuxManager.h"
#include "systick.h"
#include "Driver_GPIO.h"

typedef void (*function)(void);

static void GPIOA_PIN0_OUT(void);
static void GPIOA_PIN1_OUT(void);
static void GPIOA_PIN2_OUT(void);
static void GPIOA_PIN3_OUT(void);
static void GPIOA_PIN4_OUT(void);
static void GPIOA_PIN5_OUT(void);
static void GPIOA_PIN6_OUT(void);
static void GPIOA_PIN7_OUT(void);
static void GPIOA_PIN8_OUT(void);
static void GPIOA_PIN9_OUT(void);
static void GPIOA_PIN10_OUT(void);
static void GPIOA_PIN11_OUT(void);
static void GPIOA_PIN12_OUT(void);
static void GPIOA_PIN13_OUT(void);
static void GPIOA_PIN14_OUT(void);
static void GPIOA_PIN15_OUT(void);
static void GPIOA_PIN16_OUT(void);
static void GPIOA_PIN17_OUT(void);
static void GPIOA_PIN18_OUT(void);
static void GPIOA_PIN19_OUT(void);
static void GPIOA_PIN20_OUT(void);
static void GPIOA_PIN21_OUT(void);
static void GPIOA_PIN22_OUT(void);
static void GPIOA_PIN23_OUT(void);
static void GPIOA_PIN24_OUT(void);
static void GPIOA_PIN25_OUT(void);
static void GPIOA_PIN26_OUT(void);
static void GPIOA_PIN27_OUT(void);
static void GPIOA_PIN28_OUT(void);
static void GPIOA_PIN29_OUT(void);
static void GPIOA_PIN30_OUT(void);
static void GPIOA_PIN31_OUT(void);

static void GPIOB_PIN0_OUT(void);
static void GPIOB_PIN1_OUT(void);
static void GPIOB_PIN2_OUT(void);
static void GPIOB_PIN3_OUT(void);
static void GPIOB_PIN4_OUT(void);
static void GPIOB_PIN5_OUT(void);
static void GPIOB_PIN6_OUT(void);
static void GPIOB_PIN7_OUT(void);
static void GPIOB_PIN8_OUT(void);
static void GPIOB_PIN9_OUT(void);

static void GPIOA_PIN_STATUS(void);
static void GPIOB_PIN_STATUS(void);

static void GPIOA_X_TO_GPIOA_Y_IN(void);
static void GPIOA_X_HIGH_LEVEL_INTERRUPT(void);
static void GPIOA_X_LOW_LEVEL_INTERRUPT(void);
static void GPIOA_X_NAGETIVE_EDGE_INTERRUPT(void);
static void GPIOA_X_POSITIVE_EDGE_INTERRUPT(void);
static void GPIOA_X_DUAL_EDGE_INTERRUPT(void);
static void GPIOA_X_DEBOUNCE();

static void GPIOB_X_TO_GPIOA_Y_IN(void);
static void GPIOB_X_HIGH_LEVEL_INTERRUPT(void);
static void GPIOB_X_LOW_LEVEL_INTERRUPT(void);
static void GPIOB_X_NEGATIVE_EDGE_INTERRUPT(void);
static void GPIOB_X_POSITIVE_EDGE_INTERRUPT(void);
static void GPIOB_X_DUAL_EDGE_INTERRUPT(void);
static void GPIOB_X_DEBOUNCE(void);

static void* GPIOA_Handler = NULL;
static void* GPIOB_Handler = NULL;

static volatile uint32_t GPIOA_Event = 0;
static volatile uint32_t GPIOB_Event = 0;
static volatile uint32_t trigger_times = 0;

static void GPIO_Init_Handler(void) {
	GPIOA_Handler = GPIOA();
	GPIOB_Handler = GPIOB();
}

static function test_function_array[] = {
    // GPIOA_PIN0_OUT,
    // GPIOA_PIN1_OUT,
    // GPIOA_PIN2_OUT,
    // GPIOA_PIN3_OUT,
    // GPIOA_PIN4_OUT,
    // GPIOA_PIN5_OUT,
    // GPIOA_PIN6_OUT,
    // GPIOA_PIN7_OUT,
    // GPIOA_PIN8_OUT,
    // GPIOA_PIN9_OUT,
    // GPIOA_PIN10_OUT,
    // GPIOA_PIN11_OUT,
    // GPIOA_PIN12_OUT,
    // GPIOA_PIN13_OUT,
    // GPIOA_PIN14_OUT,
    // GPIOA_PIN15_OUT,
    // GPIOA_PIN16_OUT,
    // GPIOA_PIN17_OUT,
    // GPIOA_PIN18_OUT,
    // GPIOA_PIN19_OUT,
    // GPIOA_PIN20_OUT,
    // GPIOA_PIN21_OUT,
    // GPIOA_PIN22_OUT,
    // GPIOA_PIN23_OUT,
    // GPIOA_PIN24_OUT,
    // GPIOA_PIN25_OUT,
    // GPIOA_PIN26_OUT,
    // GPIOA_PIN27_OUT,
    // GPIOA_PIN28_OUT,
    // GPIOA_PIN29_OUT,
    // GPIOA_PIN30_OUT,
    // GPIOA_PIN31_OUT,

    // GPIOB_PIN0_OUT,
    // GPIOB_PIN1_OUT,
    // GPIOB_PIN2_OUT,
    // GPIOB_PIN3_OUT,
    // GPIOB_PIN4_OUT,
    // GPIOB_PIN5_OUT,
    // GPIOB_PIN6_OUT,
    // GPIOB_PIN7_OUT,
    // GPIOB_PIN8_OUT,
    // GPIOB_PIN9_OUT,

    // GPIOA_PIN_STATUS,
    // GPIOB_PIN_STATUS,

    // GPIOA_X_TO_GPIOA_Y_IN,
    // GPIOA_X_HIGH_LEVEL_INTERRUPT,
    // GPIOA_X_LOW_LEVEL_INTERRUPT,
    // GPIOA_X_NAGETIVE_EDGE_INTERRUPT,
    // GPIOA_X_POSITIVE_EDGE_INTERRUPT,
    // GPIOA_X_DUAL_EDGE_INTERRUPT,
    // GPIOA_X_DEBOUNCE,
    // GPIOB_X_TO_GPIOA_Y_IN,
	// GPIOB_X_HIGH_LEVEL_INTERRUPT,
	// GPIOB_X_LOW_LEVEL_INTERRUPT,
	// GPIOB_X_NEGATIVE_EDGE_INTERRUPT,
	// GPIOB_X_POSITIVE_EDGE_INTERRUPT,
	// GPIOB_X_DUAL_EDGE_INTERRUPT,
    // GPIOB_X_DEBOUNCE,
};

static void GPIOA_PIN0_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 0, CSK_IOMUX_FUNC_ALTER1);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN0);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN0, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN0, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN0, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN0, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN1_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 1, CSK_IOMUX_FUNC_ALTER1);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN1);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN1, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN1, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN1, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN1, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}


static void GPIOA_PIN2_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN2);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN2, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN2, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN2, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN2, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN3_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 3, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN3);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN3, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN3, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN3, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN3, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN4_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN4);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN4, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN4, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN4, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN4, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN5_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN5);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN5, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN5, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN5, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN5, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN6_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN6);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN6, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN6, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN6, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN6, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN7_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN7);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN7, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN7, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN7, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN7, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN8_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 8, CSK_IOMUX_FUNC_ALTER1);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN8);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN8, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN8, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN8, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN8, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN9_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 9, CSK_IOMUX_FUNC_ALTER1);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN9);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN9, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN9, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN9, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN9, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN10_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN10);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN10, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN10, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN10, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN10, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN11_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN11);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN11, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN11, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN11, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN11, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN12_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN12);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN12, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN12, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN12, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN12, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN13_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN13);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN13, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN13, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN13, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN13, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN14_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN14);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN14, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN14, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN14, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN14, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN15_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN15);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN15, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN15, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN15, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN15, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN16_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN16);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN16, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN16, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN16, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN16, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN17_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN17);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN17, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN17, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN17, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN17, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN18_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN18);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN18, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN18, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN18, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN18, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN19_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN19);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN19, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN19, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN19, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN19, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN20_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN20);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN20, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN20, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN20, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN20, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN21_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN21);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN21, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN22_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN22);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN22, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN22, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN22, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN22, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN23_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN23);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN23, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN23, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN23, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN23, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN24_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN24);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN24, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN24, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN24, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN24, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN25_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN25);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN25, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN25, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN25, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN25, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN26_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN26);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN26, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN26, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN26, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN26, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN27_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 27, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN27);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN27, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN27, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN27, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN27, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN28_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 28, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN28);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN28, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN28, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN28, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN28, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN29_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 29, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN29);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN29, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN29, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN29, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN29, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN30_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN30);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN30, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN30, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN30, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN30, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_PIN31_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
	GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN31);
	GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN31, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN31, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN31, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN31, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOB_PIN0_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN0);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN0, CSK_GPIO_DIR_OUTPUT);   

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN0, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN0, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN0, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN1_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN1);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN1, CSK_GPIO_DIR_OUTPUT);   

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN1, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN1, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN1, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN2_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN2);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN2, CSK_GPIO_DIR_OUTPUT);   

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN2, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN2, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN2, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN3_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN3);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN3, CSK_GPIO_DIR_OUTPUT);   

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN3, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN3, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN3, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN4_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 4, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN4);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN4, CSK_GPIO_DIR_OUTPUT);   

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN4, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN4, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN4, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN5_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN5);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN5, CSK_GPIO_DIR_OUTPUT);   

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN5, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN5, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN5, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN6_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN6);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN6, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN6, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN6, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN6, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN7_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN7);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN7, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN7, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN7, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN7, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN8_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 8, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN8);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN8, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN8, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN8, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN8, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_PIN9_OUT(void) {
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 9, CSK_IOMUX_FUNC_DEFAULT);
 	GPIO_Initialize(GPIOB_Handler, NULL, NULL);
	GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN9);
	GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN9, CSK_GPIO_DIR_OUTPUT);

    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN9, 1);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN9, 0);
    SysTick_Delay_Ms(100);
    GPIO_PinWrite(GPIOB_Handler, CSK_GPIO_PIN9, 1);
    SysTick_Delay_Ms(100);

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOA_PIN_STATUS(void) {
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, NULL, NULL);
    _GPIO_ *status;
    uint32_t size;

    GPIO_Status(GPIOA_Handler, &status, &size);
    CLOGD("Dir10: %d; Dir11: %d", status[10].dir, status[11].dir);

    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN10, CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN11, CSK_GPIO_DIR_INPUT);

    GPIO_Status(GPIOA_Handler, &status, &size);
    CLOGD("Dir10: %d; Dir11: %d", status[10].dir, status[11].dir);

    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN10, CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN11, CSK_GPIO_DIR_OUTPUT);

    GPIO_Status(GPIOA_Handler, &status, &size);
    CLOGD("Dir10: %d; Dir11: %d", status[10].dir, status[11].dir);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOB_PIN_STATUS(void) {
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 00, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 01, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, NULL, NULL);
    _GPIO_ *status;
    uint32_t size;

    GPIO_Status(GPIOB_Handler, &status, &size);
    CLOGD("Dir00: %d; Dir01: %d", status[0].dir, status[1].dir);

    GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN0, CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN1, CSK_GPIO_DIR_INPUT);

    GPIO_Status(GPIOB_Handler, &status, &size);
    CLOGD("Dir00: %d; Dir01: %d", status[0].dir, status[1].dir);

    GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN0, CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(GPIOB_Handler, CSK_GPIO_PIN1, CSK_GPIO_DIR_OUTPUT);

    GPIO_Status(GPIOB_Handler, &status, &size);
    CLOGD("Dir00: %d; Dir01: %d", status[0].dir, status[1].dir);

    GPIO_Uninitialize(GPIOB_Handler);
}

#define GPIOA_INT_SOUR_PIN	(10)
#define GPIOA_INT_DEST_PIN	(11)

static void GPIOA_X_TO_GPIOA_Y_IN(void) {
    uint32_t value;
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, NULL, NULL);

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    // A10 -> A11 value = 1
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);
    value = GPIO_PinRead(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN));
    CLOGD("A10 -> A11 value = %d", value);

    // A10 -> A11 value = 0
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);
    value = GPIO_PinRead(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN));
    CLOGD("A10 -> A11 value = %d", value);

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_OUTPUT);

    // A11 -> A10 value = 1
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), 1);
    value = GPIO_PinRead(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN));
    CLOGD("A11 -> A10 value = %d", value);

    // A11 -> A10 value = 0
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), 0);
    value = GPIO_PinRead(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN));
    CLOGD("A11 -> A10 value = %d", value);

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_EventCallback_High(uint32_t event, void* workspace) {
	CLOGD("Trigger GPIOA High Interrupt");
	GPIO_Control(GPIOA_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOA_INT_DEST_PIN));
	GPIOA_Event |= event;
}

static void GPIOA_X_HIGH_LEVEL_INTERRUPT(void) {
    CLOGD("[GPIOA INTERRUPT] HIGH LEVEL MODE");

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);

    GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_High, NULL);
    GPIO_Control(GPIOA_Handler, \
                            CSK_GPIO_DEBOUNCE_DISABLE | \
							CSK_GPIO_SET_INTR_HIGH_LEVEL | \
                            CSK_GPIO_INTR_ENABLE, (1UL << GPIOA_INT_DEST_PIN));

	GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);
    while(!(GPIOA_Event & (1UL << GPIOA_INT_DEST_PIN)));
    GPIOA_Event = 0;
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);
    CLOGD("[GPIOA INT] PASS");

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_EventCallback_Low(uint32_t event, void* workspace) {
    CLOGD("Trigger GPIOA Low interrupt");
    GPIO_Control(GPIOA_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOA_INT_DEST_PIN));
    GPIOA_Event |= event;
}

static void GPIOA_X_LOW_LEVEL_INTERRUPT(void) {
    CLOGD("[GPIOA INTERRUPT] LOW LEVEL MODE");

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_Low, NULL);
    GPIO_Control(GPIOA_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_LOW_LEVEL | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOA_INT_DEST_PIN));

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);
    while(!(GPIOA_Event & (1UL << GPIOA_INT_DEST_PIN)));
    GPIOA_Event = 0;
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);
    CLOGD("[GPIOA INT] PASS");

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_EventCallback_Negative(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOA Negative interrupt");
    GPIO_Control(GPIOA_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOA_INT_DEST_PIN));
    GPIOA_Event |= event;
}

static void GPIOA_X_NAGETIVE_EDGE_INTERRUPT(void) {
    CLOGD("[GPIOA INTERRUPT] NEGATIVE EDGE MODE");

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_Negative, NULL);

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);

    GPIO_Control(GPIOA_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_NEGATIVE_EDGE | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOA_INT_DEST_PIN));

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);   
    while(!(GPIOA_Event & (1UL << GPIOA_INT_DEST_PIN)));
    GPIOA_Event = 0;
    CLOGD("[GPIOA INT] PASS");

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_EventCallback_Positive(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOA Positive interrupt");
    GPIO_Control(GPIOA_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOA_INT_DEST_PIN));
    GPIOA_Event |= event;
}

static void GPIOA_X_POSITIVE_EDGE_INTERRUPT(void) {
    CLOGD("[GPIOA INTERRUPT] POSITIVE EDGE MODE");

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_Positive, NULL);

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);

    GPIO_Control(GPIOA_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_POSITIVE_EDGE | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOA_INT_DEST_PIN));

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);

    while(!(GPIOA_Event & (1UL << GPIOA_INT_DEST_PIN)));
    GPIOA_Event = 0;
    CLOGD("[GPIOA INT] PASS");

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_EventCallback_Dual(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOA Dual edge interrupt");
    GPIOA_Event |= event;
}

static void GPIOA_X_DUAL_EDGE_INTERRUPT(void) {
    CLOGD("[GPIOA INTERRUPT] DUAL EDGE MODE");

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_Dual, NULL);

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);

    GPIO_Control(GPIOA_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_DUAL_EDGE | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOA_INT_DEST_PIN));

    GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);

    while(!(GPIOA_Event & (1UL << GPIOA_INT_DEST_PIN)));
    GPIOA_Event = 0;
    CLOGD("[GPIOA INT] PASS");

    GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);

    while(!(GPIOA_Event & (1UL << GPIOA_INT_DEST_PIN)));
    GPIOA_Event = 0;
    CLOGD("[GPIOA INT] PASS");

    GPIO_Uninitialize(GPIOA_Handler);
}

static void GPIOA_EventCallback_Debounce(uint32_t event, void* workspace) {
    CLOGD("Trigger GPIOA Debounce interrupt");
    trigger_times++;
    GPIOA_Event |= event;
}

static void GPIOA_X_DEBOUNCE() {
    while (1)
    {
        CLOGD("[GPIOA DEBOUNCE]");

        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, GPIOA_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

        GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_Debounce, NULL);

        GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
        GPIO_SetDir(GPIOA_Handler, (1UL << GPIOA_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

        GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);

        GPIO_Control(GPIOA_Handler,  \
                                    CSK_GPIO_DEBOUNCE_ENABLE | \
                                    CSK_GPIO_DEBOUNCE_CLK_PCLK | \
                                    CSK_GPIO_INTR_ENABLE | \
                                    CSK_GPIO_SET_INTR_NEGATIVE_EDGE, (1UL << GPIOA_INT_DEST_PIN));

        GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_SCALE, 0xff);
        CLOGD("[TRIGGER TIMES]: %d", trigger_times);
        GPIOA_Event = 0;

        uint32_t i = 0;
        for(i = 0; i < 10; i++){
            GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);
            GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 0);
        }
        GPIO_PinWrite(GPIOA_Handler, (1UL << GPIOA_INT_SOUR_PIN), 1);

        if(GPIOA_Event != 0){
            CLOGD("DEBOUNCE ERROR");
        } else {
            CLOGD("DEBOUNCE PASS");
        }

        CLOGD("[TRIGGER TIMES]: %d", trigger_times);
    }
}

#define GPIOB_INT_SOUR_PIN          (0)
#define GPIOB_INT_DEST_PIN          (1)

static void GPIOB_X_TO_GPIOA_Y_IN(void) {
    uint32_t value = 0;

	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, NULL, NULL);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    // B0 -> B1 value = 1
	GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);
	value = GPIO_PinRead(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN));
    CLOGD("B0 -> B1 value = %d", value);

    // B0 -> B1 value = 0
	GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);
	value = GPIO_PinRead(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN));
    CLOGD("B0 -> B1 value = %d", value);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_INPUT);
	GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_OUTPUT);

    // B1 -> B0 value = 1
	GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), 1);
	value = GPIO_PinRead(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN));
    CLOGD("B1 -> B0 value = %d", value);

	// B1 -> B0 value = 0
	GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), 0);
	value = GPIO_PinRead(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN));
    CLOGD("B1 -> B0 value = %d", value);

	GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_EventCallback_High(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOB High interrupt");
    GPIO_Control(GPIOB_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOB_INT_DEST_PIN));
    GPIOB_Event |= event;
}

static void GPIOB_X_HIGH_LEVEL_INTERRUPT(void) {
    CLOGD("[GPIOB INT] HIGH LEVEL MODE");

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, GPIOB_EventCallback_High, NULL);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);

    GPIO_Control(GPIOB_Handler,  \
                            CSK_GPIO_DEBOUNCE_DISABLE | \
                            CSK_GPIO_SET_INTR_HIGH_LEVEL | \
                            CSK_GPIO_INTR_ENABLE, (1UL << GPIOB_INT_DEST_PIN));

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);
    while(!(GPIOB_Event & (1UL << GPIOB_INT_DEST_PIN)));
    GPIOB_Event = 0;
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);

    CLOGD("[GPIOB INT] PASS");
    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_EventCallback_Low(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOB Low interrupt");
    GPIO_Control(GPIOB_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOB_INT_DEST_PIN));
    GPIOB_Event |= event;
}

static void GPIOB_X_LOW_LEVEL_INTERRUPT(void){
    CLOGD("[GPIOB INT] LOW LEVEL MODE");

	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, GPIOB_EventCallback_Low, NULL);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);

    GPIO_Control(GPIOB_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_LOW_LEVEL | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOB_INT_DEST_PIN));

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);
    while(!(GPIOB_Event & (1UL << GPIOB_INT_DEST_PIN)));
    GPIOB_Event = 0;
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);

    CLOGD("[GPIOB INT] PASS");
    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_EventCallback_Negative(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOB Negative interrupt");
    GPIO_Control(GPIOB_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOB_INT_DEST_PIN));
    GPIOB_Event |= event;
}

static void GPIOB_X_NEGATIVE_EDGE_INTERRUPT(void){
    CLOGD("[GPIOB INT] NEGATIVE LEVEL MODE");

	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, GPIOB_EventCallback_Negative, NULL);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);

    GPIO_Control(GPIOB_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_NEGATIVE_EDGE | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOB_INT_DEST_PIN));

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);
    while(!(GPIOB_Event & (1UL << GPIOB_INT_DEST_PIN)));
    GPIOB_Event = 0;
    CLOGD("[GPIOB INT] PASS");

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_EventCallback_Positive(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOB Positive interrupt");
    GPIO_Control(GPIOB_Handler, CSK_GPIO_INTR_DISABLE, (1UL << GPIOB_INT_DEST_PIN));
    GPIOB_Event |= event;
}

static void GPIOB_X_POSITIVE_EDGE_INTERRUPT(void){
    CLOGD("[GPIOB INT] POSITIVE LEVEL MODE");

	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, GPIOB_EventCallback_Positive, NULL);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);

    GPIO_Control(GPIOB_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_POSITIVE_EDGE | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOB_INT_DEST_PIN));

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);
    while(!(GPIOB_Event & (1UL << GPIOB_INT_DEST_PIN)));
    GPIOB_Event = 0;
    CLOGD("[GPIOB INT] PASS");

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_EventCallback_Dual(uint32_t event, void* workspace){
    CLOGD("Trigger GPIOB Dual edge interrupt");
    GPIOB_Event |= event;
}

static void GPIOB_X_DUAL_EDGE_INTERRUPT(void){
    CLOGD("[GPIOB INT] DUAL EDGE MODE");

	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, GPIOB_EventCallback_Dual, NULL);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);

    GPIO_Control(GPIOB_Handler,  \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_DUAL_EDGE | \
                                CSK_GPIO_INTR_ENABLE, (1UL << GPIOB_INT_DEST_PIN));

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);
    while(!(GPIOB_Event & (1UL << GPIOB_INT_DEST_PIN)));
    GPIOB_Event = 0;
    CLOGD("[GPIOB INT] PASS");

    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);
    while(!(GPIOB_Event & (1UL << GPIOB_INT_DEST_PIN)));
    GPIOB_Event = 0;
    CLOGD("[GPIOB INT] PASS");

    GPIO_Uninitialize(GPIOB_Handler);
}

static void GPIOB_EventCallback_Debounce(uint32_t event, void* workspace) {
    CLOGD("Trigger GPIOB Debounce interrupt");
    trigger_times++;
    GPIOB_Event |= event;
}

static void GPIOB_X_DEBOUNCE(void) {
    CLOGD("[GPIOB DEBOUNCE]");

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_DEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, GPIOB_INT_SOUR_PIN, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOB_Handler, GPIOB_EventCallback_Debounce, NULL);

    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(GPIOB_Handler, (1UL << GPIOB_INT_DEST_PIN), CSK_GPIO_DIR_INPUT);

    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);

    GPIO_Control(GPIOB_Handler,  \
                                CSK_GPIO_DEBOUNCE_ENABLE | \
                                CSK_GPIO_DEBOUNCE_CLK_PCLK | \
                                CSK_GPIO_INTR_ENABLE | \
                                CSK_GPIO_SET_INTR_NEGATIVE_EDGE, (1UL << GPIOB_INT_DEST_PIN));

    GPIO_Control(GPIOB_Handler, CSK_GPIO_DEBOUNCE_SCALE, 0xff);
    CLOGD("[TRIGGER TIMES]: %d", trigger_times);
    GPIOB_Event = 0;

    uint32_t i = 0;
    for(i = 0; i < 3; i++) {
        GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);
        GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 0);
    }
    GPIO_PinWrite(GPIOB_Handler, (1UL << GPIOB_INT_SOUR_PIN), 1);

    if(GPIOB_Event != 0){
        CLOGD("DEBOUNCE ERROR");
    } else {
        CLOGD("DEBOUNCE PASS");
    }

    CLOGD("[TRIGGER TIMES]: %d", trigger_times);
}

int main( void )
{
	uint32_t times;
	 logInit(0, 115200);
     CLOGD("[ArcsC] GPIO VALIDATION\r\n");

    enable_GINT();

    GPIO_Init_Handler();
    for(times = 0; times < sizeof(test_function_array) / sizeof(test_function_array[0]); times++) {
        test_function_array[times]();
    }

    return 0;
}
