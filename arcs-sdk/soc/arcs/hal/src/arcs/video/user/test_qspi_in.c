#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "PSRAMManager.h"
#include "ClockManager.h"
#include "systick.h"
#include "dma.h"

#include "test_case.h"
#include "qspi_in_case_config.h"
#include "csk_driver.h"
#include "camera.h"
#include "sw_i2c.h"


static int32_t test_qspi_in_reset(void);
void test_camera_spi_gpio(void);
int32_t test_qspi_in_case(QSPI_SENSOR_IN_InitTypeDef *pcfg);
int32_t test_qspi_in_bf3901(void);
static void bf3901_init(void);

void test_qspi_in(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    test_qspi_in_bf3901();

    //CHECK_FUNC_EXIT(test_qspi_in_case(&qspi_in_cfg_case0101), error);   // MTK        pass
    //CHECK_FUNC_EXIT(test_qspi_in_case(&qspi_in_cfg_case0102), error);   // ALL          pass
    //CHECK_FUNC_EXIT(test_qspi_in_reset(), error);           // clk enable and reset   pass

    ret = SUCCESS;
    VIDEO_LOG("[%s:%d]  all case test SUCCESS\r\n", __func__, __LINE__);
    return;

error:
    ret = FAILURE;
    VIDEO_LOG("[%s:%d]  case test FAILED\r\n", __func__, __LINE__);
    return;
}


