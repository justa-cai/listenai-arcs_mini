/*
 * bt_ip_if.c
 *
 *  bt ip interface functions
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition

#include "log_print.h"
#include "nvs.h"

//#include "plf.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"

#include "bt_stack_if.h"
#include "bt_ble_if.h"
#include "bt_ip_if.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
extern void Bt_BootClock_Init(void);
extern uint32_t ls_rand(void);
extern int main();

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
#if (BUILD_ROM == 0)
//extern int main(void);
void platform_reset_patch(uint32_t error)
{
	void (*pReset)(void);

    if(error == RESET_AND_LOAD_FW || error == RESET_TO_ROM)
    {
        // Not yet supported
    }
    else
    {
        // Restart FW
        pReset = (void *)(main);
        pReset();
    }
}
#endif
void platform_reset(uint32_t error)
{
#if 0
	void (*pReset)(void);

    // Disable interrupts
    //GLOBAL_INT_STOP();

    #if PLF_UART
    // Wait UART transfer finished
    uart_finish_transfers();
    #if !(BLE_EMB_PRESENT) && !(BT_EMB_PRESENT)
    uart2_finish_transfers();
    #endif // !BLE_EMB_PRESENT && !(BT_EMB_PRESENT)
    #endif //PLF_UART

    // Store information in unloaded area
    unloaded_area->error = error;

    if(error == RESET_AND_LOAD_FW || error == RESET_TO_ROM)
    {
        // Not yet supported
    }
    else
    {
        // Restart FW
        pReset = (void * )(0x0);
        pReset();
    }
#endif

	void (*pReset)(void);

    if(error == RESET_AND_LOAD_FW || error == RESET_TO_ROM)
    {
        // Not yet supported
    }
    else
    {
        // Restart FW
        pReset = (void *)(main);
        pReset();
    }

}

void bt_platform_init(uint32_t flag)
{
    // BTIP CLK INIT
    Bt_BootClock_Init();

#if (IC_BOARD == 1)
    // Initialize random process
    srand(ls_rand());
#else
    srand(1);
#endif

}



