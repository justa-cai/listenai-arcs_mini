#include "chip.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "config.h"
#include "pin_str.h"
#include "csk_gpio.h"
#include "check.h"


static qspi_in_pin_t qspi_in_pin = {
    .mclk = PIN_QSPI_IN_MCLK,
    .pclk = PIN_QSPI_IN_PCLK,
    .d0   = PIN_QSPI_IN_D0,
    .d1   = PIN_QSPI_IN_D1,
    .d2   = PIN_QSPI_IN_D2,
    .d3   = PIN_QSPI_IN_D3,
    .pwdn = PIN_QSPI_IN_PWDN,
    .scl  = PIN_QSPI_IN_SCL,
    .sda  = PIN_QSPI_IN_SDA,
};

static qspi_out_pin_t qspi_out_pin = {
    .cs  = PIN_QSPI_OUT_CS,
    .dc  = PIN_QSPI_OUT_DC,
    .clk = PIN_QSPI_OUT_CLK,
    .d0  = PIN_QSPI_OUT_D0,
    .d1  = PIN_QSPI_OUT_D1,
    .d2  = PIN_QSPI_OUT_D2,
    .d3  = PIN_QSPI_OUT_D3,
    .rst = PIN_QSPI_OUT_RST,
    .bl  = PIN_QSPI_OUT_BL,
};

static dvp_pin_t dvp_pin = {
    .mclk = PIN_DVP_MCLK,
    .pclk = PIN_DVP_PCLK,
    .vs   = PIN_DVP_VS,
    .hs   = PIN_DVP_HS,
    .d0   = PIN_DVP_D0,
    .d1   = PIN_DVP_D1,
    .d2   = PIN_DVP_D2,
    .d3   = PIN_DVP_D3,
    .d4   = PIN_DVP_D4,
    .d5   = PIN_DVP_D5,
    .d6   = PIN_DVP_D6,
    .d7   = PIN_DVP_D7,
    .rst  = PIN_DVP_RST,
    .pwdn = PIN_DVP_PWDN,
    .scl  = PIN_DVP_SCL,
    .sda  = PIN_DVP_SDA,
};

static rgb_pin_t rgb_pin = {
    .clk = PIN_RGB_CLK,
    .vs  = PIN_RGB_VS,
    .hs  = PIN_RGB_HS,
    .de  = PIN_RGB_DE,
    .r0  = PIN_RGB_R0,
    .r1  = PIN_RGB_R1,
    .r2  = PIN_RGB_R2,
    .r3  = PIN_RGB_R3,
    .r4  = PIN_RGB_R4,
    .r5  = PIN_RGB_R5,
    .r6  = PIN_RGB_R6,
    .r7  = PIN_RGB_R7,
    .g0  = PIN_RGB_G0,
    .g1  = PIN_RGB_G1,
    .g2  = PIN_RGB_G2,
    .g3  = PIN_RGB_G3,
    .g4  = PIN_RGB_G4,
    .g5  = PIN_RGB_G5,
    .g6  = PIN_RGB_G6,
    .g7  = PIN_RGB_G7,
    .b0  = PIN_RGB_B0,
    .b1  = PIN_RGB_B1,
    .b2  = PIN_RGB_B2,
    .b3  = PIN_RGB_B3,
    .b4  = PIN_RGB_B4,
    .b5  = PIN_RGB_B5,
    .b6  = PIN_RGB_B6,
    .b7  = PIN_RGB_B7,
    .rst = PIN_RGB_RST,
    .bl  = PIN_RGB_BL,
};


static void _gpio_dev_init(void *dev_pin, uint8_t pin_num)
{
    uint8_t num = 0;
    pin_t *pin = NULL;

    for(num = 0; num < pin_num; num++)
    {
        pin = (pin_t *)(dev_pin + sizeof(pin_t) * num);
        if(pin->pad == CSK_IOMUX_PAD_A) {
            pin->dev = GPIOA();
        } else if(pin->pad == CSK_IOMUX_PAD_B) {
            pin->dev = GPIOB();
        } else {
            pin->dev = NULL;
        }
    }
}


