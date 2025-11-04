
#ifndef __UART_H
#define __UART_H


#include <stdint.h>
#include "Driver_UART.h"



// =============================================================================
//  MACROS
// =============================================================================
#define UART_RX_FIFO_SIZE                        (64)  /*64*/
#define UART_TX_FIFO_SIZE                        (16)
#define NB_RX_FIFO_BITS                          (6)   /*6*/
#define NB_TX_FIFO_BITS                          (4)

// =============================================================================
//  TYPES
// =============================================================================


//ctrl
#define UART_ENABLE                 (1<<0)
#define UART_ENABLE_DISABLE         (0<<0)
#define UART_ENABLE_ENABLE          (1<<0)
#define UART_DATA_BITS              (1<<1)
#define UART_DATA_BITS_7_BITS       (0<<1)
#define UART_DATA_BITS_8_BITS       (1<<1)
#define UART_TX_STOP_BITS           (1<<2)
#define UART_TX_STOP_BITS_1_BIT     (0<<2)
#define UART_TX_STOP_BITS_2_BITS    (1<<2)
#define UART_PARITY_ENABLE          (1<<3)
#define UART_PARITY_ENABLE_NO       (0<<3)
#define UART_PARITY_ENABLE_YES      (1<<3)
#define UART_PARITY_SELECT(n)       (((n)&3)<<4)
#define UART_PARITY_SELECT_ODD      (0<<4)
#define UART_PARITY_SELECT_EVEN     (1<<4)
#define UART_PARITY_SELECT_SPACE    (2<<4)
#define UART_PARITY_SELECT_MARK     (3<<4)
#define UART_DIVISOR_MODE           (1<<20)
#define UART_IRDA_ENABLE            (1<<21)
#define UART_DMA_MODE               (1<<22)
#define UART_DMA_MODE_DISABLE       (0<<22)
#define UART_DMA_MODE_ENABLE        (1<<22)
#define UART_AUTO_FLOW_CONTROL      (1<<23)
#define UART_AUTO_FLOW_CONTROL_ENABLE (1<<23)
#define UART_AUTO_FLOW_CONTROL_DISABLE (0<<23)
#define UART_LOOP_BACK_MODE         (1<<24)
#define UART_RX_LOCK_ERR            (1<<25)
#define UART_RX_BREAK_LENGTH(n)     (((n)&15)<<28)

//status
#define UART_RX_FIFO_LEVEL(n)       (((n)&0x7F)<<0)
#define UART_RX_FIFO_LEVEL_MASK     (0x7F<<0)
#define UART_RX_FIFO_LEVEL_SHIFT    (0)
#define UART_TX_FIFO_SPACE(n)       (((n)&31)<<8)
#define UART_TX_FIFO_SPACE_MASK     (31<<8)
#define UART_TX_FIFO_SPACE_SHIFT    (8)
#define UART_TX_ACTIVE              (1<<14)
#define UART_RX_ACTIVE              (1<<15)
#define UART_RX_OVERFLOW_ERR        (1<<16)
#define UART_TX_OVERFLOW_ERR        (1<<17)
#define UART_RX_PARITY_ERR          (1<<18)
#define UART_RX_FRAMING_ERR         (1<<19)
#define UART_RX_BREAK_INT           (1<<20)
#define UART_DCTS                   (1<<24)
#define UART_CTS                    (1<<25)
#define UART_DTR                    (1<<28)
#define UART_CLK_ENABLED            (1<<31)

//rxtx_buffer
#define UART_RX_DATA(n)             (((n)&0xFF)<<0)
#define UART_TX_DATA(n)             (((n)&0xFF)<<0)

