#include <stdio.h>
#include <stdint.h>

#include "IOMuxManager.h"
#include "Driver_I2C.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
/* ARCS_MINI: I2C0 on PB6(SDA)/PB7(SCL) */
#define IIC0_IO_PAD        CSK_IOMUX_PAD_B
#define IIC0_GPIO_SDA      6
#define IIC0_GPIO_SCL      7
#else
#define IIC0_IO_PAD        CSK_IOMUX_PAD_A
#define IIC0_GPIO_SDA      22
#define IIC0_GPIO_SCL      23
#endif

static void *gI2CDev = NULL;
static volatile uint32_t I2C_M_Event = 0;

static void i2c_cb(uint32_t event, void *workspace)
{
    I2C_M_Event |= event;
}

static void board_init(void)
{
    IOMuxManager_PinConfigure(IIC0_IO_PAD, IIC0_GPIO_SDA, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(IIC0_IO_PAD, IIC0_GPIO_SCL, CSK_IOMUX_FUNC_ALTER8);

    
    gI2CDev = I2C0();
    I2C_Initialize(gI2CDev, i2c_cb, NULL);
    I2C_PowerControl(gI2CDev, CSK_POWER_FULL);
    I2C_Control(gI2CDev, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(gI2CDev, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);   
}

int main(int argc, char **argv)
{
    board_init();

    printf("Start scanning I2C bus...\n");

    for (uint8_t i = 1; i < 128; i++) {
        I2C_M_Event = 0;
        I2C_MasterTransmit(gI2CDev, i, NULL, 0, 0);

        while (I2C_M_Event == 0) {
            ;
        }

        if ((I2C_M_Event & CSK_I2C_EVENT_ADDRESS_ACK) == CSK_I2C_EVENT_ADDRESS_ACK) {
            printf("Found I2C device at address: 0x%02X\n", i);
        }
    }

    printf("I2C bus scan finished.\n");
    I2C_PowerControl(gI2CDev, CSK_POWER_OFF);
    I2C_Uninitialize(gI2CDev);

    return 0;
}
