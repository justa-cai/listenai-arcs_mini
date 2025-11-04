/*
 * gpio_notify.c
 *
 *  Created on: Apr. 28, 2024 for MARS
 *      Author: bauldeng
 */

#include "main.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG


#if USE_ONE_I2S_PER_CHIP

void *gpioX = NULL;
static volatile uint32_t s_intr_bits = 0;

// if USE_ONE_I2S_PER_CHIP == 1, use a pair of GPIO pins on master/slave side
// to indicate that I2S slave is ready and I2S master can TX/RX now!


static void cb_notify_slv2mst(uint32_t event, void* workspace) {
    //LOGD("SLV2MST Notify Pin Interrupt");
    s_intr_bits |= event;
}

void gpio_init()
{
#if (NOTIFY_PIN_GRP == CSK_IOMUX_PAD_A)
    gpioX = GPIOA();
#elif (NOTIFY_PIN_GRP == CSK_IOMUX_PAD_B)
    gpioX = GPIOB();
#endif
    assert(gpioX != NULL);
    GPIO_Initialize(gpioX, cb_notify_slv2mst, gpioX);
    IOMuxManager_PinConfigure(NOTIFY_PIN); //Notify Pin as GPIO
}

void gpio_uninit()
{
    if (gpioX != NULL) {
        GPIO_Uninitialize(gpioX);
        gpioX = NULL;
    }
}

//-----------------------------------------------------------------------------
//
// I2S slave configure a GPIO pin as OUTPUT, initialized to LOW level;
// Write 1 to the GPIO pin, notify I2S master when slave is ready to RX or TX;
// Write 0 to the GPIO pin, restore to idle when RX or TX is completed or timeout!
//

void gpio_out_slv_init()
{
    assert(gpioX != NULL);
    GPIO_SetDir(gpioX, NOTIFY_PIN_BIT, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpioX, NOTIFY_PIN_BIT, 0); // Low initially

}

void gpio_out_slv_notify()
{
    assert(gpioX != NULL);
    GPIO_PinWrite(gpioX, NOTIFY_PIN_BIT, 1); // High
}

void gpio_out_slv_restore()
{
    GPIO_PinWrite(gpioX, NOTIFY_PIN_BIT, 0); // Low
}

//-----------------------------------------------------------------------------
//
// I2S master configure a GPIO pin as INTERRUPT (High level trigger);
// Disable the GPIO interrupt when initialized or its ISR is entered (GPIO interrupt is accepted);
// Enable the GPIO interrupt when I2S master is initialized and configured.
//

void gpio_int_mst_init()
{
    assert(gpioX != NULL);
    GPIO_Control(gpioX,
            CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_HIGH_LEVEL | CSK_GPIO_INTR_ENABLE,
            NOTIFY_PIN_BIT);

    GPIO_SetDir(gpioX, NOTIFY_PIN_BIT, CSK_GPIO_DIR_INPUT);
}

bool gpio_int_mst_wait(uint32_t max_wait_ms)
{
    bool ret = false;
    nos_timer_start();
    while(nos_timer_elapsed() < max_wait_ms) {
        if(s_intr_bits & NOTIFY_PIN_BIT) {
            s_intr_bits &= ~NOTIFY_PIN_BIT;
            ret = true;
            break;
        }
    }
    nos_timer_stop();
    return ret;
}

void gpio_int_mst_dis()
{
    assert(gpioX != NULL);
    GPIO_Control(gpioX, CSK_GPIO_INTR_DISABLE, NOTIFY_PIN_BIT);
}

void gpio_int_mst_en()
{
    assert(gpioX != NULL);
    GPIO_Control(gpioX, CSK_GPIO_INTR_ENABLE, NOTIFY_PIN_BIT);
}

#endif // USE_ONE_I2S_PER_CHIP