//irq_mask
#define UART_TX_MODEM_STATUS        (1<<0)
#define UART_RX_DATA_AVAILABLE      (1<<1)
#define UART_TX_DATA_NEEDED         (1<<2)
#define UART_RX_TIMEOUT             (1<<3)
#define UART_RX_LINE_ERR            (1<<4)
#define UART_TX_DMA_DONE            (1<<5)
#define UART_RX_DMA_DONE            (1<<6)
#define UART_RX_DMA_TIMEOUT         (1<<7)
#define UART_DTR_RISE               (1<<8)
#define UART_DTR_FALL               (1<<9)

//irq_cause
//#define UART_TX_MODEM_STATUS      (1<<0)
//#define UART_RX_DATA_AVAILABLE    (1<<1)
//#define UART_TX_DATA_NEEDED       (1<<2)
//#define UART_RX_TIMEOUT           (1<<3)
//#define UART_RX_LINE_ERR          (1<<4)
//#define UART_TX_DMA_DONE          (1<<5)
//#define UART_RX_DMA_DONE          (1<<6)
//#define UART_RX_DMA_TIMEOUT       (1<<7)
//#define UART_DTR_RISE             (1<<8)
//#define UART_DTR_FALL             (1<<9)
#define UART_TX_MODEM_STATUS_U      (1<<16)
#define UART_RX_DATA_AVAILABLE_U    (1<<17)
#define UART_TX_DATA_NEEDED_U       (1<<18)
#define UART_RX_TIMEOUT_U           (1<<19)
#define UART_RX_LINE_ERR_U          (1<<20)
#define UART_TX_DMA_DONE_U          (1<<21)
#define UART_RX_DMA_DONE_U          (1<<22)
#define UART_RX_DMA_TIMEOUT_U       (1<<23)
#define UART_DTR_RISE_U             (1<<24)
#define UART_DTR_FALL_U             (1<<25)

//triggers
#define UART_RX_TRIGGER_MASK        63UL
#define UART_RX_TRIGGER_SHIFT       0
#define UART_RX_TRIGGER(n)          (((n) & UART_RX_TRIGGER_MASK) << UART_RX_TRIGGER_SHIFT)
#define UART_TX_TRIGGER_MASK        15UL
#define UART_TX_TRIGGER_SHIFT       8
#define UART_TX_TRIGGER(n)          (((n) & UART_TX_TRIGGER_MASK) << UART_TX_TRIGGER_SHIFT)
#define UART_AFC_LEVEL_MASK         31UL
#define UART_AFC_LEVEL_SHIFT        16
#define UART_AFC_LEVEL(n)           (((n) & UART_AFC_LEVEL_MASK) << UART_AFC_LEVEL_SHIFT)

//CMD_Set
#define UART_RI                     (1<<0)
#define UART_DCD                    (1<<1)
#define UART_DSR                    (1<<2)
#define UART_TX_BREAK_CONTROL       (1<<3)
#define UART_TX_FINISH_N_WAIT       (1<<4)
#define UART_RX_RTS                 (1<<5)
#define UART_RX_FIFO_RESET          (1<<6)
#define UART_TX_FIFO_RESET          (1<<7)

//UART_CMD_CLR
#define UART_RI                        (1<<0)
#define UART_DCR                       (1<<1)
#define UART_DSR                       (1<<2)
#define UART_TX_BREAK_CONTROL          (1<<3)
#define UART_TX_FINISH_N_WAIT          (1<<4)
#define UART_RX_CPU_RTS                (1<<5)

//UART_AUTO_BAUD
#define UART_AUTO_ENABLE               (1<<0)
#define UART_AUTO_TRACKING             (1<<1)
#define UART_VERIFY_2BYTE              (1<<2)
#define UART_VERIFY_CHAR0(n)           (((n)&0xFF)<<8)
#define UART_VERIFY_CHAR1(n)           (((n)&0xFF)<<16)


/* IER Register (+0x4) */
#define UARTC_IER_DISABLE               0x00
#define UARTC_IER_RDR                   0x01 /* Data Ready Enable */
#define UARTC_IER_THRE                  0x02 /* THR Empty Enable */
#define UARTC_IER_RLS                   0x04 /* Receive Line Status Enable */
#define UARTC_IER_MS                    0x08 /* Modem Staus Enable */

