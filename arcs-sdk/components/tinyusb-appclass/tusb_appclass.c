#include "tusb.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "usbd_pvt.h"

#if CONFIG_ADB
#include "adb_device.h"
#endif

static const usbd_class_driver_t _usbd_app_driver[] = {
#if CONFIG_ADB
	{
		.name = "adb",
		.init = adb_dev_init,
		.deinit = adb_dev_deinit,
		.reset = adb_dev_reset,
		.open = adb_dev_open,
		.control_xfer_cb = adb_dev_control_xfer_cb,
		.xfer_cb = adb_dev_xfer_cb,
		.sof = NULL,
	},
#endif
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
	*driver_count = sizeof(_usbd_app_driver) / sizeof(usbd_class_driver_t);
	return &_usbd_app_driver[0];
}
