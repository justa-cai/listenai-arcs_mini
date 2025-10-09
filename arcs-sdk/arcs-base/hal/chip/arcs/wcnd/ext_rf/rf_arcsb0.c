//#include "rwnx_config.h"
//#include "phy.h"
#include "IOMuxManager.h"
#include "Driver_SPI.h"
#include "Driver_GPIO.h"
#include "arcs_ap.h"
#include <stdio.h>
//#include "dbg.h"

static volatile uint32_t g_run_flag = 0;
static inline void INIT_XFER_FLAG() { g_run_flag = 0; }
static inline void SET_XFER_DONE() { g_run_flag |= 1; }
static inline void CLR_XFER_DONE() { g_run_flag &= ~0x1UL; }
static inline bool GET_XFER_DONE() { return (g_run_flag & 1); }

// SPI event
static void SPI_DrvEvent (uint32_t event, uint32_t usr_param)
{
    if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE)
    {
        SET_XFER_DONE();
    }
}

static bool wait_xfer_done(void)
{
    uint32_t n;

    while (1)
    {
        n = GET_XFER_DONE();
        if (n) break;
    }

    CLR_XFER_DONE();
    return true;
}

#define AD_WRITE (0 << 15)
#define AD_READ (1 << 15)
#define AD_BYTE_NUM_1 (0x0)
#define AD_BYTE_NUM_2 (0x2000)
#define AD_BYTE_NUM_3 (0x4000)
#define AD_BUS_SPEED 2000000

#define DA_WRITE (0 << 7)
#define DA_READ (1 << 7)
#define DA_BYTE_NUM_1 (0x0)
#define DA_BYTE_NUM_2 (0x2000)
#define DA_BYTE_NUM_3 (0x4000)
#define DA_BUS_SPEED 2000000

void arcs_b0_rf_board_adda_init(void)
{
    uint32_t control;
    int32_t ret;
    uint8_t buf[3];
    uint16_t cmd;
    uint8_t spi_rcv[3];

    void* spi0_dev = SPI0();
    void* spi1_dev = SPI1();
    void* spi2_dev = SPI2();

    //bit11(TX) - 1:80M  0:160M  bit
    //bit8(BBDAC clock edge) 1:nega sample
    //bit9(RSSIADC clock edge) 1:nega sample
    //bit10(BBADC clock edge) 1:nega sample
    *(uint32_t *)0x46000030 = 0x10;//0x910;
//    // SPI2 config RSSIADC(ADC9468)
//    // SPI2_CLK-GPIO25 SPI2_CS-GPIO22 SPI2_MOSI-GPIO26
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, CSK_IOMUX_FUNC_ALTER7);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, CSK_IOMUX_FUNC_ALTER7);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER7);

    control = CSK_SPI_MODE_MASTER
              | CSK_SPI_TXIO_PIO
              | CSK_SPI_RXIO_PIO
              | (CSK_SPI_CPOL0_CPHA0 & CSK_SPI_FRAME_FORMAT_Msk)
              | CSK_SPI_DATA_BITS(8)
              | CSK_SPI_MSB_LSB;

    SPI_Initialize(spi2_dev, SPI_DrvEvent, (uint32_t)spi2_dev);

    ret = SPI_PowerControl(spi2_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK)
    {
        //dbg(D_INF "%s:%d\n", __func__, __LINE__);
        return;
    }

    // call SPI_Control, and arg = bus_speed when as master
    ret = SPI_Control(spi2_dev, control, AD_BUS_SPEED);
    if (ret != CSK_DRIVER_OK)
    {
        //dbg(D_INF "%s:%d\n", __func__, __LINE__);
        return;
    }

    //digital reset
    //0x08: 0x03
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x8;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x3;
    SPI_Send(spi2_dev, buf, 3);
    wait_xfer_done();

    //0x08: 0x00
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x8;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x0;
    SPI_Send(spi2_dev, buf, 3);
    wait_xfer_done();

    //0x18: 0x0
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x18;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x0;
    SPI_Send(spi2_dev, buf, 3);
    wait_xfer_done();

    //0x15: 0x3
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x15;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x3;
    SPI_Send(spi2_dev, buf, 3);
    wait_xfer_done();

    //0x17: 0x0
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x17;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x0;
    SPI_Send(spi2_dev, buf, 3);
    wait_xfer_done();

    // SPI1 config BBADC(ADC9468)
    // SPI1_CLK-GPIO17 SPI1_CS-GPIO19 SPI1_MOSI-GPIO24
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, CSK_IOMUX_FUNC_ALTER6);

    control = CSK_SPI_MODE_MASTER
              | CSK_SPI_TXIO_PIO
              | CSK_SPI_RXIO_PIO
              | (CSK_SPI_CPOL0_CPHA0 & CSK_SPI_FRAME_FORMAT_Msk)
              | CSK_SPI_DATA_BITS(8)
              | CSK_SPI_MSB_LSB;

    SPI_Initialize(spi1_dev, SPI_DrvEvent, (uint32_t)spi1_dev);

    ret = SPI_PowerControl(spi1_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK)
    {
        //dbg(D_INF "%s:%d\n", __func__, __LINE__);
        return;
    }

    // call SPI_Control, and arg = bus_speed when as master
    ret = SPI_Control(spi1_dev, control, AD_BUS_SPEED);
    if (ret != CSK_DRIVER_OK)
    {
        //dbg(D_INF "%s:%d\n", __func__, __LINE__);
        return;
    }

    //digital reset
    //0x08: 0x03
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x8;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x3;
    SPI_Send(spi1_dev, buf, 3);
    wait_xfer_done();

    //0x08: 0x00
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x8;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x0;
    SPI_Send(spi1_dev, buf, 3);
    wait_xfer_done();

    //0x18: 0x0
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x18;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x0;
    SPI_Send(spi1_dev, buf, 3);
    wait_xfer_done();

    //0x15: 0x3
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x15;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x3;
    SPI_Send(spi1_dev, buf, 3);
    wait_xfer_done();

    //0x17: 0x0
    cmd = AD_WRITE | AD_BYTE_NUM_1 | 0x17;
    buf[0] = cmd >> 8;
    buf[1] = cmd & 0xFF;
    buf[2] = 0x0;
    SPI_Send(spi1_dev, buf, 3);
    wait_xfer_done();

    // SPI0 config DAC(DAC9777)
    // SPI0_CLK-GPIO15 SPI0_CS-GPIO18 SPI0_MOSI-GPIO14 SPI0_MISO-GPIO13
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER5);

    control = CSK_SPI_MODE_MASTER
              | CSK_SPI_TXIO_PIO
              | CSK_SPI_RXIO_PIO
              | (CSK_SPI_CPOL0_CPHA0 & CSK_SPI_FRAME_FORMAT_Msk)
              | CSK_SPI_DATA_BITS(8)
              | CSK_SPI_MSB_LSB;

    SPI_Initialize(spi0_dev, SPI_DrvEvent, (uint32_t)spi0_dev);

    ret = SPI_PowerControl(spi0_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK)
    {
        //dbg(D_INF "%s:%d\n", __func__, __LINE__);
        return;
    }

    // call SPI_Control, and arg = bus_speed when as master
    ret = SPI_Control(spi0_dev, control, DA_BUS_SPEED);
    if (ret != CSK_DRIVER_OK)
    {
        //dbg(D_INF "%s:%d\n", __func__, __LINE__);
        return;
    }