int32_t test_qspi_in_bf3901(void)
{
    int32_t ret = FAILURE;
    uint32_t times = 0;
    uint32_t image_size_byte = 120 * 320 * 2;    // RAW8 / YUV422
    uint8_t *image_buf = NULL;
    uint32_t image_cnt = 0;
    uint32_t value = 0;

    QSPI_SENSOR_IN_InitTypeDef qspi_in_cfg = {
            .mode = QSPI_SENSOR_IN_SYNC_MODE_ALL,
            .lane_num = 1,
            .cp = QSPI_SENSOR_IN_CPOL0_CPOH0,
            .is_lsb = true,
            .data_merge = false,
            .wire_order = true,
            .sync_code = {0xFFFFFF, 0x01, 0x00, 0x40, 0x80},
            //{0xFF0000AB, 0xFF0000B6, 0xFF000080, 0xFF00009D}, // sof/eof/sol/eol
    };

    VIDEO_LOG("[%s:%d] lane_num=%d", __func__, __LINE__, qspi_in_cfg.lane_num);

    /* camera pinmux */
    camera_qspi_in_pinmux();
    camera_qspi_in_power_on();

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    image_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(image_buf, error);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    ret = qspi_in_gpdma_init(TEST_QSPI_IN_GPDMA_CH);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    ret = qspi_in_init(&qspi_in_cfg, 25000000);   // 25MHz
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    bf3901_init();

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    image_cnt = qspi_in_gpdma_get_cnt();
    ret = qspi_in_gpdma_start(TEST_QSPI_IN_GPDMA_CH, image_buf, image_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    ret = qspi_in_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    while(1)
    {
        /* get next image */
        if(image_cnt != qspi_in_gpdma_get_cnt())
        {
            image_cnt = qspi_in_gpdma_get_cnt();
            VIDEO_LOG("image_buf=0x%x size=0x%x cnt=%d", image_buf, image_size_byte, image_cnt);
            //qspi_in_msg_dump();
        }

//        DELAY_MS(1);
//        times++;
//        if((times % 5000) == 0)
//        {
//            qspi_in_reg_dump();
//            qspi_in_msg_dump();
//            VIDEO_LOG("");
//        }
    }

    ret = SUCCESS;

error:
    tiny_free(image_buf);
    qspi_in_reg_dump();
    gpdma_reg_dump(TEST_QSPI_IN_GPDMA_CH);
    qspi_in_stop();

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


int32_t test_qspi_in_case(QSPI_SENSOR_IN_InitTypeDef *pcfg)
{
    int32_t ret = FAILURE;
    uint32_t times = 0;
    uint32_t image_size_byte = IMAGE_WIDTH * IMAGE_HEIGHT * 2;
    uint8_t *image_buf = NULL;
    uint32_t image_cnt = 0;
    uint32_t value = 0;

    camera_config_t camera_attr_yuv422 = {
        .sccb_i2c_port = 1,
        .xclk_freq_hz = 12000000,
        .pixel_format = PIXFORMAT_YUV422,
        .frame_size = FRAMESIZE_QVGA,
        .colorbar = 0,
    };

//    pcfg->mode = QSPI_SENSOR_IN_SYNC_MODE_MTK;  //QSPI_SENSOR_IN_SYNC_MODE_MTK QSPI_SENSOR_IN_SYNC_MODE_ALL
//    pcfg->lane_num = 1;
//    pcfg->cp = QSPI_SENSOR_IN_CPOL0_CPOH0;
//    pcfg->is_lsb = false;
//    pcfg->data_merge = true;
//    pcfg->wire_order = false;
//    pcfg->sync_code.sync_code = 0xFFFFFF;
//    pcfg->sync_code.sof = 0x11;
//    pcfg->sync_code.eof = 0x88;
//    pcfg->sync_code.sol = 0x22;
//    pcfg->sync_code.eol = 0x44;

    VIDEO_LOG("[%s:%d] lane_num=%d", __func__, __LINE__, pcfg->lane_num);

    /* camera pinmux */
    camera_qspi_in_pinmux();
    camera_qspi_in_power_on();

    image_buf = tiny_malloc(image_size_byte);
    CHECK_POINT_NOT_NULL_EXIT(image_buf, error);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);

    ret = qspi_in_gpdma_init(TEST_QSPI_IN_GPDMA_CH);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = qspi_in_init(pcfg, TEST_QSPI_IN_MCLK);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    camera_init(&camera_attr_yuv422);

    ret = qspi_in_start();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    image_cnt = qspi_in_gpdma_get_cnt();
    ret = qspi_in_gpdma_start(TEST_QSPI_IN_GPDMA_CH, image_buf, image_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    while(1)
    {
        /* get next image */
        if(image_cnt != qspi_in_gpdma_get_cnt())
        {
            image_cnt = qspi_in_gpdma_get_cnt();
            VIDEO_LOG("image_buf=0x%x cnt=%d", image_buf, image_cnt);
            qspi_in_msg_dump();
        }

        DELAY_MS(1);
        times++;
        if((times % 5000) == 0)
        {
//            qspi_in_reg_dump();
//            qspi_in_msg_dump();
//            VIDEO_LOG("");
        }
    }

    ret = SUCCESS;

error:
    tiny_free(image_buf);
    qspi_in_reg_dump();
    gpdma_reg_dump(TEST_QSPI_IN_GPDMA_CH);
    qspi_in_stop();

    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


/* test qspi_in : clk enable and reset */
static int32_t test_qspi_in_reset(void)
{
    int32_t ret = FAILURE;
    QSPI_SENSOR_IN_InitTypeDef qspi_cfg = {
            .mode = QSPI_SENSOR_IN_SYNC_MODE_MTK,
            .lane_num = 4,
            .cp = QSPI_SENSOR_IN_CPOL1_CPOH1,
            .is_lsb = true,
            .data_merge = true,
            .wire_order = true,
            .sync_code = {0xFF0000, 0xAB, 0xB6, 0x80, 0x9D},    // sof/eof/sol/eol
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);

    qspi_in_reset();
    qspi_in_init(&qspi_cfg, TEST_QSPI_IN_MCLK);
    qspi_in_start();
    qspi_in_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_IDREV.all,           0x02002044, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_TRANSFMT.all,        0x00021f8f, error);
    //CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_DIRECTIO.all,        0x0000313f, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_TRANSCTRL.all,       0x02800000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_CMD.all,             0x00021600, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_ADDR.all,            0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_DATA.all,            0x000000c3, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_CTRL.all,            0x00200808, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_STATUS.all,          0x00404001, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_INTREN.all,          0x00000180, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_INTRST.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_TIMING.all,          0x00000201, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_MEMCTRL.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SLVST.all,           0x00010000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SLVDATACNT.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_PACKET_ID.all,       0xb69d80ab, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SYNC_CODE.all,       0x00ff0000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SPI_CAMERA_CTRL.all, 0x000000fb, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_CONFIG.all,          0x00004b33, error);

    qspi_in_reset();
    qspi_in_reg_dump();
    VIDEO_LOG("[%s:%d]\r\n", __func__, __LINE__);

    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_IDREV.all,           0x02002044, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_TRANSFMT.all,        0x00020780, error);
    //CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_DIRECTIO.all,        0x0000313d, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_TRANSCTRL.all,       0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_CMD.all,             0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_ADDR.all,            0x00000000, error);
    //CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_DATA.all,            0x000000c3, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_CTRL.all,            0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_STATUS.all,          0x00404000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_INTREN.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_INTRST.all,          0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_TIMING.all,          0x00000201, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_MEMCTRL.all,         0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SLVST.all,           0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SLVDATACNT.all,      0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_PACKET_ID.all,       0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SYNC_CODE.all,       0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_SPI_CAMERA_CTRL.all, 0x00000000, error);
    CHECK_RET_EQ_EXIT(IP_QSPI_SENSOR_IN->REG_CONFIG.all,          0x00004b33, error);

    ret = SUCCESS;

error:
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}


void test_camera_spi_gpio(void)
{
    uint32_t cnt = 0;
    void *gGpioADev = NULL;
    void *gGpioBDev = NULL;

    /* GPIO func */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, 0);  // GC0310_SDO_0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, 0);  // GC0310_SDO_1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, 0);  // GC0310_SDO_2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, 0);  // GC0310_SDO_3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, 0);  // GC0310_PCLK

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 0);  // GC0310_PWDN
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, 0);  // GC0310_SCL
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, 0);  // GC0310_SDA

    gGpioADev = GPIOA();
    //GPIO_Initialize(gGpioADev, NULL, NULL);
    GPIO_SetDir(gGpioADev, (1UL << 6) | (1UL << 7) | (1UL << 10) | (1UL << 11) | (1UL << 12) | (1UL << 13) | (1UL << 14) | (1UL << 15), CSK_GPIO_DIR_OUTPUT);

    /* MCLK */
    dvp_reset();
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER16);
    DVP_EnableClockout(DVP0(), TEST_QSPI_IN_MCLK);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);
        GPIO_PinWrite(gGpioADev, (1UL <<  6), 0);
        GPIO_PinWrite(gGpioADev, (1UL <<  7), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 10), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 11), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 12), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 13), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 14), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 15), 0);
        DELAY_MS(10);
        GPIO_PinWrite(gGpioADev, (1UL <<  6), 1);
        GPIO_PinWrite(gGpioADev, (1UL <<  7), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 10), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 11), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 12), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 13), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 14), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 15), 1);
        DELAY_MS(20);
    }
}



