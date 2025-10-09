#include <string.h>
#include "touch_bl6133.h"
#include "lisa_log.h"
#include "systick.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_I2C.h"

static void *gI2CDev = NULL;
static void *gGpioADev = NULL;
static void *gGpioBDev = NULL;
static volatile uint32_t I2C_M_Event = 0;

#define IIC0_SLAVE_ADDRESS (0x2c)

static bool initialized = false;
static uint8_t touch_int_pin = 0;
#define bl6133_TOUCH_MAX_NUMBER (2)

#define delay_ms SysTick_Delay_Ms

static lisa_touch_callback_t callback;

enum {
    BL6XXX_TOUCH_EVENT_DOWN,
    BL6XXX_TOUCH_EVENT_UP,
    BL6XXX_TOUCH_EVENT_CONTACT_OR_MOVE,
    BL6XXX_TOUCH_EVENT_NONE,
};

static void I2C_M_EventCallback(uint32_t event, void *workspace)
{
    I2C_M_Event |= event;
    // LOGD("[%s] event=0x%x \r\n", __func__, event);
}

static void GPIO_EventCallback(uint32_t event, void *workspace)
{
    // LOGD("[%s] event=0x%x \r\n", __func__, event);
    if (event & (1UL << touch_int_pin)) {
        if (callback) {
            callback();
        }
    }
}

static int bl6133_touch_read_reg(uint16_t reg, uint8_t *data, uint32_t len)
{
    uint8_t tx_buffer;

    tx_buffer = reg & 0xFF;

    I2C_M_Event = 0;
    I2C_MasterTransmit(gI2CDev, IIC0_SLAVE_ADDRESS, &tx_buffer, sizeof(tx_buffer), 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

    I2C_M_Event = 0;
    I2C_MasterReceive(gI2CDev, IIC0_SLAVE_ADDRESS, data, len, 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

    return 0;
}

static int bl6133_touch_init(const touch_hw_config_t *config)
{
    uint8_t reset_pin = 0;

    if (initialized) {
        return 0;
    }

    reset_pin = config->gpio_pins.reset.reset_pin;
    touch_int_pin = config->gpio_pins.intr.int_pin;

    // I2C
    gI2CDev = config->i2c_dev;
    I2C_Initialize(gI2CDev, I2C_M_EventCallback, NULL);
    I2C_PowerControl(gI2CDev, CSK_POWER_FULL);
    I2C_Control(gI2CDev, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(gI2CDev, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);

    gGpioADev = config->gpio_pins.reset.reset_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    gGpioBDev = config->gpio_pins.intr.int_pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();

    // RESET
    GPIO_SetDir(gGpioADev, (1UL << reset_pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 1);
    delay_ms(10);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 0);
    delay_ms(100);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 1);
    delay_ms(300);

    // INT
    GPIO_Control(gGpioBDev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioBDev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);
    GPIO_SetCallback(gGpioBDev, (1UL << touch_int_pin), GPIO_EventCallback, NULL);

    initialized = true;

    return 0;
}

int bl6133_read_coordinates(uint16_t *x, uint16_t *y, bool *pressed)
{
    uint8_t buf[6] = {0};

    bl6133_touch_read_reg(BL6XXX_POINT_REG, buf, sizeof(buf));
    /**
     * BYTE0 | gesture_code[7:0]                    |
     * BYTE1 | point_number[7:0]                    |
     * BYTE2 | evnet[7:6]    | x_high[5:0]          |
     * BYTE3 | x_low[7:0]                           |
     * BYTE4 | touch_id[7:4] | y_high[3:0]          |
     * BYTE5 | y_low[7:0]                           |
     * 
     */
    /** 
     * It looks like this ic is always outputs event 2 when the screen is touched,
     * and event 1 (press event) never outputs.
     */
    *pressed = (buf[2] >> 6 == BL6XXX_TOUCH_EVENT_CONTACT_OR_MOVE) ? true : false;
    *x = (((uint16_t)(buf[2] & 0x3F)) << 8) | buf[3];
    *y = (((uint16_t)(buf[4] & 0x0F)) << 8) | buf[5];

    LOGD("bl6133_read_coordinates x: %d, y: %d, pressed: %d", *x, *y, *pressed);
    return 0;
}

void bl6133_set_int_callback(lisa_touch_callback_t cb)
{
    // INT
    GPIO_Control(gGpioBDev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioBDev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);

    callback = cb;
}

int bl6133_set_enable(bool enable)
{
    LOGW("set_enable is not implemented yet");
    return 0;
}

static const struct touch_driver_api bl6133_driver_api = {
    .read_coordinates = bl6133_read_coordinates,
    .set_int_callback = bl6133_set_int_callback,
    .set_enable = bl6133_set_enable,
};

const struct touch_device touch_bl6133 = {
    .name = "bl6133",
    .device_init = bl6133_touch_init,
    .api = &bl6133_driver_api,
};