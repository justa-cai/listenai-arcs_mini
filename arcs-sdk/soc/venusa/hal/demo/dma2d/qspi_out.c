#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_DMA2D.h"
#include "Driver_QSPI_OUT.h"

//#include "test_case.h"
#include "check.h"

#define VIDEO_LOG   CLOGD
#define DELAY_MS(x) SysTick_Delay_Ms(x)
#define DELAY_US(x) SysTick_Delay_Us(x)

#define TEST_QSPI_OUT_GPDMA_CH          dma_2d_ch3
#define TEST_QSPI_OUT_CLK_HZ            100000000
#define TEST_QSPI_OUT_BUF               0x38030000

#define TEST_LCD_TYPE_SPD2010           0
#define TEST_LCD_TYPE_ST77916           1
#define TEST_LCD_TYPE                   TEST_LCD_TYPE_ST77916

#if (TEST_LCD_TYPE == TEST_LCD_TYPE_SPD2010)
// 412x412 -> 320x240  Center=(206,206)  start=(206-160,206-120)=(46,86)
#define TEST_QSPI_OUT_IMAGE_START_X      48     // must be 4N
#define TEST_QSPI_OUT_IMAGE_START_Y      86
#define TEST_QSPI_OUT_IMAGE_SIZE_W      320     // must be 4N
#define TEST_QSPI_OUT_IMAGE_SIZE_H      240
#define TEST_QSPI_OUT_LCD_WIDTH         412     // must be 4N
#define TEST_QSPI_OUT_LCD_HEIGHT        412
#endif

#if (TEST_LCD_TYPE == TEST_LCD_TYPE_ST77916)
#define TEST_QSPI_OUT_IMAGE_START_X      20     // must be 4N
#define TEST_QSPI_OUT_IMAGE_START_Y      60
#define TEST_QSPI_OUT_IMAGE_SIZE_W      320     // must be 4N
#define TEST_QSPI_OUT_IMAGE_SIZE_H      240
#define TEST_QSPI_OUT_LCD_WIDTH         360     // must be 4N
#define TEST_QSPI_OUT_LCD_HEIGHT        360
#endif

typedef enum {
    QSPI_OUT_TXIO_PIO,
    QSPI_OUT_TXIO_DMA,
    QSPI_OUT_TXIO_BUTT,
} qspi_out_txio_t;

typedef enum {
    QSPI_OUT_CPOL0_CPHA0,
    QSPI_OUT_CPOL0_CPHA1,
    QSPI_OUT_CPOL1_CPHA0,
    QSPI_OUT_CPOL1_CPHA1,
    QSPI_OUT_CPOL_CPHA_BUTT,
} qspi_out_cpol_cpha_t;

typedef enum {
    QSPI_OUT_FORMAT_RGB565,
    QSPI_OUT_FORMAT_RGB888,
    QSPI_OUT_FORMAT_BUTT,
} qspi_out_format_t;

typedef struct {
    uint32_t clk_hz;
    qspi_out_txio_t txio;                       /*!< QSPI_OUT_TXIO_PIO/QSPI_OUT_TXIO_DMA */
    qspi_out_cpol_cpha_t cp;                    /*!< QSPI_OUT_CPOL0_CPHA0/01/10/11 */
    bool is_msb;                                /*!< true/false */
    uint8_t lane_num;
    uint16_t width;
    uint16_t height;
    qspi_out_format_t format;
} qspi_out_config_t;

static int32_t qspi_out_init(qspi_out_config_t *qspi_cfg);
static int32_t qspi_out_deinit(void);
static int32_t qspi_out_write(void *pdata, uint32_t num);
static int32_t qspi_out_set_bit16_order(uint8_t order);
static int32_t qspi_out_set_data_length(uint8_t length);
static int32_t qspi_out_set_lane_num(uint8_t lane_num);
static int32_t qspi_out_set_dma_size(uint32_t size_byte);
static bool qspi_out_txfifo_is_empty(void);
static void qspi_out_reset(void);
static void qspi_out_reg_dump(void);

static uint32_t qspi_out_gpdma_finish_cnt_get(void);
static void qspi_out_gpdma_finish_cnt_clear(void);
static int32_t qspi_out_gpdma_init(csk_dma2d_ch_t dma_ch, csk_dma2d_ch_t dma_ch_trigger, uint16_t img_width, uint16_t img_height, csk_image_format_t img_format);
static int32_t qspi_out_gpdma_start(csk_dma2d_ch_t dma_ch, void *pbuf, uint32_t size_byte);
static int32_t qspi_out_gpdma_stop(csk_dma2d_ch_t dma_ch);

static void spd2010_init(void);
static void spd2010_write_mem_start(uint8_t lane_num);
static void spd2010_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h);

static int32_t st77916_init(void);
static int32_t st77916_write_mem_start(uint8_t lane_num);
static int32_t st77916_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h);

int32_t qspi_out_spd2010_image_flush(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint8_t *pbuf, uint32_t size_byte, csk_dma2d_ch_t dma_chn);
int32_t qspi_out_spd2010_image_flush_start(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint8_t *pbuf, uint32_t size_byte, csk_dma2d_ch_t dma_chn);
int32_t qspi_out_spd2010_image_flush_waitdone(void);
int32_t qspi_out_spd2010_image_flush_stop(csk_dma2d_ch_t dma_ch);
static void rgb565_colorbar_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t bar_height);


#define CONSTRAIN_UINT8(ch)  ((ch) < 0) ? 0 : (((ch) > 255) ? 255 : (ch))
/* YUV422-YVYU ->  RGB565  */
static void sw_yuv422_to_rgb565(uint8_t *yuv422, uint8_t *rgb565, uint32_t pixel_num)
{
    int y1, y2, u, v, r, g, b;

    while(pixel_num)
    {
        y1 = *yuv422++;
        u = *yuv422++;
        y2 = *yuv422++;
        v = *yuv422++;

        u -= 128;
        v -= 128;

        r = y1 + v + ((v * 103) >> 8);
        g = y1 - ((u * 88) >> 8) - ((v * 183) >> 8);
        b = y1 + u + ((u * 198) >> 8);

        r = CONSTRAIN_UINT8(r);
        g = CONSTRAIN_UINT8(g);
        b = CONSTRAIN_UINT8(b);

        *(rgb565 ++) = ((r & 0xF8) | (g >> 5));
        *(rgb565 ++) = (((g & 0x1C) << 3) | (b >> 3));

        r = y2 + v + ((v * 103) >> 8);
        g = y2 - ((u * 88) >> 8) - ((v * 183) >> 8);
        b = y2 + u + ((u * 198) >> 8);

        r = CONSTRAIN_UINT8(r);
        g = CONSTRAIN_UINT8(g);
        b = CONSTRAIN_UINT8(b);

        *(rgb565 ++) = ((r & 0xF8) | (g >> 5));
        *(rgb565 ++) = (((g & 0x1C) << 3) | (b >> 3));

        pixel_num -= 2;
    }
}

