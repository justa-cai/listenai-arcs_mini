#include "lisa_bluetooth.h"

#include <bt_os_task.h>
#include <ble_drv.h>
#include <bt_stack_if.h>
#include <aud_os_task.h>
#include <aud_pro_os_task.h>
#include <os_task_init.h>

#include "btos_al.h"

#if CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO
#include "aud_if.h"
#endif

extern void Bt_BootClock_Init(void);
extern uint32_t ls_rand(void);

void platform_reset(uint32_t error)
{
    if (error == RESET_AND_LOAD_FW || error == RESET_TO_ROM) {
        // Not yet supported
    } else {
        // Restart FW
    }
}

void bt_platform_init(uint32_t flag)
{
    // BTIP CLK INIT
    Bt_BootClock_Init();
    // Initialize random process
    srand(ls_rand());
}

const struct lsip_eif_api *lsip_eif_get(uint8_t index)
{
    const struct lsip_eif_api *ret = NULL;
    switch (index) {
    case 0: {
        ret = NULL;
    } break;
    case 1: {
        ret = NULL;
    } break;
    default: {
        // ASSERT_INFO(0, index, 0);
    } break;
    }
    return ret;
}

int lisa_bluetooth_init(void)
{
    extern os_task_cb_t *bt_stack_if_get_cb(void);
    bt_os_init((os_task_cb_t *)bt_stack_if_get_cb());

#if CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO
    aud_os_init(aud_if_get_cb());
    aud_pro_os_init(aud_pro_if_get_cb());

    btos_task_create(aud_os_task, AUD_OS_TASK_NAME, OS_TASK_ID_AUD,
                     CONFIG_LISA_BLUETOOTH_AUD_TASK_STACK_SIZE, NULL,
                     CONFIG_LISA_BLUETOOTH_AUD_TASK_PRIORITY, NULL);

    btos_task_create(aud_pro_os_task, AUD_PRO_OS_TASK_NAME, OS_TASK_ID_AUD_PRO,
                     CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_STACK_SIZE, NULL,
                     CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_PRIORITY, NULL);
#endif

    btos_task_create(bt_os_task, BT_OS_TASK_NAME, OS_TASK_ID_BT, CONFIG_LISA_BLUETOOTH_TASK_STACK_SIZE, NULL,
                     CONFIG_LISA_BLUETOOTH_TASK_PRIORITY, NULL);

    return 0;
}
