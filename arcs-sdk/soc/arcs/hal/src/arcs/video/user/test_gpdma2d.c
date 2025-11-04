#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "systick.h"
#include "ClockManager.h"
#include "PSRAMManager.h"
#include "Driver_GPDMA.h"
#include "Driver_DVP.h"

#include "test_case.h"
#include "check.h"
#include "csk_driver.h"


#define TEST_2D_GPDMA_CH           gp_dma_ch1
#define REG_BASE_GP_DMAC     GPDMA_BASE
/* savebin D:\src\picture\blender_back_colorbar_bgr565_96x96.bin 0x200327d0 0x4800 */


/*
 * 1280x720   640x480   320x240   128x128
 * 1. YUV422  rotate 90
 * 2. YUV444 to YUV422
 * 3. YUV444 to RGB888/BGR888/ARGB8888
 * 4. YUV422 to RGB888/BGR888/ARGB8888
 * 5. YUV422 to Y8
 * 6. RGB888/BGR888 to Y8
 * 7. YUV422/RGB888/ARGB8888 crop
 * 8. RGB888/ARGB8888 1 1/2 1/3 1/4
 * 9. RGB888/ARGB8888 crop 1 1/2 1/3 1/4
*/

static void test_gpdma_2d_rotate_regctrl(void);
static int32_t GPDMA_NormalMode_M2M_Word_Channel0_Test();
static int32_t test_gpdma_memcpy();