static int32_t _gpio_setdir_output(pin_t *pin)
{
    int32_t ret = 0;

    if(pin->dev != NULL) {
        ret = GPIO_SetDir(pin->dev, (1UL << pin->index), CSK_GPIO_DIR_OUTPUT);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    return ret;
}


static int32_t _gpio_output_high(pin_t *pin)
{
    int32_t ret = 0;

    if(pin->dev != NULL) {
        ret = GPIO_PinWrite(pin->dev, (1UL << pin->index), 1);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    return ret;
}


static int32_t _gpio_output_low(pin_t *pin)
{
    int32_t ret = 0;

    if(pin->dev != NULL) {
        ret = GPIO_PinWrite(pin->dev, (1UL << pin->index), 0);
        CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    }

    return ret;
}


static int32_t _pinmux_func(pin_t *pin)
{
    int32_t ret = 0;

    if(pin->pad != PIN_PAD_NULL)
    {
        if(pin->func != PIN_PAD_NULL) {
            ret = IOMuxManager_PinConfigure(pin->pad, pin->index, pin->func);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
        } else if(pin->gpio_func != PIN_PAD_NULL) {
            ret = IOMuxManager_PinConfigure(pin->pad, pin->index, pin->gpio_func);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            _gpio_setdir_output(pin);
        } else {
        }
    }

    return ret;
}


static int32_t _pinmux_gpio(pin_t *pin)
{
    int32_t ret = 0;

    if(pin->pad != PIN_PAD_NULL)
    {
        if(pin->gpio_func != PIN_PAD_NULL) {
            ret = IOMuxManager_PinConfigure(pin->pad, pin->index, pin->gpio_func);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            _gpio_setdir_output(pin);
        }
    }

    return ret;
}


int32_t gpio_init(void)
{
    int32_t ret = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = GPIO_Initialize(GPIOA(), NULL, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPIO_Initialize(GPIOB(), NULL, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    _gpio_dev_init(&qspi_in_pin, sizeof(qspi_in_pin) / sizeof(pin_t));
    _gpio_dev_init(&qspi_out_pin, sizeof(qspi_out_pin) / sizeof(pin_t));
    _gpio_dev_init(&dvp_pin, sizeof(dvp_pin) / sizeof(pin_t));
    _gpio_dev_init(&rgb_pin, sizeof(rgb_pin) / sizeof(pin_t));

    return ret;
}


/*************************************************************************/
void camera_qspi_in_pinmux(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_func(&qspi_in_pin.mclk);
    _pinmux_func(&qspi_in_pin.pclk);
    _pinmux_func(&qspi_in_pin.d0);
    _pinmux_func(&qspi_in_pin.d1);
    _pinmux_func(&qspi_in_pin.d2);
    _pinmux_func(&qspi_in_pin.d3);

    _pinmux_func(&qspi_in_pin.pwdn);
    _pinmux_func(&qspi_in_pin.scl);
    _pinmux_func(&qspi_in_pin.sda);
}


void camera_qspi_in_power_down(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d output high", __func__, __LINE__, qspi_in_pin.pwdn.pad, qspi_in_pin.pwdn.index);

    _pinmux_gpio(&qspi_in_pin.pwdn);
    _gpio_setdir_output(&qspi_in_pin.pwdn);
    _gpio_output_high(&qspi_in_pin.pwdn);
}


void camera_qspi_in_power_on(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d output low", __func__, __LINE__, qspi_in_pin.pwdn.pad, qspi_in_pin.pwdn.index);

    _pinmux_gpio(&qspi_in_pin.pwdn);
    _gpio_setdir_output(&qspi_in_pin.pwdn);
    _gpio_output_low(&qspi_in_pin.pwdn);
}


qspi_in_pin_t* camera_qspi_in_pin_attr(void)
{
    return &qspi_in_pin;
}


void camera_qspi_in_gpio_pwm(void)
{
    uint32_t cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_gpio(&qspi_in_pin.mclk);
    _pinmux_gpio(&qspi_in_pin.pclk);
    _pinmux_gpio(&qspi_in_pin.d0);
    _pinmux_gpio(&qspi_in_pin.d1);
    _pinmux_gpio(&qspi_in_pin.d2);
    _pinmux_gpio(&qspi_in_pin.d3);
    _pinmux_gpio(&qspi_in_pin.pwdn);
    _pinmux_gpio(&qspi_in_pin.scl);
    _pinmux_gpio(&qspi_in_pin.sda);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _gpio_setdir_output(&qspi_in_pin.mclk);
    _gpio_setdir_output(&qspi_in_pin.pclk);
    _gpio_setdir_output(&qspi_in_pin.d0);
    _gpio_setdir_output(&qspi_in_pin.d1);
    _gpio_setdir_output(&qspi_in_pin.d2);
    _gpio_setdir_output(&qspi_in_pin.d3);
    _gpio_setdir_output(&qspi_in_pin.pwdn);
    _gpio_setdir_output(&qspi_in_pin.scl);
    _gpio_setdir_output(&qspi_in_pin.sda);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        _gpio_output_high(&qspi_in_pin.mclk);
        _gpio_output_high(&qspi_in_pin.pclk);
        _gpio_output_high(&qspi_in_pin.d0);
        _gpio_output_high(&qspi_in_pin.d1);
        _gpio_output_high(&qspi_in_pin.d2);
        _gpio_output_high(&qspi_in_pin.d3);
        _gpio_output_high(&qspi_in_pin.pwdn);
        _gpio_output_high(&qspi_in_pin.scl);
        _gpio_output_high(&qspi_in_pin.sda);
        VIDEO_LOG("[%s:%d] gpio_output_high cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(20);

        _gpio_output_low(&qspi_in_pin.mclk);
        _gpio_output_low(&qspi_in_pin.pclk);
        _gpio_output_low(&qspi_in_pin.d0);
        _gpio_output_low(&qspi_in_pin.d1);
        _gpio_output_low(&qspi_in_pin.d2);
        _gpio_output_low(&qspi_in_pin.d3);
        _gpio_output_low(&qspi_in_pin.pwdn);
        _gpio_output_low(&qspi_in_pin.scl);
        _gpio_output_low(&qspi_in_pin.sda);
        VIDEO_LOG("[%s:%d] gpio_output_low cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(10);

        cnt++;
    }
}


/*************************************************************************/
void camera_dvp_pinmux(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_func(&dvp_pin.mclk);
    _pinmux_func(&dvp_pin.pclk);
    _pinmux_func(&dvp_pin.vs);
    _pinmux_func(&dvp_pin.hs);
    _pinmux_func(&dvp_pin.d0);
    _pinmux_func(&dvp_pin.d1);
    _pinmux_func(&dvp_pin.d2);
    _pinmux_func(&dvp_pin.d3);
    _pinmux_func(&dvp_pin.d4);
    _pinmux_func(&dvp_pin.d5);
    _pinmux_func(&dvp_pin.d6);
    _pinmux_func(&dvp_pin.d7);

    _pinmux_func(&dvp_pin.rst);
    _pinmux_func(&dvp_pin.pwdn);
    _pinmux_func(&dvp_pin.scl);
    _pinmux_func(&dvp_pin.sda);
}


void camera_dvp_reset(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d output high", __func__, __LINE__, dvp_pin.rst.pad, dvp_pin.rst.index);

    _pinmux_gpio(&dvp_pin.rst);
    _gpio_setdir_output(&dvp_pin.rst);
    _gpio_output_low(&dvp_pin.rst);
    DELAY_MS(100);
    _gpio_output_high(&dvp_pin.rst);
    DELAY_MS(100);
}


void camera_dvp_power_down(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d output high", __func__, __LINE__, dvp_pin.pwdn.pad, dvp_pin.pwdn.index);

    _pinmux_gpio(&dvp_pin.pwdn);
    _gpio_setdir_output(&dvp_pin.pwdn);
    _gpio_output_high(&dvp_pin.pwdn);
}


void camera_dvp_power_on(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d output low", __func__, __LINE__, dvp_pin.pwdn.pad, dvp_pin.pwdn.index);

    _pinmux_gpio(&dvp_pin.pwdn);
    _gpio_setdir_output(&dvp_pin.pwdn);
    _gpio_output_low(&dvp_pin.pwdn);
}


dvp_pin_t* camera_dvp_pin_attr(void)
{
    return &dvp_pin;
}


void camera_dvp_gpio_pwm(void)
{
    uint32_t cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_gpio(&dvp_pin.mclk);
    _pinmux_gpio(&dvp_pin.pclk);
    _pinmux_gpio(&dvp_pin.vs);
    _pinmux_gpio(&dvp_pin.hs);
    _pinmux_gpio(&dvp_pin.d0);
    _pinmux_gpio(&dvp_pin.d1);
    _pinmux_gpio(&dvp_pin.d2);
    _pinmux_gpio(&dvp_pin.d3);
    _pinmux_gpio(&dvp_pin.d4);
    _pinmux_gpio(&dvp_pin.d5);
    _pinmux_gpio(&dvp_pin.d6);
    _pinmux_gpio(&dvp_pin.d7);
    _pinmux_gpio(&dvp_pin.rst);
    _pinmux_gpio(&dvp_pin.pwdn);
    _pinmux_gpio(&dvp_pin.scl);
    _pinmux_gpio(&dvp_pin.sda);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _gpio_setdir_output(&dvp_pin.mclk);
    _gpio_setdir_output(&dvp_pin.pclk);
    _gpio_setdir_output(&dvp_pin.vs);
    _gpio_setdir_output(&dvp_pin.hs);
    _gpio_setdir_output(&dvp_pin.d0);
    _gpio_setdir_output(&dvp_pin.d1);
    _gpio_setdir_output(&dvp_pin.d2);
    _gpio_setdir_output(&dvp_pin.d3);
    _gpio_setdir_output(&dvp_pin.d4);
    _gpio_setdir_output(&dvp_pin.d5);
    _gpio_setdir_output(&dvp_pin.d6);
    _gpio_setdir_output(&dvp_pin.d7);
    _gpio_setdir_output(&dvp_pin.rst);
    _gpio_setdir_output(&dvp_pin.pwdn);
    _gpio_setdir_output(&dvp_pin.scl);
    _gpio_setdir_output(&dvp_pin.sda);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        _gpio_output_high(&dvp_pin.mclk);
        _gpio_output_high(&dvp_pin.pclk);
        _gpio_output_high(&dvp_pin.vs);
        _gpio_output_high(&dvp_pin.hs);
        _gpio_output_high(&dvp_pin.d0);
        _gpio_output_high(&dvp_pin.d1);
        _gpio_output_high(&dvp_pin.d2);
        _gpio_output_high(&dvp_pin.d3);
        _gpio_output_high(&dvp_pin.d4);
        _gpio_output_high(&dvp_pin.d5);
        _gpio_output_high(&dvp_pin.d6);
        _gpio_output_high(&dvp_pin.d7);
        _gpio_output_high(&dvp_pin.rst);
        _gpio_output_high(&dvp_pin.pwdn);
        _gpio_output_high(&dvp_pin.scl);
        _gpio_output_high(&dvp_pin.sda);
        VIDEO_LOG("[%s:%d] gpio_output_high cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(20);

        _gpio_output_low(&dvp_pin.mclk);
        _gpio_output_low(&dvp_pin.pclk);
        _gpio_output_low(&dvp_pin.vs);
        _gpio_output_low(&dvp_pin.hs);
        _gpio_output_low(&dvp_pin.d0);
        _gpio_output_low(&dvp_pin.d1);
        _gpio_output_low(&dvp_pin.d2);
        _gpio_output_low(&dvp_pin.d3);
        _gpio_output_low(&dvp_pin.d4);
        _gpio_output_low(&dvp_pin.d5);
        _gpio_output_low(&dvp_pin.d6);
        _gpio_output_low(&dvp_pin.d7);
        _gpio_output_low(&dvp_pin.rst);
        _gpio_output_low(&dvp_pin.pwdn);
        _gpio_output_low(&dvp_pin.scl);
        _gpio_output_low(&dvp_pin.sda);
        VIDEO_LOG("[%s:%d] gpio_output_low cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(10);

        cnt++;
    }
}


/*************************************************************************/
void lcd_qspi_out_pinmux(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_func(&qspi_out_pin.cs);
    _pinmux_func(&qspi_out_pin.clk);
    _pinmux_func(&qspi_out_pin.d0);
    _pinmux_func(&qspi_out_pin.d1);
    _pinmux_func(&qspi_out_pin.d2);
    _pinmux_func(&qspi_out_pin.d3);

    _pinmux_func(&qspi_out_pin.rst);
    _pinmux_func(&qspi_out_pin.bl);
}


void lcd_qspi_out_reset(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d output high", __func__, __LINE__, qspi_out_pin.rst.pad, qspi_out_pin.rst.index);

    _pinmux_gpio(&qspi_out_pin.rst);
    _gpio_setdir_output(&qspi_out_pin.rst);
    _gpio_output_low(&qspi_out_pin.rst);
    DELAY_MS(100);
    _gpio_output_high(&qspi_out_pin.rst);
    DELAY_MS(100);
}


void lcd_qspi_out_bl_enable(void)
{
    uint8_t times = 10;

    VIDEO_LOG("[%s:%d] GPIO%d_%d output high", __func__, __LINE__, qspi_out_pin.bl.pad, qspi_out_pin.bl.index);

    _pinmux_gpio(&qspi_out_pin.bl);
    _gpio_setdir_output(&qspi_out_pin.bl);

    do {
        _gpio_output_low(&qspi_out_pin.bl);
        DELAY_US(5);
        _gpio_output_high(&qspi_out_pin.bl);
        DELAY_US(5);
    } while(times--);
}


qspi_out_pin_t* lcd_qspi_out_pin_attr(void)
{
    return &qspi_out_pin;
}


void lcd_qspi_out_gpio_pwm(void)
{
    uint32_t cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_gpio(&qspi_out_pin.cs);
    _pinmux_gpio(&qspi_out_pin.clk);
    _pinmux_gpio(&qspi_out_pin.d0);
    _pinmux_gpio(&qspi_out_pin.d1);
    _pinmux_gpio(&qspi_out_pin.d2);
    _pinmux_gpio(&qspi_out_pin.d3);
    _pinmux_gpio(&qspi_out_pin.rst);
    _pinmux_gpio(&qspi_out_pin.bl);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _gpio_setdir_output(&qspi_out_pin.cs);
    _gpio_setdir_output(&qspi_out_pin.clk);
    _gpio_setdir_output(&qspi_out_pin.d0);
    _gpio_setdir_output(&qspi_out_pin.d1);
    _gpio_setdir_output(&qspi_out_pin.d2);
    _gpio_setdir_output(&qspi_out_pin.d3);
    _gpio_setdir_output(&qspi_out_pin.rst);
    _gpio_setdir_output(&qspi_out_pin.bl);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        _gpio_output_high(&qspi_out_pin.cs);
        _gpio_output_high(&qspi_out_pin.clk);
        _gpio_output_high(&qspi_out_pin.d0);
        _gpio_output_high(&qspi_out_pin.d1);
        _gpio_output_high(&qspi_out_pin.d2);
        _gpio_output_high(&qspi_out_pin.d3);
        _gpio_output_high(&qspi_out_pin.rst);
        _gpio_output_high(&qspi_out_pin.bl);
        VIDEO_LOG("[%s:%d] gpio_output_high cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(20);

        _gpio_output_low(&qspi_out_pin.cs);
        _gpio_output_low(&qspi_out_pin.clk);
        _gpio_output_low(&qspi_out_pin.d0);
        _gpio_output_low(&qspi_out_pin.d1);
        _gpio_output_low(&qspi_out_pin.d2);
        _gpio_output_low(&qspi_out_pin.d3);
        _gpio_output_low(&qspi_out_pin.rst);
        _gpio_output_low(&qspi_out_pin.bl);
        VIDEO_LOG("[%s:%d] gpio_output_low cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(10);

        cnt++;
    }
}


/*************************************************************************/
void lcd_rgb_pinmux(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_func(&rgb_pin.clk);
    _pinmux_func(&rgb_pin.vs);
    _pinmux_func(&rgb_pin.hs);
    _pinmux_func(&rgb_pin.de);
    _pinmux_func(&rgb_pin.r0);
    _pinmux_func(&rgb_pin.r1);
    _pinmux_func(&rgb_pin.r2);
    _pinmux_func(&rgb_pin.r3);
    _pinmux_func(&rgb_pin.r4);
    _pinmux_func(&rgb_pin.r5);
    _pinmux_func(&rgb_pin.r6);
    _pinmux_func(&rgb_pin.r7);
    _pinmux_func(&rgb_pin.g0);
    _pinmux_func(&rgb_pin.g1);
    _pinmux_func(&rgb_pin.g2);
    _pinmux_func(&rgb_pin.g3);
    _pinmux_func(&rgb_pin.g4);
    _pinmux_func(&rgb_pin.g5);
    _pinmux_func(&rgb_pin.g6);
    _pinmux_func(&rgb_pin.g7);
    _pinmux_func(&rgb_pin.b0);
    _pinmux_func(&rgb_pin.b1);
    _pinmux_func(&rgb_pin.b2);
    _pinmux_func(&rgb_pin.b3);
    _pinmux_func(&rgb_pin.b4);
    _pinmux_func(&rgb_pin.b5);
    _pinmux_func(&rgb_pin.b6);
    _pinmux_func(&rgb_pin.b7);

    _pinmux_func(&rgb_pin.rst);
    _pinmux_func(&rgb_pin.bl);
}


void lcd_rgb_reset(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d output high", __func__, __LINE__, rgb_pin.rst.pad, rgb_pin.rst.index);

    _pinmux_gpio(&rgb_pin.rst);
    _gpio_setdir_output(&rgb_pin.rst);
    _gpio_output_low(&rgb_pin.rst);
    DELAY_MS(100);
    _gpio_output_high(&rgb_pin.rst);
    DELAY_MS(100);
}


void lcd_rgb_bl_enable(void)
{
    uint8_t times = 10;

    VIDEO_LOG("[%s:%d] GPIO%d_%d output high", __func__, __LINE__, rgb_pin.bl.pad, rgb_pin.bl.index);

    _pinmux_gpio(&rgb_pin.bl);
    _gpio_setdir_output(&rgb_pin.bl);

    do {
        _gpio_output_low(&rgb_pin.bl);
        DELAY_US(5);
        _gpio_output_high(&rgb_pin.bl);
        DELAY_US(5);
    } while(times--);
}


rgb_pin_t* lcd_rgb_pin_attr(void)
{
    return &rgb_pin;
}


void lcd_rgb_gpio_pwm(void)
{
    uint32_t cnt = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_gpio(&rgb_pin.clk);
    _pinmux_gpio(&rgb_pin.vs);
    _pinmux_gpio(&rgb_pin.hs);
    _pinmux_gpio(&rgb_pin.de);
    _pinmux_gpio(&rgb_pin.r0);
    _pinmux_gpio(&rgb_pin.r1);
    _pinmux_gpio(&rgb_pin.r2);
    _pinmux_gpio(&rgb_pin.r3);
    _pinmux_gpio(&rgb_pin.r4);
    _pinmux_gpio(&rgb_pin.r5);
    _pinmux_gpio(&rgb_pin.r6);
    _pinmux_gpio(&rgb_pin.r7);
    _pinmux_gpio(&rgb_pin.g0);
    _pinmux_gpio(&rgb_pin.g1);
    _pinmux_gpio(&rgb_pin.g2);
    _pinmux_gpio(&rgb_pin.g3);
    _pinmux_gpio(&rgb_pin.g4);
    _pinmux_gpio(&rgb_pin.g5);
    _pinmux_gpio(&rgb_pin.g6);
    _pinmux_gpio(&rgb_pin.g7);
    _pinmux_gpio(&rgb_pin.b0);
    _pinmux_gpio(&rgb_pin.b1);
    _pinmux_gpio(&rgb_pin.b2);
    _pinmux_gpio(&rgb_pin.b3);
    _pinmux_gpio(&rgb_pin.b4);
    _pinmux_gpio(&rgb_pin.b5);
    _pinmux_gpio(&rgb_pin.b6);
    _pinmux_gpio(&rgb_pin.b7);
    _pinmux_gpio(&rgb_pin.rst);
    _pinmux_gpio(&rgb_pin.bl);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _gpio_setdir_output(&rgb_pin.clk);
    _gpio_setdir_output(&rgb_pin.vs);
    _gpio_setdir_output(&rgb_pin.hs);
    _gpio_setdir_output(&rgb_pin.de);
    _gpio_setdir_output(&rgb_pin.r0);
    _gpio_setdir_output(&rgb_pin.r1);
    _gpio_setdir_output(&rgb_pin.r2);
    _gpio_setdir_output(&rgb_pin.r3);
    _gpio_setdir_output(&rgb_pin.r4);
    _gpio_setdir_output(&rgb_pin.r5);
    _gpio_setdir_output(&rgb_pin.r6);
    _gpio_setdir_output(&rgb_pin.r7);
    _gpio_setdir_output(&rgb_pin.g0);
    _gpio_setdir_output(&rgb_pin.g1);
    _gpio_setdir_output(&rgb_pin.g2);
    _gpio_setdir_output(&rgb_pin.g3);
    _gpio_setdir_output(&rgb_pin.g4);
    _gpio_setdir_output(&rgb_pin.g5);
    _gpio_setdir_output(&rgb_pin.g6);
    _gpio_setdir_output(&rgb_pin.g7);
    _gpio_setdir_output(&rgb_pin.b0);
    _gpio_setdir_output(&rgb_pin.b1);
    _gpio_setdir_output(&rgb_pin.b2);
    _gpio_setdir_output(&rgb_pin.b3);
    _gpio_setdir_output(&rgb_pin.b4);
    _gpio_setdir_output(&rgb_pin.b5);
    _gpio_setdir_output(&rgb_pin.b6);
    _gpio_setdir_output(&rgb_pin.b7);
    _gpio_setdir_output(&rgb_pin.rst);
    _gpio_setdir_output(&rgb_pin.bl);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        _gpio_output_high(&rgb_pin.clk);
        _gpio_output_high(&rgb_pin.vs);
        _gpio_output_high(&rgb_pin.hs);
        _gpio_output_high(&rgb_pin.de);
        _gpio_output_high(&rgb_pin.r0);
        _gpio_output_high(&rgb_pin.r1);
        _gpio_output_high(&rgb_pin.r2);
        _gpio_output_high(&rgb_pin.r3);
        _gpio_output_high(&rgb_pin.r4);
        _gpio_output_high(&rgb_pin.r5);
        _gpio_output_high(&rgb_pin.r6);
        _gpio_output_high(&rgb_pin.r7);
        _gpio_output_high(&rgb_pin.g0);
        _gpio_output_high(&rgb_pin.g1);
        _gpio_output_high(&rgb_pin.g2);
        _gpio_output_high(&rgb_pin.g3);
        _gpio_output_high(&rgb_pin.g4);
        _gpio_output_high(&rgb_pin.g5);
        _gpio_output_high(&rgb_pin.g6);
        _gpio_output_high(&rgb_pin.g7);
        _gpio_output_high(&rgb_pin.b0);
        _gpio_output_high(&rgb_pin.b1);
        _gpio_output_high(&rgb_pin.b2);
        _gpio_output_high(&rgb_pin.b3);
        _gpio_output_high(&rgb_pin.b4);
        _gpio_output_high(&rgb_pin.b5);
        _gpio_output_high(&rgb_pin.b6);
        _gpio_output_high(&rgb_pin.b7);
        _gpio_output_high(&rgb_pin.rst);
        _gpio_output_high(&rgb_pin.bl);
        VIDEO_LOG("[%s:%d] gpio_output_high cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(20);

        _gpio_output_low(&rgb_pin.clk);
        _gpio_output_low(&rgb_pin.vs);
        _gpio_output_low(&rgb_pin.hs);
        _gpio_output_low(&rgb_pin.de);
        _gpio_output_low(&rgb_pin.r0);
        _gpio_output_low(&rgb_pin.r1);
        _gpio_output_low(&rgb_pin.r2);
        _gpio_output_low(&rgb_pin.r3);
        _gpio_output_low(&rgb_pin.r4);
        _gpio_output_low(&rgb_pin.r5);
        _gpio_output_low(&rgb_pin.r6);
        _gpio_output_low(&rgb_pin.r7);
        _gpio_output_low(&rgb_pin.g0);
        _gpio_output_low(&rgb_pin.g1);
        _gpio_output_low(&rgb_pin.g2);
        _gpio_output_low(&rgb_pin.g3);
        _gpio_output_low(&rgb_pin.g4);
        _gpio_output_low(&rgb_pin.g5);
        _gpio_output_low(&rgb_pin.g6);
        _gpio_output_low(&rgb_pin.g7);
        _gpio_output_low(&rgb_pin.b0);
        _gpio_output_low(&rgb_pin.b1);
        _gpio_output_low(&rgb_pin.b2);
        _gpio_output_low(&rgb_pin.b3);
        _gpio_output_low(&rgb_pin.b4);
        _gpio_output_low(&rgb_pin.b5);
        _gpio_output_low(&rgb_pin.b6);
        _gpio_output_low(&rgb_pin.b7);
        _gpio_output_low(&rgb_pin.rst);
        _gpio_output_low(&rgb_pin.bl);
        VIDEO_LOG("[%s:%d] gpio_output_low cnt=%d", __func__, __LINE__, cnt);
        DELAY_MS(10);

        cnt++;
    }
}


/*************************************************************************/
void sw_qspi_out_gpio_init(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    _pinmux_gpio(&qspi_out_pin.cs);
    _pinmux_gpio(&qspi_out_pin.clk);
    _pinmux_gpio(&qspi_out_pin.d0);
    _pinmux_gpio(&qspi_out_pin.d1);
    _pinmux_gpio(&qspi_out_pin.d2);
    _pinmux_gpio(&qspi_out_pin.d3);
    _pinmux_gpio(&qspi_out_pin.rst);
    //_pinmux_gpio(&qspi_out_pin.bl);

    _gpio_setdir_output(&qspi_out_pin.cs);
    _gpio_setdir_output(&qspi_out_pin.clk);
    _gpio_setdir_output(&qspi_out_pin.d0);
    _gpio_setdir_output(&qspi_out_pin.d1);
    _gpio_setdir_output(&qspi_out_pin.d2);
    _gpio_setdir_output(&qspi_out_pin.d3);
    _gpio_setdir_output(&qspi_out_pin.rst);
    //_gpio_setdir_output(&qspi_out_pin.bl);
}

void sw_qspi_out_cs_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.cs.dev, (1UL << qspi_out_pin.cs.index), 1);
}

void sw_qspi_out_dc_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.dc.dev, (1UL << qspi_out_pin.dc.index), 1);
}

void sw_qspi_out_clk_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.clk.dev, (1UL << qspi_out_pin.clk.index), 1);
}

void sw_qspi_out_d0_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.d0.dev, (1UL << qspi_out_pin.d0.index), 1);
}

