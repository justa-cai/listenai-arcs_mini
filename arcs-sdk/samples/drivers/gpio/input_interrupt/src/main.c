#include <stdio.h>

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "FreeRTOS.h"
#include "task.h"

static void* GPIOA_Handler = NULL;
static volatile uint32_t GPIOA_Event = 0;

static void GPIOA_EventCallback_Negative(uint32_t event, void* workspace){
    printf("Trigger GPIOA Negative interrupt, event: 0x%x\n", event);
    GPIO_Control(GPIOA_Handler, CSK_GPIO_INTR_DISABLE, CSK_GPIO_PIN20);
    GPIOA_Event |= event;
}

void gpio_interrupt(void)
{
    /* 设置PA20和PA21引脚为GPIO，具体IOMUX列表见芯片手册的APPENDIX章节 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_Negative, NULL);

    /* 设置PA21为输出引脚, 并输出高电平 */
    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN21, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 1);

    /* 设置PA20为输入引脚 */
    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN20, CSK_GPIO_DIR_INPUT);

    /* 设置PA20为下降沿触发中断 */
    GPIO_Control(GPIOA_Handler,  \
        CSK_GPIO_DEBOUNCE_DISABLE | \
        CSK_GPIO_SET_INTR_NEGATIVE_EDGE | \
        CSK_GPIO_INTR_ENABLE, CSK_GPIO_PIN20);

    vTaskDelay(pdMS_TO_TICKS(1000));

    /* 开始触发中断 */
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 0); 

    /* 等待中断事件 */
    while(!(GPIOA_Event & CSK_GPIO_PIN20));
    GPIOA_Event = 0;
    printf("[GPIOA INT] PASS\n");

    GPIO_Uninitialize(GPIOA_Handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! \n");
    
    GPIOA_Handler = GPIOA();

    gpio_interrupt();

    return 0;
}
