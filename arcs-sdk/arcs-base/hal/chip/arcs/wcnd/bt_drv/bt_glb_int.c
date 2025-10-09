/*
 * bt_glb_int.c
 *
 *  bt global interrupt
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include "arcs_ap.h"
#include "bt_config.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

#if (BT_DUAL_MODE) || (SINGLE_RUN_ON_DUAL)
extern void lsip_isr(void);
#elif (BLE_EMB_PRESENT)
extern void lsble_single_isr(void);  // TODO: Change the name
#elif (BT_EMB_PRESENT)
#endif

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
uint8_t bt_irq_ready = 0;

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
void bt_enable_GINT()
{
    enable_GINT();
}

void bt_disable_GINT()
{
    disable_GINT();
}

void bt_enable_IRQ()
{
    if (bt_irq_ready)
        enable_IRQ(IRQ_BT_VECTOR);
}

void bt_disable_IRQ()
{
    if (bt_irq_ready)
        disable_IRQ(IRQ_BT_VECTOR);
}

void lsip_int_enable(void)
{
    //PTCH(void, lsip_int_enable);
#if (BT_DUAL_MODE) || (SINGLE_RUN_ON_DUAL)
    register_ISR(IRQ_BT_VECTOR, lsip_isr, NULL);
    enable_IRQ(IRQ_BT_VECTOR);

    register_ISR(IRQ_TIMER0_VECTOR, lsip_isr, NULL);
    enable_IRQ(IRQ_TIMER0_VECTOR);

#elif (BLE_EMB_PRESENT)
    register_ISR(IRQ_BT_VECTOR, lsble_single_isr, NULL);
    enable_IRQ(IRQ_BT_VECTOR);

    register_ISR(IRQ_TIMER0_VECTOR, lsble_single_isr, NULL);
    enable_IRQ(IRQ_TIMER0_VECTOR);
#elif (BT_EMB_PRESENT)
#endif
    bt_irq_ready = 1;
}

void lsip_int_clear(void)
{
    clear_IRQ(IRQ_BT_VECTOR);
    bt_irq_ready = 0;
}



