#include <string.h>
#include "touch_cst328.h"
#include "lisa_log.h"
#include "systick.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_I2C.h"
#include "arcs_ap.h"
static void *gI2CDev = NULL;
static void *gGpioADev = NULL;
static volatile uint32_t I2C_M_Event = 0;
#ifdef CONFIG_CORE_SPINLOCK
#include "core_spinlock.h"
SPINLOCK_DEFINE(_i2c_spinlock);
#endif
static bool initialized = false;
static uint8_t touch_int_pin = 0;

#define delay_ms SysTick_Delay_Ms

static lisa_touch_callback_t callback;

static void cst328_i2c_cb(uint32_t event, void *workspace)
{
    I2C_M_Event |= event;
}

static void cst328_gpio_int_cb(uint32_t event, void *workspace)
{
    if (event & (1UL << touch_int_pin)) {
        if (callback) {
            callback();
        }
    }
}

static int cst328_touch_read_reg(uint16_t reg, uint8_t *data, uint32_t len)
{
#ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&_i2c_spinlock);
    enable_IRQ(IRQ_I2C1_VECTOR);
    I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);
#endif

    I2C_M_Event = 0;
    I2C_MasterTransmit(gI2CDev, CST328_I2C_SLAVE_ADDRESS, (uint8_t *)&reg, sizeof(reg), 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

    I2C_M_Event = 0;
    I2C_MasterReceive(gI2CDev, CST328_I2C_SLAVE_ADDRESS, data, len, 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

#ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C1_VECTOR);
    spinlock_release(&_i2c_spinlock);
#endif
    return 0;
}

static int cst328_touch_init(const touch_hw_config_t *config)
{
    uint8_t reset_pin = 0;

    if (initialized) {
        return 0;
    }
    reset_pin = config->gpio_pins.reset.reset_pin;
    touch_int_pin = config->gpio_pins.intr.int_pin;

    // I2C
    gI2CDev = (config) ? config->i2c_dev : I2C0();
#ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&_i2c_spinlock);
#endif
    I2C_Initialize(gI2CDev, cst328_i2c_cb, NULL);
    I2C_PowerControl(gI2CDev, CSK_POWER_FULL);
    I2C_Control(gI2CDev, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(gI2CDev, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);
#ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C1_VECTOR);
    spinlock_release(&_i2c_spinlock);
#endif

    gGpioADev = config->gpio_pins.reset.reset_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();

    // RESET
    GPIO_SetDir(gGpioADev, (1UL << reset_pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 1);
    delay_ms(10);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 0);
    delay_ms(10);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 1);
    delay_ms(200);

    // INT
    GPIO_Control(gGpioADev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_POSITIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioADev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);
    GPIO_SetCallback(gGpioADev, (1UL << touch_int_pin), cst328_gpio_int_cb, NULL);

    initialized = true;
    return 0;
}

static int cst328_read_coordinates(uint16_t *x, uint16_t *y, bool *pressed)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;
    uint8_t read_buf[4] = {0};
    cst328_touch_read_reg(CST328_TOUCH_INFO_REG, read_buf, sizeof(read_buf));

    *pressed = ((read_buf[0] & 0x0F) == CST328_STATUS_PRESSED) ? true : false;
    *x = (read_buf[1] << 4) | ((read_buf[3] >> 4) & 0x0F); // Calculate X coordinate (combining high and low bits)
    *y = (read_buf[2] << 4) | (read_buf[3] & 0x0F);        // Calculate Y coordinate (combining high and low bits)

    if (*pressed) {
        last_x = *x;
        last_y = *y;
    } else {
        *x = last_x;
        *y = last_y;
    }

    LOGD("cst328_read_coordinates x: %d, y: %d, pressed: %d", *x, *y, *pressed);

    return 0;
}

void cst328_set_int_callback(lisa_touch_callback_t cb)
{
    // INT
    GPIO_Control(gGpioADev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioADev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);

    callback = cb;
}

static const struct touch_driver_api cst328_driver_api = {
    .read_coordinates = cst328_read_coordinates,
    .set_int_callback = cst328_set_int_callback,
};

const struct touch_device touch_cst328 = {
    .name = "cst328",
    .device_init = cst328_touch_init,
    .api = &cst328_driver_api,
};