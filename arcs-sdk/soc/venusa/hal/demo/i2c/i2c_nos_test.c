/*
 * i2c_nos_chk.c
 *
 *  Created on: 2020骞�9鏈�8鏃�
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
    while(0){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 100000){\
            break;\
        }\
    }\
    }while(0)

typedef int (*function)(void);

static int I2Cx_Master_Interrupt_TransmitReceive();
static int I2Cx_Master_Interrupt_Page_TransmitReceive();
static int I2Cx_Master_Interrupt_Page_TransmitReceive_Below_FIFODepth();
static int I2Cx_Master_Interrupt_Repeatedly_TransmitReceive();
static int I2Cx_Master_DMA_TransmitReceive();
static int I2Cx_Master_DMA_Page_TransmitReceive();
static int I2Cx_Master_DMA_Repeatedly_TransmitReceive();

static int I2Cx_Master_Transmit_Slave_Receive_DMA();
static int I2Cx_Master_Transmit_Slave_Receive_INT();
static int I2Cx_Master_Receive_Slave_Transmit_DMA();
static int I2Cx_Master_Receive_Slave_Transmit_INT();

static int I2Cx_Strss_Test_Master_TransmitReceive_Int_STDMODE();
static int I2Cx_Strss_Test_Master_TransmitReceive_DMA_STDMODE();
static int I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTMODE();
static int I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTMODE();
static int I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTPMODE();
static int I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTPMODE();

static int I2Cx_Master_Transmit_Int_Abort_Test();
static int I2Cx_Master_Transmit_DMA_Abort_Test();
static int I2Cx_Master_Receive_Int_Abort_Test();
static int I2Cx_Master_Receive_DMA_Abort_Test();

static function test_function_array[] = {
    I2Cx_Master_Interrupt_TransmitReceive,  // success
    I2Cx_Master_Interrupt_Page_TransmitReceive,  // success
    I2Cx_Master_Interrupt_Page_TransmitReceive_Below_FIFODepth,  // success
    I2Cx_Master_Interrupt_Repeatedly_TransmitReceive,  // success
    I2Cx_Master_DMA_TransmitReceive,  // success
    I2Cx_Master_DMA_Page_TransmitReceive,  // success
    I2Cx_Master_DMA_Repeatedly_TransmitReceive,  // success
//    I2Cx_Master_Transmit_Slave_Receive_DMA,  // success
//    I2Cx_Master_Transmit_Slave_Receive_INT,  // success
//    I2Cx_Master_Receive_Slave_Transmit_DMA,  // success
//    I2Cx_Master_Receive_Slave_Transmit_INT,  // success

    I2Cx_Master_Interrupt_TransmitReceive,  // success
    I2Cx_Master_Transmit_Int_Abort_Test,  // success
    I2Cx_Master_Transmit_DMA_Abort_Test,  // success
    I2Cx_Master_Receive_Int_Abort_Test,  // success
    I2Cx_Master_Receive_DMA_Abort_Test,  // success
    I2Cx_Master_Interrupt_TransmitReceive,  // success

    I2Cx_Strss_Test_Master_TransmitReceive_Int_STDMODE,  // success  pclk=20;@50MHz
    I2Cx_Strss_Test_Master_TransmitReceive_DMA_STDMODE,  // success
    I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTMODE,  // success
    I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTMODE,  // success
    I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTPMODE,  // success
    I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTPMODE,  // success
};

#define IIC0_GPIO_SCL              (23)  // TC6
#define IIC0_GPIO_SDA              (22)  // TC7

#define IIC1_GPIO_SCL              (6)  // TC3
#define IIC1_GPIO_SDA              (7)  // TC2

#define DEV_SLAVE_ADDRESS    0x50   // AT24C02  256Byte  8Byte/page
#define OWN_SLAVE_ADDRESS    0x35

#define START_EEPROM_ADDRESS 0x00
#define EEPROM_ADDRESS_MARK  0xff
#define EEPROM_PAGE_MAX      8

static void* I2C_M_Handler = NULL;
static void* I2C_S_Handler = NULL;
static volatile uint32_t I2C_M_Event = 0;
static volatile uint32_t I2C_S_Event = 0;

static void I2C_Init_Handler(void){
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 1, 7);  // master:i2c0_scl
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 0, 7);  // master:i2c0_sda

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, 8);  // slave:i2c1_scl
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 31, 8);  // slave:i2c1_sda

    I2C_M_Handler = I2C0();
    I2C_S_Handler = I2C1();
}

static void I2C_M_EventCallback(uint32_t event, void* workspace){
    I2C_M_Event |= event;
}
static void I2C_S_EventCallback(uint32_t event, void* workspace){
    I2C_S_Event |= event;
}

static int I2Cx_Master_Interrupt_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[3] = {0x1, 0x50, 0x5A};
    uint8_t read_p = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

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
    Master_Transmit_Data[1] = 0x70;

    // move the pointer to 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, &read_p, 1, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    if(read_p == Master_Transmit_Data[2]){
        CLOGD("The read data from EEPROM equal to the write data");
    }else{
        ret = -1;
        CLOGD("Compare error in read data[0x%x], write data[0x%x], i=%d", read_p, Master_Transmit_Data[2]);
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

// Trigger full interrupt
static int I2Cx_Master_Interrupt_Page_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[] = {0x00, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    uint8_t read_p[32] = {0};
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

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
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, sizeof(Master_Transmit_Data) - 1, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    uint32_t i = 0;

    for(i = 0; i < sizeof(Master_Transmit_Data) - 1; i++){
        if(read_p[i] == Master_Transmit_Data[i + 1]){
            CLOGD("The read data from EEPROM equal to the write data: 0x%x, i=%d", Master_Transmit_Data[i + 1], i);
        }else{
            CLOGD("Compare error in read data[0x%x], write data[0x%x], i=%d", read_p[i], Master_Transmit_Data[i + 1], i);
            ret = -1;
            goto error;
        }
    }

    for(i = 0; i < sizeof(Master_Transmit_Data) - 2; i++){
        // move the pointer to 0x154
        Master_Transmit_Data[0]++;
        CLOG("[%s:%d] reg=0x%x", __func__, __LINE__, Master_Transmit_Data[0]);

        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));
        CLOG("[%s:%d] reg=0x%x value=0x%x", __func__, __LINE__, Master_Transmit_Data[0], read_p[0]);
        if(read_p[0] != Master_Transmit_Data[Master_Transmit_Data[0]+1])
        {
            ret = -2;
            goto error;
        }
    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Interrupt_Page_TransmitReceive_Below_FIFODepth(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[] = {0x00, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7};
    uint8_t read_p[32] = {0};
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

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
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, sizeof(Master_Transmit_Data) - 1, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    uint32_t i = 0;

    for(i = 0; i < sizeof(Master_Transmit_Data) - 1; i++){
        if(read_p[i] == Master_Transmit_Data[i + 1]){
            CLOGD("The read data from EEPROM equal to the write data: 0x%x", Master_Transmit_Data[i + 1]);
        }else{
            CLOGD("Compare error in read data[0x%x], write data[0x%x]", read_p[i], Master_Transmit_Data[i + 1]);
            ret = -1;
            goto error;
        }
    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Interrupt_Repeatedly_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[2] = {0x00, 0x55};
    uint8_t read_p = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    uint32_t loop = 20;
    while(loop--){
        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        SysTick_Delay_Ms(10);
        CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));
        Master_Transmit_Data[0]++;
    }

    loop = 20;
    while(loop--){
        Master_Transmit_Data[0]--;

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

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
            CLOGD("Compare error in read data[0x%x], write data 0x55", read_p);
            ret = -1;
            goto error;
        }
    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_DMA_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[2] = {0xA1, 0x55};
    uint8_t read_p = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    // write 0x55 in 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    SysTick_Delay_Ms(10);

    // move the pointer to 0x154
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

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
        CLOGD("Compare error in read data[0x%x], write data 0x55", read_p);
        ret = -1;
        goto error;
    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_DMA_Page_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[] = {0x00, 0x0A, 0x1A, 0x2A, 0x3A, 0x4A, 0x5A, 0x6A, 0x7A};
    uint8_t read_p[32] = {0};
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

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
    I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    // read the pointer data from EEPROM
    I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, sizeof(Master_Transmit_Data) - 1, 0);

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    uint32_t i = 0;

    for(i = 0; i < sizeof(Master_Transmit_Data) - 1; i++){
        if(read_p[i] == Master_Transmit_Data[i + 1]){
            CLOGD("The read data from EEPROM equal to the write data: %d", Master_Transmit_Data[i + 1]);
        }else{
            CLOGD("Compare error in read data[0x%x], write data[0x%x], i=%d", read_p[i], Master_Transmit_Data[i + 1], i);
            ret = -1;
            goto error;
        }
    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_DMA_Repeatedly_TransmitReceive(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[2] = {0x61, 0x55};
    uint8_t read_p = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    uint32_t loop = 20;
    while(loop--){
        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 2, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        SysTick_Delay_Ms(10);
        CLOGD("TX count: %d", I2C_GetDataCount(I2C_M_Handler));
        Master_Transmit_Data[0]++;
    }

    loop = 20;
    while(loop--){
        Master_Transmit_Data[0]--;

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

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
            CLOGD("Compare error in read data[0x%x], write data 0x55", read_p);
            ret = -1;
            goto error;
        }
    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
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

static int I2Cx_Master_Transmit_Slave_Receive_DMA(){
    uint8_t Master_Transmit_Data[10] = {0x5A, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t read_p[10] = {0};
    int ret = 0;
    uint32_t i = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    I2C_Initialize(I2C_S_Handler, I2C_S_EventCallback, NULL);
    I2C_PowerControl(I2C_S_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_S_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_S_Handler, CSK_I2C_OWN_ADDRESS, OWN_SLAVE_ADDRESS);

    // Send to slave, it will cause slave trigger address hit interrupt
    I2C_MasterTransmit(I2C_M_Handler, OWN_SLAVE_ADDRESS, Master_Transmit_Data, sizeof(Master_Transmit_Data), 0);
    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("MASTER TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    // Detect trigger slave receive interrupt
    while(!(I2C_S_Event & CSK_I2C_EVENT_SLAVE_RECEIVE));
    while(!(I2C_S_Event & CSK_I2C_EVENT_TRANSFER_DONE));
    I2C_S_Event = 0;

    I2C_SlaveReceive(I2C_S_Handler, read_p, sizeof(Master_Transmit_Data));

    CLOGD("SLAVE RX count: %d", I2C_GetDataCount(I2C_S_Handler));

    for(i = 0; i < sizeof(Master_Transmit_Data); i++){
        if(read_p[i] == Master_Transmit_Data[i]){
            CLOGD("The read data from EEPROM equal to the write data: 0x%x, i=%d", Master_Transmit_Data[i], i);
        }else{
            CLOGD("Compare error in read data[0x%x], write data[0x%x], i=%d", read_p[i], Master_Transmit_Data[i], i);
            ret = -1;
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    I2C_PowerControl(I2C_S_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_S_Handler);

    I2C_S_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Transmit_Slave_Receive_INT(){
    uint8_t Master_Transmit_Data[10] = {0x5A, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t read_p[10] = {0};
    int ret = 0;
    uint32_t i = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

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
    I2C_MasterTransmit(I2C_M_Handler, OWN_SLAVE_ADDRESS, Master_Transmit_Data, sizeof(Master_Transmit_Data), 0);
    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("MASTER TX count: %d", I2C_GetDataCount(I2C_M_Handler));

    // Detect trigger slave receive interrupt
    while(!(I2C_S_Event & CSK_I2C_EVENT_SLAVE_RECEIVE));
    while(!(I2C_S_Event & CSK_I2C_EVENT_TRANSFER_DONE));
    I2C_S_Event = 0;

    I2C_SlaveReceive(I2C_S_Handler, read_p, sizeof(Master_Transmit_Data));

    CLOGD("SLAVE RX count: %d", I2C_GetDataCount(I2C_S_Handler));

    for(i = 0; i < sizeof(Master_Transmit_Data); i++){
        if(read_p[i] == Master_Transmit_Data[i]){
            CLOGD("The read data from EEPROM equal to the write data: 0x%x, i=%d", Master_Transmit_Data[i], i);
        }else{
            CLOGD("Compare error in read data[0x%x], write data[0x%x], i=%d", read_p[i], Master_Transmit_Data[i], i);
            ret = -1;
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    I2C_PowerControl(I2C_S_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_S_Handler);

    I2C_S_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Receive_Slave_Transmit_DMA(){
    uint8_t Slave_Transmit_Data[10] = {0x5A, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t Master_Read_Data[10] = {0};
    uint32_t i = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

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
    I2C_MasterReceive(I2C_M_Handler, OWN_SLAVE_ADDRESS, Master_Read_Data, sizeof(Slave_Transmit_Data), 0);

    // Waiting slave trigger transmit interrupt
    while(!(I2C_S_Event & CSK_I2C_EVENT_SLAVE_TRANSMIT));

    I2C_S_Event = 0;

    I2C_SlaveTransmit(I2C_S_Handler, Slave_Transmit_Data, sizeof(Slave_Transmit_Data));

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("MASTER RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    while(!(I2C_S_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_S_Event = 0;

    CLOGD("SLAVE TX count: %d", I2C_GetDataCount(I2C_S_Handler));

    for(i = 0; i < sizeof(Slave_Transmit_Data); i++){
        if(Slave_Transmit_Data[i] == Master_Read_Data[i]){
            CLOGD("The read data from EEPROM equal to the write data: 0x%x, i=%d", Master_Read_Data[i], i);
        }else{
            CLOGD("Compare error in read data[0x%x], write data[0x%x], i=%d", Master_Read_Data[i], Slave_Transmit_Data[i], i);
            ret = -1;
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    I2C_PowerControl(I2C_S_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_S_Handler);

    I2C_S_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Receive_Slave_Transmit_INT(){
    uint8_t Slave_Transmit_Data[10] = {0x5A, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t Master_Read_Data[10] = {0};
    uint32_t i = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

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
    I2C_MasterReceive(I2C_M_Handler, OWN_SLAVE_ADDRESS, Master_Read_Data, sizeof(Slave_Transmit_Data), 0);

    // Waiting slave trigger transmit interrupt
    while(!(I2C_S_Event & CSK_I2C_EVENT_SLAVE_TRANSMIT));

    I2C_S_Event = 0;

    I2C_SlaveTransmit(I2C_S_Handler, Slave_Transmit_Data, sizeof(Slave_Transmit_Data));

    while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_M_Event = 0;

    CLOGD("MASTER RX count: %d", I2C_GetDataCount(I2C_M_Handler));

    while(!(I2C_S_Event & CSK_I2C_EVENT_TRANSFER_DONE));

    I2C_S_Event = 0;

    CLOGD("SLAVE TX count: %d", I2C_GetDataCount(I2C_S_Handler));

    for(i = 0; i < sizeof(Slave_Transmit_Data); i++){
        if(Slave_Transmit_Data[i] == Master_Read_Data[i]){
            CLOGD("The read data from EEPROM equal to the write data: 0x%x, i=%d", Master_Read_Data[i], i);
        }else{
            CLOGD("Compare error in read data[0x%x], write data[0x%x], i=%d", Master_Read_Data[i], Slave_Transmit_Data[i], i);
            ret = -1;
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    I2C_PowerControl(I2C_S_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_S_Handler);

    I2C_S_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Strss_Test_Master_TransmitReceive_Int_STDMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[10] = {0};
    uint8_t Master_Receive_Data[10] = {0};
    uint8_t i = 0;
    int ret = 0;

    uint8_t EEPROM_address = START_EEPROM_ADDRESS;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(i = 0; i < EEPROM_PAGE_MAX; i++){
        Master_Transmit_Data[i+1] = i;
    }

    while(1){
        Master_Transmit_Data[0] = EEPROM_address & EEPROM_ADDRESS_MARK;

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        for(i = 0; i < EEPROM_PAGE_MAX; i++){
            if(Master_Receive_Data[i] == Master_Transmit_Data[i + 1]){
                CLOGD("The read data from EEPROM equal to the write data: 0x%x, addr=0x%x, i=%d", Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
            }else{
                CLOGD("Compare error in read data[0x%x], write data[0x%x], addr=0x%x, i=%d", Master_Receive_Data[i], Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
                ret = -1;
                goto error;
            }
        }

        EEPROM_address += EEPROM_PAGE_MAX;
        if((EEPROM_address & EEPROM_ADDRESS_MARK) == START_EEPROM_ADDRESS) {
            break;
        }

        SysTick_Delay_Ms(10);

    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Strss_Test_Master_TransmitReceive_DMA_STDMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[10] = {0};
    uint8_t Master_Receive_Data[10] = {0};
    uint8_t i = 0;
    int ret = 0;

    uint8_t EEPROM_address = START_EEPROM_ADDRESS;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(i = 0; i < EEPROM_PAGE_MAX; i++){
        Master_Transmit_Data[i+1] = i;
    }

    while(1){
        Master_Transmit_Data[0] = EEPROM_address & EEPROM_ADDRESS_MARK;

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        for(i = 0; i < EEPROM_PAGE_MAX; i++){
            if(Master_Receive_Data[i] == Master_Transmit_Data[i + 1]){
                CLOGD("The read data from EEPROM equal to the write data: 0x%x, addr=0x%x, i=%d", Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
            }else{
                CLOGD("Compare error in read data[0x%x], write data[0x%x], addr=0x%x, i=%d", Master_Receive_Data[i], Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
                ret = -1;
                goto error;
            }
        }

        EEPROM_address += EEPROM_PAGE_MAX;
        if((EEPROM_address & EEPROM_ADDRESS_MARK) == START_EEPROM_ADDRESS) {
            break;
        }

        SysTick_Delay_Ms(10);

    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[10] = {0};
    uint8_t Master_Receive_Data[10] = {0};
    uint8_t i = 0;
    int ret = 0;

    uint8_t EEPROM_address = START_EEPROM_ADDRESS;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(i = 0; i < EEPROM_PAGE_MAX; i++){
        Master_Transmit_Data[i+1] = i;
    }

    while(1){
        Master_Transmit_Data[0] = EEPROM_address & EEPROM_ADDRESS_MARK;

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        for(i = 0; i < EEPROM_PAGE_MAX; i++){
            if(Master_Receive_Data[i] == Master_Transmit_Data[i + 1]){
                CLOGD("The read data from EEPROM equal to the write data: 0x%x, addr=0x%x, i=%d", Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
            }else{
                CLOGD("Compare error in read data[0x%x], write data[0x%x], addr=0x%x, i=%d", Master_Receive_Data[i], Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
                ret = -1;
                goto error;
            }
        }

        EEPROM_address += EEPROM_PAGE_MAX;
        if((EEPROM_address & EEPROM_ADDRESS_MARK) == START_EEPROM_ADDRESS) {
            break;
        }

        SysTick_Delay_Ms(10);

    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[10] = {0};
    uint8_t Master_Receive_Data[10] = {0};
    uint8_t i = 0;
    int ret = 0;

    uint8_t EEPROM_address = START_EEPROM_ADDRESS;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(i = 0; i < EEPROM_PAGE_MAX; i++){
        Master_Transmit_Data[i+1] = i;
    }

    while(1){
        Master_Transmit_Data[0] = EEPROM_address & EEPROM_ADDRESS_MARK;

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        for(i = 0; i < EEPROM_PAGE_MAX; i++){
            if(Master_Receive_Data[i] == Master_Transmit_Data[i + 1]){
                CLOGD("The read data from EEPROM equal to the write data: 0x%x, addr=0x%x, i=%d", Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
            }else{
                CLOGD("Compare error in read data[0x%x], write data[0x%x], addr=0x%x, i=%d", Master_Receive_Data[i], Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
                ret = -1;
                goto error;
            }
        }

        EEPROM_address += EEPROM_PAGE_MAX;
        if((EEPROM_address & EEPROM_ADDRESS_MARK) == START_EEPROM_ADDRESS) {
            break;
        }

        SysTick_Delay_Ms(10);

    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Strss_Test_Master_TransmitReceive_Int_FASTPMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[10] = {0};
    uint8_t Master_Receive_Data[10] = {0};
    uint8_t i = 0;
    int ret = 0;

    uint8_t EEPROM_address = START_EEPROM_ADDRESS;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST_PLUS);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(i = 0; i < EEPROM_PAGE_MAX; i++){
        Master_Transmit_Data[i+1] = i;
    }

    while(1){
        Master_Transmit_Data[0] = EEPROM_address & EEPROM_ADDRESS_MARK;

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        for(i = 0; i < EEPROM_PAGE_MAX; i++){
            if(Master_Receive_Data[i] == Master_Transmit_Data[i + 1]){
                CLOGD("The read data from EEPROM equal to the write data: 0x%x, addr=0x%x, i=%d", Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
            }else{
                CLOGD("Compare error in read data[0x%x], write data[0x%x], addr=0x%x, i=%d", Master_Receive_Data[i], Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
                ret = -1;
                goto error;
            }
        }

        EEPROM_address += EEPROM_PAGE_MAX;
        if((EEPROM_address & EEPROM_ADDRESS_MARK) == START_EEPROM_ADDRESS) {
            break;
        }

        SysTick_Delay_Ms(10);

    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Strss_Test_Master_TransmitReceive_DMA_FASTPMODE(){
    // msb address, lsb address, data
    uint8_t Master_Transmit_Data[10] = {0};
    uint8_t Master_Receive_Data[10] = {0};
    uint8_t i = 0;
    int ret = 0;

    uint8_t EEPROM_address = START_EEPROM_ADDRESS;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST_PLUS);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    for(i = 0; i < EEPROM_PAGE_MAX; i++){
        Master_Transmit_Data[i+1] = i;
    }

    while(1){
        Master_Transmit_Data[0] = EEPROM_address & EEPROM_ADDRESS_MARK;

        // write 0x55 in 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, EEPROM_PAGE_MAX + 1, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;


        SysTick_Delay_Ms(10);

        // move the pointer to 0x154
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

       // read the pointer data from EEPROM
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Receive_Data, EEPROM_PAGE_MAX, 0);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        for(i = 0; i < EEPROM_PAGE_MAX; i++){
            if(Master_Receive_Data[i] == Master_Transmit_Data[i + 1]){
                CLOGD("The read data from EEPROM equal to the write data: 0x%x, addr=0x%x, i=%d", Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
            }else{
                CLOGD("Compare error in read data[0x%x], write data[0x%x], addr=0x%x, i=%d", Master_Receive_Data[i], Master_Transmit_Data[i + 1], Master_Transmit_Data[0], i);
                ret = -1;
                goto error;
            }
        }

        EEPROM_address += EEPROM_PAGE_MAX;
        if((EEPROM_address & EEPROM_ADDRESS_MARK) == START_EEPROM_ADDRESS) {
            break;
        }

        SysTick_Delay_Ms(10);

    }

error:
    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Transmit_Int_Abort_Test(){
    uint8_t Master_Transmit_Data[12] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t times = 100;
    uint8_t i2c_cnt = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(times--){

        // Transmit data to slave device
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 12, 0);

        // delay a moment

        SysTick_Delay_Ms(1);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);

        i2c_cnt = I2C_GetDataCount(I2C_M_Handler);
        CLOGD("MASTER TX count: %d", i2c_cnt);
        if(ret == 12) {
            ret = -1;
            CLOGD("error: MASTER TX count: %d, times=%d", i2c_cnt, times);
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Transmit_DMA_Abort_Test(){
    uint8_t Master_Transmit_Data[12] = {0x1, 0x54, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t times = 100;
    uint8_t i2c_cnt = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(times--){

        // Transmit data to slave device
        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, Master_Transmit_Data, 12, 0);

        // delay a moment

        SysTick_Delay_Ms(1);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);

        i2c_cnt = I2C_GetDataCount(I2C_M_Handler);
        CLOGD("MASTER TX count: %d", i2c_cnt);
        if(ret == 12) {
            ret = -1;
            CLOGD("error: MASTER TX count: %d, times=%d", i2c_cnt, times);
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Receive_Int_Abort_Test(){
    uint8_t addr = 0;
    uint8_t read_p[8] = {0};
    uint8_t times = 100;
    uint8_t i2c_cnt = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(times--){

        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, &addr, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        // Transmit data to slave device
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, 8, 0);

        // delay a moment

        SysTick_Delay_Ms(1);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);

        i2c_cnt = I2C_GetDataCount(I2C_M_Handler);
        CLOGD("MASTER RX count: %d", i2c_cnt);
        if(ret == 8) {
            ret = -1;
            CLOGD("error: MASTER RX count: %d, times=%d", i2c_cnt, times);
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

static int I2Cx_Master_Receive_DMA_Abort_Test(){
    uint8_t addr = 0;
    uint8_t read_p[8] = {0};
    uint8_t times = 100;
    uint8_t i2c_cnt = 0;
    int ret = 0;

    CLOG("******** [%s:%d] test begin ********", __func__, __LINE__);

    I2C_Initialize(I2C_M_Handler, I2C_M_EventCallback, NULL);
    I2C_PowerControl(I2C_M_Handler, CSK_POWER_FULL);
    I2C_Control(I2C_M_Handler, CSK_I2C_TRANSMIT_MODE, 1);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);
    I2C_Control(I2C_M_Handler, CSK_I2C_BUS_CLEAR, 0);

    while(times--){

        I2C_MasterTransmit(I2C_M_Handler, DEV_SLAVE_ADDRESS, &addr, 1, 1);

        while(!(I2C_M_Event & CSK_I2C_EVENT_TRANSFER_DONE));

        I2C_M_Event = 0;

        // Transmit data to slave device
        I2C_MasterReceive(I2C_M_Handler, DEV_SLAVE_ADDRESS, read_p, 8, 0);

        // delay a moment

        SysTick_Delay_Ms(1);

        // abort Master transmit
        I2C_Control(I2C_M_Handler, CSK_I2C_ABORT_TRANSFER, 0);

        i2c_cnt = I2C_GetDataCount(I2C_M_Handler);
        CLOGD("MASTER RX count: %d", i2c_cnt);
        if(ret == 8) {
            ret = -1;
            CLOGD("error: MASTER RX count: %d, times=%d", i2c_cnt, times);
        }
    }

    FAKE_WHILE();

    I2C_PowerControl(I2C_M_Handler, CSK_POWER_OFF);

    I2C_Uninitialize(I2C_M_Handler);

    I2C_M_Event = 0;

    if(ret == 0) {
        CLOG("******** [%s:%d] test success ********", __func__, __LINE__);
    } else {
        CLOG("******** [%s:%d] test failed ********", __func__, __LINE__);
    }
    CLOG("");
    return ret;
}

int main(){
    uint8_t case_inex = 0;
    int case_ret[sizeof(test_function_array)/sizeof(test_function_array[0])] = {0};

    logInit(0, 921600);
    CLOGD("I2C VALIDATION");

    //i2c clk en
//    __HAL_CRM_I2C0_CLK_ENABLE();
//    __HAL_CRM_I2C1_CLK_ENABLE();
    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_I2C0_CLK = 1;
    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_I2C1_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I2C0_RESET = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I2C1_RESET = 1;

    //__HAL_CRM_MTIME_CLK_ENABLE();

    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_13 = 0;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_14 = 0;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_29 = 0;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_30 = 0;

    CLOG("[%s:%d] all case test begin", __func__, __LINE__);

    I2C_Init_Handler();

    for(case_inex = 0; case_inex < sizeof(test_function_array)/sizeof(test_function_array[0]); case_inex++){
        CLOG("[%s:%d] test case index=%d", __func__, __LINE__, case_inex);
        case_ret[case_inex] = test_function_array[case_inex]();
    }
    CLOG("\n************************************************");
    for(case_inex = 0; case_inex < sizeof(test_function_array)/sizeof(test_function_array[0]); case_inex++){

        CLOG("[%s:%d] index=%d ret=%d", __func__, __LINE__, case_inex, case_ret[case_inex]);
    }
    CLOG("************************************************\n");

    CLOG("[%s:%d] all case test end", __func__, __LINE__);
    while(1);
}
