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
#include "cache.h"

#include "test_case.h"
#include "dvp_case_config.h"
#include "csk_driver.h"
#include "config.h"
#include "camera.h"
#include "csk_clk_reset.h"


static int32_t test_dvp_pin(void);
static int32_t test_dvp_mclk_out(void);
static int32_t test_dvp_reset(void);
static int32_t test_dvp_case(uint16_t id, bool colorbar);
/* savebin D:\src\picture\blender_back_colorbar_bgr565_96x96.bin 0x200327d0 0x4800 */


/*
    image_proc_reg  0x4500_0000
    vic_reg         0x4500_0800
    vic_buf         0x4500_1000
    GPDAMC          0x4590_0000

| Sub-Addr | Register Name  |
| -------- | -------------- |
| 0x00     | F_HOR          |
| 0x04     | F_VER          |
| 0x08     | P_OFFSET       |
| 0x0C     | L_OFFSET       |
| 0x10     | CLK_OUTEN      |
| 0x14     | POL_CNTL       |
| 0x18     | JLB_HANSHK_SEL |
| 0x1C     | INPUT_FORM     |
| 0x20     | VI_EN          |
| 0x24     | DMA_BURST_THD  |
| 0x28     | INTR_MSK       |
| 0x2C     | INTR_CLR       |
| 0x30     | VIC_IRQ        |
| 0x34     | VIC_INT_STATUS |
| 0x38     | VIC_INT_RAW    |

TEST_DVP_01_01  camera: YUV422 1280x720     DVP: YUV422 1280x720
TEST_DVP_01_02  camera: YUV422  640x480     DVP: YUV422  640x480
TEST_DVP_01_03  camera: YUV422  320x240     DVP: YUV422  320x240
TEST_DVP_01_04  camera: YUV422  320x240     DVP: RAW8    640x240
TEST_DVP_01_05  camera: RGB565  320x240     DVP: RAW8    640x240
TEST_DVP_02_01  camera: YUV422  320x240     DVP: YUV422  160x120    start(80,60)
TEST_DVP_02_02  camera: YUV422  320x240     DVP: RAW8    320x120    start(160,60)
TEST_DVP_03_01  camera: YUV422  320x240     DVP: YUV422  320x240    PCKPolarity   GC0308 reg0x26 bit2
TEST_DVP_03_02  camera: YUV422  320x240     DVP: YUV422  320x240    VSPolarity    GC0308 reg0x26 bit0
TEST_DVP_03_03  camera: YUV422  320x240     DVP: YUV422  320x240    HSPolarity    GC0308 reg0x26 bit1
TEST_DVP_03_04  camera: YUV422  320x240     DVP: YUV422  320x240    DataAlign

 running -> standby -> resume
*/

void test_dvp(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    //CHECK_FUNC_EXIT(test_dvp_case(0x0101, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0102, true), error);    //
    CHECK_FUNC_EXIT(test_dvp_case(0x0103, false), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0104, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0105, false), error);   //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0201, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0202, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0301, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0302, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0303, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0304, true), error);    //
    //CHECK_FUNC_EXIT(test_dvp_mclk_out(), error);             // MCLK out  //  pass
    //CHECK_FUNC_EXIT(test_dvp_case(0x0501, false), error);    // 720x96  //
    //CHECK_FUNC_EXIT(test_dvp_case(0x0502, true), error);    // 304x232  //
    //CHECK_FUNC_EXIT(test_dvp_reset(), error);     // clk enable and reset     // pass
    //test_dvp_pin();

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


static uint8_t dvp_get_format_byte(DVP_emInputFormat format)
{
    switch(format)
    {
        case DVP_INPUT_FORM_LUMINA_8BIT:
            return 1;

        case DVP_INPUT_FORM_YUV422_Y0CBY1CR:
        case DVP_INPUT_FORM_YUV422_CBY0CRY1:
        case DVP_INPUT_FORM_YUV422_Y0CRY1CB:
        case DVP_INPUT_FORM_YUV422_CRY0CBY1:
            return 2;

        default:
            return 0;
    }
}


static void get_dvp_cfg(uint16_t id, DVP_InitTypeDef **pdvp_cfg, camera_config_t **pcamera_cfg)
{
    uint32_t i = 0;

    for(i = 0; i < (sizeof(dvp_case_tab) / sizeof(dvp_CaseTypeDef)); i++)
    {
        if(id == dvp_case_tab[i].id)
        {
            *pdvp_cfg = dvp_case_tab[i].pdvp_cfg;
            *pcamera_cfg = dvp_case_tab[i].pcamera_cfg;
            return;
        }
    }
    VIDEO_LOG("[%s:%d] error id=0x%x ", __func__, __LINE__, id);
}


