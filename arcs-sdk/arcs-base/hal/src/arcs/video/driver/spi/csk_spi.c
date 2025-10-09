#include "csk_spi.h"


_DMA static uint8_t DMA_Data;

static void *gSpiDev = NULL;

static void *spi_dma_data = NULL;
static uint32_t spi_dma_size = 0;

/*SPI Event*/
//static volatile uint32_t SPIEvent = 0;
//SemaphoreHandle_t xSemaphore;
static volatile uint8_t g_run_flag = 0;

static inline void INIT_XFER_FLAG()
{
    g_run_flag = 0;
}

static inline void SET_XFER_DONE()
{
    g_run_flag = 1;
}

static inline void CLR_XFER_DONE()
{
    g_run_flag = 0;
}

static inline uint8_t GET_XFER_DONE()
{
    return g_run_flag;
}


// wait receive done with timeout
static bool wait_xfer_done_timeout(uint32_t max_wait_ms)
{
    bool ret = false;
    uint32_t cnt = 0;

    //csk_timer_start();
    cnt = csk_timer_elapsed();
    while(csk_timer_elapsed() < (cnt + max_wait_ms))
    {
        if(GET_XFER_DONE())
        {
            CLR_XFER_DONE();
            ret = true;
            break;
        }
    }
    //csk_timer_stop();
    CLR_XFER_DONE();
    return ret;
}


// SPI event
static void SPI_DrvEvent (uint32_t event, uint32_t usr_param)
{
    //notify SPI to send next block of data
    if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE)
    {
        if((spi_dma_size > 0) && (spi_dma_data != NULL))
        {
            if(spi_dma_size <= SPI_DMA_MAX_TRANCNT)
            {
                SPI_Send(gSpiDev, spi_dma_data, spi_dma_size);
                spi_dma_data = NULL;
                spi_dma_size = 0;
            }
            else
            {
                SPI_Send(gSpiDev, spi_dma_data, SPI_DMA_MAX_TRANCNT);
                spi_dma_data += SPI_DMA_MAX_TRANCNT;
                spi_dma_size -= SPI_DMA_MAX_TRANCNT;
            }
        }
        else
        {
            SET_XFER_DONE();
        }
    }
}


void csk_spi_init(uint8_t spi_index)
{
	int32_t ret;

	//VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

//    IOMuxManager_PinConfigure(LCD_SPI_IOMUX_PAD, LCD_SPI_CLK_PIN, LCD_SPI_IOMUX_FUN); //FIXME:
//    IOMuxManager_PinConfigure(LCD_SPI_IOMUX_PAD, LCD_SPI_MOSI_PIN, LCD_SPI_IOMUX_FUN); //FIXME:
//    IOMuxManager_PinConfigure(LCD_SPI_IOMUX_PAD, LCD_SPI_CS_PIN, LCD_SPI_IOMUX_FUN); //FIXME:

    if(spi_index == 0) {
        gSpiDev = SPI0();
    } else {
        gSpiDev = SPI1();
    }

    //SPI configuration
    uint32_t bus_speed = SPI_BUS_SPEED;
    ret = SPI_Initialize(gSpiDev, SPI_DrvEvent, (uint32_t)gSpiDev);
    if (ret != CSK_DRIVER_OK) {
    	VIDEO_LOG("[%s:%d] SPI_Initialize (spi_no = %d) call failed!!\r\n", __func__, __LINE__, ret);
        return;
    }

    ret = SPI_PowerControl (gSpiDev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
    	VIDEO_LOG("[%s:%d] SPI_PowerControl (spi_no = %d) call failed!!\r\n", __func__, __LINE__, ret);
        return;
    }

    ret = SPI_Control(gSpiDev, CSK_SPI_MODE_MASTER | CSK_SPI_TXIO_DMA  | //FIXME: CSK_SPI_TXIO_PIO / CSK_SPI_TXIO_DMA
                 CSK_SPI_CPOL1_CPHA1 |
                 CSK_SPI_DATA_BITS(SPI_DATA_BITS) |
                 CSK_SPI_MSB_LSB, bus_speed);
    if (ret != CSK_DRIVER_OK) {
    	VIDEO_LOG("[%s:%d] SPI_Control (spi_no = %d) call failed!!\r\n", __func__, __LINE__, ret);
        return;
    }

    bus_speed = SPI_Control(gSpiDev, CSK_SPI_GET_BUS_SPEED, 0);
    VIDEO_LOG("[%s:%d] Current actual SPI speed: %d", __func__, __LINE__, bus_speed);

    // divided by 2 if bus speed is greater than 100MHz
    if (bus_speed > 100000000) { // 100MHz
        bus_speed /= 2;
        SPI_Control(gSpiDev, CSK_SPI_SET_BUS_SPEED, bus_speed);
        bus_speed = SPI_Control(gSpiDev, CSK_SPI_GET_BUS_SPEED, 0);
        VIDEO_LOG("Current newest SPI speed: %d", bus_speed);
    }

    INIT_XFER_FLAG();
}