void test_gpdma2d(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    test_gpdma_memcpy();

    //GPDMA_NormalMode_M2M_Word_Channel0_Test();

    //CHECK_FUNC_EXIT(test_dvp_case(0x0101, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0102, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0103, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0104, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0105, false), error);   // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0201, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0202, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0301, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0302, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0303, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0304, true), error);    // pass
    //CHECK_FUNC_EXIT(test_dvp_04_01(), error);     // MCLK out                 // pass
    //CHECK_FUNC_EXIT(test_dvp_06_01(), error);     // clk enable and reset     // pass

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static void test_gpdma_2d_rotate_regctrl(void)
{
    uint32_t cnt = 0;
    static uint32_t src_buf[100] = {0};
    static volatile uint32_t dst_buf[100] = {0};

    uint8_t *image_buf = NULL;
    uint8_t *image_8x8_buf = NULL;
    uint32_t image_size_byte = 0;
    uint16_t image_width = 64;
    uint16_t image_height = 64;

    for(cnt = 0; cnt < (sizeof(src_buf) / sizeof(uint32_t)); cnt++)
    {
        src_buf[cnt] = cnt;
    }

    /* GP_DMAC 0x45100000 */
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 0x1;          // 0x45000008 bit14
    IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 0x1;          // 0x45000000 bit1
    DELAY_MS(10);

    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 5, 4, 28);       // DMA_CH1_CTRL  handsharking=5
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 2, 14);       // src  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 2, 16);       // dst  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 2, 2, 4);        // 0:p2m  1:m2p  2:mem
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 1, 9);        // src address  0:add read  1:fix read
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 1, 10);       // dst address  0:add read  1:fix read
    mmio_write32(REG_BASE_GP_DMAC + 0x30, sizeof(src_buf) / sizeof(uint32_t));      // DMA_BLOCK_LEN_CH1: word
    mmio_write32(REG_BASE_GP_DMAC + 0x64, (uint32_t)src_buf);    // DMA_SRC_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x6C, (uint32_t)dst_buf);    // DMA_DST_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x28, 0x11);                 // DMA irq enable
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 0);        // DMA channel  0:disable  1:enable
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 1);        // DMA start

    VIDEO_LOG("DMA_CH1_CTRL=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x04));
    VIDEO_LOG("DMA_SRC_ADDR0_CH1=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x64));
    VIDEO_LOG("DMA_DST_ADDR0_CH1=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x6C));

    while(1)
    {
        DELAY_MS(1000);
        VIDEO_LOG("DMA status=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x158));
        VIDEO_LOG("src_buf *0x%08x = 0x%x  0x%x  0x%x  0x%x", src_buf, src_buf[0], src_buf[1], src_buf[2], src_buf[3]);
        VIDEO_LOG("dst_buf *0x%08x = 0x%x  0x%x  0x%x  0x%x", dst_buf, dst_buf[0], dst_buf[1], dst_buf[2], dst_buf[3]);

        for(cnt = 0; cnt < (sizeof(src_buf) / sizeof(uint32_t)); cnt++) {
            src_buf[cnt]++;
        }
        mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 1);        // DMA start
    }
}



static void gpdma2d_reg_dump(void)
{
    //VIDEO_LOG("[DVP] F_HOR          *0x%08x = 0x%08x", &IP_DVP->F_HOR, IP_DVP->F_HOR);

}

static void gpdma2d_reset(void)
{
    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 0x1;
    DELAY_MS(1);
    mmio_write32(IMAGE_PROC_BASE + 0x04, 1);   // vic enable
    mmio_write32(IMAGE_PROC_BASE + 0x18, 1);   // 0:spi  1:dvp
}


/****************************************** DMA ************************************************/
static volatile uint32_t gpdma2d_gpdma_finish_flag = 0;

__attribute__((section(".itcm.text")))
static void gpdma2d_gpdma_callback(uint32_t event, void* workspace)
{
    //VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    gpdma2d_gpdma_finish_flag++;
}

static int32_t gpdma2d_gpdma_init(void)
{
    int32_t ret;

    csk_gpdma_init_t dvp_output = {
            .dma_ch = TEST_2D_GPDMA_CH,
            .burst_len = gpdma_burst_len_8spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_p2m,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = dvp_hs_num5,
    };

    ret = GPDMA_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = GPDMA_Config(&dvp_output, gpdma2d_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return CSK_DRIVER_OK;
}


static int32_t gpdma2d_gpdma_start(void* dst, uint32_t size_word)
{
    int32_t ret;

    CHECK_POINT_NOT_NULL(dst);

    ret = GPDMA_Start_Normal(TEST_2D_GPDMA_CH, (uint32_t*)VIC_BUF, dst, size_word);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}



void test_gpdma_regctrl(void)
{
    uint32_t cnt = 0;
    static uint32_t src_buf[100] = {0};
    static volatile uint32_t dst_buf[100] = {0};

    for(cnt = 0; cnt < (sizeof(src_buf) / sizeof(uint32_t)); cnt++)
    {
        src_buf[cnt] = cnt;
    }

    /* GP_DMAC 0x45100000 */
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 0x1;          // 0x45000008 bit14
    IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 0x1;          // 0x45000000 bit1
    DELAY_MS(10);

    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 5, 4, 28);       // DMA_CH1_CTRL  handsharking=5
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 2, 14);       // src  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 2, 16);       // dst  0:1words  1:2words  2:4words  3:8words
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 2, 2, 4);        // 0:p2m  1:m2p  2:mem
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 1, 9);        // src address  0:add read  1:fix read
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 0, 1, 10);       // dst address  0:add read  1:fix read
    mmio_write32(REG_BASE_GP_DMAC + 0x30, sizeof(src_buf) / sizeof(uint32_t));      // DMA_BLOCK_LEN_CH1: word
    mmio_write32(REG_BASE_GP_DMAC + 0x64, (uint32_t)src_buf);    // DMA_SRC_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x6C, (uint32_t)dst_buf);    // DMA_DST_ADDR0_CH1
    mmio_write32(REG_BASE_GP_DMAC + 0x28, 0x11);                 // DMA irq enable
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 0);        // DMA channel  0:disable  1:enable
    mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 1);        // DMA start

    VIDEO_LOG("DMA_CH1_CTRL=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x04));
    VIDEO_LOG("DMA_SRC_ADDR0_CH1=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x64));
    VIDEO_LOG("DMA_DST_ADDR0_CH1=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x6C));

    while(1)
    {
        DELAY_MS(1000);
        VIDEO_LOG("DMA status=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x158));
        VIDEO_LOG("src_buf *0x%08x = 0x%x  0x%x  0x%x  0x%x", src_buf, src_buf[0], src_buf[1], src_buf[2], src_buf[3]);
        VIDEO_LOG("dst_buf *0x%08x = 0x%x  0x%x  0x%x  0x%x", dst_buf, dst_buf[0], dst_buf[1], dst_buf[2], dst_buf[3]);

        for(cnt = 0; cnt < (sizeof(src_buf) / sizeof(uint32_t)); cnt++) {
            src_buf[cnt]++;
        }
        mmio_write32_field(REG_BASE_GP_DMAC + 0x04, 1, 1, 1);        // DMA start
    }
}



#define TEST_IMAGE_W            320
#define TEST_IMAGE_H            120
#define TEST_IMAGE_BUF_IN       0x28000000
#define TEST_IMAGE_BUF_OUT      0x28100000

static int32_t GPDMA_NormalMode_M2M_Word_Channel0_Test()
{
    uint32_t image_size_byte = 0;
    uint32_t times = 0;
    void *image_buf = NULL;
    void *out_buf = NULL;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    image_size_byte = TEST_IMAGE_W * TEST_IMAGE_H * 2;   // RGB565
    VIDEO_LOG("malloc size=0x%x byte, tiny_mem_perused=%d", image_size_byte, tiny_mem_perused());

#ifdef TEST_IMAGE_BUF_IN
    image_buf = (uint8_t *)TEST_IMAGE_BUF_IN;
#else
    image_buf = tiny_malloc(image_size_byte);
#endif
    CHECK_POINT_NOT_NULL(image_buf);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);

#ifdef TEST_IMAGE_BUF_OUT
    out_buf = (uint8_t *)TEST_IMAGE_BUF_OUT;
#else
    out_buf = tiny_malloc(image_size_byte);
#endif
    CHECK_POINT_NOT_NULL(out_buf);
    VIDEO_LOG("out_buf=0x%08x size=0x%x byte", out_buf, image_size_byte);

    rgb565_colorbar_create((uint16_t *)image_buf, TEST_IMAGE_W, TEST_IMAGE_H, TEST_IMAGE_H / 5);
    rgb565_grid_create((uint16_t *)image_buf, TEST_IMAGE_W, TEST_IMAGE_H, TEST_IMAGE_H);
    memset(out_buf, 0, image_size_byte);

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_8spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        times++;
        if(times >= 0x10000) {
            break;
        }

        image_buf = (void*)((uint32_t)image_buf + 4);
        out_buf = (void*)((uint32_t)out_buf + 4);
        VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);
        VIDEO_LOG("out_buf=0x%08x size=0x%x byte", out_buf, image_size_byte);

        GPDMA_Config(&gpdma_para, gpdma2d_gpdma_callback, NULL);

        gpdma2d_gpdma_finish_flag = 0;
        GPDMA_Start_Normal(gp_dma_ch0, (void*)image_buf, (void*)out_buf, image_size_byte / sizeof(uint32_t));

        while(!gpdma2d_gpdma_finish_flag);

        int32_t ret = 0;
        ret = memcmp(image_buf, out_buf, image_size_byte);
        if (ret != 0){
            VIDEO_LOG("transfer error");
        } else {
            VIDEO_LOG("transfer success!!!");
        }


    }

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    while(1);

#ifndef TEST_IMAGE_BUF_IN
    tiny_free(image_buf);
#endif
#ifndef TEST_IMAGE_BUF_OUT
    tiny_free(out_buf);
#endif

    return 0;
}