#if 0
    //read 0x0 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x0;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x0: 0x%x\n", spi_rcv[1]);

    //read 0x1 : 0x4
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x1;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x1: 0x%x\n", spi_rcv[1]);

    //read 0x2 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x2;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x2: 0x%x\n", spi_rcv[1]);

    //read 0x3 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x3;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x3: 0x%x\n", spi_rcv[1]);

    //read 0x4 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x4;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x4: 0x%x\n", spi_rcv[1]);

    //read 0x5 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x5;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x5: 0x%x\n", spi_rcv[1]);

    //read 0x6 : 0xf
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x6;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x6: 0x%x\n", spi_rcv[1]);

    //read 0x7 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x7;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x7: 0x%x\n", spi_rcv[1]);

    //read 0x8 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x8;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x8: 0x%x\n", spi_rcv[1]);

    //read 0x9 : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0x9;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0x9: 0x%x\n", spi_rcv[1]);

    //read 0xa : 0xf
    cmd = DA_READ | DA_BYTE_NUM_1 | 0xa;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0xa: 0x%x\n", spi_rcv[1]);

    //read 0xb : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0xb;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0xb: 0x%x\n", spi_rcv[1]);

    //read 0xc : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0xc;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0xc: 0x%x\n", spi_rcv[1]);

    //read 0xd : 0x0
    cmd = DA_READ | DA_BYTE_NUM_1 | 0xd;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xff;
    memset(spi_rcv, 0, 3);
    SPI_Transfer(spi0_dev, buf, spi_rcv, 2);
    wait_xfer_done();
    //printf("spi 0xd: 0x%x\n", spi_rcv[1]);
#endif

#if 1
    //0x1: 0x5
    cmd = DA_WRITE | DA_BYTE_NUM_1 | 0x1;
    buf[0] = cmd & 0xFF;
    buf[1] = 0x5;
    SPI_Send(spi0_dev, buf, 2);
    wait_xfer_done();

    //0x2: 0xa0
    cmd = DA_WRITE | DA_BYTE_NUM_1 | 0x2;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xa0;//0xa0;
    SPI_Send(spi0_dev, buf, 2);
    wait_xfer_done();

    //0x5: 0x0
    cmd = DA_WRITE | DA_BYTE_NUM_1 | 0x5;
    buf[0] = cmd & 0xFF;
    buf[1] = 0x0;
    SPI_Send(spi0_dev, buf, 2);
    wait_xfer_done();

    //0x6: 0x2
    cmd = DA_WRITE | DA_BYTE_NUM_1 | 0x6;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xf;
    SPI_Send(spi0_dev, buf, 2);
    wait_xfer_done();

    //0xa: 0x2
    cmd = DA_WRITE | DA_BYTE_NUM_1 | 0xa;
    buf[0] = cmd & 0xFF;
    buf[1] = 0xf;
    SPI_Send(spi0_dev, buf, 2);
    wait_xfer_done();

    //0x0: 0x10   // shuts down
    cmd = DA_WRITE | DA_BYTE_NUM_1 | 0x0;
    buf[0] = cmd & 0xFF;
    buf[1] = 0x00;
    SPI_Send(spi0_dev, buf, 2);
    wait_xfer_done();
#endif

    printf("RF board ADDA init done\n");
}

void arcs_b0_rf_board_hop_init()
{
	// channel data transmit time keep 5us
	IP_BT_CTRL->REG_BT_CTRL_RSVD_RW_REG.all = 24 * 5;
}

void arcs_b0_rf_driver()
{
    arcs_b0_rf_board_adda_init();
    arcs_b0_rf_board_hop_init();
}

