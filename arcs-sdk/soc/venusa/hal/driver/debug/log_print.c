/**
 * @file log_print.c
 * @brief Logging Print System Implementation File
 *        This file implements the low-level logging print functionality using UART peripherals.
 *        It provides character output routines, baud rate configuration, and debug messaging capabilities.
 *
 * @details
 *   Key Features:
 *     - Supports both standard I/O redirection (printf) and direct log APIs
 *     - Configurable UART selection (UART0/UART1) via build flags
 *     - Automated baud rate divisor calculation for accurate timing
 *     - Hardware FIFO buffer management with flow control
 *     - RTOS-aware interprocess communication path for multicore systems
 *     - Optional SEGGER RTT viewer integration for embedded visualization
 *
 * @note
 *    Requires proper hardware initialization before calling logInit().
 *    Must be compiled with either CFG_RTOS+CFG_AMP_IPC or standalone mode.
 */

#include <stdio.h>
#include "tinyprintf.h"
#include "log_print.h"
#include "venusa_ap.h"
#include "ClockManager.h"
#include "IOMuxManager.h"

/**
 * @def UART_TX_FIFO_DEPTH
 * @brief UART Transmit FIFO Depth
 *       Maximum number of bytes that can be stored in transmit FIFO buffer
 */
#define UART_TX_FIFO_DEPTH                16

/**
 * @var uart
 * @brief Global UART Register Block Pointer
 *       Points to currently active UART peripheral register block
 */
static UART_RegDef* uart = NULL;

/**
 * @var cloglvl
 * @brief Log Level Control
 *        Controls maximum allowed log severity level (higher numbers = more detailed logs)
 */
uint32_t cloglvl = 5;

/**
 * @var taskmask
 * @brief Task Masking Value
 *        Used for temporary task masking during critical sections
 */
uint32_t taskmask = 0xFFFFFFFF;

/**
 * @macro hal_SendByte
 * @brief Send Single Byte Through UART
 * @param[in] uart  UART register block pointer
 * @param[in] byte_to_send Byte to transmit
 * @note Directly writes to RXTX buffer register (shared Tx/Rx path)
 */
#define hal_SendByte(uart, byte_to_send)      uart->REG_RXTX_BUFFER.all = byte_to_send;

#if defined(CFG_RTOS) && defined(CFG_AMP_IPC) && (CFG_IPC_PRINT)
extern int32_t ipc_slave_putchar(char c);
#endif

/**
 * @fn fputc
 * @brief Standard Library Put Char Function
 * @param[in] ch Character to send
 * @param[in] f File pointer (unused but required by spec)
 * @return Returns received character
 * @note Specialized implementation for Clang compiler using hardware acceleration
 *       Polls for FIFO space before transmission
 */
#ifdef __clang__
__attribute__((used))
int fputc(int ch, FILE *f)
{
    /* Place your implementation of fputc here */
    while(!uart->REG_STATUS.bit.TX_FIFO_SPACE);
    hal_SendByte(uart, ch);

    return ch;
}
#else
/**
 * @fn _write
 * @brief Low-Level Write Function Alternative
 * @param[in] file File descriptor (ignored)
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of bytes to write
 * @return Returns number of bytes written
 * @note Fallback implementation when not using Clang compiler
 *       Performs byte-by-byte transmission with FIFO checking
 */
__attribute__((used))
int _write(int file, char *data, int len){
    int i;
    for (i = 0; i < len; i++){
        uart->REG_RXTX_BUFFER.all = data[i];
        while(!uart->REG_STATUS.bit.TX_FIFO_SPACE);
    }
    return len;
}
#endif

/**
 * @fn log_write
 * @brief Logging System Write Function
 * @param[in] unused Unused parameter (maintains API consistency)
 * @param[in] c Character to log
 * @note Handles both RTOS IPC paths and direct UART paths
 *       Priority: IPC first, fallback to direct UART if IPC unavailable
 */