int32_t qspi_out_task_start(csk_dma2d_ch_t src_chn, csk_dma2d_ch_t dst_chn, uint16_t img_width, uint16_t img_height, csk_image_format_t img_format)
{
    int32_t ret = FAILURE;
    uint32_t times = 0;
    void *gpio_dev = NULL;
    static qspi_out_config_t qspi_out_cfg = {
            .clk_hz = TEST_QSPI_OUT_CLK_HZ,
            .txio = QSPI_OUT_TXIO_PIO,
            .cp = QSPI_OUT_CPOL0_CPHA0,
            .is_msb = true,
            .lane_num = 4,
            .width = TEST_QSPI_OUT_LCD_WIDTH,
            .height = TEST_QSPI_OUT_LCD_HEIGHT,
            .format = QSPI_OUT_FORMAT_RGB565,
    };

    /* PINMUX */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, CSK_IOMUX_FUNC_DEFAULT); // CS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  2, CSK_IOMUX_FUNC_ALTER19); // CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, CSK_IOMUX_FUNC_ALTER19); // D0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, CSK_IOMUX_FUNC_ALTER19); // D1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, CSK_IOMUX_FUNC_ALTER19); // D2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, CSK_IOMUX_FUNC_ALTER19); // D3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_DEFAULT); // RST
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_DEFAULT); // BL

    /* LCD reset */
    gpio_dev = GPIOA();
    GPIO_SetDir(gpio_dev, (1UL << 10) | (1UL << 3), CSK_GPIO_DIR_OUTPUT);  // CS and RST out
    GPIO_PinWrite(gpio_dev, (1UL << 10), 0);
    DELAY_MS(100);
    GPIO_PinWrite(gpio_dev, (1UL << 10), 1);
    DELAY_MS(100);

    /* LCD BL enable */
    GPIO_SetDir(gpio_dev, (1UL << 11), CSK_GPIO_DIR_OUTPUT);
    times = 100;
    do {
        GPIO_PinWrite(gpio_dev, (1UL << 11), 1);
        DELAY_US(5);
        GPIO_PinWrite(gpio_dev, (1UL << 11), 0);
        DELAY_US(5);
    } while(times--);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* dma init */
    ret = qspi_out_gpdma_init(dst_chn, src_chn, img_width, img_height, img_format);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    /* qspi_out init */
    ret = qspi_out_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    /* LCD init */
#if (TEST_LCD_TYPE == TEST_LCD_TYPE_SPD2010)
    spd2010_init();
#elif (TEST_LCD_TYPE == TEST_LCD_TYPE_ST77916)
    st77916_init();
#else
#endif
    DELAY_MS(100);

    ret = SUCCESS;
    goto error0;

error0:
    qspi_out_reg_dump();
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}

#if 0
int32_t test_qspi_out_demo(void)
{
    int32_t ret = FAILURE;
    uint32_t times = 0;
    void *gpio_dev = NULL;
    //uint8_t *image_buf = (uint8_t *)TEST_QSPI_OUT_BUF;
    uint32_t image_size_byte = TEST_QSPI_OUT_IMAGE_SIZE_W * TEST_QSPI_OUT_IMAGE_SIZE_H * 2;  // RGB565
    static qspi_out_config_t qspi_out_cfg = {
            .clk_hz = TEST_QSPI_OUT_CLK_HZ,
            .txio = QSPI_OUT_TXIO_PIO,
            .cp = QSPI_OUT_CPOL0_CPHA0,
            .is_msb = true,
            .lane_num = 4,
            .width = TEST_QSPI_OUT_LCD_WIDTH,
            .height = TEST_QSPI_OUT_LCD_HEIGHT,
            .format = QSPI_OUT_FORMAT_RGB565,
    };

    VIDEO_LOG("[%s:%d] test start", __func__, __LINE__);
    VIDEO_LOG("image_buf=0x%08x size=0x%x byte", image_buf, image_size_byte);
    rgb565_colorbar_create((uint16_t *)image_buf, TEST_QSPI_OUT_IMAGE_SIZE_W, TEST_QSPI_OUT_IMAGE_SIZE_H, TEST_QSPI_OUT_IMAGE_SIZE_H/5);
    MFlushDCache();

    /* PINMUX */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, CSK_IOMUX_FUNC_DEFAULT); // CS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  2, CSK_IOMUX_FUNC_ALTER19); // CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, CSK_IOMUX_FUNC_ALTER19); // D0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, CSK_IOMUX_FUNC_ALTER19); // D1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, CSK_IOMUX_FUNC_ALTER19); // D2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, CSK_IOMUX_FUNC_ALTER19); // D3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, CSK_IOMUX_FUNC_DEFAULT); // RST
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_DEFAULT); // BL

    /* LCD reset */
    gpio_dev = GPIOA();
    GPIO_SetDir(gpio_dev, (1UL << 10) | (1UL << 3), CSK_GPIO_DIR_OUTPUT);  // CS and RST out
    GPIO_PinWrite(gpio_dev, (1UL << 10), 0);
    DELAY_MS(100);
    GPIO_PinWrite(gpio_dev, (1UL << 10), 1);
    DELAY_MS(100);

    /* LCD BL enable */
    GPIO_SetDir(gpio_dev, (1UL << 11), CSK_GPIO_DIR_OUTPUT);
    times = 100;
    do {
        GPIO_PinWrite(gpio_dev, (1UL << 11), 1);
        DELAY_US(5);
        GPIO_PinWrite(gpio_dev, (1UL << 11), 0);
        DELAY_US(5);
    } while(times--);
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* dma init */
    ret = qspi_out_gpdma_init(TEST_QSPI_OUT_GPDMA_CH);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    /* qspi_out init */
    ret = qspi_out_init(&qspi_out_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    /* LCD SPD2010 init */
#if (TEST_LCD_TYPE == TEST_LCD_TYPE_SPD2010)
    spd2010_init();
#elif (TEST_LCD_TYPE == TEST_LCD_TYPE_ST77916)
    st77916_init();
#else
#endif
    DELAY_MS(100);

    while(0)
    {
        qspi_out_image_flush(TEST_QSPI_OUT_IMAGE_START_X, TEST_QSPI_OUT_IMAGE_START_Y, \
                TEST_QSPI_OUT_IMAGE_SIZE_W, TEST_QSPI_OUT_IMAGE_SIZE_H, image_buf, image_size_byte);
        times++;
        VIDEO_LOG("[%s:%d] times=%d", __func__, __LINE__, times);
        DELAY_MS(2000);
    }

    while(1)
    {
        memset(image_buf, 0xFF, image_size_byte);
        MFlushDCache();
        qspi_out_image_flush(TEST_QSPI_OUT_IMAGE_START_X, TEST_QSPI_OUT_IMAGE_START_Y, \
                        TEST_QSPI_OUT_IMAGE_SIZE_W, TEST_QSPI_OUT_IMAGE_SIZE_H, image_buf, image_size_byte);
        times++;
        VIDEO_LOG("[%s:%d] times=%d", __func__, __LINE__, times);
        DELAY_MS(1000);

        rgb565_colorbar_create((uint16_t *)image_buf, TEST_QSPI_OUT_IMAGE_SIZE_W, TEST_QSPI_OUT_IMAGE_SIZE_H, TEST_QSPI_OUT_IMAGE_SIZE_H/5);
        MFlushDCache();
        qspi_out_image_flush(TEST_QSPI_OUT_IMAGE_START_X, TEST_QSPI_OUT_IMAGE_START_Y, \
                        TEST_QSPI_OUT_IMAGE_SIZE_W, TEST_QSPI_OUT_IMAGE_SIZE_H, image_buf, image_size_byte);
        times++;
        VIDEO_LOG("[%s:%d] times=%d", __func__, __LINE__, times);
        DELAY_MS(1000);
    }

    ret = SUCCESS;
    goto error0;

error0:
    qspi_out_reg_dump();
    if(ret == SUCCESS) {
        VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);
    } else {
        VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);
    }
    return ret;
}
#endif

static inline void qspi_out_cs_gpio_clr(void)
{
    //DELAY_US(1);
    GPIO_PinWrite(GPIOA(), (1UL << 3), 0);
    //DELAY_US(1);
}

static inline void qspi_out_cs_gpio_set(void)
{
    //DELAY_US(1);
    GPIO_PinWrite(GPIOA(), (1UL << 3), 1);
    //DELAY_US(1);
}

int32_t qspi_out_spd2010_image_flush(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint8_t *pbuf, uint32_t size_byte, csk_dma2d_ch_t dma_chn)
{
    int32_t ret = FAILURE;

    ret = qspi_out_spd2010_image_flush_start(start_x, start_y, image_w, image_h, pbuf, size_byte, dma_chn);
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error0);

    ret = qspi_out_spd2010_image_flush_waitdone();
    CHECK_RET_EQ_EXIT(ret, SUCCESS, error0);

error0:
    qspi_out_spd2010_image_flush_stop(dma_chn);

    return ret;
}