static void gc0308_set_polarity(DVP_InitTypeDef *dvp_cfg)
{
    uint8_t value = 0;

    value = CAMERA_READ_REG8(GC0308_SCCB_ADDR, 0x26);
    VIDEO_LOG("[%s:%d] GC0308 P0 *0x26=0x%x", __func__, __LINE__, value);      // 0x3

    if(DVP_POL_FALLING == dvp_cfg->PCKPolarity) {   // bit2
        value |= (1<<2);
    } else {
        value &= ~(1<<2);
    }

    if(DVP_POL_RISING == dvp_cfg->VSPolarity) {   // bit0
        value |= (1<<0);
    } else {
        value &= ~(1<<0);
    }

    if(DVP_POL_RISING == dvp_cfg->HSPolarity) {   // bit1
        value |= (1<<1);
    } else {
        value &= ~(1<<1);
    }

    CAMERA_WRITE_REG8(GC0308_SCCB_ADDR, 0x26, value);

    value = CAMERA_READ_REG8(GC0308_SCCB_ADDR, 0x26);
    VIDEO_LOG("[%s:%d] GC0308 P0 *0x26=0x%x", __func__, __LINE__, value);      // 0x3
}


/****************************************** CASE ************************************************/
static int32_t test_dvp_case(uint16_t id, bool colorbar)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t gpdma_finish_cnt = 0;
    uint32_t dvp_eof_cnt = 0;
    uint32_t image_cnt = 0;
    uint8_t *image_buf = NULL;
    uint32_t size_byte = 0;
    DVP_InitTypeDef *dvp_cfg = NULL;
    camera_config_t *camera_cfg = NULL;

    VIDEO_LOG("[%s:%d] test start id=0x%x", __func__, __LINE__, id);

    get_dvp_cfg(id, &dvp_cfg, &camera_cfg);
    CHECK_POINT_NOT_NULL_EXIT(dvp_cfg, error0);
    CHECK_POINT_NOT_NULL_EXIT(camera_cfg, error0);
    if(true == colorbar) {
        camera_cfg->colorbar = 1;
    }

    camera_dvp_pinmux();
    camera_dvp_reset();
    camera_dvp_power_on();
    dvp_reset();
    DELAY_MS(10);

    /* malloc */
    size_byte = (dvp_cfg->FrameWidth * dvp_cfg->FrameHeight * dvp_get_format_byte(dvp_cfg->InputFormat));
    image_buf = tiny_malloc(size_byte);
    //image_buf = (uint8_t *)0x28000000;   // PSRAM
    CHECK_POINT_NOT_NULL_EXIT(image_buf, error1);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, size_byte);

    /* gpdma init */
#if TEST_DVP_GPDMA_AUTO_MODE
    ret = dvp_gpdma_init(TEST_DVP_GPDMA_CH, TRUE);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
#else
    ret = dvp_gpdma_init(TEST_DVP_GPDMA_CH, FALSE);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);
#endif

    /* dvp init */
    ret = dvp_init(dvp_cfg, TEST_DVP_MCLK_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error1);

    /* camera init colorbar*/
    camera_init(camera_cfg);
    //gc0308_set_polarity(dvp_cfg);
    DELAY_MS(100);
    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    while(0)
    {
        DELAY_MS(100);
        sw_i2c_detect_all(camera_cfg->sccb_i2c_port);
    }

    gpdma_finish_cnt = dvp_gpdma_frame_get();
    dvp_eof_cnt = dvp_frame_get();
    //mmio_write32_field(0x45100000 + 0x04 * TEST_DVP_GPDMA_CH, 1, 1, 13);  // GP_DMAC_BASE:0x45100000  bit13:auto
#if TEST_DVP_GPDMA_AUTO_MODE
    ret = dvp_gpdma_start(TEST_DVP_GPDMA_CH, image_buf, size_byte / sizeof(uint32_t), TRUE);
#else
    ret = dvp_gpdma_start(TEST_DVP_GPDMA_CH, image_buf, size_byte / sizeof(uint32_t), FALSE);