/* IIR Register (+0x8) */
#define UARTC_IIR_NONE                  0x01 /* No interrupt pending */
#define UARTC_IIR_RLS                   0x06 /* Receive Line Status */
#define UARTC_IIR_RDA                   0x04 /* Receive Data Available */
#define UARTC_IIR_RTO                   0x0c /* Receive Time Out */
#define UARTC_IIR_THRE                  0x02 /* THR Empty */
#define UARTC_IIR_MODEM                 0x00 /* Modem Status */
#define UARTC_IIR_INT_MASK              0x0f /* Initerrupt Status Bits Mask */
#define UARTC_IIR_FIFO_EN               0xc0 /* FIFO mode is enabled, set when FCR[0] is 1 */

/* FCR Register (+0x8) */
#define UARTC_FCR_FIFO_EN               0x01 /* FIFO Enable */
#define UARTC_FCR_RFIFO_RESET           0x02 /* Rx FIFO Reset */
#define UARTC_FCR_TFIFO_RESET           0x04 /* Tx FIFO Reset */
#define UARTC_FCR_DMA_EN                0x08 /* Select UART DMA mode */

#define UARTC_FCR_TFIFO_TRGL0           0x00 /* TX FIFO int trigger level - Empty */
#define UARTC_FCR_TFIFO_TRGL1           0x10 /* TX FIFO int trigger level - 2 char */
#define UARTC_FCR_TFIFO_TRGL2           0x20 /* TX FIFO int trigger level - 1/4 full */
#define UARTC_FCR_TFIFO_TRGL3           0x30 /* TX FIFO int trigger level - 1/2 full */

#define UARTC_FCR_RFIFO_TRGL0           0x00 /* RX FIFO int trigger level - 1 char */
#define UARTC_FCR_RFIFO_TRGL1           0x40 /* RX FIFO int trigger level - 1/4 full */
#define UARTC_FCR_RFIFO_TRGL2           0x80 /* RX FIFO int trigger level - 1/2 full */
#define UARTC_FCR_RFIFO_TRGL3           0xc0 /* RX FIFO int trigger level - 2 less than full */

/* LCR Register (+0xc) */
#define UARTC_LCR_BITS5                 0x00
#define UARTC_LCR_BITS6                 0x01
#define UARTC_LCR_BITS7                 0x02
#define UARTC_LCR_BITS8                 0x03
#define UARTC_LCR_STOP1                 0x00
#define UARTC_LCR_STOP1_5OR2            0x04 // When LCR[1:0] is zero 1.5 stop bits
                                             // else 2 stop bits

#define UARTC_LCR_PARITY_NONE           0x00 /* No Parity Check */
#define UARTC_LCR_PARITY_EVEN           0x18 /* Even Parity */
#define UARTC_LCR_PARITY_ODD            0x08 /* Odd Parity */
#define UARTC_LCR_SETBREAK              0x40 /* Set Break condition */
#define UARTC_LCR_DLAB                  0x80 /* Divisor Latch Access Bit */

#define UARTC_LCR_DLAB                  0x80 /* Divisor Latch Access Bit */

/* MCR Register (+0x10) */
#define UARTC_MCR_DTR                   0x01 /* Data Terminal Ready */
#define UARTC_MCR_RTS                   0x02 /* Request to Send */
#define UARTC_MCR_OUT1                  0x04 /* output1 */
#define UARTC_MCR_OUT2                  0x08 /* output2 or global interrupt enable */
#define UARTC_MCR_LPBK                  0x10 /* loopback mode */
#define UARTC_MCR_AFE                   0x20 /* Auto flow control */