void log_write(void *unused, char c){
#if defined(CFG_RTOS) && defined(CFG_AMP_IPC) && (CFG_IPC_PRINT)
    if (rtos_os_started())
    {
        ipc_slave_putchar(c);
    }
    else
    {
        uart->REG_RXTX_BUFFER.all = c;
        while(!uart->REG_STATUS.bit.TX_FIFO_SPACE);
    }
#else
    uart->REG_RXTX_BUFFER.all = c;
    while(!uart->REG_STATUS.bit.TX_FIFO_SPACE);
#endif
}

/**
 * @fn compute_gcd
 * @brief Greatest Common Divisor Calculation
 * @param[in] a First number
 * @param[in] c Second number
 * @return GCD of a and c
 * @note Simple Euclidean algorithm implementation
 *        Used for baud rate divisor calculations
 */
static uint32_t compute_gcd(uint32_t a, uint32_t c)
{
	uint32_t t;
    while(c != 0) {
    	t = a % c;
        a = c;
        c = t;
    }
return a;
}

/**
 * @fn __log_compute_div
 * @brief Private Baud Rate Divisor Calculation
 * @param[in] dbg UART selection flag (0=UART0, 1=UART1)
 * @param[in] baudrate Desired baud rate
 * @param[out] pm Numerator result
 * @param[out] pn Denominator result
 * @param[in] div Prescaler value (4 or 16)
 * @return 0 on success, -1 on error
 * @note Attempts to find valid divisor pair within hardware constraints
 *       Valid ranges: m <= 1023, n <= 511
 */
static int32_t __log_compute_div(int dbg, uint32_t baudrate, uint32_t *pm, uint32_t *pn, uint32_t div)
{
	uint32_t gcd, m, n, tmp;
	int32_t ret = 0;

	tmp = div * baudrate;
	uint32_t uart_clk;

    if (dbg == 0){
    	uart_clk = 24000000;
    } else if (dbg == 1){
    	uart_clk = 24000000;
    }

	gcd = compute_gcd(uart_clk, tmp);
	m = uart_clk / gcd;
	n = tmp / gcd;

	if ((m > 1023) || (n > 511)){
		return -1;
	}

	*pm = m;
	*pn = n;

	return 0;
}

/**
 * @fn log_compute_div
 * @brief Public Baud Rate Divisor Calculator
 * @param[in] dbg UART selection flag
 * @param[in] baudrate Desired baud rate
 * @param[out] pm Numerator result
 * @param[out] pn Denominator result
 * @param[out] pdiv Chosen prescaler value
 * @return 0 on success, -1 on error
 * @note Tries preferred prescalers (4 then 16) until valid combination found
 */
static int32_t log_compute_div(int dbg, uint32_t baudrate, uint32_t *pm, uint32_t *pn, uint32_t *pdiv)
{
	int32_t ret = 0;

	do {
		if(__log_compute_div(dbg, baudrate, pm, pn, 4) == 0) {
			*pdiv = 4;
			break;
		}
		if(__log_compute_div(dbg, baudrate, pm, pn, 16) == 0) {
			*pdiv = 16;
			break;
		}
		ret = -1;
	} while(0);

	return ret;
}

/**
 * @def UART0_TX_PIN_NUM
 * @brief UART0 Transmit Pin Number
 *       Physical pin assignment for UART0 TX function
 */
#define UART0_TX_PIN_NUM                      (5)

/**
 * @def UART1_TX_PIN_NUM
 * @brief UART1 Transmit Pin Number
 *       Physical pin assignment for UART1 TX function
 */
#define UART1_TX_PIN_NUM                      (4)

/**
 * @def UART0_TX_IO_MUX_FUNC_SEL
 * @brief UART0 TX MUX Function Select
 *       Multiplexer setting for UART0 TX pin function
 */
#define UART0_TX_IO_MUX_FUNC_SEL               (0x2)

/**
 * @def UART1_TX_IO_MUX_FUNC_SEL
 * @brief UART1 TX MUX Function Select
 *       Multiplexer setting for UART1 TX pin function
 */
#define UART1_TX_IO_MUX_FUNC_SEL               (0x3)

/**
 * @fn __log_io_config
 * @brief Weak UART Pin Configuration Function
 * @param[in] dbg UART selection flag
 * @note Can be overridden by user applications
 *       Default implementation configures appropriate TX pin based on selected UART
 */
