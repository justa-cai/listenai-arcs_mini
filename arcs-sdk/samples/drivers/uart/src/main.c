#include <stdio.h>
#include <stdint.h>

#include "IOMuxManager.h"
#include "Driver_UART.h"

#include "FreeRTOS.h"
#include "task.h"

/* UART2 PIN*/
#define UART2_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_TX_PIN          (21)
#define UART2_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER4)

#define UART2_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_RX_PIN          (20)
#define UART2_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER4)

#define RX_BUFFER_SIZE           (64)

static void* UART_Handler = NULL;

static int rpos = 0; 
static uint8_t recv_buffer[RX_BUFFER_SIZE];
static void UART_EventCallback(uint32_t event, void* workspace)
{
    int ipos = 0;
    switch (event) {
    case CSK_UART_EVENT_RECEIVE_COMPLETE:   /* 接收完成中断 */
        if (rpos < RX_BUFFER_SIZE) {
            /* 把收到的数据发送出去 */
            UART_Send(UART_Handler, &recv_buffer[rpos], RX_BUFFER_SIZE - rpos);
        }

        rpos = 0;
        /* 再一次启动串口接收，接收数据量为RX_BUFFER_SIZE */
        UART_Receive(UART_Handler, recv_buffer, RX_BUFFER_SIZE);
        break;
    case CSK_UART_EVENT_RX_TIMEOUT:         /* 接收超时中断 */
        if ((ipos = UART_GetRxCount(UART_Handler)) > rpos) {
            /* 把收到的数据发送出去 */
            UART_Send(UART_Handler, &recv_buffer[rpos], ipos-rpos);
        }
        rpos = ipos;

        break;
    default:
        break;
    }
}

static void UART_Interrupt_RXTX(void)
{
    /* 注册用户回调 */
    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    /* 使能UART2时钟，注册UART2中断回调、使能UART2中断 */
    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    /* UART配置，包含下面配置
     * 中断模式：选择中断模式，而不是DMA模式来传输
     * 异步模式：使能异步模式，以及空闲超时中断
     * 串口数据的格式：8位数据位，无校验位，1位停止位，无流控，波特率115200
     */
    UART_Control(UART_Handler, CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
                        CSK_UART_DATA_BITS_8 |
                        CSK_UART_PARITY_NONE |
                        CSK_UART_STOP_BITS_1 |
                        CSK_UART_FLOW_CONTROL_NONE |
                        CSK_UART_Function_CONTROL_Int |
                        CSK_UART_GPIO_CONTROL_DEFAULT, 115200);

    /* 使能串口发送 */
    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);

    /* 使能串口接收 */
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    /* 启动串口接收，接收数据量为RX_BUFFER_SIZE */
    UART_Receive(UART_Handler, recv_buffer, RX_BUFFER_SIZE);
}

int main(int argc, char **argv)
{
    printf("Hello, world! UART RX TX\n");

    /* PA20和PA21引脚配置为UART2,  具体IOMUX列表见芯片手册的APPENDIX章节*/
    IOMuxManager_PinConfigure(UART2_IO_TX_PAD, UART2_IO_TX_PIN, UART2_IO_TX_SEL);
    IOMuxManager_PinConfigure(UART2_IO_RX_PAD, UART2_IO_RX_PIN, UART2_IO_RX_SEL);

    /* 获取UART指针 */
    UART_Handler = UART2();

    /* 启动UART操作 */
    UART_Interrupt_RXTX();

    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
