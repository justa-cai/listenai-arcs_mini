#include "unity.h"
#include "usbd_core.h"

/* Exercise the real driver against register storage. RXIS=0 models an
 * interrupt status already consumed while that endpoint was masked. */
#include "../../../modules/chryusb/port/musb/usb_dc_musb.c"

static uint32_t registers[256];
struct usbd_bus g_usbdev_bus[1];
static unsigned completions, critical_depth;
static uint32_t completed_bytes;
static bool in_irq;
static unsigned setups, in_completions, ep0_out_completions;

size_t usb_osal_enter_critical_section(void) { return critical_depth++; }
void usb_osal_leave_critical_section(size_t flags)
{
    TEST_ASSERT_EQUAL_UINT(flags + 1, critical_depth);
    critical_depth--;
}
uint8_t usbd_get_musb_fifo_cfg(struct musb_fifo_cfg **cfg)
{
    (void)cfg;
    return 0;
}
uint32_t usb_get_musb_ram_size(void) { return 4096; }
void usbd_musb_delay_ms(uint8_t ms) { (void)ms; }
void usbd_event_reset_handler(uint8_t busid) { (void)busid; }
void usbd_event_resume_handler(uint8_t busid) { (void)busid; }
void usbd_event_suspend_handler(uint8_t busid) { (void)busid; }
void usbd_event_sof_handler(uint8_t busid) { (void)busid; }
void usbd_event_ep0_setup_complete_handler(uint8_t busid, uint8_t *setup)
{ (void)busid; (void)setup; setups++; }
void usbd_event_ep_in_complete_handler(uint8_t busid, uint8_t ep, uint32_t nbytes)
{ (void)busid; (void)ep; (void)nbytes; in_completions++; }
void usbd_event_ep_out_complete_handler(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    TEST_ASSERT_TRUE(in_irq);
    if (ep == 0) { ep0_out_completions++; return; }
    TEST_ASSERT_EQUAL_UINT8(0, busid);
    TEST_ASSERT_EQUAL_UINT8(2, ep);
    completions++;
    completed_bytes = nbytes;
}

void setUp(void)
{
    memset(registers, 0, sizeof(registers));
    memset(&g_musb_udc, 0, sizeof(g_musb_udc));
    g_usbdev_bus[0].reg_base = (uintptr_t)registers;
    g_musb_udc.out_ep[2].ep_enable = true;
    g_musb_udc.out_ep[2].ep_mps = 512;
    musb_rx_recheck = 0;
    completions = critical_depth = completed_bytes = 0;
    in_irq = false;
    setups = in_completions = ep0_out_completions = 0;
    usb_ep0_state = USB_EP0_STATE_SETUP;
#ifdef CONFIG_USBDEV_SOF_ENABLE
    HWREGB(USB_BASE + MUSB_IE_OFFSET) = USB_IE_SOF;
#endif
}
void tearDown(void) { TEST_ASSERT_EQUAL_UINT(0, critical_depth); }
static void receive_byte(void)
{
    HWREGB(USB_RXCSRL_BASE(2)) = USB_RXCSRL1_RXRDY;
    HWREGH(USB_RXCOUNT_BASE(2)) = 1;
    HWREGB(USB_FIFO_BASE(2)) = 'v';
}
static void run_irq(void)
{
    in_irq = true;
    USBD_IRQHandler(0);
    in_irq = false;
    /* Real MUSB interrupt status registers clear when read. */
    HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = 0;
    HWREGB(USB_BASE + MUSB_IS_OFFSET) = 0;
}
void test_rearm_recovers_fifo_after_interrupt_status_was_cleared(void)
{
    uint8_t data = 0;
    receive_byte();
    TEST_ASSERT_EQUAL_INT(0, usbd_ep_start_read(0, 2, &data, 1));
    TEST_ASSERT_TRUE(HWREGB(USB_BASE + MUSB_IE_OFFSET) & USB_IE_SOF);
    TEST_ASSERT_EQUAL_UINT(0, completions);
    HWREGB(USB_BASE + MUSB_IS_OFFSET) = USB_IS_SOF;
    run_irq();
#ifdef CONFIG_USBDEV_SOF_ENABLE
    TEST_ASSERT_TRUE(HWREGB(USB_BASE + MUSB_IE_OFFSET) & USB_IE_SOF);
#else
    TEST_ASSERT_FALSE(HWREGB(USB_BASE + MUSB_IE_OFFSET) & USB_IE_SOF);
#endif
    TEST_ASSERT_EQUAL_UINT(1, completions);
    TEST_ASSERT_EQUAL_UINT(1, completed_bytes);
    TEST_ASSERT_EQUAL_UINT8('v', data);
    run_irq();
    TEST_ASSERT_EQUAL_UINT(1, completions);
}
void test_empty_fifo_waits_for_hardware_interrupt(void)
{
    uint8_t data = 0;
    TEST_ASSERT_EQUAL_INT(0, usbd_ep_start_read(0, 2, &data, 1));
#ifndef CONFIG_USBDEV_SOF_ENABLE
    TEST_ASSERT_FALSE(HWREGB(USB_BASE + MUSB_IE_OFFSET) & USB_IE_SOF);
#endif
    receive_byte();
    HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = 1U << 2;
    run_irq();
    TEST_ASSERT_EQUAL_UINT(1, completions);
    TEST_ASSERT_EQUAL_UINT8('v', data);
}
void test_hardware_and_software_pending_complete_only_once(void)
{
    uint8_t data = 0;
    receive_byte();
    HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = 1U << 2;
    TEST_ASSERT_EQUAL_INT(0, usbd_ep_start_read(0, 2, &data, 1));
    run_irq();
    run_irq();
    TEST_ASSERT_EQUAL_UINT(1, completions);
}
void test_reset_discards_pending_completion(void)
{
    uint8_t data = 0;
    receive_byte();
    TEST_ASSERT_EQUAL_INT(0, usbd_ep_start_read(0, 2, &data, 1));
    HWREGB(USB_BASE + MUSB_IS_OFFSET) = USB_IS_RESET;
    run_irq();
    TEST_ASSERT_EQUAL_UINT(0, completions);
    TEST_ASSERT_EQUAL_UINT(0, musb_rx_recheck);
    TEST_ASSERT_EQUAL_UINT8(0, data);
}
/* A new SETUP may already be in the FIFO when the previous transfer's
 * STALL/SETUPEND interrupt is serviced. Both share the same EP0 IRQ bit. */
