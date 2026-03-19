#include <stdio.h>

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
/*
 * ARCS_MINI: 使用 POWER_KEY_PIN (PB4) 读取物理按键状态
 * 按键按下为低电平，松开为高电平
 */
void gpio_input(void)
{
    uint32_t value;
    uint32_t pin_mask = (1 << POWER_KEY_PIN);
    void *gpiob = GPIOB();

    printf("gpio input enter (POWER_KEY @ PB%d)...\n", POWER_KEY_PIN);

    GPIO_Initialize(gpiob, NULL, NULL);
    GPIO_Control(gpiob, CSK_GPIO_DEBOUNCE_DISABLE, pin_mask);
    GPIO_SetDir(gpiob, pin_mask, CSK_GPIO_DIR_INPUT);

    while (1) {
        value = GPIO_PinRead(gpiob, pin_mask);
        printf("POWER_KEY (PB%d) = %d\n", POWER_KEY_PIN, value);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    GPIO_Uninitialize(gpiob);
}
#else
void gpio_input(void)
{
    uint32_t value;
    void *gpioa = GPIOA();

    printf("gpio input enter...\n");

    /* 设置PA20和PA21引脚为GPIO，具体IOMUX列表见芯片手册的APPENDIX章节 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_DEFAULT);

    GPIO_Initialize(gpioa, NULL, NULL);

    /* 设置PA20为输入引脚，PA21为输出引脚 */
    GPIO_SetDir(gpioa, CSK_GPIO_PIN20, CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(gpioa, CSK_GPIO_PIN21, CSK_GPIO_DIR_OUTPUT);

    while(1) {
        // PA21 -> PA20 value = 1
        GPIO_PinWrite(gpioa, CSK_GPIO_PIN21, 1);
        value = GPIO_PinRead(gpioa, CSK_GPIO_PIN20);
        printf("PA21 -> PA20 value = %d\n", value);
    
        vTaskDelay(pdMS_TO_TICKS(1000));
    
        // PA21 -> PA20 value = 0
        GPIO_PinWrite(gpioa, CSK_GPIO_PIN21, 0);
        value = GPIO_PinRead(gpioa, CSK_GPIO_PIN20);
        printf("PA21 -> PA20 value = %d\n", value);
    
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    GPIO_Uninitialize(gpioa);
}
#endif

int main(int argc, char **argv)
{
    printf("Hello, world! \n");

    gpio_input();

    return 0;
}
