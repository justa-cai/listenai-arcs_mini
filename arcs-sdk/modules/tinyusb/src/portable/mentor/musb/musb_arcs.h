#ifndef TUSB_MUSB_ARCS_H_
#define TUSB_MUSB_ARCS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "arcs_ap.h"

#define MUSB_CFG_SHARED_FIFO 0
#define MUSB_CFG_DYNAMIC_FIFO 1
#define MUSB_CFG_DYNAMIC_FIFO_SIZE 4096

const uintptr_t MUSB_BASES[] = {IP_USBC};

// Header supports both device and host modes. Only include what's necessary
#if CFG_TUD_ENABLED

static void usb_irq_handler(void) {
  dcd_int_handler(0);
}

// Mapping of IRQ numbers to port. Currently just 1.
static const IRQn_Type musb_irqs[] = {
  IRQ_USBC_VECTOR};

static inline void musb_dcd_phy_init(uint8_t rhport) {
  (void) rhport;
  register_ISR(IRQ_USBC_VECTOR, usb_irq_handler, NULL);
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_enable(uint8_t rhport) {
  

  // CSK_USBC->INTRUSBE = usb_venus_ctrl.intr_usbe;
  // CSK_USBC->INTRTXE = usb_venus_ctrl.intr_txe;
  // CSK_USBC->INTRRXE = usb_venus_ctrl.intr_rxe;


  // Enable global interrupt
  enable_IRQ(IRQ_USBC_VECTOR);

#if CONFIG_SOF_CNT
  enable_IRQ(IRQ_SOF_CNT_VECTOR);
#endif
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_disable(uint8_t rhport) {
  // Disable global interrupt
  disable_IRQ(IRQ_USBC_VECTOR);

#if CONFIG_SOF_CNT
  disable_IRQ(IRQ_SOF_CNT_VECTOR);
#endif
}

TU_ATTR_ALWAYS_INLINE static inline unsigned musb_dcd_get_int_enable(uint8_t rhport) {
  return IRQ_enabled(IRQ_USBC_VECTOR);
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_clear(uint8_t rhport) {
  
  clear_IRQ(IRQ_USBC_VECTOR);
}

static inline void musb_dcd_int_handler_enter(uint8_t rhport) {
  (void) rhport;
  //Nothing to do for this part
}

#endif// CFG_TUD_ENABLED

#ifdef __cplusplus
}
#endif

#endif// TUSB_MUSB_TI_H_
