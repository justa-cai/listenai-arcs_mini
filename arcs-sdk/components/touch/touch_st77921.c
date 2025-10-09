#include <string.h>
#include "touch_st77921.h"
#include "log_print.h"
#include "systick.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_I2C.h"

static void *gI2CDev = NULL;
static void *gGpioADev = NULL;
static void *gGpioBDev = NULL;
static volatile uint32_t I2C_M_Event = 0;

#define IIC0_GPIO_SCL      (10)
#define IIC0_GPIO_SDA      (11)
#define TOUCH_RESET_PIN    (27) // PA27
#define TOUCH_INT_PIN      (1)  // PB1
#define IIC0_SLAVE_ADDRESS (0x55)

#define ST77921_TOUCH_REG_X_RESOLUTION_HIGH ((uint16_t)0x0005U)
#define ST77921_TOUCH_REG_X_RESOLUTION_LOW  ((uint16_t)0x0006U)
#define ST77921_TOUCH_REG_Y_RESOLUTION_HIGH ((uint16_t)0x0007U)
#define ST77921_TOUCH_REG_Y_RESOLUTION_LOW  ((uint16_t)0x0008U)
#define ST77921_TOUCH_REG_MAX_TOUCHES       ((uint16_t)0x0009U)
#define ST77921_TOUCH_REG_TOUCH_INFO        ((uint16_t)0x0010U)
#define ST77921_TOUCH_REG_TOUCH_DATA        ((uint16_t)0x0014U)

struct sitronix_ts_device_info {
    uint8_t max_touches;
    uint16_t x_res;
    uint16_t y_res;
};

struct sitronix_ts_obj {
    struct sitronix_ts_device_info info;
};

static struct sitronix_ts_obj g_ts_obj;
static bool initialized = false;
static uint8_t touch_int_pin = 0;
#define ST77921_TOUCH_MAX_NUMBER (2)

#define delay_ms SysTick_Delay_Ms

static lisa_touch_callback_t callback;

static void I2C_M_EventCallback(uint32_t event, void *workspace)
{
    I2C_M_Event |= event;
    // CLOGD("[%s] event=0x%x \r\n", __func__, event);
}

static void GPIO_EventCallback(uint32_t event, void *workspace)
{
    // CLOGD("[%s] event=0x%x \r\n", __func__, event);
    if (event & (1UL << touch_int_pin)) {
        if (callback) {
            callback();
        }
    }
}

static int st77921_touch_read_reg(uint16_t reg, uint8_t *data, uint32_t len)
{
    uint8_t tx_buffer[2];

    tx_buffer[0] = (reg >> 8) & 0xFF;
    tx_buffer[1] = reg & 0xFF;

    I2C_M_Event = 0;
    I2C_MasterTransmit(gI2CDev, IIC0_SLAVE_ADDRESS, tx_buffer, sizeof(tx_buffer), 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

    I2C_M_Event = 0;
    I2C_MasterReceive(gI2CDev, IIC0_SLAVE_ADDRESS, data, len, 0);
    while (!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE)) {
    };

    return 0;
}

static int st77921_touch_read_info(void)
{
    uint8_t rx_buffer[4] = {0};
    st77921_touch_read_reg(ST77921_TOUCH_REG_X_RESOLUTION_HIGH, rx_buffer, sizeof(rx_buffer));

    g_ts_obj.info.x_res = ((rx_buffer[0] & 0x3F) << 8) + rx_buffer[1];
    g_ts_obj.info.y_res = ((rx_buffer[2] & 0x3F) << 8) + rx_buffer[3];

    CLOGD("[%s] x_resolution = %d y_resolution = %d", __func__, g_ts_obj.info.x_res, g_ts_obj.info.y_res);

    st77921_touch_read_reg(ST77921_TOUCH_REG_MAX_TOUCHES, rx_buffer, 1);
    g_ts_obj.info.max_touches = rx_buffer[0];

    if (g_ts_obj.info.max_touches > ST77921_TOUCH_MAX_NUMBER) {
        CLOGE("[%s] unexpected max_touches = %d", __func__, g_ts_obj.info.max_touches);
        return -1;
    }

    return 0;
}

int st77921_touch_init(const touch_hw_config_t *config)
{
    if (initialized) {
        return 0;
    }

    uint8_t reset_pin = 0;
    reset_pin = config->gpio_pins.reset.reset_pin;
    touch_int_pin = config->gpio_pins.intr.int_pin;

    // I2C
    gI2CDev = (config) ? config->i2c_dev : I2C1();
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
    delay_ms(10);
    GPIO_PinWrite(gGpioADev, (1UL << reset_pin), 1);
    delay_ms(300);

    // INT
    GPIO_Control(gGpioBDev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioBDev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);
    GPIO_SetCallback(gGpioBDev, (1UL << touch_int_pin), GPIO_EventCallback, NULL);

    int ret = st77921_touch_read_info();
    if (ret) {
        return -1;
    }
    
    initialized = true;

    return 0;
}

int st77921_read_coordinates(uint16_t *x, uint16_t *y, bool *pressed)
{
    uint8_t read_buf[4 + 7 * ST77921_TOUCH_MAX_NUMBER];

    memset(read_buf, 0, sizeof(read_buf));

    /**
     * 只有完整读取数据后，TS才会清除中断，否则INT引脚会一直触发
     */
    st77921_touch_read_reg(ST77921_TOUCH_REG_TOUCH_INFO, read_buf, 4);

    if (read_buf[0] & (1 << 3)) {
        uint8_t *data = read_buf + 4;
        st77921_touch_read_reg(ST77921_TOUCH_REG_TOUCH_DATA, data, 7 * ST77921_TOUCH_MAX_NUMBER);
        *x = ((data[0] & 0x0F) << 8) + data[1];
        *y = ((data[2] & 0x0F) << 8) + (data[3]);
        *pressed = *data & 0x80 ? true : false;
        return 0;
    }

    return -1;
}

void st77921_set_int_callback(lisa_touch_callback_t cb)
{
    // INT
    GPIO_Control(gGpioBDev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                 (1UL << touch_int_pin));
    GPIO_SetDir(gGpioBDev, (1UL << touch_int_pin), CSK_GPIO_DIR_INPUT);

    callback = cb;
}

int st77921_set_enable(bool enable)
{
    CLOGW("set_enable is not implemented yet");
    return 0;
}

int st77921_set_inverted_x(bool inverted)
{
    CLOGW("set_inverted_x is not implemented yet");
    return 0;
}

int st77921_set_inverted_y(bool inverted)
{
    CLOGW("set_inverted_y is not implemented yet");
    return 0;
}

int st77921_set_swap_xy(bool swap)
{
    CLOGW("set_swap_xy is not implemented yet");
    return 0;
}

static const struct touch_driver_api st77921_driver_api = {
    .read_coordinates = st77921_read_coordinates,
    .set_int_callback = st77921_set_int_callback,
    .set_enable = st77921_set_enable,
    .set_inverted_x = st77921_set_inverted_x,
    .set_inverted_y = st77921_set_inverted_y,
    .set_swap_xy = st77921_set_swap_xy,
};

const struct touch_device touch_st77921 = {
    .name = "st77921",
    .device_init = st77921_touch_init,
    .api = &st77921_driver_api,
};