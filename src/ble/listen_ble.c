#include "listen_ble.h"
#include "bt_os_task.h"
#include "ble_drv.h"
#include "bt_stack_if.h"

#if HCIT_UART_PRESENT
#if 1

void test_uart_read(uint8_t *bufptr, uint32_t size, ble_eif_callback callback, void* dummy)
{
    return;
}

void test_uart_write(uint8_t *bufptr, uint32_t size, ble_eif_callback callback, void* dummy)
{
    return;
}

void test_uart_flow_on(void)
{
    return;
}

bool test_uart_flow_off(void)
{
    return false;
}

const struct ble_eif_api uart_api =
{
    test_uart_read,
    test_uart_write,
    test_uart_flow_on,
    test_uart_flow_off,
}
#else
#include "rom_uart.h"
const struct ble_eif_api uart_api =
{
    uart_read,
    uart_write,
    uart_flow_on,
    uart_flow_off,
};
#endif
#endif

const struct lsip_eif_api* lsip_eif_get(uint8_t idx)
{
#if HCIT_UART_PRESENT
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
#else
    return NULL;
#endif
}

int ls_ble_init(void)
{
    extern os_task_cb_t *bt_stack_if_get_cb(void);
    bt_os_init((os_task_cb_t *)bt_stack_if_get_cb());
    os_task_init(NULL);
    
    // bt_uart_comm();
    return 0;
}