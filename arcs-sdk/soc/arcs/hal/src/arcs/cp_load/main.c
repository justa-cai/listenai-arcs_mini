/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "log_print.h"
#include "chip.h"

#define WAKEUP_ACT_JUMP_RAM         (0xAA)
#define WAKEUP_ACT_JUMP_NONE        (0xFF)

#if defined(CONFIG_PM) && (CONFIG_PM == 1)
void startup_check(void)
{
    if (IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_NONE)
    {
        IP_AON_CTRL->REG_AON_DIG_RSVD0.all = 0;
        goto WFI_LOOP;
    }
    else if (IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_RAM)
    {
        if ((IP_AON_CTRL->REG_WAKEUP_ISR.all == 0) && (IP_AON_CTRL->REG_AON_DIG_RSVD2.all == WAKEUP_ACT_JUMP_RAM))
        {
            /*It's possible that the system reached this point because it was woken up by UART,
            *but the REG_WAKEUP_ISR register was not set.
            */
            IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = IP_AON_CTRL->REG_AON_DIG_RSVD3.all;
            IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
        }
        goto WFI_LOOP;
    }

    return;

WFI_LOOP:
    do {
        __WFI();
    } while(1);
}
#endif

int main( void )
{
#if PSRAM_SEC
    logInit(0, 115200);
    PSRAM_Initialize(NULL, NULL, 1);
#endif

    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30010000;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

    do {
        __WFI();
    } while (1);
}