/*
 * PSRAM -> SRAM

chip/arcs/driver/dma/dma.c +2330
DMA_CH_CTLL_DST_INC -> DMA_CH_CTLL_DST_FIX

chip/arcs/include/PSRAMManager.h
+#define PSRAM_PREFETCH_EN                   0
+#define PSRAM_PREFETCH_FIFO0_HM_EN           0
+#define PSRAM_PREFETCH_FIFO1_HM_EN           0

    EnableDCache();
    EnableICache();
    non_cacheable_region_enable(WIFI_RAM_REGION, (CMN_PSRAM_REGION - WIFI_RAM_REGION));

    DisableDCache();
    EnableICache();
    //non_cacheable_region_enable(WIFI_RAM_REGION, (CMN_PSRAM_REGION - WIFI_RAM_REGION));
 */
#include "nos_timer.h"
#include "dma.h"
#include "Driver_GPDMA.h"

// PSRAM  ->  SRAM
#define PSRAM_DMA_ADDR 0x28000000
#define TOTAL_SIZE_BYTE (64*1024)

static uint8_t sram_buf[64*1024] = {0};  // DMA_CH_CTLL_SRC_FIX

static int32_t DMA_memcpy_start(uint32_t src_addr, uint32_t dst_addr, uint32_t total_bytes, DMA_CACHE_SYNC cache_sync);
static void GPDMA_memcpy_init(void);
static int32_t GPDMA_memcpy_start(uint32_t src_addr, uint32_t dst_addr, uint32_t total_bytes);

