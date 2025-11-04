#ifndef _SHELL_UART_H_
#define _SHELL_UART_H_


#define SHELL_UART0              0
#define SHELL_UART1              1

#define SHELL_UART               CONFIG_SYSLOG_UART_PORT
#define SHELL_UART_BAUDRATE      CONFIG_SYSLOG_UART_BAUDRATE


#define UART0_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART0_IO_TX_PIN          (2)
#define UART0_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART0_IO_RX_PIN          (3)
#define UART0_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER2)

#define UART1_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART1_IO_TX_PIN          (21)
#define UART1_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART1_IO_RX_PIN          (20)
#define UART1_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER3)



typedef void* uartHandle_t;

#endif