void sw_qspi_out_d1_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.d1.dev, (1UL << qspi_out_pin.d1.index), 1);
}

void sw_qspi_out_d2_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.d2.dev, (1UL << qspi_out_pin.d2.index), 1);
}

void sw_qspi_out_d3_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.d3.dev, (1UL << qspi_out_pin.d3.index), 1);
}

void sw_qspi_out_rst_gpio_set(void)
{
    GPIO_PinWrite(qspi_out_pin.rst.dev, (1UL << qspi_out_pin.rst.index), 1);
}

void sw_qspi_out_cs_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.cs.dev, (1UL << qspi_out_pin.cs.index), 0);
}

void sw_qspi_out_dc_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.dc.dev, (1UL << qspi_out_pin.dc.index), 0);
}

void sw_qspi_out_clk_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.clk.dev, (1UL << qspi_out_pin.clk.index), 0);
}

void sw_qspi_out_d0_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.d0.dev, (1UL << qspi_out_pin.d0.index), 0);
}

void sw_qspi_out_d1_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.d1.dev, (1UL << qspi_out_pin.d1.index), 0);
}

void sw_qspi_out_d2_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.d2.dev, (1UL << qspi_out_pin.d2.index), 0);
}

void sw_qspi_out_d3_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.d3.dev, (1UL << qspi_out_pin.d3.index), 0);
}

