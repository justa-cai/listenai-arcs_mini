#include "IOMuxManager.h"
#include "arcs_ap.h"

#if CONFIG_BOARD_ARCS_MINI_MODEM_UART2_REMAP

#define MODEM_UART2_TX_PAD   CSK_IOMUX_PAD_A
#define MODEM_UART2_TX_PIN   9
#define MODEM_UART2_RX_PAD   CSK_IOMUX_PAD_A
#define MODEM_UART2_RX_PIN   8
#define MODEM_UART2_FUNC     CSK_IOMUX_FUNC_ALTER4

void lisa_uart2_pinmux(void)
{
    /* UART2 中断设为 Priority 1（更高更优先），优先搬运modem数据 */
    ECLIC_SetPriorityIRQ(IRQ_UART2_VECTOR, 1);
    IOMuxManager_PinConfigure(MODEM_UART2_TX_PAD, MODEM_UART2_TX_PIN, MODEM_UART2_FUNC);
    IOMuxManager_PinConfigure(MODEM_UART2_RX_PAD, MODEM_UART2_RX_PIN, MODEM_UART2_FUNC);
}

#endif