int32_t qspi_out_spd2010_image_flush_start(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h, uint8_t *pbuf, uint32_t size_byte, csk_dma2d_ch_t dma_chn)
{
    int32_t ret = FAILURE;

    //VIDEO_LOG("[%s:%d] start(%d,%d) w=%d h=%d", __func__, __LINE__, start_x, start_y, image_w, image_h);
    //VIDEO_LOG("[%s:%d] pbuf=0x%x size=%d Byte", __func__, __LINE__, pbuf, size_byte);
    CHECK_POINT_NOT_NULL(pbuf);

#if (TEST_LCD_TYPE == TEST_LCD_TYPE_SPD2010)
    spd2010_window_set(start_x, start_y, image_w, image_h);
    qspi_out_cs_gpio_clr();
    spd2010_write_mem_start(4);
#elif (TEST_LCD_TYPE == TEST_LCD_TYPE_ST77916)
    st77916_window_set(start_x, start_y, image_w, image_h);
    qspi_out_cs_gpio_clr();
    st77916_write_mem_start(4);
#else
#endif

    qspi_out_set_bit16_order(1);
    qspi_out_set_lane_num(4);
    qspi_out_set_dma_size(size_byte);

    qspi_out_gpdma_finish_cnt_clear();
    ret = qspi_out_gpdma_start(dma_chn, pbuf, size_byte);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    ret = SUCCESS;

error0:
    return ret;
}


int32_t qspi_out_spd2010_image_flush_waitdone(void)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;

    timeout = 3000000;  // wait GPDMA done, timeout=1000ms
    while(!qspi_out_gpdma_finish_cnt_get())
    {
        DELAY_US(1);
        if(timeout-- == 0)
        {
            VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
            ret = FAILURE;
            goto error0;
        }
    }

    timeout = 100; // wait QSPI OUT txfifo empty
    while(false == qspi_out_txfifo_is_empty())
    {
        DELAY_US(1);
        if(timeout-- == 0)
        {
            VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
            ret = FAILURE;
            goto error0;
        }
    }

    ret = SUCCESS;

error0:
    return ret;
}


int32_t qspi_out_spd2010_image_flush_stop(csk_dma2d_ch_t dma_ch)
{
    qspi_out_cs_gpio_set();
    qspi_out_set_lane_num(1);
    qspi_out_set_bit16_order(0);

    return SUCCESS;
}


#define RGB565_RED         0xF800
#define RGB565_GREEN       0x07E0
#define RGB565_BLUE        0x001F
#define RGB565_YELLOW      0xFFE0
#define RGB565_BRED        0XF81F
#define RGB565_GBLUE       0X07FF
#define RGB565_WHITE       0xFFFF
#define RGB565_BLACK       0x0000

const uint16_t rgb565_color_tab[] = {
        RGB565_WHITE,
        RGB565_RED,
        RGB565_GREEN,
        RGB565_BLUE,
        RGB565_WHITE,
        RGB565_BLACK,
        RGB565_YELLOW,
        RGB565_BRED,
        RGB565_GBLUE,
};

static void rgb565_colorbar_create(uint16_t *rgb565, uint16_t img_width, uint16_t img_height, uint16_t bar_height)
{
    uint16_t x, y, color;
    uint16_t index;

    for(y = 0; y < img_height; y++)
    {
        index = ((y / bar_height)) % (sizeof(rgb565_color_tab) / sizeof(uint16_t));
        color = rgb565_color_tab[index];

        for(x = 0; x < img_width; x++)
        {
            if(index == 0) {
                color = rgb565_color_tab[((x / bar_height)) % (sizeof(rgb565_color_tab) / sizeof(uint16_t))];
            } else {
                if(rgb565_color_tab[index] == RGB565_WHITE) {
                    if(color >= 0x0841) {   // ((1<<11) | (1<<6) | 1)     2^5=32
                        color -= 0x0841;
                    } else {
                        color = RGB565_WHITE;
                    }
                } else if(rgb565_color_tab[index] == RGB565_BLACK) {
                    if(color <= (0xFFFF - 0x0841)) {
                        color += 0x0841;
                    } else {
                        color = RGB565_BLACK;
                    }
                } else {
                }
            }

            *rgb565++ = color;
        }
    }
}


/****************************************** GPDMA ************************************************/
static volatile uint32_t qspi_out_gpdma_finish_cnt = 0;

static void qspi_out_gpdma_callback(uint32_t event, void *workspace)
{
    VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    qspi_out_gpdma_finish_cnt++;
}


static uint32_t qspi_out_gpdma_finish_cnt_get(void)
{
    return qspi_out_gpdma_finish_cnt;
}

static void qspi_out_gpdma_finish_cnt_clear(void)
{
    qspi_out_gpdma_finish_cnt = 0;
}

uint32_t qspi_out_pixel_bitw(csk_image_format_t format)
{
    uint32_t bitw = 0;

    switch(format)
    {
        case csk_image_format_yuv444_packed:
        case csk_image_format_rgb888:
        case csk_image_format_bgr888:
            bitw = 3;
            break;

        case csk_image_format_yuv422_yuyv_packed:
        case csk_image_format_yuv422_uyvy_packed:
        case csk_image_format_yuv422_yvyu_packed:
        case csk_image_format_yuv422_vyuy_packed:
        case csk_image_format_rgb565:
        case csk_image_format_bgr565:
            bitw = 2;
            break;

        case csk_image_format_y8:
            bitw = 1;
            break;

        default:
            bitw = 1;
            break;
    }

    return bitw;
}

static int32_t qspi_out_gpdma_init(csk_dma2d_ch_t dma_ch, csk_dma2d_ch_t dma_ch_trigger, uint16_t img_width, uint16_t img_height, csk_image_format_t img_format)
{
    int32_t ret = 0;

    csk_dma2d_init_t dma2d_para;
    memset(&dma2d_para, 0, sizeof(dma2d_para));

    dma2d_para.dma_ch = dma_ch;
    dma2d_para.tfr_mode = tfr_mode_m2p;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_fix;
    dma2d_para.src_burst_len = dma2d_burst_len_8spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_8spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = qspi_out_hs_num1;
    dma2d_para.trigger.mode = csk_trigger_dst;
    dma2d_para.trigger.triggered_en = csk_func_enable;
    dma2d_para.trigger.triggered_src_chn = dma_ch_trigger;

    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));

    dma2d_img_cfg.img_input.img_width = img_width;
    dma2d_img_cfg.img_input.img_height = img_height;
    dma2d_img_cfg.img_input.img_format = img_format;
    dma2d_img_cfg.img_input.img_line_stride = dma2d_img_cfg.img_input.img_width * qspi_out_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_width = dma2d_img_cfg.img_input.img_width;
    dma2d_img_cfg.img_output.img_height = dma2d_img_cfg.img_input.img_height;
    dma2d_img_cfg.img_output.img_format = csk_image_format_bgr565;
    dma2d_img_cfg.img_output.img_line_stride = dma2d_img_cfg.img_output.img_width * qspi_out_pixel_bitw(dma2d_img_cfg.img_output.img_format);

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Config(&dma2d_para, qspi_out_gpdma_callback, NULL);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DMA2D_Image_Config_Extend(dma_ch, &dma2d_img_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_01 = 0;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_17 = 0;

    qspi_out_gpdma_finish_cnt = 0;
    return CSK_DRIVER_OK;
}


