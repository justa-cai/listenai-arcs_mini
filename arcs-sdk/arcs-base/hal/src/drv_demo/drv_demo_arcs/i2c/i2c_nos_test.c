/*
 * i2c_nos_chk.c
 *
 *  Created on: 2020年9月8日
 *      Author: USER
 */
#include "systick.h"

#include "IOMuxManager.h"
#include "ClockManager.h"

#include "Driver_I2C.h"
#include "log_print.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 100000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static void I2Cx_Master_Interrupt_TransmitReceive();
static void I2Cx_Master_Interrupt_Page_TransmitReceive();
static void I2Cx_Master_Interrupt_Page_TransmitReceive_Below_FIFODepth();
static void I2Cx_Master_Interrupt_Repeatedly_TransmitReceive();
static void I2Cx_Master_DMA_TransmitReceive();
static void I2Cx_Master_DMA_Page_TransmitReceive();
static void I2Cx_Master_DMA_Repeatedly_TransmitReceive();

static void I2Cx_Master_Transmit_Slave_Receive_DMA();
static void I2Cx_Master_Transmit_Slave_Receive_INT();
static void I2Cx_Master_Receive_Slave_Transmit_DMA();
static void I2Cx_Master_Receive_Slave_Transmit_INT();

static void I2Cx_Strss_Test_Master_TransmitReceive_Int_STDMODE();
static void I2Cx_Strss_Test_Master_TransmitReceive_DMA_STDMODE();
static void I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTMODE();
static void I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTMODE();
static void I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTPMODE();
static void I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTPMODE();

static void I2Cx_Master_Transmit_Int_Abort_Test();
static void I2Cx_Master_Transmit_DMA_Abort_Test();
static void I2Cx_Master_Receive_Int_Abort_Test();
static void I2Cx_Master_Receive_DMA_Abort_Test();

static function test_function_array[] = {
 //   I2Cx_Master_Interrupt_TransmitReceive,
//    I2Cx_Master_Interrupt_Page_TransmitReceive,
//    I2Cx_Master_Interrupt_Page_TransmitReceive_Below_FIFODepth,
//    I2Cx_Master_Interrupt_Repeatedly_TransmitReceive,
//    I2Cx_Master_DMA_TransmitReceive,
//    I2Cx_Master_DMA_Page_TransmitReceive,
//    I2Cx_Master_DMA_Repeatedly_TransmitReceive,
//    I2Cx_Master_Transmit_Slave_Receive_DMA,
//    I2Cx_Master_Transmit_Slave_Receive_INT,
    I2Cx_Master_Receive_Slave_Transmit_DMA,
//    I2Cx_Master_Receive_Slave_Transmit_INT,

//    I2Cx_Master_Transmit_Int_Abort_Test,
//    I2Cx_Master_Transmit_DMA_Abort_Test,
//    I2Cx_Master_Receive_Int_Abort_Test,
//    I2Cx_Master_Receive_DMA_Abort_Test,

//    I2Cx_Strss_Test_Master_TransmitReceive_Int_STDMODE,
//    I2Cx_Strss_Test_Master_TransmitReceive_DMA_STDMODE,
//    I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTMODE,
//    I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTMODE,
//    I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTPMODE,
//    I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTPMODE,
};

#define IIC0_GPIO_SCL              (4)  // TC6
#define IIC0_GPIO_SDA              (5)  // TC7

#define IIC1_GPIO_SCL              (6)  // TC3
#define IIC1_GPIO_SDA              (7)  // TC2

#define DEV_SLAVE_ADDRESS    0x50
#define OWN_SLAVE_ADDRESS    0x35

static void* I2C_M_Handler = NULL;
static void* I2C_S_Handler = NULL;
static volatile uint32_t I2C_M_Event = 0;
static volatile uint32_t I2C_S_Event = 0;

static void I2C_Init_Handler(void){
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC0_GPIO_SDA, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC0_GPIO_SCL, CSK_IOMUX_FUNC_ALTER8);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC1_GPIO_SDA, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC1_GPIO_SCL, CSK_IOMUX_FUNC_ALTER8);

    I2C_M_Handler = I2C0();
    I2C_S_Handler = I2C1();
}

static void I2C_M_EventCallback(uint32_t event, void* workspace){
    I2C_M_Event |= event;
}
static void I2C_S_EventCallback(uint32_t event, void* workspace){
    I2C_S_Event |= event;
}

