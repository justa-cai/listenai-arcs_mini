#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "IOMuxManager.h"
#include "Driver_SPI.h"

#include "FreeRTOS.h"
#include "task.h"

#define SPI_MOSI_IO_PAD         CSK_IOMUX_PAD_A
#define SPI_MOSI_IO_PIN         15
#define SPI_MOSI_IO_SEL         CSK_IOMUX_FUNC_ALTER6

#define SPI_MISO_IO_PAD         CSK_IOMUX_PAD_A
#define SPI_MISO_IO_PIN         14
#define SPI_MISO_IO_SEL         CSK_IOMUX_FUNC_ALTER6

#define SPI_CLK_IO_PAD          CSK_IOMUX_PAD_A
#define SPI_CLK_IO_PIN          17
#define SPI_CLK_IO_SEL          CSK_IOMUX_FUNC_ALTER6

#define SPI_CS_IO_PAD           CSK_IOMUX_PAD_A
#define SPI_CS_IO_PIN           19
#define SPI_CS_IO_SEL           CSK_IOMUX_FUNC_ALTER6

#define DATA_SIZE 1024

static void* spi_handler = NULL;
static volatile uint32_t spi_event = 0;
static uint8_t data[DATA_SIZE];

void spi_pin_init()
{
   /* SPI1引脚配置 */
   IOMuxManager_PinConfigure(SPI_CLK_IO_PAD,  SPI_CLK_IO_PIN,  SPI_CLK_IO_SEL);     // CLK
   IOMuxManager_PinConfigure(SPI_CS_IO_PAD,   SPI_CS_IO_PIN,   SPI_CS_IO_SEL);      // CS
   IOMuxManager_PinConfigure(SPI_MOSI_IO_PAD, SPI_MOSI_IO_PIN, SPI_MOSI_IO_SEL);    // MOSI
   IOMuxManager_PinConfigure(SPI_MISO_IO_PAD, SPI_MISO_IO_PIN, SPI_MISO_IO_SEL);    // MISO

   spi_handler = SPI1();
}

static void spi_driver_event_cb(uint32_t event, uint32_t usr_param)
{
    // printf("event: %d\n", event);
    spi_event |= event;
}

static void spi_test(void)
{
    /* 初始化SPI */
    SPI_Initialize(spi_handler, spi_driver_event_cb, (uint32_t)spi_handler);
    SPI_PowerControl(spi_handler, CSK_POWER_FULL);
    
    /* 配置SPI */
    SPI_Control(spi_handler, CSK_SPI_MODE_MASTER |      // master模式
                                CSK_SPI_TXIO_DMA |      // 使用DMA发送
                                CSK_SPI_CPOL1_CPHA1 |   // CPOL=1, CPHA=1
                                CSK_SPI_DATA_BITS(8) |  // 8位数据
                                CSK_SPI_MSB_LSB,        // MSB first
                                1000000);               // spi1的master输出时钟为1MHz

    /* 准备发送数据 */
    for (int i = 0; i < DATA_SIZE; i++) {
        data[i] = i % 256;
    }
    
    while(1){
        spi_event = 0;
        /* 发送数据 */
        SPI_Send(spi_handler, data, DATA_SIZE);

        /* 等待发送完成 */
        while(!(spi_event & CSK_SPI_EVENT_TRANSFER_COMPLETE));
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* 关闭SPI */
    SPI_PowerControl(spi_handler, CSK_POWER_OFF);
    SPI_Uninitialize(spi_handler);
}

int main(int argc, char **argv)
{
    printf("Hello, world! SPI\n");

    spi_pin_init();

    spi_test();

    return 0;
}