#endif
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
    //VIDEO_LOG("DMA_ctrl=0x%x", mmio_read32(REG_BASE_GP_DMAC + 0x04 * TEST_DVP_GPDMA_CH));

    /* dvp start */
    ret = dvp_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    while(1)
    {
        /* wait next frame, check timeout */
#if TEST_DVP_GPDMA_AUTO_MODE
        timeout = 3000000;
        CHECK_EQ_TIMEOUT_EXIT(dvp_eof_cnt, dvp_frame_get(), timeout, error2);
        gpdma_finish_cnt = dvp_gpdma_frame_get();
        dvp_eof_cnt = dvp_frame_get();
        image_cnt++;

        VIDEO_LOG("gpdma_cnt=%d eof_cnt=%d image_cnt=%d", gpdma_finish_cnt, dvp_eof_cnt, image_cnt);
        VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, size_byte);
#else
        timeout = 3000000;
        CHECK_EQ_TIMEOUT_EXIT(gpdma_finish_cnt, dvp_gpdma_frame_get(), timeout, error2);

        timeout = 3000000;
        CHECK_EQ_TIMEOUT_EXIT(dvp_eof_cnt, dvp_frame_get(), timeout, error2);
        gpdma_finish_cnt = dvp_gpdma_frame_get();
        dvp_eof_cnt = dvp_frame_get();
        image_cnt++;

        ret = dvp_stop();
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

        VIDEO_LOG("gpdma_cnt=%d eof_cnt=%d image_cnt=%d", gpdma_finish_cnt, dvp_eof_cnt, image_cnt);
        VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, size_byte);
        DELAY_MS(1000);

        ret = dvp_gpdma_start(TEST_DVP_GPDMA_CH, image_buf, size_byte / sizeof(uint32_t), FALSE);
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);

        ret = dvp_start();
        CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error2);
#endif

#if 0
        if(image_cnt >= 16)
        {
            //dvp_camera_pwd();
            //VIDEO_LOG("end");
            //while(1);
        }
#endif

    }

    ret = SUCCESS;

error2:
    dvp_reg_dump();
    gpdma_reg_dump(TEST_DVP_GPDMA_CH);
    ap_cfg_reg_dump();

    dvp_stop();
    dvp_deinit();
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, size_byte);

    //VIDEO_LOG("0x%08x=0x%08x", (image_buf+0x6B00), *(uint32_t *)(image_buf+0x6B00));
    //yyuv_to_yvyu((uint32_t *)image_buf, size_byte/4);
    //VIDEO_LOG("0x%08x=0x%08x", (image_buf+0x6B00), *(uint32_t *)(image_buf+0x6B00));

error1:
    tiny_free(image_buf);

error0:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* XCLK OUT */
static int32_t test_dvp_mclk_out(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    dvp_reset();

    // PA12 configured to function 16 - CLOCK OUT
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, CSK_IOMUX_FUNC_ALTER16);
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER16);
    //DVP_EnableClockout(DVP0(), 300000000);         // div=0 176MHz
    DVP_EnableClockout(DVP0(), 10000000);
    //DVP_EnableClockout(DVP0(), 2340000);          // div=63 2.34MHz pass

    ap_cfg_reg_dump();
    dvp_reg_dump();
    while(1);

    while(0)
    {
        DVP_EnableClockout(DVP0(), 150000000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),  75000000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),  50000000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),  30000000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),  25000000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),  15000000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),  10000000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),   7500000);  DELAY_MS(10);
        DVP_EnableClockout(DVP0(),   5000000);  DELAY_MS(10);
    }

    //DVP_EnableClockout(DVP0(), 10000000 - 400000);
    //DVP_EnableClockout(DVP0(), 10000000 - 1);
    //DVP_EnableClockout(DVP0(), 10000000 - 0);
    //DVP_EnableClockout(DVP0(), 10000000 + 1);

    return SUCCESS;
}


