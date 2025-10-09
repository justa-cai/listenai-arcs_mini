#include "lisa_display.h"
#ifdef CONFIG_LISA_DISPLAY_AXS15231B
#include "display_axs15231b.h"
#endif

#ifdef CONFIG_LISA_DISPLAY_ST77926
#include "display_st77926.h"
#endif

#ifdef CONFIG_LISA_DISPLAY_ST7789P3
#include "display_st7789p3.h"
#endif

#ifdef CONFIG_LISA_DISPLAY_ST7789V
#include "display_st7789v.h"
#endif

#ifdef CONFIG_LISA_DISPLAY_GC9309NA
#include "display_gc9309na.h"
#endif

#ifdef CONFIG_LISA_DISPLAY_NV3030B
#include "display_nv3030b.h"
#endif

#ifdef CONFIG_LISA_DISPLAY_GC9A01
#include "display_gc9a01.h"
#endif
#ifdef CONFIG_LISA_DISPLAY_UC8253C
#include "display_uc8253c.h"
#endif
const struct display_device *dev = NULL;

const struct display_device *lisa_display_get(void)
{
    return dev;
}

void *lisa_display_create(display_hw_config_t *config)
{
#ifdef CONFIG_LISA_DISPLAY_AXS15231B
	dev = &display_axs15231b;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_DISPLAY_ST77926
	dev = &display_st77926;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_DISPLAY_ST7789P3
	dev = &display_st7789p3;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_DISPLAY_ST7789V
	dev = &display_st7789v;
	if (dev->device_init) {
		if (dev->device_init()) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_DISPLAY_GC9309NA
	dev = &display_gc9309na;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_DISPLAY_NV3030B
	dev = &display_nv3030b;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_DISPLAY_GC9A01
	dev = &display_gc9a01;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_DISPLAY_UC8253C
	dev = &display_uc8253c;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

	return (void *)dev;
}

int lisa_display_blanking_on(const struct display_device *dev)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->display_blanking_on) {
		return dev->api->display_blanking_on();
	}

	return -1;
}

int lisa_display_blanking_off(const struct display_device *dev)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->display_blanking_off) {
		return dev->api->display_blanking_off();
	}

	return -1;
}

void lisa_display_get_capabilities(const struct display_device *dev, struct display_capabilities *capabilities)
{
	if (dev == NULL) {
		return;
	}

	if (dev->api->display_get_capabilities) {
		dev->api->display_get_capabilities(capabilities);
	}
}

int lisa_display_set_brightness(const struct display_device *dev, const uint8_t brightness)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->display_set_brightness) {
		return dev->api->display_set_brightness(brightness);
	}

	return -1;
}

int lisa_display_write(const struct display_device *dev, const uint16_t x, const uint16_t y,
		       const struct display_buffer_descriptor *desc, const void *buf)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->display_write) {
		return dev->api->display_write(x, y, desc, buf);
	}

	return -1;
}

int lisa_display_set_orientation(const struct display_device *dev, const enum display_orientation orientation)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->display_set_orientation) {
		return dev->api->display_set_orientation(orientation);
	}	

	return -1;
}

int lisa_display_sleep(const struct display_device *dev, const uint8_t onoff)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->display_sleep) {
		return dev->api->display_sleep(onoff);
	}	

	return -1;
}