__attribute__((weak)) void __log_io_config(int dbg){
    switch (dbg){
    case 0:
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, UART0_TX_PIN_NUM, UART0_TX_IO_MUX_FUNC_SEL);
        break;
    case 1:
    default:
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, UART1_TX_PIN_NUM, UART1_TX_IO_MUX_FUNC_SEL);
        break;
    }
}

/**
 * @fn logInit
 * @brief Logging System Initialization
 * @param[in] dbg UART selection flag (0=UART0, 1=UART1)
 * @param[in] baudrate Desired baud rate
 * @return 0 on success, -1 on error
 * @note Must be called before any logging operations
 *       Performs complete UART initialization sequence including:
 *         - Pin multiplexer configuration
 *         - Clock enable and divisor setup
 *         - FIFO reset and control register programming
 *         - tinyprintf initialization
 */
int logInit(int dbg, uint32_t baudrate)
{
    uint32_t m, n, div, uart_base, cmn_reg;

    if(log_compute_div(dbg, baudrate, &m, &n, &div) < 0) {
		return -1;
	}

    __log_io_config(dbg);

    switch (dbg){
    case 0:
    	uart = (UART_RegDef *)UART0_BASE;

        __HAL_CRM_UART0_CLK_ENABLE();
        HAL_CRM_SetUart0ClkDiv(n, m);
    	break;
    case 1:
    default:
    	uart = (UART_RegDef *)UART1_BASE;

        __HAL_CRM_UART1_CLK_ENABLE();
        HAL_CRM_SetUart1ClkDiv(n, m);
    	break;
    }

    uart->REG_IRQ_MASK.all = 0;
    uart->REG_CTRL.all = 0;

    if(div == 16) {
        uart->REG_CTRL.bit.DIVISOR_MODE = 1;  //0: sclk/4; 1: sclk/16
    } else {
        uart->REG_CTRL.bit.DIVISOR_MODE = 0;
    }

    uart->REG_CMD_SET.bit.TX_FIFO_RESET = 1; //reset tx fifo
    uart->REG_CMD_SET.bit.RX_FIFO_RESET = 1; //reset rx fifo

    uart->REG_CTRL.bit.DATA_BITS = 1;     // 8bits
    uart->REG_CTRL.bit.ENABLE = 1;        // enable uart
    uart->REG_STATUS.all = 1;                  // clear line error bits

    init_printf(NULL, log_write);
    return 0;
}

/**
 * @fn log_flush
 * @brief Flush Transmit FIFO
 * @note Ensures all pending characters are transmitted
 *       Blocks until FIFO has sufficient space for full flush
 */
void log_flush(void){
	while(UART_TX_FIFO_DEPTH - uart->REG_STATUS.bit.TX_FIFO_SPACE);
}

/**
 * @var logd_close_flag
 * @brief Debug Logging State Flag
 *       Controls whether debug logging is enabled (0=enabled, 1=disabled)
 */
static uint8_t logd_close_flag=0;

/**
 * @fn logDbg_enable_set
 * @brief Set Debug Logging State
 * @param[in] logD_on_off New state (0=enable, 1=disable)
 * @note Affects behavior of logDbg() function
 */
void logDbg_enable_set(uint8_t logD_on_off)
{
    logd_close_flag = logD_on_off;
    return;
}

/**
 * @fn logDbg
 * @brief Formatted Debug Logging Function
 * @param[in] format Format string (printf style)
 * @param[in] ... Variable arguments
 * @note Uses either SEGGER RTT viewer or regular tfp_vprintf depending on configuration
 *       Disabled when logd_close_flag is set to 1
 */
void logDbg(const char* format, ...)
{
    va_list ap;
    if (logd_close_flag == 1)
    {
        return;
    }
    va_start(ap, format);

#ifdef CONFIG_USE_RTT
#include "SEGGER_RTT.h"
    SEGGER_RTT_vprintf(0, format, &ap);
#else
    tfp_vprintf(format, ap);
#endif    
    va_end(ap);
}
