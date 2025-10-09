#include <string.h>
#include "touch_cst816d.h"
#include "lisa_log.h"
#include "systick.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_I2C.h"
#include "arcs_ap.h"
static void *gI2CDev = NULL;
static void *gGpioADev = NULL;
static void *gGpioBDev = NULL;
static volatile uint32_t I2C_M_Event = 0;
#ifdef CONFIG_CORE_SPINLOCK
#include "core_spinlock.h"
SPINLOCK_DEFINE(_i2c_spinlock);
#endif
static bool initialized = false;
static uint8_t touch_int_pin = 0;

#define IIC0_SLAVE_ADDRESS (0x15)

#define CST816D_TOUCH_UP (1)

#define delay_ms SysTick_Delay_Ms

static lisa_touch_callback_t callback;

static void cst816d_i2c_cb(uint32_t event, void *workspace)
{
    I2C_M_Event |= event;
    // CLOGD("[%s] event=0x%x \r\n", __func__, event);
}

static void cst816d_gpio_int_cb(uint32_t event, void *workspace)
{
    if (event & (1UL << touch_int_pin)) {
        if (callback) {
            callback();
        }
    }
}

static int cst816d_check_chipid(void)
{
    uint8_t read_cmd = CST816D_REG_CHIP;
    uint8_t read_buf = 0;

    I2C_M_Event = 0;
#ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&_i2c_spinlock);
    enable_IRQ(IRQ_I2C1_VECTOR);
    I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);
#endif
    I2C_MasterTransmit(gI2CDev, IIC0_SLAVE_ADDRESS, &read_cmd, sizeof(read_cmd), 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

    I2C_M_Event = 0;
    I2C_MasterReceive(gI2CDev, IIC0_SLAVE_ADDRESS, &read_buf, sizeof(read_buf), 1);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };
#ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C1_VECTOR);
    spinlock_release(&_i2c_spinlock);
#endif

    if (read_buf != CST816D_CHIP_ID) {
        LOGE("[%s] err chip id(0x%x)", __FUNCTION__, read_buf);
        return -1;
    }

    return 0;
}

static int cst816d_touch_init(const touch_hw_config_t *config)
{
    uint8_t reset_pin = 0;

    if (initialized) {
        return 0;
    }

    reset_pin = config->gpio_pins.reset.reset_pin;
    touch_int_pin = config->gpio_pins.intr.int_pin;

    // I2C
    gI2CDev = (config) ? config->i2c_dev : I2C1();
#ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&_i2c_spinlock);
#endif
    I2C_Initialize(gI2CDev, cst816d_i2c_cb, NULL);
    I2C_PowerControl(gI2CDev, CSK_POWER_FULL);
    I2C_Control(gI2CDev, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(gI2CDev, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);
#ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C1_VECTOR);
    spinlock_release(&_i2c_spinlock);
#endif

    gGpioADev = config->gpio_pins.reset.reset_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    gGpioBDev = config->gpio_pins.intr.int_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();

    // RESET
    GPIO_SetDir(gGpioADev, (1UL << reset_pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 1);
    delay_ms(10);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 0);
    delay_ms(10);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 1);
    delay_ms(300);

    // INT
    GPIO_Control(gGpioBDev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioBDev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);
    GPIO_SetCallback(gGpioBDev, (1UL << touch_int_pin), cst816d_gpio_int_cb, NULL);

    int ret = cst816d_check_chipid();
    if (ret) {
        return -1;
    }

    initialized = true;
    return 0;
}

static int cst816d_read_coordinates(uint16_t *x, uint16_t *y, bool *pressed)
{
    uint8_t point_num = 0;
    uint8_t read_cmd = 0;
    uint8_t read_buf[7] = {0};

    I2C_M_Event = 0;
#ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&_i2c_spinlock);
    enable_IRQ(IRQ_I2C1_VECTOR);
    I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);
#endif
    I2C_MasterTransmit(gI2CDev, IIC0_SLAVE_ADDRESS, &read_cmd, sizeof(read_cmd), 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

    I2C_M_Event = 0;
    I2C_MasterReceive(gI2CDev, IIC0_SLAVE_ADDRESS, read_buf, sizeof(read_buf), 1);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };
#ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C1_VECTOR);
    spinlock_release(&_i2c_spinlock);
#endif

    point_num = read_buf[2] & 0x0F;
    // Invalid data was read
    if (!(point_num == 1)) {
        return -1;
    }

    *x = ((read_buf[3] & 0x0F) << 8) + read_buf[4];
    *y = ((read_buf[5] & 0x0F) << 8) + (read_buf[6]);
    *pressed = ((read_buf[3] >> 4) == CST816D_TOUCH_UP) ? false : true;

    LOGD("press: %d, point_num: %d, row: %d, col: %d", *pressed, point_num, *x, *y);

    return 0;
}

void cst816d_set_int_callback(lisa_touch_callback_t cb)
{
    // INT
    GPIO_Control(gGpioBDev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioBDev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);

    callback = cb;
}

int cst816d_set_enable(bool enable)
{
    CLOGW("set_enable is not implemented yet");
    return 0;
}

int cst816d_set_inverted_x(bool inverted)
{
    CLOGW("set_inverted_x is not implemented yet");
    return 0;
}

int cst816d_set_inverted_y(bool inverted)
{
    CLOGW("set_inverted_y is not implemented yet");
    return 0;
}

int cst816d_set_swap_xy(bool swap)
{
    CLOGW("set_swap_xy is not implemented yet");
    return 0;
}

static const struct touch_driver_api cst816d_driver_api = {
    .read_coordinates = cst816d_read_coordinates,
    .set_int_callback = cst816d_set_int_callback,
    .set_enable = cst816d_set_enable,
    .set_inverted_x = cst816d_set_inverted_x,
    .set_inverted_y = cst816d_set_inverted_y,
    .set_swap_xy = cst816d_set_swap_xy,
};

const struct touch_device touch_cst816d = {
    .name = "cst816d",
    .device_init = cst816d_touch_init,
    .api = &cst816d_driver_api,
};