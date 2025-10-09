/*
 * hci_uart.c
 *
 *  Created on: 2021������4������25������
 *      Author: USER
 */
#include "chip.h"
#include "IOMuxManager.h"
#include "Driver_UART.h"
#include "log_print.h"
#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
// #include "rom_main.h"
#include "rom_uart.h"

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
/// uart environment structure
static volatile struct uart_env_tag uart1_env, uart2_env;

static void UART_EventCallback(uint32_t event, void* workspace){
    void (*callback) (void*, uint8_t) = NULL;
    void* data =NULL;

    volatile struct uart_env_tag *uart = (struct uart_env_tag*)workspace;

    uart->uart_event |= event;

    if(event & CSK_UART_EVENT_RECEIVE_COMPLETE)
    {
        // Retrieve callback pointer
        callback = uart->rx.callback;
        data     = uart->rx.dummy;

        if(callback != NULL)
        {
            // Clear callback pointer
            uart->rx.callback = NULL;
            uart->rx.dummy    = NULL;

            // Call handler
            callback(data, LSIP_EIF_STATUS_OK);
        }
        else
        {
            ASSERT_ERR(0);
        }
    }
    if(event & CSK_UART_EVENT_SEND_COMPLETE)
    {
        // Retrieve callback pointer
        callback = uart->tx.callback;
        data     = uart->tx.dummy;

        if(callback != NULL)
        {
            // Clear callback pointer
            uart->tx.callback = NULL;
            uart->tx.dummy    = NULL;
            // Call handler
            callback(data, LSIP_EIF_STATUS_OK);
        }
        else
        {
            ASSERT_ERR(0);
        }
    }

}


void uart_init(void){
	void *UART_Handler = NULL;

    IOMuxManager_PinConfigure(UART0_IO_TX_PAD, UART0_IO_TX_PIN, UART0_IO_TX_SEL);
    IOMuxManager_PinConfigure(UART0_IO_RX_PAD, UART0_IO_RX_PIN, UART0_IO_RX_SEL);
    IOMuxManager_PinConfigure(UART0_IO_CTS_PAD, UART0_IO_CTS_PIN, UART0_IO_CTS_SEL);
    IOMuxManager_PinConfigure(UART0_IO_RTS_PAD, UART0_IO_RTS_PIN, UART0_IO_RTS_SEL);

    UART_Handler = UART0();
    uart1_env.uart_event = 0;

    UART_Uninitialize(UART_Handler);
    UART_Initialize(UART_Handler, UART_EventCallback, (void *)&uart1_env);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
						CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, BAUD_RATE_UART0);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    uart1_env.UART_Handler = UART_Handler;
}

void uart2_init(void){
	void *UART_Handler = NULL;

    IOMuxManager_PinConfigure(UART1_IO_TX_PAD, UART1_IO_TX_PIN, UART1_IO_TX_SEL);
    IOMuxManager_PinConfigure(UART1_IO_RX_PAD, UART1_IO_RX_PIN, UART1_IO_RX_SEL);
    IOMuxManager_PinConfigure(UART1_IO_CTS_PAD, UART1_IO_CTS_PIN, UART1_IO_CTS_SEL);
    IOMuxManager_PinConfigure(UART1_IO_RTS_PAD, UART1_IO_RTS_PIN, UART1_IO_RTS_SEL);

    UART_Handler = UART1();
    uart1_env.uart_event = 0;

    UART_Initialize(UART_Handler, UART_EventCallback, (void *)&uart1_env);

    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, BAUD_RATE_UART1);

    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    uart1_env.UART_Handler = UART_Handler;
}

void uart_flow_on(void)
{
    // Configure modem (HW flow control enable)
    //uart_force_rts_setf(0);
}

bool uart_flow_off(void)
{
	return true;
}

void uart_finish_transfers(void)
{
	while(!(uart1_env.uart_event & CSK_UART_EVENT_SEND_COMPLETE));
}

void uart2_flow_on(void)
{
    // Configure modem (HW flow control enable)
    //uart_force_rts_setf(0);
}

bool uart2_flow_off(void)
{
	return true;
}

void uart2_finish_transfers(void)
{
	while(!(uart2_env.uart_event & CSK_UART_EVENT_SEND_COMPLETE));
}

void uart_read(uint8_t *bufptr, uint32_t size, void (*callback) (void*, uint8_t), void* dummy)
{
    // Sanity check
    ASSERT_ERR(bufptr != NULL);
    ASSERT_ERR(size != 0);
    ASSERT_ERR(callback != NULL);
    uart1_env.rx.callback = callback;
    uart1_env.rx.dummy    = dummy;

    uart1_env.uart_event &= ~CSK_UART_EVENT_RECEIVE_COMPLETE;
	UART_Receive(uart1_env.UART_Handler, bufptr, size);

}

void uart_write(uint8_t *bufptr, uint32_t size, void (*callback) (void*, uint8_t), void* dummy)
{
    // Sanity check
    ASSERT_ERR(bufptr != NULL);
    ASSERT_ERR(size != 0);
    ASSERT_ERR(callback != NULL);
    uart1_env.tx.callback = callback;
    uart1_env.tx.dummy    = dummy;

    uart1_env.uart_event &= ~CSK_UART_EVENT_SEND_COMPLETE;
    UART_Send(uart1_env.UART_Handler, bufptr, size);
}

void uart2_read(uint8_t *bufptr, uint32_t size, void (*callback) (void*, uint8_t), void* dummy)
{
    // Sanity check
    ASSERT_ERR(bufptr != NULL);
    ASSERT_ERR(size != 0);
    ASSERT_ERR(callback != NULL);
    uart2_env.rx.callback = callback;
    uart2_env.rx.dummy    = dummy;

    uart2_env.uart_event &= ~CSK_UART_EVENT_RECEIVE_COMPLETE;
	UART_Receive(uart2_env.UART_Handler, bufptr, size);

}

void uart2_write(uint8_t *bufptr, uint32_t size, void (*callback) (void*, uint8_t), void* dummy)
{
    // Sanity check
    ASSERT_ERR(bufptr != NULL);
    ASSERT_ERR(size != 0);
    ASSERT_ERR(callback != NULL);
    uart2_env.tx.callback = callback;
    uart2_env.tx.dummy    = dummy;

    uart2_env.uart_event &= ~CSK_UART_EVENT_SEND_COMPLETE;
    UART_Send(uart2_env.UART_Handler, bufptr, size);
}
