#include <stdio.h>

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "FreeRTOS.h"
#include "task.h"

static void* GPIOA_Handler = NULL;

void gpio_output(void)
{
    _GPIO_ *status;
    uint32_t size;

    /* 设置PA20引脚为GPIO，具体IOMUX列表见芯片手册的APPENDIX章节 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);

    /* 初始化GPIOA外设，包含使能GPIOA的时钟，注册GPIOA的中断回调，使能GPIOA的中断等 */
    GPIO_Initialize(GPIOA_Handler, NULL, NULL);

    /* 获取GPIOA所有引脚(GPIOA共32个引脚)的状态，包含方向，上下拉模式、中断模式等配置 */
    GPIO_Status(GPIOA_Handler, &status, &size);
    printf("PA20 direction: %d\n", status[20].dir);

    /* 设置PA20引脚不使能消抖功能 
     * 此函数主要功能如下
     *  - 设置GPIO引脚的消抖功能
     *  - 设置GPIO输入引脚的中断模式
     *  - 设置GPIO引脚上拉/下拉模式
     */
    GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN20);

    /* 设置PA20为输出引脚 */
    GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN20, CSK_GPIO_DIR_OUTPUT);
    GPIO_Status(GPIOA_Handler, &status, &size);
    printf("PA20 direction: %d\n", status[20].dir);

    while(1) {
        /* PA20引脚输出高电平 */
        GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN20, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    
        /* PA20引脚输出低电平 */
        GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN20, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* GPIOA外设逆初始化，包含关闭GPIOA的时钟，关闭GPIOA的中断等 */
    GPIO_Uninitialize(GPIOA_Handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! \n");
    
    GPIOA_Handler = GPIOA();

    gpio_output();

    return 0;
}
