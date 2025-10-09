#include <stdbool.h>
#include <stdint.h>
#include "Driver_GPDMA.h"
#include "Driver_GPIO.h"
#include "Driver_DVP.h"
#include "xutils.h"
// #include "log_print.h"
#include "Driver_QSPI_SENSOR_IN.h"
#include "spi_xfer.h"
#include "systick.h"

#define SPI_VIC_IN_GPDMA_CH         gp_dma_ch0


static volatile uint32_t gspi_drv_event = 0;
static volatile uint32_t gspi_camera_cs_event = 0;
static volatile uint32_t gvicspi_frame_event = 0;

static void _spi_vic_gpdma_callback(uint32_t event, void *workspace)
{
    // CLOGI("[%s:%d] event=%d\r\n", __func__, __LINE__, event);
    if (event != CSK_GPDMA_EVENT_TRANSFER_DONE) {
        LOGI("[%s:%d] event=%d\r\n", __func__, __LINE__, event);
    }

    // CLOGI("count: %d %d\r\n", GPDMA_GetCnt(SPI_VIC_IN_GPDMA_CH, &cnt), cnt);
    gspi_drv_event |= event;
}

int32_t _spi_vic_gdma_init(csk_gpdma_ch_t gpdma_ch)
{
    int32_t ret;

    csk_gpdma_init_t gpdma_cfg = {
        .dma_ch = gpdma_ch,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_byte,
        .handshake = dvp_hs_num5,
    };

    ret = GPDMA_Initialize();
    if (ret != CSK_DRIVER_OK) {
        LOGE("GPDMA_Initialize() fail %d\n", ret);
        return ret;
    }
    SysTick_Delay_Ms(10);

    ret = GPDMA_Config(&gpdma_cfg, _spi_vic_gpdma_callback, NULL);
    if (ret != CSK_DRIVER_OK) {
        LOGE("GPDMA_Config() fail %d\n", ret);
        return ret;
    }
    
    return ret;
}

static void _spi_vic_callback(em_QSPI_SENSOR_IN_IrqEvent event, uint32_t param)
{
    LOGI("QSPI_IN event: %d\n", event);
    gvicspi_frame_event |= event;
    switch (event) {
    case QSPI_SENSOR_IN_IRQ_EVENT_FRAME_START:
        LOGI("QSPI_IN FRAME START\r\n");
        break;
    
    case QSPI_SENSOR_IN_IRQ_EVENT_FRAME_END:
        LOGI("QSPI_IN FRAME END\r\n");
        break;
    
    case QSPI_SENSOR_IN_IRQ_EVENT_RXFIFOOR:
        break;
    default:
        LOGI("[%s:%d] QSPI_IN error event: %d\r\n", __func__, __LINE__, event);
        break;
    }
}

static void _gpio_drv_event(uint32_t event, void *usr_param)
{
    // CLOGI("spi camera cs pin interrupt: %x %x\n", event, GPIO_PinRead(GPIOA(), (1 << SPI_CAMERA_CS_PIN)));
    gspi_camera_cs_event |= event;
}


int spi_vic_init(void)
{
    LOGI("%s\r\n", __func__);

    void *spi_vic_dev = QSPI_SENSOR_IN0();
    int ret = 0;

    QSPI_SENSOR_IN_InitTypeDef qspi_in_cfg = {
        .mode = QSPI_SENSOR_IN_SYNC_MODE_ALL,
        .lane_num = 1,
        .cp = QSPI_SENSOR_IN_CPOL0_CPOH0,
        .is_lsb = true,
        .data_merge = false,
        .wire_order = true,
        .sync_code = {
            .sync_code = 0xFF0000,
            .sof = 0xab,
            .eof = 0xb6,
            .sol = 0x80,
            .eol = 0x9d,
        }
    };

    _spi_vic_gdma_init(SPI_VIC_IN_GPDMA_CH);

    ret = QSPI_SENSOR_IN_Initialize(spi_vic_dev, _spi_vic_callback, &qspi_in_cfg);
    if (ret != CSK_DRIVER_OK) {
        LOGE("QSPI_SENSOR_IN_Initialize() fail %d\n", ret);
    }

    ret = QSPI_SENSOR_IN_Start(spi_vic_dev);
    if (ret != CSK_DRIVER_OK) {
        LOGE("QSPI_SENSOR_IN_Start() fail %d\n", ret);
    }

    DVP_EnableClockout(DVP0(), 8*1000*1000);
#if SPI_XFER_NOCS
    GPIO_Initialize(GPIOA(), _gpio_drv_event, NULL);
    GPIO_SetDir(GPIOA(), 1 << SPI_XFER_CS_PIN, CSK_GPIO_DIR_INPUT);
    GPIO_Control(GPIOA(), CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_POSITIVE_EDGE |
                                                CSK_GPIO_INTR_ENABLE, (1UL << SPI_XFER_CS_PIN));
    
    GPIO_SetDir(GPIOA(), 1 << SPI_XFER_DEBUG_PIN, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOA(), 1 << SPI_XFER_DEBUG_PIN, 1);
#endif
    return 0;
}

int spi_vic_recv(uint8_t *rxbuf, uint32_t len)
{
    int32_t ret;

    gspi_camera_cs_event = 0;
    while (!(gspi_camera_cs_event & (1 << SPI_XFER_CS_PIN)));

    gspi_drv_event = 0;
    GPIO_PinWrite(GPIOA(), 1 << SPI_XFER_DEBUG_PIN, 0);
    // ret = GPDMA_Start_PiPo(SPI_VIC_IN_GPDMA_CH, (uint32_t*)QSPI_SENSOR_IN0_Buf(), (uint32_t*)QSPI_SENSOR_IN0_Buf(), rxbuf, rxbuf, len);
    ret = GPDMA_Start_Normal(SPI_VIC_IN_GPDMA_CH, (uint32_t *)QSPI_SENSOR_IN0_Buf(), rxbuf, len);

    while (!(gspi_drv_event & CSK_GPDMA_EVENT_TRANSFER_DONE));
    GPIO_PinWrite(GPIOA(), 1 << SPI_XFER_DEBUG_PIN, 1);
    LOGI("spi_vic_recv done...\r\n");

    return ret;
}

int spi_vic_pp_recv(uint8_t *ping_buf, uint8_t *pong_buf, uint32_t len, spi_recv_pp_cb pp_cb)
{

    return 0;
}

int spi_vic_deinit(void)
{
    int32_t ret = 0;
    void *spi_vic_dev = QSPI_SENSOR_IN0();

    ret = QSPI_SENSOR_IN_Stop(spi_vic_dev);
    if (ret != CSK_DRIVER_OK) {
        LOGE("QSPI_SENSOR_IN_Stop() fail %d\n", ret);
    }

    ret = GPDMA_Stop(SPI_VIC_IN_GPDMA_CH);
    if (ret != CSK_DRIVER_OK) {
        LOGE("GPDMA_Stop() fail %d\n", ret);
    }

    return ret;
}

static struct cam_xfer_ops spi_vic_xfer_ops = {
    .spi_xfer_init = spi_vic_init,
    .spi_xfer_recv = spi_vic_recv,
    .spi_xfer_deinit = spi_vic_deinit,
};

static struct cam_xfer spi_vic_xfer = {
    .ops = &spi_vic_xfer_ops,
    .priv = NULL,
};

struct cam_xfer *spi_xfer_get(void)
{
    return &spi_vic_xfer;
}