/*
 * log_print.c
 *
 */
#include <stdio.h>
#include <stdint.h>

#include "tinyprintf.h"
#include "uart_reg.h"
#include "log_print.h"
#include "arcs_ap.h"
#include "ClockManager.h"
#include "IOMuxManager.h"

#define UART_TX_FIFO_DEPTH                16

static UART_RegDef* uart = NULL;
uint32_t taskmask = 0xFFFFFFFF;

#define WEAK_LOG_LEVEL_VAR(val) __attribute__((weak)) uint32_t cloglvl = (val);

#if defined(CONFIG_ARCS_HAL_LOG_LEVEL_NONE)
WEAK_LOG_LEVEL_VAR(0)
#elif defined(CONFIG_ARCS_HAL_LOG_LEVEL_ERROR)
WEAK_LOG_LEVEL_VAR(1)
#elif defined(CONFIG_ARCS_HAL_LOG_LEVEL_WARN)
WEAK_LOG_LEVEL_VAR(2)
#elif defined(CONFIG_ARCS_HAL_LOG_LEVEL_INFO)
WEAK_LOG_LEVEL_VAR(3)
#elif defined(CONFIG_ARCS_HAL_LOG_LEVEL_DEBUG)
WEAK_LOG_LEVEL_VAR(4)
#elif defined(CONFIG_ARCS_HAL_LOG_LEVEL_VERBOSE)
WEAK_LOG_LEVEL_VAR(5)
#else
WEAK_LOG_LEVEL_VAR(5)
#endif

void (*log_print_hook)(const char *, va_list) = NULL;

#define hal_SendByte(uart, byte_to_send)      uart->REG_RXTX_BUFFER.all = byte_to_send;

#if defined(CFG_RTOS) && defined(CFG_AMP_IPC) && (CFG_IPC_PRINT)
extern int32_t ipc_dbg_output(char *string, int32_t len);
#endif
//// put char function can accelerate by using FIFO
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
__attribute__((used))
__attribute__((weak)) int _write(int file, char *data, int len){
    int i;
    for (i = 0; i < len; i++){
        uart->REG_RXTX_BUFFER.all = data[i];
        while(!uart->REG_STATUS.bit.TX_FIFO_SPACE);
    }
    return len;
}
#endif
__attribute__((weak)) void log_write(void *unused, char c){
#if defined(CFG_AMP_IPC) && defined(CFG_AMP_IPC_SLAVE) && (CFG_IPC_PRINT)
    if (rtos_os_started())
    {
        ipc_dbg_output(c);
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

#define UART0_TX_PIN_NUM                       (3)
#define UART1_TX_PIN_NUM                       (4)

#define UART0_TX_IO_MUX_FUNC_SEL               (0x2)
#define UART1_TX_IO_MUX_FUNC_SEL               (0x3)

__attribute__((weak)) void __log_io_config(int dbg){
    switch (dbg){
    case 0:
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, UART0_TX_PIN_NUM, UART0_TX_IO_MUX_FUNC_SEL);
        break;
    case 1:
    default:
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, UART1_TX_PIN_NUM, UART1_TX_IO_MUX_FUNC_SEL);
        break;
    }
}

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

__attribute__((weak)) void log_flush(void){
	while(UART_TX_FIFO_DEPTH - uart->REG_STATUS.bit.TX_FIFO_SPACE);
}

static uint8_t logd_close_flag=0;
void logDbg_enable_set(uint8_t logD_on_off)
{
    logd_close_flag = logD_on_off;
    return;
}

__attribute__((weak)) void logDbg(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    if (log_print_hook) {
        log_print_hook(format, args);
    }
    va_end(args);
}

int log_print_hook_set(void (*hook)(const char *, va_list))
{
    log_print_hook = hook;
    return 0;
}

void log_print_level_set(uint32_t level)
{
    cloglvl = level;
}

uint32_t log_print_level_get(void)
{
    return cloglvl;
}


