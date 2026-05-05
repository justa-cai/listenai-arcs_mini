#include <stdint.h>

/*
 * TinyUSB discovers app drivers through a weak callback in usbd.c.
 * Touch a strong symbol from tusb_appclass.c so the linker keeps that
 * object in standalone boot images and ADB registration stays live.
 */
extern uint8_t tusb_appclass_driver_count(void);

void boot_tusb_appclass_force_link(void)
{
    (void)tusb_appclass_driver_count();
}
