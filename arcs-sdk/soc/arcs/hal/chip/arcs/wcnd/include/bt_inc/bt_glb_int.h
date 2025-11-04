#ifndef BT_FOR_INNER_H_
#define BT_FOR_INNER_H_


#if 1
// disable and enable global interrupt, set MSTATUS->MIE
extern void bt_disable_GINT();
extern void bt_enable_GINT();
#define BT_INT_DISABLE()        bt_disable_GINT()
#define BT_INT_ENABLE()         bt_enable_GINT()
#define ENABLE_NESTED_LOOP_RESTORE_IRQ  1
#endif

#if 0
// disable and enable global interrupt, set ECLIC->MTH, used by rtos
extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
#define BT_INT_DISABLE()        vPortEnterCritical()
#define BT_INT_ENABLE()         vPortExitCritical()
#define ENABLE_NESTED_LOOP_RESTORE_IRQ  0
#endif

#if 0
// disable and enable BT_irq only, setECLIC->INTIE
extern void bt_disable_IRQ();
extern void bt_enable_IRQ();
#define BT_INT_DISABLE()        bt_disable_IRQ()
#define BT_INT_ENABLE()         bt_enable_IRQ()
#define ENABLE_NESTED_LOOP_RESTORE_IRQ  1
#endif




#endif
