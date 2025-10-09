/*
 * uart_nos_chk.c
 *
 *  Created on: 2020年8月17日
 *      Author: USER
 */

#include "IOMuxManager.h"
#include "ClockManager.h"
#include "chip.h"

#include "Driver_UART.h"
#include "log_print.h"

#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "systick.h"

/* UART0 PIN*/
#define UART0_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART0_IO_TX_PIN          3
#define UART0_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART0_IO_RX_PIN          2
#define UART0_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_CTS_PAD         (CSK_IOMUX_PAD_A)
#define UART0_IO_CTS_PIN         20
#define UART0_IO_CTS_SEL         (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_RTS_PAD         (CSK_IOMUX_PAD_A)
#define UART0_IO_RTS_PIN         19
#define UART0_IO_RTS_SEL         (CSK_IOMUX_FUNC_ALTER2)

/* UART1 PIN*/
#define UART1_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART1_IO_TX_PIN          (4)
#define UART1_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART1_IO_RX_PIN          (5)
#define UART1_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_CTS_PAD         (CSK_IOMUX_PAD_A)
#define UART1_IO_CTS_PIN         (13)
#define UART1_IO_CTS_SEL         (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_RTS_PAD         (CSK_IOMUX_PAD_A)
#define UART1_IO_RTS_PIN         (14)
#define UART1_IO_RTS_SEL         (CSK_IOMUX_FUNC_ALTER3)

/* UART2 PIN*/
#define UART2_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_TX_PIN          (11)
#define UART2_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER4)

#define UART2_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_RX_PIN          (12)
#define UART2_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER4)

#define UART2_IO_CTS_PAD         (CSK_IOMUX_PAD_A)
#define UART2_IO_CTS_PIN         (28)
#define UART2_IO_CTS_SEL         (CSK_IOMUX_FUNC_ALTER4)

#define UART2_IO_RTS_PAD         (CSK_IOMUX_PAD_A)
#define UART2_IO_RTS_PIN         (29)
#define UART2_IO_RTS_SEL         (CSK_IOMUX_FUNC_ALTER4)


#define FAKE_WHILE()   do{\
    volatile int fake_i = 0;\
    while(1){\
        fake_i++;\
        fake_i--;\
        if(fake_i > 1000000){\
            break;\
        }\
    }\
    }while(0)

typedef void (*function)(void);

static void UART_Interrupt_Tx_Primary(void);
static void UART_Interrupt_Rx_Primary(void);
static void UART_Interrupt_TxRx_Primary(void);
static void UART_Interrupt_TxRx_Primary_1M(void);
static void UART_Interrupt_TxRx_Primary_2M(void);
static void UART_Interrupt_TxRx_Primary_3M(void);
static void UART_Interrupt_TxRx_HalfDuplex_Primary(void);
static void UART_Interrupt_TxRx_HalfDuplex_Timeout_Primary(void);

static void UART_Interrupt_Rx_Timeout(void);
static void UART_Interrupt_Tx_Abort(void);
static void UART_Interrupt_Rx_Abort(void);
static void UART_Interrupt_Tx_Odd(void);
static void UART_Interrupt_Tx_Even(void);
static void UART_Interrupt_Tx_Stop2(void);
static void UART_Interrupt_Tx_Data7_Stop1(void);


static void UART_DMA_Tx_Primary(void);
static void UART_DMA_Rx_Primary(void);

static void UART_Auto_Flow_Rx_Primary(void);
static void UART_Auto_Flow_Tx_Primary(void);