/* LSR Register (+0x14) */
#define UARTC_LSR_RDR                   0x1 /* Data Ready */
#define UARTC_LSR_OE                    0x2 /* Overrun Error */
#define UARTC_LSR_PE                    0x4 /* Parity Error */
#define UARTC_LSR_FE                    0x8 /* Framing Error */
#define UARTC_LSR_BI                    0x10 /* Break Interrupt */
#define UARTC_LSR_THRE                  0x20 /* THR/FIFO Empty */
#define UARTC_LSR_TEMT                  0x40 /* THR/FIFO and TFR Empty */
#define UARTC_LSR_ERRF                  0x80 /* FIFO Data Error */

/* MSR Register (+0x18) */
#define UARTC_MSR_DCTS                  0x1 /* Delta CTS */
#define UARTC_MSR_DDSR                  0x2 /* Delta DSR */
#define UARTC_MSR_TERI                  0x4 /* Trailing Edge RI */
#define UARTC_MSR_DDCD                  0x8 /* Delta CD */
#define UARTC_MSR_CTS                   0x10 /* Clear To Send */
#define UARTC_MSR_DSR                   0x20 /* Data Set Ready */
#define UARTC_MSR_RI                    0x40 /* Ring Indicator */
#define UARTC_MSR_DCD                   0x80 /* Data Carrier Detect */


// CONSTANT MACRO
// ******************************************UART0
#define CSK_UART0_TX_DMA_CH       (0)
#define CSK_UART0_TX_DMA_REQSEL   (DMA_HSID_UART0_TX)
#define CSK_UART0_RX_DMA_CH       (1)
#define CSK_UART0_RX_DMA_REQSEL   (DMA_HSID_UART0_RX)

// ******************************************UART1
#define CSK_UART1_TX_DMA_CH       (0xFF)  // auto allocate
#define CSK_UART1_TX_DMA_REQSEL   (DMA_HSID_UART1_TX)
#define CSK_UART1_RX_DMA_CH       (0xFF)  // auto allocate
#define CSK_UART1_RX_DMA_REQSEL   (DMA_HSID_UART1_RX)

// ******************************************UART2
#define CSK_UART2_TX_DMA_CH       (0xFF)  // auto allocate
#define CSK_UART2_TX_DMA_REQSEL   (DMA_HSID1_UART2_TX)
#define CSK_UART2_RX_DMA_CH       (0xFF)  // auto allocate
#define CSK_UART2_RX_DMA_REQSEL   (DMA_HSID1_UART2_RX)


#define CSK_UART_RX_TRIG_LVL_0   (1)           // 1 character in FIFO
#define CSK_UART_RX_TRIG_LVL_1   (7)           // FIFO 1/4 full
#define CSK_UART_RX_TRIG_LVL_2   (15)          // FIFO 1/2 full
#define CSK_UART_RX_TRIG_LVL_3   (29)          // FIFO 2 less than full

#define CSK_UART_RX_TRIG_LVL                   (CSK_UART_RX_TRIG_LVL_1)// 1 character in FIFO
#define CSK_UART_RX_TRIG_LVL_PARA              (UARTC_FCR_RFIFO_TRGL1)
#define CSK_UART_RX_DMA_WIDTH_PARA             (DMA_WIDTH_BYTE)
#define CSK_UART_RX_DMA_BSIZE_PARA             (DMA_BSIZE_16)


#define CSK_UART_TX_TRIG_LVL                   (8)// FIFO Empty
#define CSK_UART_TX_TRIG_LVL_PARA              (UARTC_FCR_TFIFO_TRGL2)
#define CSK_UART_TX_DMA_WIDTH_PARA             (DMA_WIDTH_BYTE)
#define CSK_UART_TX_DMA_BSIZE_PARA             (DMA_BSIZE_8)


// Modem configure marco
#define CSK_UART_MODEM_NONE            (0x00)
#define CSK_UART_AUTO_FLOW_EN_RTS      (0x01 << 0U)
#define CSK_UART_AUTO_FLOW_EN_CTS      (0x01 << 1U)


