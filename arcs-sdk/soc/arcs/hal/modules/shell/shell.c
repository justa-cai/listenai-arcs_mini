#ifndef WIN32
#include <string.h>
#include "IOMuxManager.h"
#include "Driver_UART.h"
#include "rtos_al.h"
#include "shell_def.h"
#include "shell_uart.h"
#include "shell_input.h"
#include "ClockManager.h"

const char *banner = {
"\r\n"
"Welcome to\r\n"
" _     _       _                   _    _____\r\n"
"| |   (_)-----. |  .-----.-----.  / \\  |_____|\r\n"
"| |   | |  ___| |__|  _  |     | / _ \\   | |\r\n"
"| |___| |____ |  __|   __|  |  |/ /_\\ \\ _| |_\r\n"
"|_____|_|_____|____|_____|__|__|_/   \\_\\_|_|_|\r\n"
};
static rtos_task_handle shell_task_handle;
static struct shell_env *shell = NULL;
static uartHandle_t uart_handle = NULL;
static rtos_semaphore read_semaphore = NULL;
static uint8_t shell_enable_flg = 1;

static volatile uint32_t uart_event = 0;
static void uart_event_callback(uint32_t event, void* workspace)
{
    if ((event & CSK_UART_EVENT_RX_TIMEOUT) || (event & CSK_UART_EVENT_RECEIVE_COMPLETE))
    {
        rtos_semaphore_signal(read_semaphore, 1);
        uart_event |= CSK_UART_EVENT_RECEIVE_COMPLETE;
    }

    if (event & CSK_UART_EVENT_SEND_COMPLETE)
        uart_event |= event;
}

void shell_output_fmt_string(char *str)
{
    if(NULL != shell)
        shell->output(str, strlen(str));
}

void shell_output_data(char *data, uint32_t len)
{
    if(NULL!= shell)
        shell->output(data, len);
}

static uartHandle_t shell_uart_init(int32_t uart, int32_t baudrate)
{
    uartHandle_t  handle = NULL;

    if (SHELL_UART0 == uart)
    {
        IOMuxManager_PinConfigure(UART0_IO_TX_PAD, UART0_IO_TX_PIN, UART0_IO_TX_SEL);
        IOMuxManager_PinConfigure(UART0_IO_RX_PAD, UART0_IO_RX_PIN, UART0_IO_RX_SEL);
        handle = UART0();
        HAL_CRM_SetUart0ClkDiv(1,1);
    }
    else if (SHELL_UART1 == uart)
    {
        IOMuxManager_PinConfigure(UART1_IO_TX_PAD, UART1_IO_TX_PIN, UART1_IO_TX_SEL);
        IOMuxManager_PinConfigure(UART1_IO_RX_PAD, UART1_IO_RX_PIN, UART1_IO_RX_SEL);
        HAL_CRM_SetUart1ClkDiv(1,1);
        handle = UART1();
    }
    else
    {
        return NULL;
    }

    UART_Initialize(handle, uart_event_callback, NULL);
    UART_PowerControl(handle, CSK_POWER_FULL);

    UART_Control(handle, CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, SHELL_UART_BAUDRATE);

    UART_Control(handle, CSK_UART_CONTROL_TX, 1);
    UART_Control(handle, CSK_UART_CONTROL_RX, 1);
    UART_Control(handle, CSK_UART_DISABLE_TX_INT, 1);

    return handle;
}

static void shell_uart_deinit(void)
{
    if (NULL != uart_handle)
    {
        UART_PowerControl(uart_handle, CSK_POWER_OFF);
        UART_Uninitialize(uart_handle);
        uart_handle = NULL;
    }
}

static uint32_t shell_uart_read(uint8_t* buffer, int32_t len)
{
    uint32_t  cnt = 0;

    UART_Receive(uart_handle, buffer, len);
    rtos_semaphore_wait(read_semaphore, portMAX_DELAY);
    //while(!(uart_event & CSK_UART_EVENT_RECEIVE_COMPLETE));
    uart_event &= ~(CSK_UART_EVENT_RECEIVE_COMPLETE);
    cnt = UART_GetRxCount(uart_handle);

    return cnt;
}

static int32_t shell_uart_write(uint8_t* buffer, int32_t len)
{
    UART_Send(uart_handle, buffer, len);

    return 0;
}

static void shell_deinit(void)
{
    if (NULL != shell)
    {
        shell_free(shell);
        shell = NULL;
    }
}

static void shell_print_banner(struct shell_env* shell)
{
    shell_write_string(shell, banner);
    shell_print_time(shell);
    shell_print_prompt(shell);
}

static RTOS_TASK_FCT(shell_task)
{
    int32_t len, i;
    uint8_t data;

    while (1)
    {
        if(shell_enable_flg) {
            len = shell->input(&data, 1);
            if (len)
                shell_parse(shell, data);
        } else {
            shell->input(&data, 1);
        }
    }
}

int32_t shell_init(shell_process process_fun)
{
    int32_t ret = 0;

    rtos_semaphore_create(&read_semaphore, 1, 0);
    if ( (NULL == read_semaphore))
        return -1;

    shell = (struct shell_env*)shell_malloc(sizeof(struct shell_env));
    if (NULL == shell)
        return -1;

    memset(shell, 0, sizeof(struct shell_env));
    shell->input   = shell_uart_read;
    shell->output  = shell_uart_write;
    shell->parser.buffer_size = SHELL_CMD_LEN_MAX;
    shell->process = process_fun;

    uart_handle = shell_uart_init(SHELL_UART, SHELL_UART_BAUDRATE);
    if (NULL == uart_handle)
    {
        shell_deinit();
        return -1;
    }

    shell_print_banner(shell);

#ifdef TASK_CREATE_STATIC
    static rtos_stack_type shell_task_stack_buf[SHELL_TASK_STACK_SIZE];
    static rtos_static_task_tcb shell_task_control;
    if (rtos_task_create_static(shell_task, "shell task",
                                SHELL_TASK, SHELL_TASK_STACK_SIZE, NULL, SHELL_TASK_PRIORITY, &shell_task_handle, shell_task_stack_buf, &shell_task_control))
#else
    if (rtos_task_create(shell_task, "shell task", 
                                SHELL_TASK, SHELL_TASK_STACK_SIZE, NULL, SHELL_TASK_PRIORITY, &shell_task_handle))
#endif
    {
        shell_deinit();
        shell_uart_deinit();
        return -1;
    }

    return ret;
}
void shell_enable(void)
{
    shell_enable_flg = 1;
}

void shell_disable(void)
{
    shell_enable_flg = 0;
}

int32_t shell_status(void)
{
    return shell_enable_flg;
}

#endif
