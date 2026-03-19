#include <stdio.h>

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
/* ARCS_MINI: 使用 LED_PIN (PB1) 作为输出示例 */
#define GPIO_HANDLER        GPIOB()
#define GPIO_PAD            CSK_IOMUX_PAD_B
#define GPIO_PIN_NUM        LED_PIN
#define GPIO_PIN_MASK       (1 << LED_PIN)
#define GPIO_PIN_LABEL      "PB1 (LED)"
#else
#define GPIO_HANDLER        GPIOA()
#define GPIO_PAD            CSK_IOMUX_PAD_A
#define GPIO_PIN_NUM        20
#define GPIO_PIN_MASK       CSK_GPIO_PIN20
#define GPIO_PIN_LABEL      "PA20"
#endif

void gpio_output(void *handler)
{
    _GPIO_ *status;
    uint32_t size;

    /* 设置引脚为GPIO，具体IOMUX列表见芯片手册的APPENDIX章节 */
    IOMuxManager_PinConfigure(GPIO_PAD, GPIO_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);

    /* 初始化GPIO外设，包含使能时钟，注册中断回调，使能中断等 */
    GPIO_Initialize(handler, NULL, NULL);

    /* 获取所有引脚的状态，包含方向，上下拉模式、中断模式等配置 */
    GPIO_Status(handler, &status, &size);
    printf("%s direction: %d\n", GPIO_PIN_LABEL, status[GPIO_PIN_NUM].dir);

    GPIO_Control(handler, CSK_GPIO_DEBOUNCE_DISABLE, GPIO_PIN_MASK);

    /* 设置为输出引脚 */
    GPIO_SetDir(handler, GPIO_PIN_MASK, CSK_GPIO_DIR_OUTPUT);
    GPIO_Status(handler, &status, &size);
    printf("%s direction: %d\n", GPIO_PIN_LABEL, status[GPIO_PIN_NUM].dir);

    while(1) {
        GPIO_PinWrite(handler, GPIO_PIN_MASK, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    
        GPIO_PinWrite(handler, GPIO_PIN_MASK, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    GPIO_Uninitialize(handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! \n");

    gpio_output(GPIO_HANDLER);

    return 0;
}