void csk_spi_data_bit_set(uint8_t bit)
{
    int32_t ret;

    //VIDEO_LOG("[%s:%d] %d", __func__, __LINE__, bit);

    ret = SPI_Control(gSpiDev, CSK_SPI_DATA_BITS(bit), 0);
    if (ret != CSK_DRIVER_OK) {
        VIDEO_LOG("[%s:%d] (spi_no = %d) failed!!\r\n", __func__, __LINE__, ret);
    }
}


void csk_spi_write(void *pdata, uint32_t num)
{
    if(spi_dma_size > 0) {
        VIDEO_LOG("[%s:%d] SPI DMA transfer is busy!!", __func__, __LINE__);
        return;
    }

    CLR_XFER_DONE();

    if(num <= SPI_DMA_MAX_TRANCNT) {
        SPI_Send(gSpiDev, pdata, num);
    } else {
        SPI_Send(gSpiDev, pdata, SPI_DMA_MAX_TRANCNT);
        spi_dma_data = pdata + SPI_DMA_MAX_TRANCNT;
        spi_dma_size = num - SPI_DMA_MAX_TRANCNT;
    }
}


void csk_spi_write_hold(void *pdata, uint32_t num)
{
    uint32_t i = 0;
    int32_t ret;

    if(spi_dma_size > 0) {
        VIDEO_LOG("[%s:%d] SPI DMA transfer is busy!!", __func__, __LINE__);
        return;
    }

    CLR_XFER_DONE();

    for(i = 0; i < (num / SPI_DMA_MAX_TRANCNT); i++) {
        ret = SPI_Send(gSpiDev, pdata, SPI_DMA_MAX_TRANCNT);
        pdata += SPI_DMA_MAX_TRANCNT;

        if (!wait_xfer_done_timeout(1000)) { // 1000ms
            VIDEO_LOG("[%s:%d] SPI DMA transfer is timeout!! ret=%d", __func__, __LINE__, ret);
            if(gSpiDev == SPI0()) {
                csk_spi_init(0);
            } else {
                csk_spi_init(1);
            }
            return;
        }
    }

    CLR_XFER_DONE();

    if((i * SPI_DMA_MAX_TRANCNT) < num) {
        ret = SPI_Send(gSpiDev, pdata, num - (i * SPI_DMA_MAX_TRANCNT));

        if (!wait_xfer_done_timeout(1000)) { // 1000ms
            VIDEO_LOG("[%s:%d] SPI DMA transfer is timeout!! ret=%d", __func__, __LINE__, ret);
            VIDEO_LOG("[%s:%d] *%p=0x%x", __func__, __LINE__, pdata, *(uint8_t *)pdata);
            if(gSpiDev == SPI0()) {
                csk_spi_init(0);
            } else {
                csk_spi_init(1);
            }
            return;
        }
    }
}


uint32_t csk_spi_get_dma_size(void)
{
    return spi_dma_size;
}


bool csk_spi_get_dma_done(void)
{
    if (GET_XFER_DONE()) {
        CLR_XFER_DONE();
        return true;
    } else {
        return false;
    }
}


bool csk_spi_check_timeout(void)
{
    if (!wait_xfer_done_timeout(1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] SPI DMA transfer is timeout!!", __func__, __LINE__);
        return false;
    }
    return true;
}