/* test dvp : clk enable and reset */
static int32_t test_dvp_reset(void)
{
    int32_t ret = FAILURE;

    DVP_InitTypeDef dvp_attr_yuv422 = {
            .FrameWidth = 320,
            .FrameHeight = 240,
            .PixelOffset = 2,
            .LineOffset = 1,
            .InputFormat = DVP_INPUT_FORM_YUV422_CBY0CRY1,
            .PCKPolarity = DVP_POL_RISING,
            .VSPolarity = DVP_POL_FALLING,
            .HSPolarity = DVP_POL_RISING,
            .DataAlign = DVP_DATA_ALIGN_RIGHT,
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    dvp_reset();
    dvp_init(&dvp_attr_yuv422, 1000000);
    dvp_start();
    ap_cfg_reg_dump();
    dvp_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_DVP->REG_F_HOR.all,            0x00000140, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_F_VER.all,            0x000000F0, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_P_OFFSET.all,         0x00000002, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_L_OFFSET.all,         0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_CLK_OUTEN.all,        0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_POL_CNTL.all,         0x00000004, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_JLB_HANSHK_SEL.all,   0x0000010B, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_INPUT_FORM.all,       0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VI_EN.all,            0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_DMA_BURST_THD.all,    0x00000008, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_INTR_MSK.all,         0x0000013F, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_INTR_CLR.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VIC_IRQ.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VIC_INT_STATUS.all,   0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VIC_INT_RAW.all,      0x00000010, error);

    dvp_reset();
    dvp_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_DVP->REG_F_HOR.all,            0x00000500, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_F_VER.all,            0x000002D0, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_P_OFFSET.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_L_OFFSET.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_CLK_OUTEN.all,        0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_POL_CNTL.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_JLB_HANSHK_SEL.all,   0x00000100, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_INPUT_FORM.all,       0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VI_EN.all,            0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_DMA_BURST_THD.all,    0x00000001, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_INTR_MSK.all,         0x000001FF, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_INTR_CLR.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VIC_IRQ.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VIC_INT_STATUS.all,   0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_DVP->REG_VIC_INT_RAW.all,      0x00000010, error);

    ret = SUCCESS;

error:
    dvp_stop();
    dvp_deinit();

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


static int32_t test_dvp_pin(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    camera_dvp_gpio_pwm();

    return SUCCESS;
}


#if 0
#define CHECK_RESULT(result, expect) do{ \
    if(result != expect) { \
        failed_cnt++; \
        VIDEO_LOG("[%s:%d] test failed", __func__, __LINE__); \
    } \
} while(0)