/* BF3901 camera, slv_addr_7bit=0x6e  120x320  Y   MCLK=25MHz  PCLK=21MHz  32fps */
/*
 * {0x62,0x81}, // bit[1:0]  01:spi mode   10:2bit mode   11:4bit mode
 * {0x6b,0x01}, //BSD: no frame HEAD, no frame END, no line HEAD   bit6:CCIR656
 * {0xb9,0x80}, // test pattern
 * {0x12,0x20}, //BSD: use LSB, bit[5]: 0=MSB, 1=LSB   bit4: 0=MTK 1=zhan xun    bit[2][0]= 00:YUV422 01:RAW8 10:RGB
 */
static const uint8_t bf3901_default_regs[][2] = {
{0x15,0x10}, //BSD: Bit[1]: VSYNC = Active low

{0x62,0x81}, // bit[1:0]  01:spi mode   10:2bit mode   11:4bit mode
{0x11,0xB0},
{0x1b,0x80},

{0x6b,0x01}, //BSD: no frame HEAD, no frame END, no line HEAD   bit6:CCIR656

{0x08,0xa0},

//{0xb9,0x80}, // test pattern

{0x12,0x20}, //BSD: use LSB, bit[5]: 0=MSB, 1=LSB   bit4: 0=MTK 1=zhan xun    bit[2][0]= 00:YUV422 01:RAW8 10:RGB
{0x0c,0x40},


{0x06,0x68},
{0x27,0x97},
{0x2b,0x20},
{0x13,0x05},
{0x01,0x0d},
{0x02,0x0d},
{0x87,0x16},
{0x8c,0x00},
{0x8d,0xb8},
{0x20,0x09},
{0x09,0x03}, //BSD: Standby mode, bit[4]: 0=Disable, 1=Enable
{0x33,0x10},
{0x34,0x1d},
{0x35,0x46},
{0x36,0x40},
{0x37,0xa4},
{0x38,0x7c},
{0x65,0x46},
{0x66,0x46},
{0x6e,0x20},
{0x9b,0xa4},
{0x9c,0x7c},
{0xbc,0x0c},
{0xbd,0xa4},
{0xbe,0x7c},
{0x70,0x0f},
{0x71,0x46},
{0x72,0x2f},
{0x73,0x2f},
{0x74,0xa7},
{0x75,0x12},
{0x76,0x90},
{0x77,0xdd},
{0x78,0x4e},
{0x79,0x85},
{0x7a,0x00},
{0x7b,0x55},
{0x7e,0xfa},
{0x7c,0x88},
{0x7d,0xba},
{0x60,0xe7},
{0x61,0xc8},
{0x6d,0x70},
{0x13,0x0f},
{0x8a,0x11},
{0x8b,0x21},
{0x8e,0x24},
{0x8f,0x31},
{0x94,0x38},
{0x95,0x6e},
{0x96,0x7f},
{0x97,0xf3},
{0x13,0x85},
{0x24,0x50},
{0x97,0x48},
{0x25,0x88},
{0x94,0x42},
{0x95,0xb0},
{0x80,0xd6},
{0x81,0xff},
{0x82,0x18},
{0x83,0x30},
{0x84,0x30},
{0x85,0x40},
{0x86,0x77},
{0x89,0x2d},
{0x8a,0x5e},
{0x8b,0x4c},
{0x98,0x1a},
{0x39,0x98},
{0x3f,0x98},
{0x90,0xa0},
{0x91,0xe0},
{0x40,0x3b},
{0x41,0x36},
{0x42,0x2b},
{0x43,0x1d},
{0x44,0x1a},
{0x45,0x14},
{0x46,0x11},
{0x47,0x0e},
{0x48,0x0d},
{0x49,0x0c},
{0x4b,0x0b},
{0x4c,0x09},
{0x4e,0x09},
{0x4f,0x08},
{0x50,0x07},
{0x5a,0x56},
{0x51,0x12},
{0x52,0x0d},
{0x53,0x92},
{0x54,0x7d},
{0x57,0x97},
{0x58,0x43},
{0x5a,0xd6},
{0x51,0x1f},
{0x52,0x0f},
{0x53,0x47},
{0x54,0x20},
{0x57,0x2f},
{0x58,0x24},
{0x5b,0xc2},
{0x5c,0x28},
{0xb0,0xe0},
{0xb3,0x4f},
{0xb4,0xe3},
{0xb1,0xf0},
{0xb2,0xa0},
{0xb4,0x63},
{0xb1,0xd0},
{0xb2,0xc0},
{0x55,0x00},
{0x56,0x40},
{0xa0,0xd0},
{0xa1,0x31},
{0xa6,0x04},
{0xa2,0x0f},
{0xa3,0x2b},
{0xa4,0x0f},
{0xa5,0x2b},
{0xa7,0x9a},
{0xa8,0x1c},
{0xd0,0xb4},
{0xd1,0x00},
{0xd2,0x78},
{0xa9,0x11},
{0xaa,0x16},
{0xab,0x16},
{0xac,0x3c},
{0xad,0xf0},
{0xae,0x57},
{0xc6,0xaa},
{0xc8,0x0d},
{0xc9,0x10},
{0xd3,0x09},
{0xd4,0x24},
{0x6a,0x81},
{0x23,0x33},
{0x69,0x00},
{0x1e,0x39},
{0xee,0x4c},
{0x0b,0x03},
{0xf1,0x6A},
{0x4a,0x0e},
{0xda,0x00},
{0xdb,0xf8}, // 248
{0xdc,0x00},
{0xdd,0x48},
{0xde,0x10}, // 328
{0x17,0x00},
{0x18,0x78}, // 240
{0x19,0x00},
{0x1a,0xa0}, // 320
{0x03,0x00},
{0x89,0x14},
{0x2b,0x00},
{0x2F,0x04},
{0x16,0xA7},
{0xbb,0x23},
};



