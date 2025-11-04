/*
 * ir_nos_chk.c
 *
 *  Created on: 2020年9月27日
 *      Author: USER
 */
#include "IOMuxManager.h"

#include "Driver_IR.h"
#include "log_print.h"
#include "systick.h"
#include "chip.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define FAKE_WHILE()   do{\
    int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 1000000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static void IR_HardWare_NEC_Tx();
static void IR_HardWare_NEC_Rx();
static void IR_HardWare_NEC_Rx_Repeatedly();
static void IR_HardWare_NEC_Tx_Repeat();
static void IR_HardWare_9012_Tx();
static void IR_HardWare_9012_Rx();
static void IR_HardWare_RC5_Tx();
static void IR_HardWare_RC5_Rx();
static void IR_SoftWare_Tx();
static void IR_SoftWare_Tx_Abort();
static void IR_SoftWare_Change_Carry();
static void IR_SoftWare_Rx();
static void IR_SoftWare_Rx_Abort();
static void IR_SoftWare_Change_Timeout();
static void IR_SoftWare_Training();
static function test_function_array[] = {
    IR_HardWare_NEC_Tx,
    IR_HardWare_NEC_Rx,
    IR_HardWare_NEC_Rx_Repeatedly,
    IR_HardWare_NEC_Tx_Repeat,
    IR_HardWare_9012_Tx,
    IR_HardWare_9012_Rx,
    IR_HardWare_RC5_Tx,
    IR_HardWare_RC5_Rx,
    IR_SoftWare_Tx,
    IR_SoftWare_Tx_Abort,
    IR_SoftWare_Change_Carry,
    IR_SoftWare_Rx,
    IR_SoftWare_Rx_Abort,
    IR_SoftWare_Change_Timeout,
    IR_SoftWare_Training,
};

#define IR_DATA_OUT             (2)
#define IR_DATA_IN              (3)

static void* IR0_Handler = NULL;

static void IR_Init_Handler(){
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IR_DATA_OUT, CSK_IOMUX_FUNC_ALTER13);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IR_DATA_IN, CSK_IOMUX_FUNC_ALTER13);

    IR0_Handler = IR0();
}

static volatile uint32_t IR_Event = 0;

static void IR_EventCallback(uint32_t event, void* workspace){
    IR_Event |= event;
    CLOGD("Trigger: %d", event);
}