static int32_t qspi_out_gpdma_start(csk_dma2d_ch_t dma_ch, void *pbuf, uint32_t size_byte)
{
    int32_t ret = 0;

    CHECK_POINT_NOT_NULL(pbuf);

    ret = DMA2D_Start_Normal(dma_ch, pbuf, (void *)QSPI_OUT_Buf(), size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


static int32_t qspi_out_gpdma_stop(csk_dma2d_ch_t dma_ch)
{
    int32_t ret = 0;

    ret = DMA2D_Stop(dma_ch);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


/****************************************************************************************/

static volatile uint32_t qspi_out_finish_cnt = 0;

// SPI event
static void QSPI_DrvEvent (QSPI_OUT_emIrqEvent event, uint32_t usr_param)
{
    //notify SPI to send next block of data
    if (event == QSPI_OUT_IRQ_EVENT_TRANSFER_COMPLETE)
    {
        qspi_out_finish_cnt++;
    }

    //VIDEO_LOG("[%s:%d] event=%d flag=%d", __func__, __LINE__, event, qspi_out_finish_flag);
}

static uint32_t qspi_out_finish_cnt_get(void)
{
    return qspi_out_finish_cnt;
}

static void qspi_out_finish_cnt_clear(void)
{
    qspi_out_finish_cnt = 0;
}

static int32_t qspi_out_init(qspi_out_config_t *qspi_cfg)
{
    int32_t ret = 0;
    uint32_t bus_speed = qspi_cfg->clk_hz;
    uint32_t control = 0;

    CHECK_POINT_NOT_NULL(qspi_cfg);

    //qspi_out_reset();

    ret = QSPI_OUT_Initialize(QSPI_OUT(), QSPI_DrvEvent, (uint32_t)qspi_cfg);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    switch (qspi_cfg->cp)
    {
        case QSPI_OUT_CPOL0_CPHA0:
            control |= QSPI_OUT_CONTROL_CPOL0_CPHA0;
            break;

        case QSPI_OUT_CPOL0_CPHA1:
            control |= QSPI_OUT_CONTROL_CPOL0_CPHA1;
            break;

        case QSPI_OUT_CPOL1_CPHA0:
            control |= QSPI_OUT_CONTROL_CPOL1_CPHA0;
            break;

        case QSPI_OUT_CPOL1_CPHA1:
            control |= QSPI_OUT_CONTROL_CPOL1_CPHA1;
            break;

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    if (true == qspi_cfg->is_msb) {
            control |= QSPI_OUT_CONTROL_BIT_MSB;
    } else {
            control |= QSPI_OUT_CONTROL_BIT_LSB;
    }

    control |= QSPI_OUT_CONTROL_DATA_1LANE;
    control |= QSPI_OUT_CONTROL_CLK_OUT_HZ;
    control |= QSPI_OUT_CONTROL_DATA_LENGTH_8;

    ret = QSPI_OUT_Control(QSPI_OUT(), control, bus_speed);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    qspi_out_finish_cnt_clear();

    return ret;
}


static int32_t qspi_out_deinit(void)
{
    int32_t ret = 0;

    ret = QSPI_OUT_Uninitialize(QSPI_OUT());
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    qspi_out_finish_cnt_clear();

    return ret;
}


static int32_t qspi_out_write(void *pdata, uint32_t num)
{
    int32_t ret;

    if((pdata == NULL) || (num == 0))
    {
        VIDEO_LOG("[%s:%d] invalid parameter", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    ret = QSPI_OUT_Control(QSPI_OUT(), QSPI_OUT_CONTROL_FIFO_CLEAR, 0);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    qspi_out_finish_cnt_clear();

    ret = QSPI_OUT_Send(QSPI_OUT(), pdata, num);
    if(ret != CSK_DRIVER_OK) {
        VIDEO_LOG("pdata=0x%x, *pdata=0x%x, num=%d", pdata, *(uint8_t *)pdata, num);
    }
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    uint32_t timeout = 3000000;  // wait done, timeout=1000ms
    while(!qspi_out_finish_cnt_get())
    {
        DELAY_US(1);
        if(timeout-- == 0)
        {
            VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
            ret = FAILURE;
        }
    }
    qspi_out_finish_cnt_clear();

    return ret;
}

static int32_t qspi_out_set_lane_num(uint8_t lane_num)
{
    int32_t ret = 0;
    uint32_t control = 0;

    switch (lane_num)
    {
        case 1:
            control = QSPI_OUT_CONTROL_DATA_1LANE;
            break;

        case 2:
            control = QSPI_OUT_CONTROL_DATA_2LANE;
            break;

        case 4:
            control = QSPI_OUT_CONTROL_DATA_4LANE;
            break;

        default:
            VIDEO_LOG("[%s:%d] (lane_num = %d) failed!!\r\n", __func__, __LINE__, lane_num);
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    ret = QSPI_OUT_Control(QSPI_OUT(), control, 0);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

static int32_t qspi_out_set_dma_size(uint32_t size_byte)
{
    int32_t ret;

    ret = QSPI_OUT_Control(QSPI_OUT(), QSPI_OUT_CONTROL_DMA_SIZE, size_byte);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

static int32_t qspi_out_set_bit16_order(uint8_t order)
{
    int32_t ret;
    uint32_t control = 0;

    if(order) {
        control = QSPI_OUT_CONTROL_HALFWORD_MSB;
    } else {
        control = QSPI_OUT_CONTROL_HALFWORD_LSB;
    }

    ret = QSPI_OUT_Control(QSPI_OUT(), control, 0);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

static int32_t qspi_out_set_data_length(uint8_t length)
{
    int32_t ret = 0;
    uint32_t control = 0;

    switch (length)
    {
        case 8:
            control = QSPI_OUT_CONTROL_DATA_LENGTH_8;
            break;

        case 16:
            control = QSPI_OUT_CONTROL_DATA_LENGTH_16;
            break;

        case 24:
            control = QSPI_OUT_CONTROL_DATA_LENGTH_24;
            break;

        case 32:
            control = QSPI_OUT_CONTROL_DATA_LENGTH_32;
            break;

        default:
            VIDEO_LOG("[%s:%d] (length = %d) failed!!\r\n", __func__, __LINE__, length);
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    ret = QSPI_OUT_Control(QSPI_OUT(), control, 0);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}

static bool qspi_out_txfifo_is_empty(void)
{
    int32_t ret = 0;
    uint32_t is_empty = 0;

    ret = QSPI_OUT_Control(QSPI_OUT(), QSPI_OUT_CONTROL_GET_FIFO_EMPTY, (uint32_t)(&is_empty));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    if(is_empty) {
        return true;
    } else {
        return false;
    }
}

static void qspi_out_reset(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_QSPI1_CLK      = 1; // bit 19~19
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.QSPI1_RESET        = 1; // bit 23~23
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.SEL_QSPI1_CLK      = 1; // bit 10~10  0:24MHz  1:syspll peri clk
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_M    = 1; // bit 12~15
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_N    = 1; // bit 16~18
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_LD   = 1; // bit 11~11
}

static void qspi_out_reg_dump(void)
{
    QSPI_LCD_RegDef *qspi_out = (QSPI_LCD_RegDef *)QSPI_OUT_BASE;

    VIDEO_LOG("0x00 REG_IDREV        *0x%08x=0x%08x", &qspi_out->REG_IDREV.all, qspi_out->REG_IDREV.all);
    VIDEO_LOG("0x10 REG_TRANSFMT     *0x%08x=0x%08x", &qspi_out->REG_TRANSFMT.all, qspi_out->REG_TRANSFMT.all);
    VIDEO_LOG("0x14 REG_DIRECTIO     *0x%08x=0x%08x", &qspi_out->REG_DIRECTIO.all, qspi_out->REG_DIRECTIO.all);
    VIDEO_LOG("0x20 REG_TRANSCTRL    *0x%08x=0x%08x", &qspi_out->REG_TRANSCTRL.all, qspi_out->REG_TRANSCTRL.all);
    VIDEO_LOG("0x24 REG_CMD          *0x%08x=0x%08x", &qspi_out->REG_CMD.all, qspi_out->REG_CMD.all);
    VIDEO_LOG("0x28 REG_ADDR         *0x%08x=0x%08x", &qspi_out->REG_ADDR.all, qspi_out->REG_ADDR.all);
    //VIDEO_LOG("0x2C REG_DATA         *0x%08x=0x%08x", &qspi_out->REG_DATA.all, qspi_out->REG_DATA.all);
    VIDEO_LOG("0x30 REG_CTRL         *0x%08x=0x%08x", &qspi_out->REG_CTRL.all, qspi_out->REG_CTRL.all);
    VIDEO_LOG("0x34 REG_STATUS       *0x%08x=0x%08x", &qspi_out->REG_STATUS.all, qspi_out->REG_STATUS.all);
    VIDEO_LOG("0x38 REG_INTREN       *0x%08x=0x%08x", &qspi_out->REG_INTREN.all, qspi_out->REG_INTREN.all);
    VIDEO_LOG("0x3C REG_INTRST       *0x%08x=0x%08x", &qspi_out->REG_INTRST.all, qspi_out->REG_INTRST.all);
    VIDEO_LOG("0x40 REG_TIMING       *0x%08x=0x%08x", &qspi_out->REG_TIMING.all, qspi_out->REG_TIMING.all);
    VIDEO_LOG("0x50 REG_MEMCTRL      *0x%08x=0x%08x", &qspi_out->REG_MEMCTRL.all, qspi_out->REG_MEMCTRL.all);
    VIDEO_LOG("0x60 REG_SLVST        *0x%08x=0x%08x", &qspi_out->REG_SLVST.all, qspi_out->REG_SLVST.all);
    VIDEO_LOG("0x64 REG_SLVDATACNT   *0x%08x=0x%08x", &qspi_out->REG_SLVDATACNT.all, qspi_out->REG_SLVDATACNT.all);
    VIDEO_LOG("0x68 REG_LCD_TX       *0x%08x=0x%08x", &qspi_out->REG_LCD_TX.all, qspi_out->REG_LCD_TX.all);
    VIDEO_LOG("0x7C REG_CONFIG       *0x%08x=0x%08x", &qspi_out->REG_CONFIG.all, qspi_out->REG_CONFIG.all);
}



/************************** SPD2010 ***************************************************************/
#define SPD2010_CS_SET                      qspi_out_cs_gpio_set
#define SPD2010_CS_CLR                      qspi_out_cs_gpio_clr
#define SPD2010_DELAY_MS                    DELAY_MS
#define SPD2010_WRITE_BUF(_pdata, _num)     qspi_out_write(_pdata, _num)

static void inline spd2010_write_byte(uint8_t data)
{
    SPD2010_WRITE_BUF(&data, 1);
}

static void inline spd2010_write_buf(uint8_t *pdata, uint32_t num)
{
    SPD2010_WRITE_BUF(pdata, num);
}

static void spd2010_write_reg(uint8_t reg)
{
    spd2010_write_byte(0x02);
    spd2010_write_byte(0x00);
    spd2010_write_byte(reg);
    spd2010_write_byte(0x00);
}

static void inline spd2010_write_data(uint8_t dat)
{
    spd2010_write_byte(dat);
}


/********************************************************************************/
// lane_num: 1/2/4
static void spd2010_write_mem_start(uint8_t lane_num)
{
    uint8_t data[4] = {0};

    switch(lane_num)
    {
        case 1:
            data[0] = (0x02);
            data[1] = (0x00);
            data[2] = (0x3c);
            data[3] = (0x00);
            spd2010_write_buf(data, 4);
            break;

        case 2:
            data[0] = (0xA2);
            data[1] = (0x00);
            data[2] = (0x3c);
            data[3] = (0x00);
            spd2010_write_buf(data, 4);
            break;

        case 4:
            data[0] = (0x32);
            data[1] = (0x00);
            data[2] = (0x3c);
            data[3] = (0x00);
            spd2010_write_buf(data, 4);
            break;

        default:
            break;
    }
}


// start_x: 0~411
static void spd2010_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h)
{
    uint8_t data[8] = {0};

    SPD2010_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (0x2A);
    data[3] = (0x00);
    data[4] = (start_x >> 8);
    data[5] = (start_x & 0xff);
    data[6] = ((start_x + image_w - 1) >> 8);
    data[7] = ((start_x + image_w - 1) & 0xff);
    spd2010_write_buf(data, 8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (0x2B);
    data[3] = (0x00);
    data[4] = (start_y >> 8);
    data[5] = (start_y & 0xff);
    data[6] = ((start_y + image_h - 1) >> 8);
    data[7] = ((start_y + image_h - 1) & 0xff);
    spd2010_write_buf(data, 8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (0x2C);
    data[3] = (0x00);
    spd2010_write_buf(data, 4);
    SPD2010_CS_SET();
}


typedef enum {
    LCD_PIXFORMAT_RGB565 = 0x0,
    LCD_PIXFORMAT_RGB666 = 0x1,
    LCD_PIXFORMAT_RGB888 = 0x2,
    LCD_PIXFORMAT_BGR565 = 0x3,
    LCD_PIXFORMAT_BGR666 = 0x4,
    LCD_PIXFORMAT_BGR888 = 0x5,
    LCD_PIXFORMAT_BUTT,
} lcd_format_e;

/* 0x3A bit0~2: 7=RGB888  6=RGB666  5=RGB565   */
/* 0x36 bit3:   0:RGB     1:BGR   */
static void spd2010_init(void)
{
    lcd_format_e format = LCD_PIXFORMAT_RGB565;

    //spd2010_reset();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x10);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x62);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x61);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x5C);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x58);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x55);spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x54);spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x51);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x4B);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x4A);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x49);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x47);spd2010_write_data(0x77);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x46);spd2010_write_data(0x66);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x45);spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x44);spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x43);spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x42);spd2010_write_data(0x22);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x41);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x36);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x34);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x33);spd2010_write_data(0x20);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x32);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x31);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23);spd2010_write_data(0x20);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22);spd2010_write_data(0x72);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21);spd2010_write_data(0x82);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20);spd2010_write_data(0x81);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1B);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1A);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0C);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B);spd2010_write_data(0x43);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF);spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x11);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x6A);spd2010_write_data(0x10);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x69);spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x68);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x67);spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x66);spd2010_write_data(0x38);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x65);spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x64);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x63);spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x62);spd2010_write_data(0x38);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x61);spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x60);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x55);spd2010_write_data(0x06);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x50);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30);spd2010_write_data(0xEE);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1E);spd2010_write_data(0x88);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1D);spd2010_write_data(0x88);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1C);spd2010_write_data(0x88);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16);spd2010_write_data(0x99);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15);spd2010_write_data(0x99);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x13);spd2010_write_data(0xf0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0c);spd2010_write_data(0xF0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B);spd2010_write_data(0xF0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0A);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08);spd2010_write_data(0x70);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF);spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x36);spd2010_write_data(0xA0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2E);spd2010_write_data(0x1e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2D);spd2010_write_data(0x2D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2C);spd2010_write_data(0x26);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2B);spd2010_write_data(0x1e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2A);spd2010_write_data(0x2D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21);spd2010_write_data(0x70);//vcom
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1F);spd2010_write_data(0xE6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18);spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15);spd2010_write_data(0x0F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x12);spd2010_write_data(0x89);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10);spd2010_write_data(0x0F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0D);spd2010_write_data(0x66);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06);spd2010_write_data(0x06);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00);spd2010_write_data(0xCC);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF);spd2010_write_data(0x20);spd2010_write_data(0x10);spd2010_write_data(0x15);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2F);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2E);spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2D);spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2C);spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2B);spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2A);spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29);spd2010_write_data(0x1D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x1B);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0x19);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x17);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0x09);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x05);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0F); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0E); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0D); spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0C); spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0A); spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x16);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x18);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x1A);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x1C);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x08);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x16);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2F); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2E); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2D); spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2C); spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2B); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2A); spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29); spd2010_write_data(0x1C);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x1A);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0x18);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x16);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0x08);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0F); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0E); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0D); spd2010_write_data(0x35);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0C); spd2010_write_data(0x34);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0A); spd2010_write_data(0x33);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x17);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x19);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x1B);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x1D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x05);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x09);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x17);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x39); spd2010_write_data(0x3c);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1F); spd2010_write_data(0x80);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1A); spd2010_write_data(0x80);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18); spd2010_write_data(0xA0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16); spd2010_write_data(0x12);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14); spd2010_write_data(0xAA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11); spd2010_write_data(0xAA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10); spd2010_write_data(0x0E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0B); spd2010_write_data(0xC3);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x18);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x3A); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1F); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0x1E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x2D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x3E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xff); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x31);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x39); spd2010_write_data(0xf0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x38); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37); spd2010_write_data(0xe8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    switch(format)
    {
        case LCD_PIXFORMAT_RGB565:
        case LCD_PIXFORMAT_RGB666:
        case LCD_PIXFORMAT_RGB888:
            spd2010_write_reg(0x36); spd2010_write_data(0x03);   // bit3: 0=RGB  1=BGR
            break;

        default:
            spd2010_write_reg(0x36); spd2010_write_data(0x0B);   // bit3: 0=RGB  1=BGR
            break;
    }
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35); spd2010_write_data(0xCF);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x34); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x33); spd2010_write_data(0xBA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x32); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x31); spd2010_write_data(0xA2);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2f); spd2010_write_data(0x95);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2e); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2d); spd2010_write_data(0x7e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2c); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2b); spd2010_write_data(0x62);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2a); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29); spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0xfc);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0xd0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x6f);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1f); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1e); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1d); spd2010_write_data(0xf6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1c); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1b); spd2010_write_data(0xb8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1a); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x19); spd2010_write_data(0x6E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x17); spd2010_write_data(0x41);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15); spd2010_write_data(0xfd);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x13); spd2010_write_data(0xCf);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x12); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0f); spd2010_write_data(0x89);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0e); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0d); spd2010_write_data(0x79);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0c); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0b); spd2010_write_data(0x67);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0a); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x3F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x28);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x0E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xff); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x39); spd2010_write_data(0xf0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x38); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x37); spd2010_write_data(0xe8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    switch(format)
    {
        case LCD_PIXFORMAT_RGB565:
        case LCD_PIXFORMAT_RGB666:
        case LCD_PIXFORMAT_RGB888:
            spd2010_write_reg(0x36); spd2010_write_data(0x03);   // bit3: 0=RGB  1=BGR
            break;

        default:
            spd2010_write_reg(0x36); spd2010_write_data(0x0B);   // bit3: 0=RGB  1=BGR
            break;
    }
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35); spd2010_write_data(0xCF);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x34); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x33); spd2010_write_data(0xBA);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x32); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x31); spd2010_write_data(0xA2);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x30); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2f); spd2010_write_data(0x95);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2e); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2d); spd2010_write_data(0x7e);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2c); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2b); spd2010_write_data(0x62);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x2a); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29); spd2010_write_data(0x44);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x28); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x27); spd2010_write_data(0xfc);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x26); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x25); spd2010_write_data(0xd0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x24); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x23); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x22); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x21); spd2010_write_data(0x6f);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x20); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1f); spd2010_write_data(0x32);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1e); spd2010_write_data(0x02);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1d); spd2010_write_data(0xf6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1c); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1b); spd2010_write_data(0xb8);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x1a); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x19); spd2010_write_data(0x6E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x18); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x17); spd2010_write_data(0x41);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x16); spd2010_write_data(0x01);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x15); spd2010_write_data(0xfd);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x14); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x13); spd2010_write_data(0xCf);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x12); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x11); spd2010_write_data(0x98);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x10); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0f); spd2010_write_data(0x89);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0e); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0d); spd2010_write_data(0x79);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0c); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0b); spd2010_write_data(0x67);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x0a); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x09); spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x07); spd2010_write_data(0x3F);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x28);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x04); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x0E);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x02); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xff); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x40);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x86); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x83); spd2010_write_data(0xC4);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x42);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x06); spd2010_write_data(0x03);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x3D);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x43);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x04);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x45);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x03); spd2010_write_data(0x9C);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0x9C);
    SPD2010_CS_SET();


    /*
    // TP settings, if TP function is not used, the following lines of code will be cancelled

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x50);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0x55);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x05); spd2010_write_data(0x08);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x01); spd2010_write_data(0xA6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x00); spd2010_write_data(0xA6);
    SPD2010_CS_SET();
    // TP code end
    */

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0xA0);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x08); spd2010_write_data(0xE6);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0xFF); spd2010_write_data(0x20); spd2010_write_data(0x10); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x35); spd2010_write_data(0x00);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();

    switch(format)
    {
        case LCD_PIXFORMAT_RGB565:
        case LCD_PIXFORMAT_BGR565:
            spd2010_write_reg(0x3A); spd2010_write_data(0x5); //0x07=RGB24, 0x06=RGB18, 0x05=RGB16
            break;

        case LCD_PIXFORMAT_RGB666:
        case LCD_PIXFORMAT_BGR666:
            spd2010_write_reg(0x3A); spd2010_write_data(0x6); //0x07=RGB24, 0x06=RGB18, 0x05=RGB16
            break;

        default:
            spd2010_write_reg(0x3A); spd2010_write_data(0x7); //0x07=RGB24, 0x06=RGB18, 0x05=RGB16
                        break;
    }
    SPD2010_CS_SET();


    SPD2010_CS_CLR();
    spd2010_write_reg(0x11);
    SPD2010_DELAY_MS(150);
    SPD2010_CS_SET();

    SPD2010_CS_CLR();
    spd2010_write_reg(0x29);
    SPD2010_DELAY_MS(10);


}