static int write_regs(uint8_t slv_addr, const uint8_t (*regs)[2], size_t regs_size)
{
    int i = 0;
    int ret = 0;

    while (!ret && (i < regs_size)) {
        ret = sw_i2c_write_reg8(0, slv_addr, regs[i][0], regs[i][1]);
        i++;
    }

    return ret;
}


static void bf3901_init(void)
{
    int ret = 0;
    sw_i2c_port_callback_t i2c_port;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    i2c_port.gpio_init    = sw_qspi_in_i2c_gpio_init;
    i2c_port.scl_set      = sw_qspi_in_i2c_scl_gpio_set;
    i2c_port.scl_clr      = sw_qspi_in_i2c_scl_gpio_clr;
    i2c_port.sda_set      = sw_qspi_in_i2c_sda_gpio_set;
    i2c_port.sda_clr      = sw_qspi_in_i2c_sda_gpio_clr;
    i2c_port.sda_get      = sw_qspi_in_i2c_sda_gpio_get;
    i2c_port.sda_dirout   = sw_qspi_in_i2c_sda_gpio_setdir_output;
    i2c_port.sda_dirin    = sw_qspi_in_i2c_sda_gpio_setdir_input;
    sw_i2c_init(QSPI_IN_I2C_INDEX, &i2c_port);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    ret = write_regs(0x6E, bf3901_default_regs, sizeof(bf3901_default_regs)/(sizeof(uint8_t) * 2));
    if (ret == 0) {
        VIDEO_LOG("[%s:%d] Camera defaults loaded", __func__, __LINE__);
        DELAY_MS(100);
    } else {
        VIDEO_LOG("[%s:%d] error", __func__, __LINE__);
    }

}