// Event flag
volatile static uint32_t cpdma_finish_flag = 0;

__attribute__((section(".itcm.text")))
static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    //VIDEO_LOG("[%s]: event = %d, channel = %d, xfer_bytes = %d\r\n", __func__, event_info & 0xFF, (event_info >> 8) & 0xFF, xfer_bytes);
    if(event_info & DMA_EVENT_TRANSFER_COMPLETE){
        cpdma_finish_flag = 1;
    }
}


#define CYCLE_TO_US(_cycle)                         ((_cycle) / 3 / 100)   // 300MHz
//#define CYCLE_TO_US(_cycle)                         ((_cycle) / 3 / 25)   // 75MHz
#define CYCLE_TO_BANDWIDTH(_cycle, _total_bytes)    ((_total_bytes) / 1024 * 1000000 / 1024 / (CYCLE_TO_US(_cycle)))

void test_gpdma_cpdma(void)
{
    int32_t ret = FAILURE;
    uint32_t src_addr = 0;
    uint32_t dst_addr = 0;
    uint32_t total_bytes = TOTAL_SIZE_BYTE;

    uint32_t cycle_start = 0;
    uint32_t cycle_end = 0;
    uint32_t cycle_cpdma_end = 0;
    uint32_t cycle_gpdma_end = 0;
    uint32_t memcpy_cnt = 0;
    uint32_t memcpy_addr_offset = 0;

    src_addr = PSRAM_DMA_ADDR;
    dst_addr = (uint32_t)(&sram_buf[0]);  // DMA_CH_CTLL_SRC_FIX

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    dma_initialize();

    GPDMA_memcpy_init();

    while(1)
    {
        VIDEO_LOG("\r\nCheck memcpy: src = 0x%08X, dst = 0x%08X, size = 0x%08X", src_addr, dst_addr, total_bytes);

        /************* CPDMA and GPDMA and CPU ******************/
        VIDEO_LOG("\r\n[%s:%d] CPDMA and GPDMA and CPU", __func__, __LINE__);
        memcpy_cnt = 0;
        ret = DMA_memcpy_start(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_AUTO);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        //cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        ret = GPDMA_memcpy_start(src_addr + 0x100000, dst_addr, total_bytes);
        CHECK_RET_EQ_EXIT(ret, 0, error);
        //cycle_gpdma_end = __RV_CSR_READ(CSR_MCYCLE);
        //VIDEO_LOG("GPDMA cfg=%d =%dus", cycle_gpdma_end - cycle_start, (cycle_gpdma_end - cycle_start) / 3 / 100);  // cfg=430 =1us

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!cpdma_finish_flag){
            memcpy_addr_offset = 1024 * memcpy_cnt;
            if((memcpy_addr_offset + 1024) > sizeof(sram_buf)) {
                memcpy_addr_offset = 0;
            }
            memcpy((void*)(src_addr + memcpy_addr_offset), (void*)(dst_addr + memcpy_addr_offset), 1024);
            memcpy_cnt++;
        }
        cycle_cpdma_end = __RV_CSR_READ(CSR_MCYCLE);
        while(!gpdma2d_gpdma_finish_flag){
            memcpy_addr_offset = 1024 * memcpy_cnt;
            if((memcpy_addr_offset + 1024) > sizeof(sram_buf)) {
                memcpy_addr_offset = 0;
            }
            memcpy((void*)(src_addr + memcpy_addr_offset), (void*)(dst_addr + memcpy_addr_offset), 1024);
            memcpy_cnt++;
        }
        cycle_gpdma_end = __RV_CSR_READ(CSR_MCYCLE);

        VIDEO_LOG("CPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_cpdma_end - cycle_start, CYCLE_TO_US(cycle_cpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("GPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_gpdma_end - cycle_start, CYCLE_TO_US(cycle_gpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("CPU:   cycle=%d =%dus@%dByte =%dMB/s", cycle_gpdma_end - cycle_start, CYCLE_TO_US(cycle_gpdma_end - cycle_start), memcpy_cnt * 1024, CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, memcpy_cnt * 1024));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes) + CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes) + CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, memcpy_cnt * 1024));

    }

    while(0)
    {
        VIDEO_LOG("\r\nCheck memcpy: src = 0x%08X, dst = 0x%08X, size = 0x%08X", src_addr, dst_addr, total_bytes);

        /************* CPDMA only ******************/
        VIDEO_LOG("\r\n[%s:%d] CPDMA only", __func__, __LINE__);
        ret = DMA_memcpy_start(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_AUTO);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!cpdma_finish_flag);
        cycle_end = __RV_CSR_READ(CSR_MCYCLE);
        VIDEO_LOG("CPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes));


        /************* GPDMA only ******************/
        VIDEO_LOG("\r\n[%s:%d] GPDMA only", __func__, __LINE__);
        ret = GPDMA_memcpy_start(src_addr, dst_addr, total_bytes);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!gpdma2d_gpdma_finish_flag);
        cycle_end = __RV_CSR_READ(CSR_MCYCLE);
        VIDEO_LOG("GPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes));


        /************* CPU only ******************/
        VIDEO_LOG("\r\n[%s:%d] CPU only", __func__, __LINE__);
        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        memcpy((void*)src_addr, (void*)dst_addr, sizeof(sram_buf));
        cycle_end = __RV_CSR_READ(CSR_MCYCLE);
        VIDEO_LOG("CPU: cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), sizeof(sram_buf), CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, sizeof(sram_buf)));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, sizeof(sram_buf)));


        /************* CPDMA and CPU ******************/
        VIDEO_LOG("\r\n[%s:%d] CPDMA and CPU", __func__, __LINE__);
        memcpy_cnt = 0;
        ret = DMA_memcpy_start(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_AUTO);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!cpdma_finish_flag){
            memcpy_addr_offset = 1024 * memcpy_cnt;
            if((memcpy_addr_offset + 1024) > sizeof(sram_buf)) {
                memcpy_addr_offset = 0;
            }
            memcpy((void*)(src_addr + memcpy_addr_offset), (void*)(dst_addr + memcpy_addr_offset), 1024);
            memcpy_cnt++;
        }
        cycle_end = __RV_CSR_READ(CSR_MCYCLE);
        VIDEO_LOG("CPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes));
        VIDEO_LOG("CPU:   cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), memcpy_cnt * 1024, CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, memcpy_cnt * 1024));
        //VIDEO_LOG("CPU:   cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), memcpy_cnt * 1024, ((memcpy_cnt * 1024) / 1024 * 1000000 / 1024 / (CYCLE_TO_US(cycle_end - cycle_start))));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes + memcpy_cnt * 1024));


        /************* GPDMA and CPU ******************/
        VIDEO_LOG("\r\n[%s:%d] GPDMA and CPU", __func__, __LINE__);
        memcpy_cnt = 0;
        ret = GPDMA_memcpy_start(src_addr, dst_addr, total_bytes);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!gpdma2d_gpdma_finish_flag){
            memcpy_addr_offset = 1024 * memcpy_cnt;
            if((memcpy_addr_offset + 1024) > sizeof(sram_buf)) {
                memcpy_addr_offset = 0;
            }
            memcpy((void*)(src_addr + memcpy_addr_offset), (void*)(dst_addr + memcpy_addr_offset), 1024);
            memcpy_cnt++;
        }
        cycle_end = __RV_CSR_READ(CSR_MCYCLE);
        VIDEO_LOG("GPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes));
        VIDEO_LOG("CPU:   cycle=%d =%dus@%dByte =%dMB/s", cycle_end - cycle_start, CYCLE_TO_US(cycle_end - cycle_start), memcpy_cnt * 1024, CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, memcpy_cnt * 1024));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_end - cycle_start, total_bytes + memcpy_cnt * 1024));


        /************* CPDMA and GPDMA ******************/
        VIDEO_LOG("\r\n[%s:%d] CPDMA and GPDMA", __func__, __LINE__);
        ret = DMA_memcpy_start(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_AUTO);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        //cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        ret = GPDMA_memcpy_start(src_addr + 0x100000, dst_addr, total_bytes);
        CHECK_RET_EQ_EXIT(ret, 0, error);
        //cycle_gpdma_end = __RV_CSR_READ(CSR_MCYCLE);
        //VIDEO_LOG("GPDMA cfg=%d =%dus", cycle_gpdma_end - cycle_start, (cycle_gpdma_end - cycle_start) / 3 / 100);  // cfg=430 =1us

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!cpdma_finish_flag);
        cycle_cpdma_end = __RV_CSR_READ(CSR_MCYCLE);
        while(!gpdma2d_gpdma_finish_flag);
        cycle_gpdma_end = __RV_CSR_READ(CSR_MCYCLE);

        VIDEO_LOG("CPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_cpdma_end - cycle_start, CYCLE_TO_US(cycle_cpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("GPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_gpdma_end - cycle_start, CYCLE_TO_US(cycle_gpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes) + CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes));


        /************* GPDMA and CPDMA ******************/
        VIDEO_LOG("\r\n[%s:%d] GPDMA and CPDMA", __func__, __LINE__);
        ret = DMA_memcpy_start(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_AUTO);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        ret = GPDMA_memcpy_start(src_addr + 0x100000, dst_addr, total_bytes);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!gpdma2d_gpdma_finish_flag);
        cycle_gpdma_end = __RV_CSR_READ(CSR_MCYCLE);
        while(!cpdma_finish_flag);
        cycle_cpdma_end = __RV_CSR_READ(CSR_MCYCLE);

        VIDEO_LOG("CPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_cpdma_end - cycle_start, CYCLE_TO_US(cycle_cpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("GPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_gpdma_end - cycle_start, CYCLE_TO_US(cycle_gpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes) + CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes));


        /************* CPDMA and GPDMA and CPU ******************/
        VIDEO_LOG("\r\n[%s:%d] CPDMA and GPDMA and CPU", __func__, __LINE__);
        memcpy_cnt = 0;
        ret = DMA_memcpy_start(src_addr, dst_addr, total_bytes, DMA_CACHE_SYNC_AUTO);
        CHECK_RET_EQ_EXIT(ret, 0, error);

        //cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        ret = GPDMA_memcpy_start(src_addr + 0x100000, dst_addr, total_bytes);
        CHECK_RET_EQ_EXIT(ret, 0, error);
        //cycle_gpdma_end = __RV_CSR_READ(CSR_MCYCLE);
        //VIDEO_LOG("GPDMA cfg=%d =%dus", cycle_gpdma_end - cycle_start, (cycle_gpdma_end - cycle_start) / 3 / 100);  // cfg=430 =1us

        cycle_start = __RV_CSR_READ(CSR_MCYCLE);
        while(!cpdma_finish_flag){
            memcpy_addr_offset = 1024 * memcpy_cnt;
            if((memcpy_addr_offset + 1024) > sizeof(sram_buf)) {
                memcpy_addr_offset = 0;
            }
            memcpy((void*)(src_addr + memcpy_addr_offset), (void*)(dst_addr + memcpy_addr_offset), 1024);
            memcpy_cnt++;
        }
        cycle_cpdma_end = __RV_CSR_READ(CSR_MCYCLE);
        while(!gpdma2d_gpdma_finish_flag){
            memcpy_addr_offset = 1024 * memcpy_cnt;
            if((memcpy_addr_offset + 1024) > sizeof(sram_buf)) {
                memcpy_addr_offset = 0;
            }
            memcpy((void*)(src_addr + memcpy_addr_offset), (void*)(dst_addr + memcpy_addr_offset), 1024);
            memcpy_cnt++;
        }
        cycle_gpdma_end = __RV_CSR_READ(CSR_MCYCLE);

        VIDEO_LOG("CPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_cpdma_end - cycle_start, CYCLE_TO_US(cycle_cpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("GPDMA: cycle=%d =%dus@%dByte =%dMB/s", cycle_gpdma_end - cycle_start, CYCLE_TO_US(cycle_gpdma_end - cycle_start), total_bytes, CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes));
        VIDEO_LOG("CPU:   cycle=%d =%dus@%dByte =%dMB/s", cycle_gpdma_end - cycle_start, CYCLE_TO_US(cycle_gpdma_end - cycle_start), memcpy_cnt * 1024, CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, memcpy_cnt * 1024));
        VIDEO_LOG("PSRAM: %dMB/s", CYCLE_TO_BANDWIDTH(cycle_cpdma_end - cycle_start, total_bytes) + CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, total_bytes) + CYCLE_TO_BANDWIDTH(cycle_gpdma_end - cycle_start, memcpy_cnt * 1024));

    }


    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


