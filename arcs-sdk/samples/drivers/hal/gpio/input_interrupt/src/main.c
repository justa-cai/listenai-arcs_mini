#include <stdio.h>

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
#define GPIO_HANDLER()      GPIOB()
#define GPIO_PIN_MASK       (1 << POWER_KEY_PIN)
#define GPIO_PIN_LABEL      "POWER_KEY (PB4)"
#else
#define GPIO_HANDLER()      GPIOA()
#define GPIO_PIN_MASK       CSK_GPIO_PIN20
#define GPIO_PIN_LABEL      "PA20"
#endif

static void *gpio_handler = NULL;
static volatile uint32_t gpio_event = 0;

static void gpio_event_callback(uint32_t event, void *workspace)
{
    printf("Trigger interrupt, event: 0x%x\n", event);
    GPIO_Control(gpio_handler, CSK_GPIO_INTR_DISABLE, GPIO_PIN_MASK);
    gpio_event |= event;
}

void gpio_interrupt(void)
{
    gpio_handler = GPIO_HANDLER();

#ifndef CONFIG_BOARD_ARCS_MINI
    /* EVB: PA20 输入, PA21 输出回环 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_DEFAULT);
#endif

    GPIO_Initialize(gpio_handler, gpio_event_callback, NULL);

#ifndef CONFIG_BOARD_ARCS_MINI
    /* EVB: PA21 输出高电平 */
    GPIO_SetDir(gpio_handler, CSK_GPIO_PIN21, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpio_handler, CSK_GPIO_PIN21, 1);
#endif

    /* 设置输入引脚 */
    GPIO_SetDir(gpio_handler, GPIO_PIN_MASK, CSK_GPIO_DIR_INPUT);

    /* 设置下降沿触发中断 */
    GPIO_Control(gpio_handler,
        CSK_GPIO_DEBOUNCE_DISABLE |
        CSK_GPIO_SET_INTR_NEGATIVE_EDGE |
        CSK_GPIO_INTR_ENABLE, GPIO_PIN_MASK);

    printf("Waiting for %s falling edge...\n", GPIO_PIN_LABEL);

#ifndef CONFIG_BOARD_ARCS_MINI
    vTaskDelay(pdMS_TO_TICKS(1000));
    /* EVB: 软件触发下降沿 */
    GPIO_PinWrite(gpio_handler, CSK_GPIO_PIN21, 0);
#endif

    /* 等待中断事件 */
    while (!(gpio_event & GPIO_PIN_MASK));
    gpio_event = 0;
    printf("[GPIO INT] PASS\n");

    GPIO_Uninitialize(gpio_handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! \n");

    gpio_interrupt();

    return 0;
}