void sw_qspi_out_rst_gpio_clr(void)
{
    GPIO_PinWrite(qspi_out_pin.rst.dev, (1UL << qspi_out_pin.rst.index), 0);
}


/*************************************************************************/
void sw_dvp_i2c_gpio_init(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d is SCL, dev=0x%x", __func__, __LINE__, dvp_pin.scl.pad, dvp_pin.scl.index, dvp_pin.scl.dev);
    VIDEO_LOG("[%s:%d] GPIO%d_%d is SDA, dev=0x%x", __func__, __LINE__, dvp_pin.sda.pad, dvp_pin.sda.index, dvp_pin.sda.dev);

    _pinmux_gpio(&dvp_pin.scl);
    _pinmux_gpio(&dvp_pin.sda);

    _gpio_setdir_output(&dvp_pin.scl);
    _gpio_setdir_output(&dvp_pin.sda);
}

void sw_dvp_i2c_scl_gpio_set(void)
{
    GPIO_PinWrite(dvp_pin.scl.dev, (1UL << dvp_pin.scl.index), 1);
}

void sw_dvp_i2c_scl_gpio_clr(void)
{
    GPIO_PinWrite(dvp_pin.scl.dev, (1UL << dvp_pin.scl.index), 0);
}

void sw_dvp_i2c_sda_gpio_set(void)
{
    GPIO_PinWrite(dvp_pin.sda.dev, (1UL << dvp_pin.sda.index), 1);
}

