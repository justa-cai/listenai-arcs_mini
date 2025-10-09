/* Standard includes. */
#include <stdio.h>
#include <string.h>

#include "log_print.h"
#include "chip.h"

int main( void )
{
#if PSRAM_SEC
    logInit(0, 115200);
    PSRAM_Initialize(NULL, NULL, 1);
#endif
    extern void BootClock_Init();
    BootClock_Init();

    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30010000;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
    __WFI();

    while(1);
}
