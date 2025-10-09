#include <stdio.h>

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "FreeRTOS.h"
#include "task.h"

static void* GPIOA_Handler = NULL;

void gpio_input(void)
{
    uint32_t value;

    printf("gpio input enter...\n");

    /* 设置PA20和PA21引脚为GPIO，具体IOMUX列表见芯片手册的APPENDIX章节 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(GPIOA_Handler, NULL, NULL);

    /* 设置PA20为输入引脚，PA21为输出引脚 */
    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN20, CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN21, CSK_GPIO_DIR_OUTPUT);

    while(1) {
        // PA21 -> PA20 value = 1
        GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 1);
        value = GPIO_PinRead(GPIOA_Handler, CSK_GPIO_PIN20);
        printf("PA21 -> PA20 value = %d\n", value);
    
        vTaskDelay(pdMS_TO_TICKS(1000));
    
        // PA21 -> PA20 value = 0
        GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 0);
        value = GPIO_PinRead(GPIOA_Handler, CSK_GPIO_PIN20);
        printf("PA21 -> PA20 value = %d\n", value);
    
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    GPIO_Uninitialize(GPIOA_Handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! \n");
    
    GPIOA_Handler = GPIOA();

    gpio_input();

    return 0;
}