static void I2Cx_Master_Interrupt_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[3] = {0x1, 0x54, 0x55};
    uint8_t read_p = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    // write 0x55 in 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 3, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    SysTick_Delay_Ms(10);

    // move the pointer to 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, &read_p, 1, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    if(read_p == 0x55){
        CLOGD("The read data from EEPROM equal to the write data: 0x55");
    }else{
        CLOGD("Compare error");
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

// Trigger full interrupt
static void I2Cx_Master_Interrupt_Page_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,\
            0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    uint8_t read_p[32] = {0};

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    // write 0x55 in 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, sizeof(Master_Transmit_Data), 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    SysTick_Delay_Ms(10);

    // move the pointer to 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, sizeof(Master_Transmit_Data) - 2, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    uint32_t i = 0;

    for(i = 0; i < sizeof(Master_Transmit_Data) - 2; i++){
        if(read_p[i] == Master_Transmit_Data[i + 2]){
            CLOGD("The read data from EEPROM equal to the write data: %d", Master_Transmit_Data[i + 2]);
        }else{
            CLOGD("Compare error in read data[%d], write data[%d]", read_p[i], Master_Transmit_Data[i + 2]);
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_Interrupt_Page_TransmitReceive_Below_FIFODepth(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t read_p[32] = {0};

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    // write 0x55 in 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, sizeof(Master_Transmit_Data), 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    SysTick_Delay_Ms(10);

    // move the pointer to 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, sizeof(Master_Transmit_Data) - 2, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    uint32_t i = 0;

    for(i = 0; i < sizeof(Master_Transmit_Data) - 2; i++){
        if(read_p[i] == Master_Transmit_Data[i + 2]){
            CLOGD("The read data from EEPROM equal to the write data: %d", Master_Transmit_Data[i + 2]);
        }else{
            CLOGD("Compare error in read data[%d], write data[%d]", read_p[i], Master_Transmit_Data[i + 2]);
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_Interrupt_Repeatedly_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[3] = {0x1, 0x54, 0x55};
    uint8_t read_p = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    uint32_t loop = 20;
    while(loop--){
        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 3, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        SysTick_Delay_Ms(10);
        CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));
        Master_Transmit_Data[1]++;
    }

    loop = 20;
    while(loop--){
        Master_Transmit_Data[1]--;

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, &read_p, 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

        if(read_p == 0x55){
            CLOGD("The read data from EEPROM equal to the write data: 0x55");
        }else{
            CLOGD("Compare error");
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_DMA_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[3] = {0x1, 0x54, 0x55};
    uint8_t read_p = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    // write 0x55 in 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 3, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    SysTick_Delay_Ms(10);

    // move the pointer to 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, &read_p, 1, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    if(read_p == 0x55){
        CLOGD("The read data from EEPROM equal to the write data: 0x55");
    }else{
        CLOGD("Compare error");
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_DMA_Page_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,\
            0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    uint8_t read_p[32] = {0};

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    // write 0x55 in 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, sizeof(Master_Transmit_Data), 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    SysTick_Delay_Ms(10);

    // move the pointer to 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, sizeof(Master_Transmit_Data) - 2, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    uint32_t i = 0;

    for(i = 0; i < sizeof(Master_Transmit_Data) - 2; i++){
        if(read_p[i] == Master_Transmit_Data[i + 2]){
            CLOGD("The read data from EEPROM equal to the write data: %d", Master_Transmit_Data[i + 2]);
        }else{
            CLOGD("Compare error in read data[%d], write data[%d]", read_p[i], Master_Transmit_Data[i + 2]);
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_DMA_Repeatedly_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[3] = {0x1, 0x54, 0x55};
    uint8_t read_p = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    uint32_t loop = 20;
    while(loop--){
        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 3, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        SysTick_Delay_Ms(10);
        CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));
        Master_Transmit_Data[1]++;
    }

    loop = 20;
    while(loop--){
        Master_Transmit_Data[1]--;

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, &read_p, 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

        if(read_p == 0x55){
            CLOGD("The read data from EEPROM equal to the write data: 0x55");
        }else{
            CLOGD("Compare error");
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

volatile static uint8_t slave_read_pending = 0;
volatile static uint8_t slave_send_pending = 0;
static uint8_t slave_read_buffer[100] = {0};
static uint8_t slave_send_memory[] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

static void I2C_EventCallback(uint32_t event, void* workspace){
    if (event & CSK_I2C_EVENT_SLAVE_RECEIVE){
        // means master will send data to slave, address hit
        slave_read_pending = 1;
    }

    if (event & CSK_I2C_EVENT_SLAVE_TRANSMIT){
        slave_send_pending = 1;

        I2C_SlaveTransmit(I2C_M_Handler, &slave_send_memory[0], 3);
    }

    if (event & CSK_I2C_EVENT_TRANSFER_DONE){
        if (slave_read_pending){
            uint8_t size = 0;
            size = I2C_GetDataCount(I2C_M_Handler);

            I2C_SlaveReceive(I2C_M_Handler, slave_read_buffer, size);

            slave_read_pending = 0;
        } else if (slave_send_pending){
            slave_send_pending = 0;
        }
    }
}

static void I2Cx_Master_Transmit_Slave_Receive_DMA(){
#define I2C0_SLAVE_ADDRESS     0x35

    I2C_Initialize(I2C_M_Handler, I2C_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_OWN_ADDRESS, I2C0_SLAVE_ADDRESS);

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_Transmit_Slave_Receive_INT(){
    uint8_t Master_Transmit_Data[10] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t read_p[10] = {0};

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    I2C_Initialize(I2C_S_Handler, I2C_S_EventCallback, NULL);
    I2C_PowerControl(I2C_S_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_S_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_S_Handler, CSK_I2C_OWN_ADDRESS, OWN_SLAVE_ADDRESS);

    // Send to slave, it will cause slave trigger address hit interrupt
    I2C_MasterTransmit(I2C_M_Handler, OWN_SLAVE_ADDRESS, Master_Transmit_Data, 10, 0);
    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("MASTER TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    // Detect trigger slave receive interrupt
    while(!(I2C_S_Event & CSK_I2C_EVENT_SLAVE_RECEIVE));
    while(!(I2C_S_Event & CSK_I2C_EVENT_TRANSFER_DONE));
    I2C_S_Event = 0;

    I2C_SlaveReceive(I2C_S_Handler, read_p, 10);

    CLOGD("SLAVE RX count: %d", I2C_GetDataCount(I2C_S_Handler));

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    I2C_PowerControl(I2C_S_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_S_Handler);

    I2C_S_Event = 0;
}

static void I2Cx_Master_Receive_Slave_Transmit_DMA(){
    uint8_t Slave_Transmit_Data[10] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t Master_Read_Data[10] = {0};
    uint32_t i = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    I2C_Initialize(I2C_S_Handler, I2C_S_EventCallback, NULL);
    I2C_PowerControl(I2C_S_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_S_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_S_Handler, CSK_I2C_OWN_ADDRESS, OWN_SLAVE_ADDRESS);

    // Master read data from slave
    I2C_MasterReceive(I2C_M_Handler, OWN_SLAVE_ADDRESS, Master_Read_Data, 10, 0);

    // Waiting slave trigger transmit interrupt
    while(!(I2C_S_Event & CSK_I2C_EVENT_SLAVE_TRANSMIT));

    I2C_S_Event = 0;

    I2C_SlaveTransmit(I2C_S_Handler, Slave_Transmit_Data, 10);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("MASTER RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    while(!(I2C_S_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_S_Event = 0;

    CLOGD("SLAVE TX count: %d", I2C_GetDataCount(I2C_S_Handler));

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    I2C_PowerControl(I2C_S_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_S_Handler);

    I2C_S_Event = 0;
}

static void I2Cx_Master_Receive_Slave_Transmit_INT(){
    uint8_t Slave_Transmit_Data[10] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t Master_Read_Data[10] = {0};
    uint32_t i = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    I2C_Initialize(I2C_S_Handler, I2C_S_EventCallback, NULL);
    I2C_PowerControl(I2C_S_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_S_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_S_Handler, CSK_I2C_OWN_ADDRESS, OWN_SLAVE_ADDRESS);

    // Master read data from slave
    I2C_MasterReceive(I2C_M_Handler, OWN_SLAVE_ADDRESS, Master_Read_Data, 10, 0);

    // Waiting slave trigger transmit interrupt
    while(!(I2C_S_Event & CSK_I2C_EVENT_SLAVE_TRANSMIT));

    I2C_S_Event = 0;

    I2C_SlaveTransmit(I2C_S_Handler, Slave_Transmit_Data, 10);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("MASTER RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    while(!(I2C_S_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_S_Event = 0;

    CLOGD("SLAVE TX count: %d", I2C_GetDataCount(I2C_S_Handler));

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    I2C_PowerControl(I2C_S_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_S_Handler);

    I2C_S_Event = 0;
}

static void I2Cx_Strss_Test_Master_TransmitReceive_Int_STDMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[66] = {0};
    uint8_t Master_Receive_Data[64] = {0};
    uint8_t Transmit_Data = 0;

#define START_EEPROM_ADDRESS 0x0
#define EEPROM_ADDRESS_MARK  0x3fff
#define EEPROM_PAGE_MAX      64

    uint16_t EEPROM_address = START_EEPROM_ADDRESS;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(Transmit_Data = 0; Transmit_Data < EEPROM_PAGE_MAX; Transmit_Data++){
        Master_Transmit_Data[Transmit_Data+2] = Transmit_Data;
    }

    while(1){
        uint8_t address[2] = {0};
        address[0] = ((EEPROM_address & EEPROM_ADDRESS_MARK) >> 8) & 0xff;
        address[1] = (EEPROM_address & EEPROM_ADDRESS_MARK) & 0xff;
        Master_Transmit_Data[0] = address[0];
        Master_Transmit_Data[1] = address[1];

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        EEPROM_address += EEPROM_PAGE_MAX;

        SysTick_Delay_Ms(10);

    }
}

static void I2Cx_Strss_Test_Master_TransmitReceive_DMA_STDMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[66] = {0};
    uint8_t Master_Receive_Data[64] = {0};
    uint8_t Transmit_Data = 0;

#define START_EEPROM_ADDRESS 0x0
#define EEPROM_ADDRESS_MARK  0x3fff
#define EEPROM_PAGE_MAX      64

    uint16_t EEPROM_address = START_EEPROM_ADDRESS;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(Transmit_Data = 0; Transmit_Data < EEPROM_PAGE_MAX; Transmit_Data++){
        Master_Transmit_Data[Transmit_Data+2] = Transmit_Data;
    }

    while(1){
        uint8_t address[2] = {0};
        address[0] = ((EEPROM_address & EEPROM_ADDRESS_MARK) >> 8) & 0xff;
        address[1] = (EEPROM_address & EEPROM_ADDRESS_MARK) & 0xff;
        Master_Transmit_Data[0] = address[0];
        Master_Transmit_Data[1] = address[1];

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        EEPROM_address += EEPROM_PAGE_MAX;

        SysTick_Delay_Ms(10);

    }
}

static void I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[66] = {0};
    uint8_t Master_Receive_Data[64] = {0};
    uint8_t Transmit_Data = 0;

#define START_EEPROM_ADDRESS 0x0
#define EEPROM_ADDRESS_MARK  0x3fff
#define EEPROM_PAGE_MAX      64

    uint16_t EEPROM_address = START_EEPROM_ADDRESS;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(Transmit_Data = 0; Transmit_Data < EEPROM_PAGE_MAX; Transmit_Data++){
        Master_Transmit_Data[Transmit_Data+2] = Transmit_Data;
    }

    while(1){
        uint8_t address[2] = {0};
        address[0] = ((EEPROM_address & EEPROM_ADDRESS_MARK) >> 8) & 0xff;
        address[1] = (EEPROM_address & EEPROM_ADDRESS_MARK) & 0xff;
        Master_Transmit_Data[0] = address[0];
        Master_Transmit_Data[1] = address[1];

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        EEPROM_address += EEPROM_PAGE_MAX;

        SysTick_Delay_Ms(10);

    }
}

static void I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[66] = {0};
    uint8_t Master_Receive_Data[64] = {0};
    uint8_t Transmit_Data = 0;

#define START_EEPROM_ADDRESS 0x0
#define EEPROM_ADDRESS_MARK  0x3fff
#define EEPROM_PAGE_MAX      64

    uint16_t EEPROM_address = START_EEPROM_ADDRESS;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(Transmit_Data = 0; Transmit_Data < EEPROM_PAGE_MAX; Transmit_Data++){
        Master_Transmit_Data[Transmit_Data+2] = Transmit_Data;
    }

    while(1){
        uint8_t address[2] = {0};
        address[0] = ((EEPROM_address & EEPROM_ADDRESS_MARK) >> 8) & 0xff;
        address[1] = (EEPROM_address & EEPROM_ADDRESS_MARK) & 0xff;
        Master_Transmit_Data[0] = address[0];
        Master_Transmit_Data[1] = address[1];

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        EEPROM_address += EEPROM_PAGE_MAX;

        SysTick_Delay_Ms(10);

    }
}

static void I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTPMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[66] = {0};
    uint8_t Master_Receive_Data[64] = {0};
    uint8_t Transmit_Data = 0;

#define START_EEPROM_ADDRESS 0x0
#define EEPROM_ADDRESS_MARK  0x3fff
#define EEPROM_PAGE_MAX      64

    uint16_t EEPROM_address = START_EEPROM_ADDRESS;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST_PLUS);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(Transmit_Data = 0; Transmit_Data < EEPROM_PAGE_MAX; Transmit_Data++){
        Master_Transmit_Data[Transmit_Data+2] = Transmit_Data;
    }

    while(1){
        uint8_t address[2] = {0};
        address[0] = ((EEPROM_address & EEPROM_ADDRESS_MARK) >> 8) & 0xff;
        address[1] = (EEPROM_address & EEPROM_ADDRESS_MARK) & 0xff;
        Master_Transmit_Data[0] = address[0];
        Master_Transmit_Data[1] = address[1];

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        EEPROM_address += EEPROM_PAGE_MAX;

        SysTick_Delay_Ms(10);

    }
}

static void I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTPMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[66] = {0};
    uint8_t Master_Receive_Data[64] = {0};
    uint8_t Transmit_Data = 0;

#define START_EEPROM_ADDRESS 0x0
#define EEPROM_ADDRESS_MARK  0x3fff
#define EEPROM_PAGE_MAX      64

    uint16_t EEPROM_address = START_EEPROM_ADDRESS;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST_PLUS);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(Transmit_Data = 0; Transmit_Data < EEPROM_PAGE_MAX; Transmit_Data++){
        Master_Transmit_Data[Transmit_Data+2] = Transmit_Data;
    }

    while(1){
        uint8_t address[2] = {0};
        address[0] = ((EEPROM_address & EEPROM_ADDRESS_MARK) >> 8) & 0xff;
        address[1] = (EEPROM_address & EEPROM_ADDRESS_MARK) & 0xff;
        Master_Transmit_Data[0] = address[0];
        Master_Transmit_Data[1] = address[1];

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        EEPROM_address += EEPROM_PAGE_MAX;

        SysTick_Delay_Ms(10);

    }
}

static void I2Cx_Master_Transmit_Int_Abort_Test(){
    uint8_t Master_Transmit_Data[12] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(1){

        // Transmit data to slave device
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 12, 0);

        // delay a moment

        SysTick_Delay_Ms(1);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);

       // CLOGD("MASTER TX count: %d", I2C_GetDataCount(I2C_M_Handler));
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_Transmit_DMA_Abort_Test(){
    uint8_t Master_Transmit_Data[12] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(1){

        // Transmit data to slave device
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 12, 0);

        // delay a moment

        SysTick_Delay_Ms(1);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);

        CLOGD("MASTER TX count: %d", I2C_GetDataCount(I2C_M_Handler));
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_Receive_Int_Abort_Test(){
    uint8_t read_p = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(1){

        // Transmit data to slave device
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, &read_p, 1, 0);

        // delay a moment

        SysTick_Delay_Ms(1);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

static void I2Cx_Master_Receive_DMA_Abort_Test(){
    uint8_t read_p = 0;

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(1){

        // Transmit data to slave device
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, &read_p, 1, 0);

        // delay a moment

        SysTick_Delay_Ms(10);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;
}

int main(){
    uint32_t times;
    logInit(0, 115200);
    CLOGD("I2C VALIDATION");

    //i2c clk en
   // __HAL_CRM_I2C0_CLK_ENABLE();
   // __HAL_CRM_I2C1_CLK_ENABLE();

    __HAL_CRM_MTIME_CLK_ENABLE();

    I2C_Init_Handler();
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}
