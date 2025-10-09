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
#include "rom_main.h"

#ifndef _ROM_UART_H_
#define  _ROM_UART_H_

#define BAUD_RATE_UART0 1000000
#define BAUD_RATE_UART1 1000000

/// Enumeration of External Interface status codes
enum
{
    /// EIF status OK
    LSIP_EIF_STATUS_OK,
    /// EIF status KO
    LSIP_EIF_STATUS_ERROR,
};

/* UART0 PIN*/
#define UART0_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART0_IO_TX_PIN          (3)
#define UART0_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART0_IO_RX_PIN          (2)
#define UART0_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_CTS_PAD         (CSK_IOMUX_PAD_A)
#define UART0_IO_CTS_PIN         (12)
#define UART0_IO_CTS_SEL         (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_RTS_PAD         (CSK_IOMUX_PAD_A)
#define UART0_IO_RTS_PIN         (11)
#define UART0_IO_RTS_SEL         (CSK_IOMUX_FUNC_ALTER2)

/* UART1 PIN*/
#define UART1_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART1_IO_TX_PIN          (4)
#define UART1_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART1_IO_RX_PIN          (5)
#define UART1_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_CTS_PAD         (CSK_IOMUX_PAD_A)
#define UART1_IO_CTS_PIN         (20)
#define UART1_IO_CTS_SEL         (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_RTS_PAD         (CSK_IOMUX_PAD_A)
#define UART1_IO_RTS_PIN         (19)
#define UART1_IO_RTS_SEL         (CSK_IOMUX_FUNC_ALTER3)

/* UART2 PIN*/
#define UART2_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_TX_PIN          (18)
#define UART2_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER3)

#define UART2_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_RX_PIN          (15)
#define UART2_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER3)

#define UART2_IO_CTS_PAD         (CSK_IOMUX_PAD_A)
#define UART2_IO_CTS_PIN         (16)
#define UART2_IO_CTS_SEL         (CSK_IOMUX_FUNC_ALTER3)

#define UART2_IO_RTS_PAD         (CSK_IOMUX_PAD_A)
#define UART2_IO_RTS_PIN         (17)
#define UART2_IO_RTS_SEL         (CSK_IOMUX_FUNC_ALTER3)


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

#define ASSERT_ERR(cond)                              \
    do {                                              \
        if (!(cond)) {                                \
            assert_err(#cond, __FILE__, __LINE__);    \
        }                                             \
    } while(0)

/*
 * STRUCT DEFINITIONS
 *****************************************************************************************
 */
/* TX and RX channel class holding data used for asynchronous read and write data
 * transactions
 */
/// UART TX RX Channel
struct uart_txrxchannel
{
    /// call back function pointer
    void (*callback) (void*, uint8_t);
    /// Dummy data pointer returned to callback when operation is over.
    void* dummy;
};

/// UART environment structure
struct uart_env_tag
{
	/// device handler
	void* UART_Handler;
	/// event
	uint32_t uart_event;
    /// tx channel
    struct uart_txrxchannel tx;
    /// rx channel
    struct uart_txrxchannel rx;
    /// error detect
    uint8_t errordetect;
    /// external wakeup
    bool ext_wakeup;
};

#endif /* _ROM_UART_H_ */