// DMA memcpy check for cache sync/coherence, different src/dst address start.
// return the result of memory comparison: true = same, false = different or failed to start DMA.
static int32_t DMA_memcpy_start(uint32_t src_addr, uint32_t dst_addr, uint32_t total_bytes, DMA_CACHE_SYNC cache_sync)
{
    uint8_t ch = 0;

    if (!dma_channel_is_reserved(ch)) {
        ch = dma_channel_reserve(ch, DMA_DrvEvent, 0, cache_sync);
    }

    if (ch == DMA_CHANNEL_ANY) {
        ch = dma_channel_select(&ch, DMA_DrvEvent, 0, cache_sync);
    }
    if (ch == DMA_CHANNEL_ANY) {
        VIDEO_LOG("[FAILED] NO free DMA channel!!");
        return -1;
    }

    cpdma_finish_flag = 0;
    dma_memcpy (ch, src_addr, dst_addr, total_bytes);

    return 0;
}



static void GPDMA_memcpy_init(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    csk_gpdma_init_t gpdma_para = {
            .dma_ch = gp_dma_ch0,
            .burst_len = gpdma_burst_len_8spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2m,
            .sample_unit = gpdma_sample_unit_word,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_increase,
            .prio_lvl = prio_mode_vhigh,
            .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma2d_gpdma_callback, NULL);
}