static void ep0_new_setup(uint8_t status)
{
    HWREGB(USB_TXCSRL_BASE(0)) = status | USB_CSRL0_RXRDY;
    HWREGH(USB_RXCOUNT_BASE(0)) = 8;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = USB_TXIE_EP0;
    run_irq();
    TEST_ASSERT_EQUAL_UINT(1, setups);
    TEST_ASSERT_EQUAL_UINT(0, in_completions);
}
void test_ep0_stall_does_not_drop_new_setup(void)
{
    usb_ep0_state = USB_EP0_STATE_STALL;
    ep0_new_setup(USB_CSRL0_STALLED);
}
void test_ep0_setupend_aborts_old_data_phase(void)
{
    usb_ep0_state = USB_EP0_STATE_IN_DATA;
    ep0_new_setup(USB_CSRL0_SETEND);
}
void test_ep0_new_setup_supersedes_delayed_in_completion(void)
{
    usb_ep0_state = USB_EP0_STATE_IN_DATA;
    g_musb_udc.in_ep[0].xfer_len = 4;
    g_musb_udc.in_ep[0].ep_mps = 64;
    ep0_new_setup(0);
}
void test_ep0_new_setup_supersedes_delayed_status_completion(void)
{
    usb_ep0_state = USB_EP0_STATE_IN_STATUS;
    ep0_new_setup(0);
}
void test_ep0_eight_byte_out_payload_is_not_a_setup(void)
{
    uint8_t data[8] = {0};
    usb_ep0_state = USB_EP0_STATE_OUT_DATA;
    g_musb_udc.out_ep[0].xfer_buf = data;
    g_musb_udc.out_ep[0].ep_mps = 64;
    HWREGB(USB_TXCSRL_BASE(0)) = USB_CSRL0_RXRDY;
    HWREGH(USB_RXCOUNT_BASE(0)) = 8;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = USB_TXIE_EP0;
    run_irq();
    TEST_ASSERT_EQUAL_UINT(0, setups);
    TEST_ASSERT_EQUAL_UINT(1, ep0_out_completions);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rearm_recovers_fifo_after_interrupt_status_was_cleared);
    RUN_TEST(test_empty_fifo_waits_for_hardware_interrupt);
    RUN_TEST(test_hardware_and_software_pending_complete_only_once);
    RUN_TEST(test_reset_discards_pending_completion);
    RUN_TEST(test_ep0_stall_does_not_drop_new_setup);
    RUN_TEST(test_ep0_setupend_aborts_old_data_phase);
    RUN_TEST(test_ep0_new_setup_supersedes_delayed_in_completion);
    RUN_TEST(test_ep0_new_setup_supersedes_delayed_status_completion);
    RUN_TEST(test_ep0_eight_byte_out_payload_is_not_a_setup);
    return UNITY_END();
}
