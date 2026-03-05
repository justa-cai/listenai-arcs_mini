#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "Driver_WDT.h"

#include "FreeRTOS.h"
#include "task.h"

static void WDT_Callback_hook(void* workspace){
	// printf("Resetting\n");
}

static void wdt_test(void)
{
    static void* WDT_Handler = NULL;
    WDT_Handler = WDT();

    /* 初始化看门狗 */
    WDT_Initialize(WDT_Handler, WDT_Callback_hook, NULL);
    WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);
    
    /* 配置看门狗的时钟源、中断时间、复位时间 */
    hal_driver_wdt_cfg_t wdt_cfg = {
        /* WDT有两个时钟源可以选择：
         * hal_driver_wdt_clk_src_32k: 外部时钟 32k
         * hal_driver_wdt_clk_src_apb: 内部APB clock
         */
        .clk_src = hal_driver_wdt_clk_src_32k,
        /* 中断阶段为2^15 / 32k = 1s，中断产生后就开始进入复位阶段
         * 如果中断阶段内WDT没有刷新，就会触发中断，然后进入芯片复位阶段
         * hal_driver_wdt_int_time_15: 2^15 / 32k = 1s (interrupt stage)
         */
        .int_time = hal_driver_wdt_int_time_15,
        /* 复位阶段为2^14 / 32k = 0.5s
         * hal_driver_wdt_rst_time_14: 2^14 / 32k = 0.5s (reset stage)
         */
        .rst_time = hal_driver_wdt_rst_time_14,
    };
    WDT_Control(WDT_Handler, &wdt_cfg);

    /* 启动看门狗 */
    WDT_Enable(WDT_Handler);

    uint32_t i = 0;
    while(1){
        vTaskDelay(pdMS_TO_TICKS(800));
        if (++i <= 10) {
            /* 刷新看门狗 */
            WDT_Refresh(WDT_Handler);
            printf("refresh\n");
        } else {
            printf("waiting WDT interrupt trigger\n");
        }
    }

    /* 关闭看门狗 */
    WDT_PowerControl(WDT_Handler, CSK_POWER_OFF);
    WDT_Uninitialize(WDT_Handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! WDT\n");

    wdt_test();

    return 0;
}