#include "lisa_touch.h"
#include "Driver_GPIO.h"
#include "Driver_I2C.h"
#include "IOMuxManager.h"

#if CONFIG_LISA_TOUCH_READ_FREQUENCY > 0
#include "FreeRTOS.h"
#include "task.h"
#endif

#ifdef CONFIG_LISA_TOUCH_AXS15231B
#include "touch_axs15231b.h"
#endif

// Forward declarations for driver-specific init functions with config
#ifdef CONFIG_LISA_TOUCH_AXS15231B
int axs15231b_touch_init_with_config(const touch_hw_config_t *config);
#endif

#ifdef CONFIG_LISA_TOUCH_ST77921
#include "touch_st77921.h"
#endif

#ifdef CONFIG_LISA_TOUCH_CST816D
#include "touch_cst816d.h"
#endif

#ifdef CONFIG_LISA_TOUCH_FT5336
#include "touch_ft5336.h"
#endif

#ifdef CONFIG_LISA_TOUCH_BL6133
#include "touch_bl6133.h"
#endif

#ifdef CONFIG_LISA_TOUCH_CST328
#include "touch_cst328.h"
#endif

static bool initialized = false;

static void lisa_touch_hardware_init(const touch_hw_config_t *config)
{
	void *gpio_dev = NULL;

	if (config) {
		IOMuxManager_PinConfigure(config->i2c_pins.scl.scl_pad, config->i2c_pins.scl.scl_pin, config->i2c_pins.scl.scl_func);
		IOMuxManager_PinConfigure(config->i2c_pins.sda.sda_pad, config->i2c_pins.sda.sda_pin, config->i2c_pins.sda.sda_func);
		IOMuxManager_PinConfigure(config->gpio_pins.reset.reset_pad, config->gpio_pins.reset.reset_pin, config->gpio_pins.reset.reset_func);
		IOMuxManager_PinConfigure(config->gpio_pins.intr.int_pad, config->gpio_pins.intr.int_pin, config->gpio_pins.intr.int_func);

		gpio_dev = config->gpio_pins.reset.reset_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
		GPIO_Initialize(gpio_dev, NULL, NULL);
	
		gpio_dev = config->gpio_pins.intr.int_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
		GPIO_Initialize(gpio_dev, NULL, NULL);
	}
}

void *lisa_touch_create(touch_hw_config_t *config)
{
	const struct touch_device *dev = NULL;
	
	lisa_touch_hardware_init(config);

#ifdef CONFIG_LISA_TOUCH_AXS15231B
	dev = &touch_axs15231b;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_TOUCH_ST77921
	dev = &touch_st77921;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_TOUCH_CST816D
	dev = &touch_cst816d;
	if (dev->device_init) {
		if (dev->device_init()) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_TOUCH_FT5336
	dev = &touch_ft5336;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_TOUCH_BL6133
	dev = &touch_bl6133;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

#ifdef CONFIG_LISA_TOUCH_CST328
	dev = &touch_cst328;
	if (dev->device_init) {
		if (dev->device_init(config)) {
			return NULL;
		}
	}
#endif

	return (void *)dev;
}

#ifdef CONFIG_LISA_TOUCH_INTERRUPT
void lisa_touch_set_int_callback(const struct touch_device *dev, lisa_touch_callback_t cb)
{
	if (dev == NULL) {
		return;
	}

	if (dev->api->set_int_callback) {
		dev->api->set_int_callback(cb);
	}
}
#endif

int lisa_touch_read_coordinates(const struct touch_device *dev, uint16_t *x, uint16_t *y, bool *pressed)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->read_coordinates) {
		return dev->api->read_coordinates(x, y, pressed);
	}

	return -1;
}

int lisa_touch_set_enable(const struct touch_device *dev, bool enable)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->set_enable) {
		return dev->api->set_enable(enable);
	}

	return -1;
}

int lisa_touch_set_inverted_x(const struct touch_device *dev, bool inverted)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->set_inverted_x) {
		return dev->api->set_inverted_x(inverted);
	}

	return -1;
}

int lisa_touch_set_inverted_y(const struct touch_device *dev, bool inverted)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->set_inverted_y) {
		return dev->api->set_inverted_y(inverted);
	}

	return -1;
}

int lisa_touch_set_swap_xy(const struct touch_device *dev, bool swap)
{
	if (dev == NULL) {
		return -1;
	}

	if (dev->api->set_swap_xy) {
		return dev->api->set_swap_xy(swap);
	}

	return -1;
}