/**********************************************************************************************/
/**********************************************************************************************/
/**********************************************************************************************/


#define ST77916_CMD_SWRESET         0x01
#define ST77916_CMD_SLEEP_IN        0x10
#define ST77916_CMD_SLEEP_OUT       0x11
#define ST77916_CMD_DISP_OFF        0x28
#define ST77916_CMD_DISP_ON         0x29
#define ST77916_CMD_CASET           0x2A
#define ST77916_CMD_RASET           0x2B
#define ST77916_CMD_RAMWR_START     0x2C
#define ST77916_CMD_RAMWR_CONTINUE  0x3C
#define ST77916_CMD_1LANE           0x02
#define ST77916_CMD_2LANE           0xA2
#define ST77916_CMD_4LANE           0x32

#define ST77916_CS_SET                      qspi_out_cs_gpio_set
#define ST77916_CS_CLR                      qspi_out_cs_gpio_clr
#define ST77916_DELAY_MS                    DELAY_MS
#define ST77916_WRITE_BUF(_pdata, _num)     qspi_out_write(_pdata, _num)

static void inline st77916_write_byte(uint8_t data)
{
    //CLOG("[%s:%d] %d", __func__, __LINE__, sw_gpio_enable);
    ST77916_WRITE_BUF(&data, 1);
}