static void IR_HardWare_NEC_Tx(){
    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    while(1){
        IR_HW_Send(IR0_Handler, IR_MODE_NEC, 0x55aa, 0x55aa);
        while(!(IR_Event & CSK_IR_EVENT_HW_SEND_COMPLETE));
        IR_Event = 0;
        SysTick_Delay_Ms(1000);
    }

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_HardWare_NEC_Rx(){
    uint16_t address = 0;
    uint16_t command = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_HW_Receive(IR0_Handler, IR_MODE_NEC, &address, &command);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static uint16_t glb_address = 0;
static uint16_t glb_command = 0;

static void IR_EventCallback_Repeatedly(uint32_t event, void* workspace){
    if (event == CSK_IR_EVENT_HW_RECEIVE_COMPLETE){
        CLOGD("Receive address: 0x%x     command: 0x%x", glb_address, glb_command);
        IR_HW_Receive(IR0_Handler, IR_MODE_NEC, &glb_address, &glb_command);
    }
    if (event == CSK_IR_EVENT_HW_RX_REPEAT_TRIGGER){
        CLOGD("Receive repeat");
    }
}

static void IR_HardWare_NEC_Rx_Repeatedly(){
    IR_Initialize(IR0_Handler, IR_EventCallback_Repeatedly, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_HW_Receive(IR0_Handler, IR_MODE_NEC, &glb_address, &glb_command);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_HardWare_NEC_Tx_Repeat(){
    IR_Event = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_HW_Send(IR0_Handler, IR_MODE_NEC, 0x55aa, 0x55aa);

    while(!(IR_Event & CSK_IR_EVENT_HW_SEND_COMPLETE));
    IR_Event = 0;

    SysTick_Delay_Ms(110);

    // generate repeat signal
    IR_Control(IR0_Handler, CSK_IR_CONTROL_TX_REPEAT, 0);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_HardWare_9012_Tx(){
    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_HW_Send(IR0_Handler, IR_MODE_TOSHIBA_9012, 0x1234, 0x5678);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_HardWare_9012_Rx(){
    uint16_t address = 0;
    uint16_t command = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_HW_Receive(IR0_Handler, IR_MODE_TOSHIBA_9012, &address, &command);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_HardWare_RC5_Tx(){
    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_HW_Send(IR0_Handler, IR_MODE_PHILIPS_RC5, 0x56, 0x43);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_HardWare_RC5_Rx(){
    uint16_t address = 0;
    uint16_t command = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_HW_Receive(IR0_Handler, IR_MODE_PHILIPS_RC5, &address, &command);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

void ir_nec_trans_packup(uint16_t usercode, uint16_t datacode, uint16_t *nec_trans_data ){
    uint16_t nec_header[] = {0x8004, 0x11f, 0x808f}; //idle, s1, s2
    uint16_t nec_bit_1[]  = {0x11, 0x8035};
    uint16_t nec_bit_0[]  = {0x11, 0x8011};
    uint16_t nec_end[]    = {0x10};

    nec_trans_data[0] = nec_header[0];
    nec_trans_data[1] = nec_header[1];
    nec_trans_data[2] = nec_header[2];
    for( uint32_t i=0; i<16; i++){
        if( (usercode >> i) & 0x1 ){
            nec_trans_data[2*i+3] = nec_bit_1[0];
            nec_trans_data[2*i+4] = nec_bit_1[1];
        }
        else{
            nec_trans_data[2*i+3] = nec_bit_0[0];
            nec_trans_data[2*i+4] = nec_bit_0[1];
        }
        if( (datacode >> i) & 0x1 ){
            nec_trans_data[2*i+3+32] = nec_bit_1[0];
            nec_trans_data[2*i+4+32] = nec_bit_1[1];
        }
        else{
            nec_trans_data[2*i+3+32] = nec_bit_0[0];
            nec_trans_data[2*i+4+32] = nec_bit_0[1];
        }
    }
    nec_trans_data[67] = nec_end[0];
}

#define IR_SW_TX_NUM          (68)
uint16_t data[IR_SW_TX_NUM] = {0};

static void IR_SoftWare_Tx(){

    ir_nec_trans_packup(0x55aa, 0x55aa, (uint16_t*)data);

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_Control(IR0_Handler, CSK_IR_SW_CARRY_CONFIG, 36000);

    IR_SW_Send(IR0_Handler, data, IR_SW_TX_NUM);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_SoftWare_Tx_Abort(){
    ir_nec_trans_packup(0x55aa, 0x55aa, (uint16_t*)data);

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_Control(IR0_Handler, CSK_IR_SW_CARRY_CONFIG, 36000);

    IR_SW_Send(IR0_Handler, data, IR_SW_TX_NUM);

    int i = 0,j = 0;
    while(1){
        for(i = 0; i < 1000; i++){
            for(j = 0; j < 100; j++);
        }
        break;
    }

    IR_Control(IR0_Handler, CSK_IR_CONTROL_ABORT, 0);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_SoftWare_Change_Carry(){
    ir_nec_trans_packup(0x55aa, 0x55aa, (uint16_t*)data);

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_Control(IR0_Handler, CSK_IR_SW_CARRY_CONFIG, 38000);

    IR_SW_Send(IR0_Handler, data, IR_SW_TX_NUM);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

#define IR_SW_RX_NUM          (64)
static int16_t s_data[IR_SW_RX_NUM] = {0};

static void IR_SoftWare_Rx(){
    IR_Event = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_Control(IR0_Handler, CSK_IR_SW_CARRY_CONFIG, 38000);
    IR_Control(IR0_Handler, CSK_IR_CONTROL_CLEAR_RX_FIFO, 0);

    IR_SW_Receive(IR0_Handler, s_data, IR_SW_RX_NUM);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_SoftWare_Rx_Abort(){
    IR_Event = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);

    IR_SW_Receive(IR0_Handler, s_data, IR_SW_RX_NUM);

    IR_Control(IR0_Handler, CSK_IR_CONTROL_ABORT, 0);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

static void IR_SoftWare_Change_Timeout(){
    IR_Event = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);

    IR_Control(IR0_Handler, CSK_IR_CONTROL_SW_TIMEOUT_THRES , 0x200);

    IR_SW_Receive(IR0_Handler, s_data, IR_SW_RX_NUM);

    FAKE_WHILE();

    IR_PowerControl(IR0_Handler, CSK_POWER_OFF);
    IR_Uninitialize(IR0_Handler);
}

#define IR_TRAINING_RX_NUM          (200)
static uint16_t training_data[IR_TRAINING_RX_NUM] = {0};
static uint16_t training_count = 0;

static void IR_SoftWare_Training(){
    uint32_t i = 0;

    IR_Event = 0;

    IR_Initialize(IR0_Handler, IR_EventCallback, NULL);
    IR_PowerControl(IR0_Handler, CSK_POWER_FULL);
    IR_Control(IR0_Handler, CSK_IR_SW_CARRY_CONFIG, 38000);
    IR_Control(IR0_Handler, CSK_IR_DECODE_CONTROL_INPOL, 0);
    IR_Control(IR0_Handler, CSK_IR_DECODE_CONTROL_OUTPOL, 1);
    IR_Control(IR0_Handler, CSK_IR_DECODE_CONTROL_EN, 0);


    while(1){
        IR_Control(IR0_Handler, CSK_IR_CONTROL_CLEAR_RX_FIFO, 0);
        IR_SW_Receive(IR0_Handler, (int16_t*)training_data, IR_TRAINING_RX_NUM);
        while(!(IR_Event & CSK_IR_EVENT_SW_RECEIVE_TIMEOUT));
        IR_Event = 0;
        training_count = IR_GetRxCount(IR0_Handler);
        CLOGD("Training count: %d", training_count);
        CLOGD("{");
        for(i = 0; i < training_count; i++){
            CLOGD("0x%x, ", training_data[i]);
        }
        CLOGD("};");
        CLOGD("Send ir");
        IR_Control(IR0_Handler, CSK_IR_CONTROL_CLEAR_TX_FIFO, 0);
        IR_SW_Send(IR0_Handler, training_data, training_count);
        while(!(IR_Event & CSK_IR_EVENT_SW_SEND_COMPLETE));
        IR_Event = 0;
        CLOGD("Send end");
    }
}

int main(){
    uint32_t times;

    logInit(0, 115200);

    IR_Init_Handler();
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}