void test_dvp_api(void)
{
    CSK_DRIVER_VERSION version;
    void* pDvpDev = NULL;
    DVP_InitTypeDef dvp_cfg;
    DVP_emState State;
    DVP_emError Error;
    uint8_t buffer_y[1];
    uint8_t buffer_u[1];
    uint8_t buffer_v[1];
    DVP_BufferDef FrameBuf = {buffer_y, buffer_u, buffer_v, sizeof(buffer_y), sizeof(buffer_u), sizeof(buffer_v)};
    int32_t result;
    uint32_t failed_cnt = 0;
    uint32_t i;

    dvp_cfg.PCKPolarity = DVP_POL_RISING;
    dvp_cfg.HSPolarity = DVP_POL_RISING;
    dvp_cfg.VSPolarity = DVP_POL_FALLING;
    dvp_cfg.HandshakingMode = DVP_JLB_HANSHK_SOFT;
    dvp_cfg.DataAlign = DVP_DATA_ALIGN_LEFT;
    dvp_cfg.InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR;
    dvp_cfg.FrameWidth = 320;
    dvp_cfg.FrameHeight = 240;
    dvp_cfg.LineOffset = 0;
    dvp_cfg.PixelOffset = 0;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* test DVP_GetVersion */
    version = DVP_GetVersion();
    VIDEO_LOG("[%s:%d] DVP version api=%#x drv=%#x", __func__, __LINE__, version.api, version.drv);
    CHECK_RESULT(version.api, 0x100);
    CHECK_RESULT(version.drv, 0x100);

    pDvpDev = DVP0();

    /* test DVP_Initialize */
    result = DVP_Initialize(NULL, NULL, &dvp_cfg);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_Initialize(pDvpDev, NULL, NULL);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    /* Check if a valid width - multiplier of 16 */
    for(i = 0; i <= 1280; i++)
    {
        dvp_cfg.FrameWidth = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if ((i % 16) != 0 || i == 0) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.FrameWidth = 320;

    /* Check if a valid height - multiplier of 8 */
    for(i = 0; i <= 720; i++)
    {
        dvp_cfg.FrameHeight = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if ((i % 8) != 0 || i == 0) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.FrameHeight = 240;

    /* Check HandshakingMode */
    for(i = 0; i <= (DVP_JLB_HANSHK_BUTT+1); i++)
    {
        dvp_cfg.HandshakingMode = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if (i != DVP_JLB_HANSHK_SOFT) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_UNSUPPORTED);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.HandshakingMode = DVP_JLB_HANSHK_SOFT;

    /* Check InputFormat */
    for(i = 0; i <= (DVP_INPUT_FORM_BUTT+1); i++)
    {
        dvp_cfg.InputFormat = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if (i >= DVP_INPUT_FORM_BUTT) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR;

    /* Check PCKPolarity */
    for(i = 0; i <= (DVP_POL_BUTT+1); i++)
    {
        dvp_cfg.PCKPolarity = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if (i >= DVP_POL_BUTT) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.PCKPolarity = DVP_POL_RISING;

    /* Check HSPolarity */
    for(i = 0; i <= (DVP_POL_BUTT+1); i++)
    {
        dvp_cfg.HSPolarity = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if (i >= DVP_POL_BUTT) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.HSPolarity = DVP_POL_RISING;

    /* Check VSPolarity */
    for(i = 0; i <= (DVP_POL_BUTT+1); i++)
    {
        dvp_cfg.VSPolarity = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if (i >= DVP_POL_BUTT) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.VSPolarity = DVP_POL_FALLING;

    /* Check DataAlign */
    for(i = 0; i <= (DVP_DATA_ALIGN_BUTT+1); i++)
    {
        dvp_cfg.DataAlign = i;
        result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);

        if (i >= DVP_DATA_ALIGN_BUTT) {
            CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
        } else {
            CHECK_RESULT(result, CSK_DRIVER_OK);
        }
    }
    dvp_cfg.DataAlign = DVP_DATA_ALIGN_LEFT;


    /* test DVP_Uninitialize */
    result = DVP_Uninitialize(NULL);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_Uninitialize(pDvpDev);
    CHECK_RESULT(result, CSK_DRIVER_OK);


    /* test DVP_Start */
    result = DVP_Start(NULL, &FrameBuf);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_Start(pDvpDev, NULL);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    FrameBuf.pAddrY = NULL;
    FrameBuf.pAddrCb = NULL;
    FrameBuf.pAddrCr = NULL;
    result = DVP_Start(pDvpDev, &FrameBuf);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);
    FrameBuf.pAddrY = buffer_y;
    FrameBuf.pAddrCb = buffer_u;
    FrameBuf.pAddrCr = buffer_v;

    result = DVP_Start(pDvpDev, &FrameBuf);
    CHECK_RESULT(result, CSK_DRIVER_ERROR);

    result = DVP_Initialize(pDvpDev, NULL, &dvp_cfg);
    result = DVP_Start(pDvpDev, &FrameBuf);
    CHECK_RESULT(result, CSK_DRIVER_OK);


    /* test DVP_Stop */
    result = DVP_Stop(NULL);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_Stop(pDvpDev);
    CHECK_RESULT(result, CSK_DRIVER_OK);


    /* test DVP_EnableClockout */
    result = DVP_EnableClockout(NULL, 10000000);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_EnableClockout(pDvpDev, 0);
    CHECK_RESULT(result, CSK_DRIVER_OK);
    DELAY_MS(50);

    result = DVP_EnableClockout(pDvpDev, 300000000);  //300MHz
    CHECK_RESULT(result, CSK_DRIVER_OK);
    DELAY_MS(50);

    for(i = 0; i <= 64; i++)
    {
        /* Set the clock output frequency to hclk/2/(divider+1) */
        result = DVP_EnableClockout(pDvpDev, 300000000/2/(i+1));
        CHECK_RESULT(result, CSK_DRIVER_OK);
        DELAY_MS(50);
    }


    /* test DVP_DisableClockout */
    result = DVP_DisableClockout(NULL);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_DisableClockout(pDvpDev);
    CHECK_RESULT(result, CSK_DRIVER_OK);


    /* test DVP_GetState */
    result = DVP_GetState(NULL, &State);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_GetState(pDvpDev, NULL);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_GetState(pDvpDev, &State);
    CHECK_RESULT(result, CSK_DRIVER_OK);


    /* test DVP_GetError */
    result = DVP_GetError(NULL, &Error);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_GetError(pDvpDev, NULL);
    CHECK_RESULT(result, CSK_DRIVER_ERROR_PARAMETER);

    result = DVP_GetError(pDvpDev, &Error);
    CHECK_RESULT(result, CSK_DRIVER_OK);


    if(failed_cnt) {
        VIDEO_LOG("[%s:%d] test failed cnt=%d", __func__, __LINE__, failed_cnt);
    } else {
        VIDEO_LOG("[%s:%d] test success", __func__, __LINE__);
    }
}

#endif