static void inline st77916_write_buf(uint8_t *pdata, uint32_t num)
{
    //CLOG("[%s:%d] %d", __func__, __LINE__, sw_gpio_enable);
    ST77916_WRITE_BUF(pdata, num);
}


static void st77916_write_reg(uint8_t reg)
{
    st77916_write_byte(0x02);
    st77916_write_byte(0x00);
    st77916_write_byte(reg);
    st77916_write_byte(0x00);
}

static void inline st77916_write_data(uint8_t dat)
{
    st77916_write_byte(dat);
}

static void st77916_write_reg_data(uint8_t reg, uint8_t dat)
{
    ST77916_CS_CLR();
    st77916_write_byte(0x02);
    st77916_write_byte(0x00);
    st77916_write_byte(reg);
    st77916_write_byte(0x00);
    st77916_write_byte(dat);
    ST77916_CS_SET();
    DELAY_US(10);
}


// lane_num: 1/2/4
static int32_t st77916_write_mem_start(uint8_t lane_num)
{
    uint8_t data[4] = {ST77916_CMD_4LANE, 0x00, ST77916_CMD_RAMWR_CONTINUE, 0x00};

    switch(lane_num)
    {
        case 1:
            data[0] = (ST77916_CMD_1LANE);
            break;

        case 2:
            data[0] = (ST77916_CMD_2LANE);
            break;

        case 3:
            //data[0] = (ST77916_CMD_4LANE);
            break;

        default:
            break;
    }
    st77916_write_buf(data, 4);

    return 0;
}


