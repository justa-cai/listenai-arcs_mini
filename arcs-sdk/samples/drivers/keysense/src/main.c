#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "arcs_ap.h"
#include "IOMuxManager.h"
#include "Driver_KEYSENSE.h"
#include "Driver_GPADC.h"

#include "FreeRTOS.h"
#include "task.h"

#define KEYSENSE0_PIN_NUM   2   // GPIOB_02
#define KEYSENSE1_PIN_NUM   3   // GPIOB_03

static void *keysense_handler = NULL;

static void keysense_wakeup_event(void* param){
    printf("keysense wakeup event generate\n");
    HAL_KEYSENSE_InterruptDisable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP);
}

static void keysense_adctrigger_event(void* param){
    printf("keysense adctrigger event generate\n");
    HAL_KEYSENSE_InterruptDisable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
}

static void keysense_release_event(void* param){
    printf("keysense release event generate\n");
    /* 关闭按键release的中断 */
    HAL_KEYSENSE_InterruptDisable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
}

static void keysense_press_event(void* param){
    printf("keysense press event generate\n");

    /* key press事件触发后，可以通过ADC读取keysense0的电压,来区分和识别外部不同的按键，下面仅做示例 */
    HAL_GPADC_Start(GPADC());
    HAL_GPADC_PollForConversion(GPADC(), 0);
    uint32_t adc_value = HAL_GPADC_GetValue(GPADC(),CSK_GPADC_CHANNEL_SEL_KEYSENSE0);
    printf("channel type id %d, adc value 0x%lx/%dmV\n", CSK_GPADC_KEYSENSE0, adc_value, (uint16_t)(adc_value*1000.0/1024*1.2));

    /* 关闭按键press的中断 */
    HAL_KEYSENSE_InterruptDisable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_PRESS);
}

static void keysense_test(void)
{

    keysense_handler = KEYSENSE0();

    /* PB2复用为Keysense0 */
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, KEYSENSE0_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = CSK_ANA_IOMUX_FUNC_ALTER5;

    /* 初始化keysense0 */
    HAL_KEYSENSE_Initialize(keysense_handler);

    /* 设置阈值 */
    HAL_KEYSENSE_Control(keysense_handler, CSK_KEYSENSE_THD);
    
    // HAL_KEYSENSE_RegisterCallback(keysense_handler, CSK_KEYSENSE_WAKEUP, keysense_wakeup_event);
    // HAL_KEYSENSE_RegisterCallback(keysense_handler, CSK_KEYSENSE_ADCTRIG, keysense_adctrigger_event);
    // HAL_KEYSENSE_InterruptEnable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP);
    // HAL_KEYSENSE_InterruptEnable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);

    /* 注册按键release和press的事件回调 */
    HAL_KEYSENSE_RegisterCallback(keysense_handler, CSK_KEYSENSE_RELEASE, keysense_release_event);
    HAL_KEYSENSE_RegisterCallback(keysense_handler, CSK_KEYSENSE_PRESS, keysense_press_event);

    /* 使能按键release和press的中断 */
    HAL_KEYSENSE_InterruptEnable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
    HAL_KEYSENSE_InterruptEnable(keysense_handler, CSK_KEYSENSE_INTERRUPT_MODE_PRESS);


    /* 初始化GPADC */
    HAL_GPADC_Initialize(GPADC());

    /* 设置ADC的KEYSENSE0采样通道 */
    HAL_GPADC_Control(GPADC(), CSK_GPADC_CHANNEL_SEL_KEYSENSE0 | CSK_GPADC_DMA_ENABLE(0)); 
    
    HAL_GPADC_SetVrefSel(GPADC(), 0);
    HAL_GPADC_SetVinBuf_Enable(GPADC(), 0);


    /* 使能keysense0 */
    HAL_KEYSENSE_Enable(keysense_handler);
    
    while(1){
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* 关闭keysense */
    HAL_KEYSENSE_Uninitialize(keysense_handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! KEYSENSE\n");

    keysense_test();

    return 0;
}