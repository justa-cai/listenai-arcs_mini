#include "arcs_ap.h"


#include <stdio.h>
#include <stdbool.h>

#if CONFIG_MODULE_FREERTOS

#include "FreeRTOS.h"
#include "task.h"

#if configUSE_IDLE_HOOK
void vApplicationIdleHook(void)
{
    __WFI();
}
#endif//configUSE_IDLE_HOOK

#if configUSE_MALLOC_FAILED_HOOK
void vApplicationMallocFailedHook(void)
{
    // ASSERT(false, "MallocFailed@%s", x_task_name(NULL));
    printf("MallocFailed@%s\r\n", xPortIsInsideInterrupt() ? "<ISR>" : pcTaskGetName(xTaskGetCurrentTaskHandle()));
    __builtin_trap();
}

#endif//configUSE_MALLOC_FAILED_HOOK

#if configCHECK_FOR_STACK_OVERFLOW
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    // ASSERT(false, "StackOverflow@%s", name);
    // printf("StackOverflow@%s\r\n", name);

    printf("StackOverflow@%s\r\n", name);
    __builtin_trap();
}

#endif//configCHECK_FOR_STACK_OVERFLOW

#if configUSE_TICK_INITAL_HOOK
TickType_t portTickInitalHook( TickType_t tick )
{
    // Enable force wakeup
    IP_SYSCTRL->REG_RTC_FORCE_WAKEUP.bit.FORCE_WAKEUP = 0x1;
    while(!IP_CALENDAR->REG_STATUS.bit.FORCE_WAKEUP);

    // Disable CALENDAR interrupt
    IP_CALENDAR->REG_CMD.bit.ALARM_ENABLE_CLR = 0x1;
    while(IP_CALENDAR->REG_CMD.bit.ALARM_ENABLE_CLR);

    IP_CALENDAR->REG_CMD.bit.ITV_IRQ_MASK_CLR = 0x1;
    while(IP_CALENDAR->REG_CMD.bit.ITV_IRQ_MASK_CLR);

    // Clear IRQ pending status
    IP_CALENDAR->REG_CMD.bit.ITV_IRQ_CLR = 0x1;
    IP_CALENDAR->REG_CMD.bit.ALARM_CLR = 0x1;

    volatile uint32_t days = IP_CALENDAR->REG_CUR_VAL_H.bit.DAY;
    volatile uint32_t hour = IP_CALENDAR->REG_CUR_VAL_L.bit.HOUR;
    volatile uint32_t mint = IP_CALENDAR->REG_CUR_VAL_L.bit.MIN;
    volatile uint32_t secs = IP_CALENDAR->REG_CUR_VAL_L.bit.SEC;
    volatile uint32_t msec = (((days * 24 + hour) * 60 + mint) * 60 + secs) * 1000;

    return msec / portTICK_PERIOD_MS;
}
#endif

#endif