void sw_dvp_i2c_sda_gpio_clr(void)
{
    GPIO_PinWrite(dvp_pin.sda.dev, (1UL << dvp_pin.sda.index), 0);
}

uint8_t sw_dvp_i2c_sda_gpio_get(void)
{
    return GPIO_PinRead(dvp_pin.sda.dev, (1UL << dvp_pin.sda.index));
}

void sw_dvp_i2c_sda_gpio_setdir_output(void)
{
    GPIO_SetDir(dvp_pin.sda.dev, (1UL << dvp_pin.sda.index), CSK_GPIO_DIR_OUTPUT);
}

void sw_dvp_i2c_sda_gpio_setdir_input(void)
{
    GPIO_SetDir(dvp_pin.sda.dev, (1UL << dvp_pin.sda.index), CSK_GPIO_DIR_INPUT);
}


/*************************************************************************/
void sw_qspi_in_i2c_gpio_init(void)
{
    VIDEO_LOG("[%s:%d] GPIO%d_%d is SCL, dev=0x%x", __func__, __LINE__, qspi_in_pin.scl.pad, qspi_in_pin.scl.index, qspi_in_pin.scl.dev);
    VIDEO_LOG("[%s:%d] GPIO%d_%d is SDA, dev=0x%x", __func__, __LINE__, qspi_in_pin.sda.pad, qspi_in_pin.sda.index, qspi_in_pin.sda.dev);

    _pinmux_gpio(&qspi_in_pin.scl);
    _pinmux_gpio(&qspi_in_pin.sda);

    _gpio_setdir_output(&qspi_in_pin.scl);
    _gpio_setdir_output(&qspi_in_pin.sda);
}

