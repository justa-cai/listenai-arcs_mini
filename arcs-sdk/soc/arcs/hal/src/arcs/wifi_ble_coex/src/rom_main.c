/*
 * rom_main.c
 *
 *  Main C file for ROM.
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
//#include "cmn_sysctrl_reg_venus.h"
#include "rom_main.h"

#include "IOMuxManager.h"
#include "arcs_ap.h"
#include "bt_config.h" // bt configuration
#include "bt_drv.h"
#include "Driver_UART.h"
#include "log_print.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include <string.h>
#include <stdio.h>
#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"

#include "Driver_GPIO.h"
#include "spiflash.h"

//#include "gpio.h"
#include "log_print.h"
#include "dbg_assert.h"
#include "rf_drv.h"
#include "rf_cali.h"

//#include "app_os_task.h"
#include "bt_os_task.h"

/*
 * DEFINES
 ****************************************************************************************
 */

#define PLF_UART_FOR_BT      0 // bt communication used uart0,  should close shell log.
#define PLF_UART2_FOR_BT     0


/**
 ****************************************************************************************
 * @addtogroup DRIVERS
 * @{
 *
 *
 * ****************************************************************************************
 */

extern int main(void);

// Creation of uart external interface api
const struct ble_eif_api uart_api =
{
    uart_read,
    uart_write,
    uart_flow_on,
    uart_flow_off,
};

#if PLF_UART2
// Creation of uart second external interface api
const struct ble_eif_api uart2_api =
{
    uart_read,
    uart_write,
    uart_flow_on,
    uart_flow_off,
};
#endif // PLF_UART2

const struct lsip_eif_api* lsip_eif_get(uint8_t idx)
{
    const struct lsip_eif_api* ret = NULL;
    switch(idx)
    {
        case 0:
        {
            ret = (const struct lsip_eif_api*)&uart_api;
        }
        break;
        #if PLF_UART2
        case 1:
        {
            ret = &uart2_api;
        }
        break;
        #endif // PLF_UART2
        default:
        {
            //ASSERT_INFO(0, idx, 0);
        }
        break;
    }
    return ret;
}

//extern struct plf_sys_config bt_stack_plf_cfg;
void bt_uart_comm(void)
{
    // Initialize UART component
#if (PLF_UART_FOR_BT ==1)
    //bt_stack_plf_cfg.hcit_feat = PLF_HCIT_UART;
    //bt_stack_plf_cfg.core_feat = PLF_CORE_LE|PLF_CORE_BT;
    //bt_stack_plf_cfg.stack_feat = 0;

    plf_set_feats(PLF_HCIT_UART, PLF_CORE_LE|PLF_CORE_BT, 0);//PLF_HCIT_UART  PLF_STACK_LE

    if (!plf_get_feat(PLF_FEAT_STACK))
    {
        uart_init();
    }
#endif //PLF_UART

    //#else
#if (PLF_UART2_FOR_BT ==1)
    plf_set_feats(PLF_HCIT_UART, PLF_CORE_LE|PLF_CORE_BT, 0);//PLF_HCIT_UART  PLF_STACK_LE
    if (!plf_get_feat(PLF_FEAT_STACK))
    {
        uart2_init();       
    }
#endif //PLF_UART

#if (PLF_UART_FOR_BT ==1) || (PLF_UART2_FOR_BT ==1)
    //if (BLE_HOST_PRESENT || BT_STACK_PRESENT) is not present, no reset msg is receive,should make sure lsip_reset() is callback
    //lsip_reset();
#endif


    return;
}


/*
 * DEFINES
 ****************************************************************************************
 */

extern uint8_t  lsip_irq_disable_count;

#define GLOBAL_INT_DISABLE()                                               \
do{                                                                        \
    lsip_irq_disable_count++;                                              \
    __disable_irq();                          \
}while(0)

#define GLOBAL_INT_RESTORE()                                                \
do {                                                                            \
    lsip_irq_disable_count--;                                               \
    if(!lsip_irq_disable_count)                                             \
    {                                                                       \
        __enable_irq();                       \
    }                                                                       \
}while(0)

/*
 * MAIN FUNCTION
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief main function.
 *
 * This function is called right after the booting process has completed.
 *
 * @return status   exit status
 ****************************************************************************************
 */

extern void user_main();

#if (BT_WIFI_COEX == 0)
int main(void)
{
}
#else  //(BT_WIFI_COEX == 1)

int bt_demo_init(void)
{
    //patch_func_ptr = patch_func_ptr_default;

    /*
     ************************************************************************************
     * Platform initialization
     ************************************************************************************
     */

    //logInit(1, 1000000);
    CLOGD("Enter %s \r\n", __func__);

    // Initialize UART component

     /// os task init
     bt_os_init((os_task_cb_t *)bt_stack_if_get_cb());
     os_task_init(NULL);
     
#if (PLF_UART ==1)
     bt_uart_comm();
#endif


    return 0;
}

#endif

/// @} DRIVERS