static function test_function_array[] = {
//    UART_Interrupt_Tx_Primary,
//	UART_Interrupt_Rx_Primary,
//	UART_Interrupt_TxRx_Primary,
//	UART_Interrupt_TxRx_Primary_1M,
//	UART_Interrupt_TxRx_Primary_2M,
//	UART_Interrupt_TxRx_Primary_3M,
////	UART_Interrupt_TxRx_HalfDuplex_Primary,
////	UART_Interrupt_TxRx_HalfDuplex_Timeout_Primary,
//    UART_Interrupt_Rx_Timeout,
//    UART_Interrupt_Tx_Abort,
    UART_Interrupt_Rx_Abort,
//    UART_Interrupt_Tx_Odd,
//    UART_Interrupt_Tx_Even,
//    UART_Interrupt_Tx_Stop2,
//    UART_Interrupt_Tx_Data7_Stop1,
//    UART_DMA_Tx_Primary,
//    UART_DMA_Rx_Primary,
//    UART_Auto_Flow_Rx_Primary,
//    UART_Auto_Flow_Tx_Primary,
};

static void* UART_Handler = NULL;

static uint8_t rcv_buffer[100] = {0};

static void* GPIOA_Handler = NULL;

static void UART_Init_Handler(void){
    IOMuxManager_PinConfigure(UART0_IO_TX_PAD, UART0_IO_TX_PIN, UART0_IO_TX_SEL);
    IOMuxManager_PinConfigure(UART0_IO_RX_PAD, UART0_IO_RX_PIN, UART0_IO_RX_SEL);
    IOMuxManager_PinConfigure(UART0_IO_CTS_PAD, UART0_IO_CTS_PIN, UART0_IO_CTS_SEL);
    IOMuxManager_PinConfigure(UART0_IO_RTS_PAD, UART0_IO_RTS_PIN, UART0_IO_RTS_SEL);
    UART_Handler = UART0();


    // IOMuxManager_PinConfigure(UART1_IO_TX_PAD, UART1_IO_TX_PIN, UART1_IO_TX_SEL);
    // IOMuxManager_PinConfigure(UART1_IO_RX_PAD, UART1_IO_RX_PIN, UART1_IO_RX_SEL);
    // IOMuxManager_PinConfigure(UART1_IO_CTS_PAD, UART1_IO_CTS_PIN, UART1_IO_CTS_SEL);
    // IOMuxManager_PinConfigure(UART1_IO_RTS_PAD, UART1_IO_RTS_PIN, UART1_IO_RTS_SEL);
    // UART_Handler = UART1();


    // IOMuxManager_PinConfigure(UART2_IO_TX_PAD, UART2_IO_TX_PIN, UART2_IO_TX_SEL);
    // IOMuxManager_PinConfigure(UART2_IO_RX_PAD, UART2_IO_RX_PIN, UART2_IO_RX_SEL);
    // IOMuxManager_PinConfigure(UART2_IO_CTS_PAD, UART2_IO_CTS_PIN, UART2_IO_CTS_SEL);
    // IOMuxManager_PinConfigure(UART2_IO_RTS_PAD, UART2_IO_RTS_PIN, UART2_IO_RTS_SEL);
    // UART_Handler = UART2();
}

volatile uint32_t uart_event = 0;
static void UART_EventCallback(uint32_t event, void* workspace){
    uart_event |= event;
}