static int32_t GPDMA_memcpy_start(uint32_t src_addr, uint32_t dst_addr, uint32_t total_bytes)
{
    gpdma2d_gpdma_finish_flag = 0;
    GPDMA_Start_Normal(gp_dma_ch0, (void*)src_addr, (void*)dst_addr, total_bytes / sizeof(uint32_t));

    return 0;
}


#include "cache.h"

static int32_t test_gpdma_memcpy(void)
{

    int32_t ret = FAILURE;
    uint32_t src_addr = 0;
    uint32_t dst_addr = 0;
    uint32_t total_bytes = 0;

    src_addr = 0x28000000;
    dst_addr = 0x28100000;
    total_bytes = 320*240*2;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    rgb565_color_fill((uint16_t *)src_addr, RGB565_YELLOW, 320, 240);
    rgb565_color_fill((uint16_t *)dst_addr, RGB565_BLUE, 320, 240);
    HAL_FlushDCache();

    GPDMA_memcpy_init();

    do {
        VIDEO_LOG("Check memcpy: src=0x%08X, dst=0x%08X, size=0x%08X", src_addr, dst_addr, total_bytes);
        ret = GPDMA_memcpy_start(src_addr, dst_addr, total_bytes);
        CHECK_RET_EQ_EXIT(ret, 0, error);
        while(!gpdma2d_gpdma_finish_flag);
    } while(0);

    VIDEO_LOG("[%s:%d] end", __func__, __LINE__);

    ret = memcmp((void *)src_addr, (void *)dst_addr, total_bytes);
    if (ret != 0){
        VIDEO_LOG("[%s:%d] failed ret=%d", __func__, __LINE__, ret);
    } else {
        VIDEO_LOG("[%s:%d] success ret=%d", __func__, __LINE__, ret);
    }

    while(1);

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return ret;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return ret;
}