void sw_qspi_in_i2c_scl_gpio_set(void)
{
    GPIO_PinWrite(qspi_in_pin.scl.dev, (1UL << qspi_in_pin.scl.index), 1);
}

void sw_qspi_in_i2c_scl_gpio_clr(void)
{
    GPIO_PinWrite(qspi_in_pin.scl.dev, (1UL << qspi_in_pin.scl.index), 0);
}

void sw_qspi_in_i2c_sda_gpio_set(void)
{
    GPIO_PinWrite(qspi_in_pin.sda.dev, (1UL << qspi_in_pin.sda.index), 1);
}

void sw_qspi_in_i2c_sda_gpio_clr(void)
{
    GPIO_PinWrite(qspi_in_pin.sda.dev, (1UL << qspi_in_pin.sda.index), 0);
}

uint8_t sw_qspi_in_i2c_sda_gpio_get(void)
{
    return GPIO_PinRead(qspi_in_pin.sda.dev, (1UL << qspi_in_pin.sda.index));
}

void sw_qspi_in_i2c_sda_gpio_setdir_output(void)
{
    GPIO_SetDir(qspi_in_pin.sda.dev, (1UL << qspi_in_pin.sda.index), CSK_GPIO_DIR_OUTPUT);
}

void sw_qspi_in_i2c_sda_gpio_setdir_input(void)
{
    GPIO_SetDir(qspi_in_pin.sda.dev, (1UL << qspi_in_pin.sda.index), CSK_GPIO_DIR_INPUT);
}