static void UART_Interrupt_Tx_Primary(void){
    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send_IT(UART_Handler, "HELLO WORLD\r\n", 14);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

static void UART_Interrupt_Rx_Primary(void){
    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Receive_IT(UART_Handler, rcv_buffer, 10);

    while(!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

CSK_UART_STATUS status = {0};

static void UART_Interrupt_TxRx_Primary(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 115200);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send(UART_Handler, "Please input: 'Agree'\r\n", 23);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_Receive(UART_Handler, rcv_buffer, 5);

    while(!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE));

    uart_event = 0;

    if(strcmp((const char*)rcv_buffer, "Agree")){
        UART_Send(UART_Handler, rcv_buffer, 5);
    }else{
        UART_Send(UART_Handler, "Uart receive success", 22);
    }

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

static void UART_Interrupt_TxRx_Primary_1M(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 1000000);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    {
		char tx_string[] = "[UART][TXRX]Timeout 1M baudrate validation\r\n";

		UART_Send(UART_Handler, tx_string, sizeof(tx_string));

		while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

		uart_event = 0;
    }

    UART_Receive(UART_Handler, rcv_buffer, 100);

    while((!(uart_event & CSK_UART_EVENT_RX_TIMEOUT)) && (!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE)));

    uart_event = 0;

    {
    	char tx_string[] = "[UART][TXRX]Receive timeout\r\n";

        UART_Send(UART_Handler, tx_string, sizeof(tx_string));

        while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
        uart_event = 0;

        UART_Send(UART_Handler, rcv_buffer, UART_GetRxCount(UART_Handler));

        while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
        uart_event = 0;
    }

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

static void UART_Interrupt_TxRx_Primary_2M(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 2000000);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    {
		char tx_string[] = "[UART][TXRX]Timeout 2M baudrate validation\r\n";

		UART_Send(UART_Handler, tx_string, sizeof(tx_string));

		while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

		uart_event = 0;
    }

    UART_Receive(UART_Handler, rcv_buffer, 100);

    while((!(uart_event & CSK_UART_EVENT_RX_TIMEOUT)) && (!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE)));

    uart_event = 0;

    {
    	char tx_string[] = "[UART][TXRX]Receive timeout\r\n";

        UART_Send(UART_Handler, tx_string, sizeof(tx_string));

        while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
        uart_event = 0;

        UART_Send(UART_Handler, rcv_buffer, UART_GetRxCount(UART_Handler));

        while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
        uart_event = 0;
    }

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

static void UART_Interrupt_TxRx_Primary_3M(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 3000000);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    {
		char tx_string[] = "[UART][TXRX]Timeout 3M baudrate validation\r\n";

		UART_Send(UART_Handler, tx_string, sizeof(tx_string));

		while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

		uart_event = 0;
    }

    UART_Receive(UART_Handler, rcv_buffer, 100);

    while((!(uart_event & CSK_UART_EVENT_RX_TIMEOUT)) && (!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE)));

    uart_event = 0;

    {
    	char tx_string[] = "[UART][TXRX]Receive timeout\r\n";

        UART_Send(UART_Handler, tx_string, sizeof(tx_string));

        while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
        uart_event = 0;

        UART_Send(UART_Handler, rcv_buffer, UART_GetRxCount(UART_Handler));

        while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
        uart_event = 0;
    }

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

