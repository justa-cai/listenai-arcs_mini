#include "chip.h"
#include "Driver_UART.h"
#include "IOMuxManager.h"
#include "ClockManager.h"

#include "shell.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "string.h"

#define UART0_IO_TX_PAD (CSK_IOMUX_PAD_A)
#define UART0_IO_TX_PIN 3
#define UART0_IO_TX_SEL (CSK_IOMUX_FUNC_ALTER2)

#define UART0_IO_RX_PAD (CSK_IOMUX_PAD_A)
#define UART0_IO_RX_PIN 2
#define UART0_IO_RX_SEL (CSK_IOMUX_FUNC_ALTER2)

#define UART1_IO_TX_PAD (CSK_IOMUX_PAD_A)
#define UART1_IO_TX_PIN (21)
#define UART1_IO_TX_SEL (CSK_IOMUX_FUNC_ALTER3)

#define UART1_IO_RX_PAD (CSK_IOMUX_PAD_A)
#define UART1_IO_RX_PIN (20)
#define UART1_IO_RX_SEL (CSK_IOMUX_FUNC_ALTER3)

#define SHELL_UART_TX_PIN UART0_IO_TX_PIN
#define SHELL_UART_TX_PAD UART0_IO_TX_PAD
#define SHELL_UART_TX_SEL UART0_IO_TX_SEL

#define SHELL_UART_RX_PIN UART0_IO_RX_PIN
#define SHELL_UART_RX_PAD UART0_IO_RX_PAD
#define SHELL_UART_RX_SEL UART0_IO_RX_SEL

#define SHELL_UART_BAUDRATE (921600)
#define UART_PORT()         UART0()
#define SHELL_CLK_INIT()    HAL_CRM_SetUart0ClkDiv(1, 1)

static Shell *g_shell;
static SemaphoreHandle_t uart_rx_sem = NULL;

static void uart_event_callback(uint32_t event, void *workspace)
{
    if ((event & CSK_UART_EVENT_RX_TIMEOUT) || (event & CSK_UART_EVENT_RECEIVE_COMPLETE)) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(uart_rx_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static int shell_uart_init(void *port, int32_t baudrate)
{
    UART_Initialize(port, uart_event_callback, NULL);
    UART_PowerControl(port, CSK_POWER_FULL);

    UART_Control(port,
                 CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT | CSK_UART_DATA_BITS_8 | CSK_UART_PARITY_NONE |
                     CSK_UART_STOP_BITS_1 | CSK_UART_FLOW_CONTROL_NONE | CSK_UART_Function_CONTROL_Int |
                     CSK_UART_GPIO_CONTROL_DEFAULT,
                 baudrate);

    UART_Control(port, CSK_UART_CONTROL_TX, 1);
    UART_Control(port, CSK_UART_CONTROL_RX, 1);
    UART_Control(port, CSK_UART_DISABLE_TX_INT, 1);

    return 0;
}

static int lisa_shell_low_init(void)
{
    IOMuxManager_PinConfigure(SHELL_UART_TX_PAD, SHELL_UART_TX_PIN, SHELL_UART_TX_SEL);
    IOMuxManager_PinConfigure(SHELL_UART_RX_PAD, SHELL_UART_RX_PIN, SHELL_UART_RX_SEL);

    SHELL_CLK_INIT();

    return shell_uart_init(UART_PORT(), SHELL_UART_BAUDRATE);
}

static signed short shell_write(char *data, unsigned short size)
{
    UART_Send(UART_PORT(), data, size);

    return size;
}

static signed short shell_read(char *data, unsigned short size)
{
    UART_Receive(UART_PORT(), data, size);

    xSemaphoreTake(uart_rx_sem, portMAX_DELAY);

    return size;
}

static void shell_task(void *p)
{
    int r;
    Shell *sh = (Shell *)p;

    assert(sh);

    while (1) {
        uint8_t ch;
        if (sh->read && sh->read(&ch, 1) == 1) {
            shellHandler(sh, ch);
        }
    }
}

int lisa_shell_init(void)
{
    int r;

    uart_rx_sem = xSemaphoreCreateBinary();
    assert(uart_rx_sem);

    r = lisa_shell_low_init();
    assert(r == 0);

    Shell *sh = malloc(sizeof(Shell));
    assert(sh);
    memset(sh, 0, sizeof(Shell));
    uint8_t *shell_buf = malloc(512);
    assert(shell_buf);
    sh->write = shell_write;
    sh->read = shell_read;

    g_shell = sh;
    shellInit(sh, shell_buf, 512);

    xTaskCreate(shell_task, "shell", 1024, sh, configMAX_PRIORITIES - 1, NULL);
}

void lisa_shell_output_raw(const char *data, int len)
{
    if (g_shell) {
        shellWriteEndLine(g_shell, (uint8_t *)data, len);
    }
}