// UART flags
#define UART_FLAG_INITIALIZED          (1U << 0)
#define UART_FLAG_POWERED              (1U << 1)
#define UART_FLAG_CONFIGURED           (1U << 2)
#define UART_FLAG_TX_ENABLED           (1U << 3)
#define UART_FLAG_RX_ENABLED           (1U << 4)
#define UART_FLAG_SEND_ACTIVE          (1U << 5)
#define UART_FLAG_DIS_TX_INT           (1U << 6)

// UART Transfer Information (Run-Time)
typedef struct _UART_TRANSFER_INFO
{
    uint32_t rx_num;        // Total number of data to be received
    uint32_t tx_num;        // Total number of data to be send
    uint8_t *rx_buf;        // Pointer to in data buffer
    uint8_t *tx_buf;        // Pointer to out data buffer
    uint32_t rx_cnt;        // Number of data received
    uint32_t tx_cnt;        // Number of data sent
    uint32_t tx_def_val;    // Default value
    uint8_t  send_active;   // Send active flag
} UART_TRANSFER_INFO;

typedef struct _UART_RX_STATUS
{
    uint8_t rx_busy;            // Receiver busy flag
    uint8_t rx_overflow;        // Receive data overflow detected (cleared on start of next receive operation)
    uint8_t rx_break;           // Break detected on receive (cleared on start of next receive operation)
    uint8_t rx_framing_error;   // Framing error detected on receive (cleared on start of next receive operation)
    uint8_t rx_parity_error;    // Parity error detected on receive (cleared on start of next receive operation)
} UART_RX_STATUS;

// UART Information (Run-Time)
typedef struct _UART_INFO
{
    CSK_UART_SignalEvent_t cb_event;       // Event callback
    UART_RX_STATUS rx_status;              // Receive status flags
    UART_TRANSFER_INFO xfer;               // Transfer information
    uint32_t tx_trig_lvl;                  // FIFO Trigger level
    uint32_t rx_trig_lvl;                  // FIFO Trigger level
    uint32_t baudrate;                     // Baudrate
    uint8_t mode;                          // UART mode
    uint8_t flags;                         // UART driver flags

    uint8_t inter_en;                      // 0 for dma
                                           // 1 for interrupt
    void* workspace;

    uint8_t timeout;                       // 0 disable timeout
                                           // 1 enable timeout
    uint8_t half_duplex;
} UART_INFO;

// UART DMA
typedef struct _UART_DMA
{
    uint8_t channel;                       // DMA Channel
    uint8_t reqsel;                        // DMA request selection
    DMA_SignalEvent_t cb_event;            // DMA Event callback
} UART_DMA;

// UART Resources definitions
typedef const struct _UART_RESOURCES
{
    UART_RegDef *reg;                   // Pointer to UART peripheral
    uint32_t irq_num;                      // UART IRQ Number
    void (*irq_handler)(void);
    uint32_t fifo_d;                       // Depth of FIFO
    UART_DMA *dma_tx;
    UART_DMA *dma_rx;
    UART_INFO *info;                       // Run-Time Information
    uint32_t modem;
}UART_RESOURCES;

#define UART_TX_TRIG_LVL    (UART_TX_FIFO_SIZE / 2)
#define UART_RX_TRIG_LVL    (UART_RX_FIFO_SIZE / 2)
#define hal_SendByte(uart, byte_to_send)      uart->REG_RXTX_BUFFER.all = byte_to_send;
#define hal_GetByte(uart)                     uart->REG_RXTX_BUFFER.all
#define GET_BITFIELD(reg, bitfield)           (((reg) & bitfield ## _MASK) >> bitfield ## _SHIFT)
#define CLR_BITFIELD(reg, bitfield)           ((reg) & (~(bitfield ## _MASK << bitfield ## _SHIFT)))
#define SET_BITFIELD(reg, val, bitfield)      ((CLR_BITFIELD(reg, bitfield)) | bitfield(val))


#endif