/*static void UART_Interrupt_TxRx_HalfDuplex_Primary(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_HALF_DUPLEX |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 115200);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send_IT(UART_Handler, "Please input: 'Agree'\r\n", 23);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    do{
    	UART_GetStatus(UART_Handler, &status);
    }while(status.tx_busy);

    UART_Receive_IT(UART_Handler, rcv_buffer, 5);

    while(!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE));

    do {
    	UART_GetStatus(UART_Handler, &status);
    }while(status.rx_busy);

    uart_event = 0;

    if(strcmp((const char*)rcv_buffer, "Agree")){
        UART_Send_IT(UART_Handler, rcv_buffer, 5);
    }else{
        UART_Send_IT(UART_Handler, "Uart receive success", 22);
    }

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
    uart_event = 0;

    do{
    	UART_GetStatus(UART_Handler, &status);
    }while(status.tx_busy);

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}*/
/*static void UART_Interrupt_TxRx_HalfDuplex_Timeout_Primary(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_HALF_DUPLEX_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 115200);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    char tx_string[] = "[UART][TXRX]Half Duplex validation\r\n";

    UART_Send_IT(UART_Handler, tx_string, sizeof(tx_string));

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    do{
    	UART_GetStatus(UART_Handler, &status);
    }while(status.tx_busy);

    UART_Receive_IT(UART_Handler, rcv_buffer, 100);

    while(!(uart_event & CSK_UART_EVENT_RX_TIMEOUT));

    uart_event = 0;

    UART_Send_IT(UART_Handler, rcv_buffer, UART_GetRxCount(UART_Handler));

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
    uart_event = 0;

    do{
    	UART_GetStatus(UART_Handler, &status);
    }while(status.tx_busy);

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}
*/
static void UART_Interrupt_Rx_Timeout(void){

    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Receive(UART_Handler, rcv_buffer, 100);

    while(!(uart_event & CSK_UART_EVENT_RX_TIMEOUT));
    uart_event = 0;

    uint32_t count;
    count = UART_GetRxCount(UART_Handler);

    UART_Send(UART_Handler, rcv_buffer, count);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

static void UART_Interrupt_Tx_Abort(void){

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    for(int i = 0; i < sizeof(rcv_buffer); i++)
    	rcv_buffer[i] = '0' + (i % 10);
    UART_Send(UART_Handler, rcv_buffer, 100);

    int i = 0;
    for(i = 0; i < 10000; i++);

    UART_Control(UART_Handler, CSK_UART_ABORT_SEND, 1);

    UART_Send(UART_Handler, "aborted", 7);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

static void UART_Interrupt_Rx_Abort(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Receive(UART_Handler, rcv_buffer, 100);

    FAKE_WHILE();

    UART_Control(UART_Handler, CSK_UART_ABORT_RECEIVE, 1);

    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Receive(UART_Handler, rcv_buffer, 100);

    while(!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE));
    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

    FAKE_WHILE();
}

static void UART_Interrupt_Tx_Odd(void){

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_ODD |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send(UART_Handler, "HELLO WORLD\r\n", 14);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);
}

static void UART_Interrupt_Tx_Even(void){

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_EVEN |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send(UART_Handler, "HELLO WORLD\r\n", 14);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

//    FAKE_WHILE();

}

static void UART_Interrupt_Tx_Stop2(void){

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_2 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send(UART_Handler, "HELLO WORLD\r\n", 14);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

//    FAKE_WHILE();
}

static void UART_Interrupt_Tx_Data7_Stop1(void){

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_7 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send(UART_Handler, "HELLO WORLD\r\n", 14);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

//    FAKE_WHILE();
}

static void UART_DMA_Tx_Primary(void){

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Dma |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send(UART_Handler, "HELLO WORLD\r\n", 14);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

//    FAKE_WHILE();
}

static void UART_DMA_Rx_Primary(void){

    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Dma |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Send(UART_Handler, "Please input: 'Agree'\r\n", 23);

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

    uart_event = 0;

    UART_Receive(UART_Handler, rcv_buffer, 5);

    while(!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE));

    uart_event = 0;

    if(strcmp((const char*)rcv_buffer, "Agree")){
        UART_Send(UART_Handler, rcv_buffer, 5);
    }else{
        UART_Send(UART_Handler, "Uart receive success", 22);
    }

    while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));
    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

//    FAKE_WHILE();
}

static void UART_Auto_Flow_Rx_Primary(void){
    memset(rcv_buffer, 0, sizeof(rcv_buffer));

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_RTS_CTS |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    UART_Receive(UART_Handler, rcv_buffer, 100);

    while(!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE));

    uart_event = 0;

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

    FAKE_WHILE();
}

static void UART_Auto_Flow_Tx_Primary(void){

    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_RTS_CTS |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 9600);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    while(1){
        UART_Send(UART_Handler, "HELLO WORLD\r\n", 14);

        while(!(uart_event & CSK_UART_EVENT_SEND_COMPLETE));

        uart_event = 0;
    }

    UART_PowerControl(UART_Handler, CSK_POWER_OFF);

    UART_Uninitialize(UART_Handler);

    FAKE_WHILE();
}

int main(){
    uint32_t times;

    __HAL_CRM_UART0_CLK_ENABLE();
    __HAL_CRM_UART1_CLK_ENABLE();
    __HAL_CRM_UART2_CLK_ENABLE();

    UART_Init_Handler();
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }
    while(1);
}
