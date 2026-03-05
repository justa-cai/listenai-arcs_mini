/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "log_print.h"
#include "chip.h"

#define WAKEUP_ACT_JUMP_RAM         (0xAA)


void ap_startup_check(void)
{
    if (IP_AON_CTRL->REG_AON_DIG_RSVD0.all == WAKEUP_ACT_JUMP_RAM)
    {
        IP_AON_CTRL->REG_AON_DIG_RSVD0.all = 0;
        do {
            __WFI();
        } while(1);
    }
}

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