// start_x: 0~411
static int32_t st77916_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h)
{
    uint8_t data[8] = {0};

    ST77916_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (ST77916_CMD_CASET);
    data[3] = (0x00);
    data[4] = (start_x >> 8);
    data[5] = (start_x & 0xff);
    data[6] = ((start_x + image_w - 1) >> 8);
    data[7] = ((start_x + image_w - 1) & 0xff);
    st77916_write_buf(data, 8);
    ST77916_CS_SET();
    //ST77916_DELAY_MS(1);

    ST77916_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (ST77916_CMD_RASET);
    data[3] = (0x00);
    data[4] = (start_y >> 8);
    data[5] = (start_y & 0xff);
    data[6] = ((start_y + image_h - 1) >> 8);
    data[7] = ((start_y + image_h - 1) & 0xff);
    st77916_write_buf(data, 8);
    ST77916_CS_SET();
    //ST77916_DELAY_MS(1);

    ST77916_CS_CLR();
    data[0] = (0x02);
    data[1] = (0x00);
    data[2] = (ST77916_CMD_RAMWR_START);
    data[3] = (0x00);
    st77916_write_buf(data, 4);
    ST77916_CS_SET();
    //ST77916_DELAY_MS(1);

    return 0;
}


/* 0x3A bit0~2: 6=RGB666  5=RGB565   */
/* 0x36 bit3:   0:RGB     1:BGR   */
static int32_t st77916_init(void)
{
    lcd_format_e format = LCD_PIXFORMAT_RGB565;

    //CLOG("[%s:%d]  ", __func__, __LINE__);
    //st77916_reset();

//    ST77916_CS_CLR();
//    st77916_write_reg(ST77916_CMD_SWRESET);
//    ST77916_CS_SET();
//    ST77916_DELAY_MS(150);

    st77916_write_reg_data(0xF0, 0x28);
    st77916_write_reg_data(0xF2, 0x28);
    st77916_write_reg_data(0x73, 0xF0);
    st77916_write_reg_data(0x7C, 0xD1);
    st77916_write_reg_data(0x83, 0xE0);
    st77916_write_reg_data(0x84, 0x61);
    st77916_write_reg_data(0xF2, 0x82);
    st77916_write_reg_data(0xF0, 0x00);
    st77916_write_reg_data(0xF0, 0x01);
    st77916_write_reg_data(0xF1, 0x01);
    st77916_write_reg_data(0xB0, 0x5E);
    st77916_write_reg_data(0xB1, 0x55);
    st77916_write_reg_data(0xB2, 0x24);
    st77916_write_reg_data(0xB3, 0x01);
    st77916_write_reg_data(0xB4, 0x87);
    st77916_write_reg_data(0xB5, 0x44);
    st77916_write_reg_data(0xB6, 0x8B);
    st77916_write_reg_data(0xB7, 0x40);
    st77916_write_reg_data(0xB8, 0x86);
    st77916_write_reg_data(0xB9, 0x15);
    st77916_write_reg_data(0xBA, 0x00);
    st77916_write_reg_data(0xBB, 0x08);
    st77916_write_reg_data(0xBC, 0x08);
    st77916_write_reg_data(0xBD, 0x00);
    st77916_write_reg_data(0xBE, 0x00);
    st77916_write_reg_data(0xBF, 0x07);
    st77916_write_reg_data(0xC0, 0x80);
    st77916_write_reg_data(0xC1, 0x10);
    st77916_write_reg_data(0xC2, 0x37);
    st77916_write_reg_data(0xC3, 0x80);
    st77916_write_reg_data(0xC4, 0x10);
    st77916_write_reg_data(0xC5, 0x37);
    st77916_write_reg_data(0xC6, 0xA9);
    st77916_write_reg_data(0xC7, 0x41);
    st77916_write_reg_data(0xC8, 0x01);
    st77916_write_reg_data(0xC9, 0xA9);
    st77916_write_reg_data(0xCA, 0x41);
    st77916_write_reg_data(0xCB, 0x01);
    st77916_write_reg_data(0xCC, 0x7F);
    st77916_write_reg_data(0xCD, 0x7F);
    st77916_write_reg_data(0xCE, 0xFF);
    st77916_write_reg_data(0xD0, 0x91);
    st77916_write_reg_data(0xD1, 0x68);
    st77916_write_reg_data(0xD2, 0x68);

    ST77916_CS_CLR();
    st77916_write_reg(0xF5);
    st77916_write_data(0x00);
    st77916_write_data(0xA5);
    ST77916_CS_SET();
    ST77916_DELAY_MS(1);

    st77916_write_reg_data(0xDD, 0x40);
    st77916_write_reg_data(0xDE, 0x40);
    st77916_write_reg_data(0xF1, 0x10);
    st77916_write_reg_data(0xF0, 0x00);
    st77916_write_reg_data(0xF0, 0x02);

    ST77916_CS_CLR();
    st77916_write_reg(0xE0);
    st77916_write_data(0xF0);
    st77916_write_data(0x10);
    st77916_write_data(0x18);
    st77916_write_data(0x0D);
    st77916_write_data(0x0C);
    st77916_write_data(0x38);
    st77916_write_data(0x3E);
    st77916_write_data(0x44);
    st77916_write_data(0x51);
    st77916_write_data(0x39);
    st77916_write_data(0x15);
    st77916_write_data(0x15);
    st77916_write_data(0x30);
    st77916_write_data(0x34);
    ST77916_CS_SET();
    ST77916_DELAY_MS(1);

    ST77916_CS_CLR();
    st77916_write_reg(0xE1);
    st77916_write_data(0xF0);
    st77916_write_data(0x0F);
    st77916_write_data(0x17);
    st77916_write_data(0x0D);
    st77916_write_data(0x0B);
    st77916_write_data(0x07);
    st77916_write_data(0x3E);
    st77916_write_data(0x33);
    st77916_write_data(0x51);
    st77916_write_data(0x39);
    st77916_write_data(0x15);
    st77916_write_data(0x15);
    st77916_write_data(0x30);
    st77916_write_data(0x34);
    ST77916_CS_SET();
    ST77916_DELAY_MS(1);

    st77916_write_reg_data(0xF0, 0x10);
    st77916_write_reg_data(0xF3, 0x10);
    st77916_write_reg_data(0xE0, 0x08);
    st77916_write_reg_data(0xE1, 0x00);
    st77916_write_reg_data(0xE2, 0x00);
    st77916_write_reg_data(0xE3, 0x00);
    st77916_write_reg_data(0xE4, 0xE0);
    st77916_write_reg_data(0xE5, 0x06);
    st77916_write_reg_data(0xE6, 0x21);
    st77916_write_reg_data(0xE7, 0x03);
    st77916_write_reg_data(0xE8, 0x05);
    st77916_write_reg_data(0xE9, 0x02);
    st77916_write_reg_data(0xEA, 0xE9);
    st77916_write_reg_data(0xEB, 0x00);
    st77916_write_reg_data(0xEC, 0x00);
    st77916_write_reg_data(0xED, 0x14);
    st77916_write_reg_data(0xEE, 0xFF);
    st77916_write_reg_data(0xEF, 0x00);
    st77916_write_reg_data(0xF8, 0xFF);
    st77916_write_reg_data(0xF9, 0x00);
    st77916_write_reg_data(0xFA, 0x00);
    st77916_write_reg_data(0xFB, 0x30);
    st77916_write_reg_data(0xFC, 0x00);
    st77916_write_reg_data(0xFD, 0x00);
    st77916_write_reg_data(0xFE, 0x00);
    st77916_write_reg_data(0xFF, 0x00);
    st77916_write_reg_data(0x60, 0x40);
    st77916_write_reg_data(0x61, 0x05);
    st77916_write_reg_data(0x62, 0x00);
    st77916_write_reg_data(0x63, 0x42);
    st77916_write_reg_data(0x64, 0xDA);
    st77916_write_reg_data(0x65, 0x00);
    st77916_write_reg_data(0x66, 0x00);
    st77916_write_reg_data(0x67, 0x00);
    st77916_write_reg_data(0x68, 0x00);
    st77916_write_reg_data(0x69, 0x00);
    st77916_write_reg_data(0x6A, 0x00);
    st77916_write_reg_data(0x6B, 0x00);
    st77916_write_reg_data(0x70, 0x40);
    st77916_write_reg_data(0x71, 0x04);
    st77916_write_reg_data(0x72, 0x00);
    st77916_write_reg_data(0x73, 0x42);
    st77916_write_reg_data(0x74, 0xD9);
    st77916_write_reg_data(0x75, 0x00);
    st77916_write_reg_data(0x76, 0x00);
    st77916_write_reg_data(0x77, 0x00);
    st77916_write_reg_data(0x78, 0x00);
    st77916_write_reg_data(0x79, 0x00);
    st77916_write_reg_data(0x7A, 0x00);
    st77916_write_reg_data(0x7B, 0x00);
    st77916_write_reg_data(0x80, 0x48);
    st77916_write_reg_data(0x81, 0x00);
    st77916_write_reg_data(0x82, 0x07);
    st77916_write_reg_data(0x83, 0x02);
    st77916_write_reg_data(0x84, 0xD7);
    st77916_write_reg_data(0x85, 0x04);
    st77916_write_reg_data(0x86, 0x00);
    st77916_write_reg_data(0x87, 0x00);
    st77916_write_reg_data(0x88, 0x48);
    st77916_write_reg_data(0x89, 0x00);
    st77916_write_reg_data(0x8A, 0x09);
    st77916_write_reg_data(0x8B, 0x02);
    st77916_write_reg_data(0x8C, 0xD9);
    st77916_write_reg_data(0x8D, 0x04);
    st77916_write_reg_data(0x8E, 0x00);
    st77916_write_reg_data(0x8F, 0x00);
    st77916_write_reg_data(0x90, 0x48);
    st77916_write_reg_data(0x91, 0x00);
    st77916_write_reg_data(0x92, 0x0B);
    st77916_write_reg_data(0x93, 0x02);
    st77916_write_reg_data(0x94, 0xDB);
    st77916_write_reg_data(0x95, 0x04);
    st77916_write_reg_data(0x96, 0x00);
    st77916_write_reg_data(0x97, 0x00);
    st77916_write_reg_data(0x98, 0x48);
    st77916_write_reg_data(0x99, 0x00);
    st77916_write_reg_data(0x9A, 0x0D);
    st77916_write_reg_data(0x9B, 0x02);
    st77916_write_reg_data(0x9C, 0xDD);
    st77916_write_reg_data(0x9D, 0x04);
    st77916_write_reg_data(0x9E, 0x00);
    st77916_write_reg_data(0x9F, 0x00);
    st77916_write_reg_data(0xA0, 0x48);
    st77916_write_reg_data(0xA1, 0x00);
    st77916_write_reg_data(0xA2, 0x06);
    st77916_write_reg_data(0xA3, 0x02);
    st77916_write_reg_data(0xA4, 0xD6);
    st77916_write_reg_data(0xA5, 0x04);
    st77916_write_reg_data(0xA6, 0x00);
    st77916_write_reg_data(0xA7, 0x00);
    st77916_write_reg_data(0xA8, 0x48);
    st77916_write_reg_data(0xA9, 0x00);
    st77916_write_reg_data(0xAA, 0x08);
    st77916_write_reg_data(0xAB, 0x02);
    st77916_write_reg_data(0xAC, 0xD8);
    st77916_write_reg_data(0xAD, 0x04);
    st77916_write_reg_data(0xAE, 0x00);
    st77916_write_reg_data(0xAF, 0x00);
    st77916_write_reg_data(0xB0, 0x48);
    st77916_write_reg_data(0xB1, 0x00);
    st77916_write_reg_data(0xB2, 0x0A);
    st77916_write_reg_data(0xB3, 0x02);
    st77916_write_reg_data(0xB4, 0xDA);
    st77916_write_reg_data(0xB5, 0x04);
    st77916_write_reg_data(0xB6, 0x00);
    st77916_write_reg_data(0xB7, 0x00);
    st77916_write_reg_data(0xB8, 0x48);
    st77916_write_reg_data(0xB9, 0x00);
    st77916_write_reg_data(0xBA, 0x0C);
    st77916_write_reg_data(0xBB, 0x02);
    st77916_write_reg_data(0xBC, 0xDC);
    st77916_write_reg_data(0xBD, 0x04);
    st77916_write_reg_data(0xBE, 0x00);
    st77916_write_reg_data(0xBF, 0x00);
    st77916_write_reg_data(0xC0, 0x10);
    st77916_write_reg_data(0xC1, 0x47);
    st77916_write_reg_data(0xC2, 0x56);
    st77916_write_reg_data(0xC3, 0x65);
    st77916_write_reg_data(0xC4, 0x74);
    st77916_write_reg_data(0xC5, 0x88);
    st77916_write_reg_data(0xC6, 0x99);
    st77916_write_reg_data(0xC7, 0x01);
    st77916_write_reg_data(0xC8, 0xBB);
    st77916_write_reg_data(0xC9, 0xAA);
    st77916_write_reg_data(0xD0, 0x10);
    st77916_write_reg_data(0xD1, 0x47);
    st77916_write_reg_data(0xD2, 0x56);
    st77916_write_reg_data(0xD3, 0x65);
    st77916_write_reg_data(0xD4, 0x74);
    st77916_write_reg_data(0xD5, 0x88);
    st77916_write_reg_data(0xD6, 0x99);
    st77916_write_reg_data(0xD7, 0x01);
    st77916_write_reg_data(0xD8, 0xBB);
    st77916_write_reg_data(0xD9, 0xAA);
    st77916_write_reg_data(0xF3, 0x01);
    st77916_write_reg_data(0xF0, 0x00);

    switch(format)
    {
        case LCD_PIXFORMAT_RGB565:
            st77916_write_reg_data(0x36, 0x00); // bit3: 0=RGB  1=BGR
            st77916_write_reg_data(0x3A, 0x55); // 0x66=RGB18, 0x55=RGB16
            break;

        case LCD_PIXFORMAT_BGR565:
            st77916_write_reg_data(0x36, 0x08); // bit3: 0=RGB  1=BGR
            st77916_write_reg_data(0x3A, 0x55); // 0x66=RGB18, 0x55=RGB16
            break;

        case LCD_PIXFORMAT_RGB666:
        case LCD_PIXFORMAT_RGB888:
            st77916_write_reg_data(0x36, 0x00); // bit3: 0=RGB  1=BGR
            st77916_write_reg_data(0x3A, 0x66); // 0x66=RGB18, 0x55=RGB16
            break;

        case LCD_PIXFORMAT_BGR666:
        case LCD_PIXFORMAT_BGR888:
            st77916_write_reg_data(0x36, 0x08); // bit3: 0=RGB  1=BGR
            st77916_write_reg_data(0x3A, 0x66); // 0x66=RGB18, 0x55=RGB16
            break;

        default:
            st77916_write_reg_data(0x36, 0x00); // bit3: 0=RGB  1=BGR
            st77916_write_reg_data(0x3A, 0x55); // 0x66=RGB18, 0x55=RGB16
            CLOG("[%s:%d] not support", __func__, __LINE__);
            break;
    }

    ST77916_CS_CLR();
    st77916_write_reg(0x21);
    ST77916_CS_SET();
    ST77916_DELAY_MS(1);

    ST77916_CS_CLR();
    st77916_write_reg(ST77916_CMD_SLEEP_OUT);
    ST77916_CS_SET();

    ST77916_DELAY_MS(150);

    ST77916_CS_CLR();
    st77916_write_reg(ST77916_CMD_DISP_ON);
    ST77916_CS_SET();
    ST77916_DELAY_MS(10);

    return 0;
}





