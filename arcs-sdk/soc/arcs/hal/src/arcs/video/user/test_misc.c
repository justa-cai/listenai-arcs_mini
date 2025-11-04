#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "PSRAMManager.h"

#include "test_case.h"
#include "csk_driver.h"
#include "Driver_DVP.h"
#include "img_converters.h"

#include "IOMuxManager.h"
#include "PSRAMManager.h"
#include "ClockManager.h"
#include "systick.h"
#include "dma.h"

#include "SEGGER_RTT.h"

/*
 * 0x2000_0000 0x2003_FFFF 256KB       HM14-1
 * 0x2004_0000 0x2004_FFFF 64KB        HM0
 * 0x2005_0000 0x200A_FFFF 384KB       HM1-1
 * 0x200B_0000 0x200B_FFFF 64KB        HM1-2
 * 0x200C_0000 0x200F_FFFF 256KB       HM13-1
 *
 * ilm (rxa!w) : ORIGIN = 0x20040000, LENGTH = 64K
 * ram (wxa!r) : ORIGIN = 0x20050000, LENGTH = 384K
*/

void test_uart(void)
{
    unsigned int cnt = 0;
#if 1
    unsigned int *p = &cnt;
#else
    unsigned int *p = (unsigned int *)(0x20080000);
#endif

    uint32_t start = 0;
    uint32_t end = 0;

    VIDEO_LOG("[%s:%d] 0x%x", __func__, __LINE__, p);

    VIDEO_LOG("[%s:%d] CRM_GetMtimeFreq=%d", __func__, __LINE__, CRM_GetMtimeFreq());

    while(1)
    {
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);

        *p=cnt;
        VIDEO_LOG("[%s:%d] 0x%x=%d", __func__, __LINE__, p, *p);

        //DELAY_MS(1000);
        start = __get_rv_cycle();
        SysTick_Delay_Ms(100);    // 100ms  ->  2400ms
        end = __get_rv_cycle();
        VIDEO_LOG("[%s:%d] SysTick=%d", __func__, __LINE__, end - start);

        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);

        SysTick_Delay_Us(10000);  // 10ms  ->  250ms
    }
}


void test_sw_uart(void)
{
    unsigned int cnt = 0;

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    sw_uart_init();

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    while(1)
    {
        cnt++;
        DELAY_MS(1000);
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt);

        sw_uart_send_byte('C');
        sw_uart_send_byte('S');
        sw_uart_send_byte('K');
        sw_printf("test %d\n", cnt);
        sw_printf("test 0x%x\n", cnt);
        sw_printf("test 0x%08x\n", cnt);
        sw_printf("test %#x\n", cnt);
        sw_printf("test %f\n", 3.14);
        sw_printf("test %p\n", &cnt);
        sw_printf("[%s:%d] \n", __func__, __LINE__);
    }
}


void test_segger_rtt(void)
{
    unsigned int cnt = 0;

    VIDEO_LOG("[%s:%d] SEGGER_RTT addr=0x%x", __func__, __LINE__, &_SEGGER_RTT);

    while(1)
    {
        SEGGER_RTT_printf(0, "[%s:%d] test %d\n", __func__, __LINE__, cnt++);
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt);
        DELAY_MS(1000);
    }
}

void test_memcpy(void)
{
    uint32_t i = 0;
    uint32_t cnt = 0;
    uint8_t data_src[4800] = {0};
    uint8_t data_dts[4800] = {0};
    uint64_t clk_start = 0;
    uint64_t clk_end = 0;

    //VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    while(0)
    {
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);
        csk_delay_ms(1000);

        for(i = 0; i < sizeof(data_src); i++)
        {
            data_src[i] = rand() & 0xFF;
        }

        clk_start = __get_rv_cycle();
        memcpy(data_dts, data_src, sizeof(data_src));
        clk_end = __get_rv_cycle();
        VIDEO_LOG("[%s:%d] clk_diff=%lld", __func__, __LINE__, clk_end - clk_start);

        for(i = 0; i < sizeof(data_src); i++)
        {
            if(data_dts[i] != data_src[i])
            {
                VIDEO_LOG("[%s:%d] i=%d", __func__, __LINE__, i);
            }
        }
    }


    uint8_t *psrc = (uint8_t *)0x20080000;
    uint8_t *pdst = (uint8_t *)0x20080001;
    uint8_t *_psrc = NULL;
    uint8_t *_pdst = NULL;

    while(1)
    {
        //VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, cnt++);
        csk_delay_ms(1000);

        for(i = 0; i < 4800; i++)
        {
            //psrc[i] = rand() & 0xFF;
            psrc[i] = i & 0xFF;
        }

        clk_start = __get_rv_cycle();
#if 1
        memcpy(pdst, psrc, 4800);
#else
        _psrc = psrc;
        _pdst = pdst;
        i = 4800;
        while(i--)
        {
            *_pdst++ = *_psrc++;
        }
#endif
        clk_end = __get_rv_cycle();
        VIDEO_LOG("[%s:%d] clk_diff=%lld", __func__, __LINE__, clk_end - clk_start);

        for(i = 0; i < 4800; i++)
        {
            if(pdst[i] != psrc[i])
            {
                //VIDEO_LOG("[%s:%d] i=%d", __func__, __LINE__, i);
                //VIDEO_LOG("pdst[i]=%d psrc[i]=%d", pdst[i], psrc[i]);
            }
        }
    }

    while(1);
}


void test_psram(void)
{
    uint32_t cnt = 0;
    uint32_t* wr_address = (uint32_t*)PSRAM_BASE_ADDRESS;
    uint32_t* rd_address = (uint32_t*)PSRAM_BASE_ADDRESS;
    uint32_t wr_data = 0;
    uint32_t rd_data = 0;

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    dma_initialize();
    PSRAM_Initialize(NULL, NULL, 1);

    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    while(1)
    {
        *wr_address = cnt++;
        rd_data = *rd_address;
        VIDEO_LOG("[%s:%d] test %d", __func__, __LINE__, rd_data);
        DELAY_MS(1000);
    }
}

void test_cp_run_in_flash(void)
{
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    DELAY_MS(100);

    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30010000;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
    __WFI();
    while(1);
}

void test_img_converter(void)
{
    uint16_t rgb565[64 * 64] = {0};
    uint8_t rgb888[64 * 64 * 3] = {0};
    uint8_t yuv444[64 * 64 * 3] = {0};
    uint8_t yuv422[64 * 64 * 2] = {0};
    uint32_t pixel = sizeof(rgb565) / sizeof(uint16_t);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    rgb565_colorbar_create(rgb565, 64, 64, 10);
    //rgb565_grid_create(rgb565, 64, 64, 20);
    //rgb565_to_bgr565(rgb565, pixel);

    rgb565_to_rgb888(rgb565, rgb888, pixel);
    //rgb888_to_yuv422uyvy(rgb888, yuv422, pixel);
    rgb888_to_yuv444yuv(rgb888, yuv444, pixel);
    yuv444yuv_to_yuv422yuyv(yuv444, yuv422, pixel);

    VIDEO_LOG("rgb565: addr=0x%08x, size=0x%x Byte", rgb565, sizeof(rgb565));
    VIDEO_LOG("rgb888: addr=0x%08x, size=0x%x Byte", rgb888, sizeof(rgb888));
    VIDEO_LOG("yuv444: addr=0x%08x, size=0x%x Byte", yuv444, sizeof(yuv444));
    VIDEO_LOG("yuv422: addr=0x%08x, size=0x%x Byte", yuv422, sizeof(yuv422));

    /* J-Link cmmmander : savebin D:\picture\colorbar_bgr565_320x240_create.bin 0x2001a7e0 0x25800 */
}


/*
little endian
data *0x000cfff4=0x87654321
ch[0] *0x000cfff4=0x21
ch[1] *0x000cfff5=0x43
ch[2] *0x000cfff6=0x65
ch[3] *0x000cfff7=0x87
*/
void test_big_little_endian(void)
{
    unsigned int data = 0x87654321;
    char *ch = (char *)&data;
    char i = 0;

    if(*ch == (data & 0xff)) {
        VIDEO_LOG("little endian", i, ch, *ch);
    } else {
        VIDEO_LOG("big endian", i, ch, *ch);
    }

    VIDEO_LOG("data *0x%08x=0x%08x", &data, data);
    for(i = 0; i < 4; i++)
    {
        VIDEO_LOG("ch[%d] *0x%08x=0x%02x", i, ch, *ch);
        ch++;
    }
}


/*
J-Link Commander:
    mem32 0x000cfff4 1
    w4 0x000cfff4 0x100
 */
void test_jlink(void)
{
    volatile unsigned int data = 0;

    //SCB_DisableDCache();

    while(1)
    {
        VIDEO_LOG("data *0x%08x=0x%08x", &data, data);
        DELAY_MS(1000);
        data++;
    }
}


void test_timer(void)
{
    uint32_t cnt;

    //run_gpio_init();
    csk_timer_start();
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(0)
    {
        /* 0/1us/999us/1001us/1ms/1s/10s test pass */
        cnt = 1000;
        VIDEO_LOG("csk_sw_delay_us %d", cnt);

        //run_gpio_set();
        csk_sw_delay_us(cnt);
        //run_gpio_clr();
        csk_sw_delay_us(cnt);
    }

    while(0)
    {
        /* 0/1us/999us/1001us/1ms/1s/10s test pass */
        cnt = 1000;
        VIDEO_LOG("csk_delay_us %d", cnt);

        //run_gpio_set();
        csk_delay_us(cnt);
        //run_gpio_clr();
        csk_delay_us(cnt);
    }

    while(1)
    {
        /* 0/1ms/999ms/1001ms/1s/10s test pass */
        cnt = 1;
        VIDEO_LOG("csk_delay_ms %d", cnt);

        //run_gpio_set();
        csk_delay_ms(cnt);
        //run_gpio_clr();
        csk_delay_ms(cnt);
    }
}


static volatile uint32_t gpio_irq_cnt = 0;
static void gpio_callback(uint32_t event, void* workspace)
{
    gpio_irq_cnt++;
    //VIDEO_LOG("[%s:%d] event=0x%x", __func__, __LINE__, event);
}

void test_gpio_irq(void)
{
    void *gGpioADev = NULL;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, 0);  // D6   input
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 0);  // D7   output

    gGpioADev = GPIOA();
    GPIO_Initialize(gGpioADev, gpio_callback, NULL);
    GPIO_Control(gGpioADev, CSK_GPIO_MODE_PULL_NONE | \
                                CSK_GPIO_DEBOUNCE_DISABLE | \
                                CSK_GPIO_SET_INTR_POSITIVE_EDGE | \
                                CSK_GPIO_INTR_ENABLE, (1UL << 19));
    GPIO_SetDir(gGpioADev, (1UL << 19), CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(gGpioADev, (1UL << 20), CSK_GPIO_DIR_OUTPUT);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    enable_GINT();

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(1)
    {
        VIDEO_LOG("[%s:%d] gpio_irq_cnt=%d", __func__, __LINE__, gpio_irq_cnt);

        GPIO_PinWrite(gGpioADev, (1UL << 20), 0);
        DELAY_MS(100);
        GPIO_PinWrite(gGpioADev, (1UL << 20), 1);
        DELAY_MS(100);
    }
}


#define OV9655_SCCB_ADDR   0x30  // 0x60 >> 1

void test_i2c_detect(void)
{
    uint8_t i2c_index = 0;
    uint8_t value = 0;
    sw_i2c_port_callback_t i2c_port;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    i2c_port.gpio_init    = sw_dvp_i2c_gpio_init;
    i2c_port.scl_set      = sw_dvp_i2c_scl_gpio_set;
    i2c_port.scl_clr      = sw_dvp_i2c_scl_gpio_clr;
    i2c_port.sda_set      = sw_dvp_i2c_sda_gpio_set;
    i2c_port.sda_clr      = sw_dvp_i2c_sda_gpio_clr;
    i2c_port.sda_get      = sw_dvp_i2c_sda_gpio_get;
    i2c_port.sda_dirout   = sw_dvp_i2c_sda_gpio_setdir_output;
    i2c_port.sda_dirin    = sw_dvp_i2c_sda_gpio_setdir_input;
    sw_i2c_init(i2c_index, &i2c_port);

#if 0   //VENUS
    // PA11 configured to function 20 - CLOCK OUT
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 20);
    DVP_EnableClockout(DVP0(), 10000000);
#else   //ARCS_FPGA
    // PA12 configured to function 16 - CLOCK OUT
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, CSK_IOMUX_FUNC_ALTER16);
    DVP_EnableClockout(DVP0(), 35000000);  // FPGA 10MHz->1.6MHz   70MHz->12MHz
#endif

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

#if 0
    /* GPIO init */
    static void *gGpioADev = NULL;
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6, 1); //FIXME:
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, 1); //FIXME:
    gGpioADev = GPIOA();
    GPIO_Initialize(gGpioADev, NULL, NULL);
    GPIO_SetDir(gGpioADev, (1UL << 6) | (1UL << 7), CSK_GPIO_DIR_OUTPUT);

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    while(0)
    {
        VIDEO_LOG("[%s:%d] cnt=%d", __func__, __LINE__, value++);
        GPIO_PinWrite(gGpioADev, (1UL << 6), 0);
        GPIO_PinWrite(gGpioADev, (1UL << 7), 0);
        DELAY_MS(10);
        GPIO_PinWrite(gGpioADev, (1UL << 6), 1);
        GPIO_PinWrite(gGpioADev, (1UL << 7), 1);
        DELAY_MS(10);

        if(value >= 10)
        {
            value = 0;
            break;
        }
    }

    //GPIO_SetDir(gGpioADev, (1UL << USER_I2C_SDA_PIN), CSK_GPIO_DIR_INPUT);
    while(0)
    {
        VIDEO_LOG("[%s:%d] SDA=%d", __func__, __LINE__, GPIO_PinRead(gGpioADev, (1UL << USER_I2C_SDA_PIN)));
        DELAY_MS(100);
    }
#endif

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    /* GC0328 */
    while(0)                // test OK
    {
        value = sw_i2c_read_reg8(i2c_index, 0x21, 0xF0);
        VIDEO_LOG("[%s:%d] GC0328 CHIP ID = 0x%X  (0x9D)", __func__, __LINE__, value);
        DELAY_MS(10);
    }
    while(0)
    {
        value = sw_i2c_read_reg8(i2c_index, 0x21, 0xF0);
        VIDEO_LOG("[%s:%d] GC0328 CHIP ID = 0x%X  (0x9D)", __func__, __LINE__, value);
        DELAY_MS(10);
    }


    /* OV9655 */
    while(0)
    {
        value = sw_i2c_read_reg8(i2c_index, 0x30, 0x0A);
        VIDEO_LOG("[%s:%d] OV9255 CHIP ID high8bit = 0x%X", __func__, __LINE__, value);    // 0x96
        //value = CSK_I2C_READ_REG8(i2c_index, OV9655_SCCB_ADDR, 0x0B);
        //VIDEO_LOG("[%s:%d] OV9255 CHIP ID low8bit = 0x%X", __func__, __LINE__, value);     // 0x56/0x57
        DELAY_MS(10);
    }
    while(1)
    {
        DELAY_MS(10);
        value = sw_i2c_read_reg8(i2c_index, OV9655_SCCB_ADDR, 0x0A);
        VIDEO_LOG("[%s:%d] OV9255 CHIP ID high8bit = 0x%X", __func__, __LINE__, value);    // 0x96
        //value = CSK_I2C_READ_REG8(i2c_index, OV9655_SCCB_ADDR, 0x0B);
        //VIDEO_LOG("[%s:%d] OV9255 CHIP ID low8bit = 0x%X", __func__, __LINE__, value);     // 0x56/0x57
        DELAY_MS(10);
    }

    /* touch FT5336 */
    while(0)                // test ok
    {
        value = sw_i2c_read_reg8(i2c_index, ((uint8_t)0x70 >> 1), 0xA6);
        VIDEO_LOG("[%s:%d] FT5336 FIRM ID *0xA6=0x%X", __func__, __LINE__, value);    // 0x02

        value = sw_i2c_read_reg8(i2c_index, ((uint8_t)0x70 >> 1), 0xA8);
        VIDEO_LOG("[%s:%d] FT5336 CHIP ID *0xA8=0x%X", __func__, __LINE__, value);    // 0x11

        sw_i2c_write_reg8(i2c_index, ((uint8_t)0x70 >> 1), 0xA4, 0x00);
        value = sw_i2c_read_reg8(i2c_index, ((uint8_t)0x70 >> 1), 0xA4);
        VIDEO_LOG("[%s:%d] FT5336 *0xA4=0x%X", __func__, __LINE__, value);           // 0x00

        sw_i2c_write_reg8(i2c_index, ((uint8_t)0x70 >> 1), 0xA4, 0x01);
        value = sw_i2c_read_reg8(i2c_index, ((uint8_t)0x70 >> 1), 0xA4);
        VIDEO_LOG("[%s:%d] FT5336 *0xA4=0x%X", __func__, __LINE__, value);           // 0x01

        DELAY_MS(10);
    }
    while(0)
    {
        DELAY_MS(10);
        value = sw_i2c_read_reg8(i2c_index, ((uint8_t)0x70 >> 1), 0xA8);
        VIDEO_LOG("[%s:%d] FT5336 FIRMID = 0x%X", __func__, __LINE__, value);    // 0x51
        DELAY_MS(10);
    }


    while(0)
    {
        value = csk_i2c_detect(i2c_index);
        VIDEO_LOG("[%s:%d] cnt=%d", __func__, __LINE__, value);
        DELAY_MS(100);
    }

    /* Initialize I2C */
    value = sw_i2c_read_reg8(i2c_index, 0x21, 0xF0);
    VIDEO_LOG("[%s:%d] GC0328 CHIP ID = 0x%X  (0x9D)", __func__, __LINE__, value);
    DELAY_MS(10);

    value = sw_i2c_read_reg8(i2c_index, 0x38, 0xA8);
    VIDEO_LOG("[%s:%d] FT5336 CHIP ID = 0x%X  (0x11)", __func__, __LINE__, value);
    DELAY_MS(10);

    value = sw_i2c_read_reg8(i2c_index, 0x6C, 0x00);
    VIDEO_LOG("[%s:%d] CH32V003 value = 0x%X", __func__, __LINE__, value);
    DELAY_MS(10);

    while(1);
}


#include "uart_reg.h"

static void uart_tx(const char *data, uint32_t len)
{
#if 1
    UART_RegDef* UART_Reg = (UART_RegDef*)UART0_BASE;

    while (len--)
    {
        while(!UART_Reg->REG_STATUS.bit.TX_FIFO_SPACE);
        UART_Reg->REG_RXTX_BUFFER.bit.DATA = *data++;
    }
#else
    ME_UART_RegDef* UART_Reg = (ME_UART_RegDef*)UART0_BASE;

    while (len--)
    {
        while(!UART_Reg->status_bits.tx_fifo_space);
        UART_Reg->rxtxbuf_bits.data = *data++;
    }
#endif
}


void test_uart_tx(void)
{
    char data[] = "0123456789";

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    uart_tx(data, sizeof(data));
    VIDEO_LOG("\r\n");
    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
}


void test_usb_hshostdisc(void)
{
    unsigned int cnt = 0;
    void *gGpioADev = NULL;
    unsigned char state0 = 0;
    unsigned char state1 = 0;

    SEGGER_RTT_printf(0, "[%s:%d] \n", __func__, __LINE__);
    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, 0);   // GPIOA_10 connect to GPIOA_04
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 0);   // GPIOA_11 connect to GPIOA_06
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, 0);   // GPIOA_12 connect to GPIOA_02
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, 0);   // GPIOA_13 connect to GPIOB_06
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, 0);   // GPIOA_14

    gGpioADev = GPIOA();
    GPIO_Initialize(gGpioADev, NULL, NULL);
    GPIO_SetDir(gGpioADev, (1UL << 10) | (1UL << 11) | (1UL << 12) | (1UL << 14), CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(gGpioADev, (1UL << 13), CSK_GPIO_DIR_INPUT);
    GPIO_PinWrite(gGpioADev, (1UL << 10), 1);
    GPIO_PinWrite(gGpioADev, (1UL << 11), 1);
    GPIO_PinWrite(gGpioADev, (1UL << 12), 0);
    GPIO_PinWrite(gGpioADev, (1UL << 14), 0);

    dvp_reset();
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, 16);  // vic_clk_out connect to GPIOA_07
    DVP_EnableClockout(DVP0(), 10000000);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 26);  // GPIOA_03  usb_vcontrol0  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, 26);  // GPIOA_04  usb_vcontrol1  = 1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, 26);  // GPIOA_05  usb_vcontrol2  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, 26);  // GPIOA_06  usb_vcontrol3  = 1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  2, 26);  // GPIOA_02  usb_ponrst
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, 26);  // GPIOA_07  usb_coreclkin
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  6, 26);  // GPIOB_06  usb_bist_ok

    IP_SYSCTRL->REG_USB_CTRL1.all = 0x158100;

    state0 = GPIO_PinRead(gGpioADev, (1UL << 13));  // read usb_bist_ok
    SEGGER_RTT_printf(0, "usb_bist_ok=%d \n", state0);

    GPIO_PinWrite(gGpioADev, (1UL << 12), 0);  // rst
    DELAY_US(1);
    GPIO_PinWrite(gGpioADev, (1UL << 12), 1);

    GPIO_PinWrite(gGpioADev, (1UL << 14), 1);  // test start flag

    while(1)
    {
        state1 = GPIO_PinRead(gGpioADev, (1UL << 13));
        if(state1)
        {
            break;
        }

        cnt++;
        //DELAY_US(1);
    }

    GPIO_PinWrite(gGpioADev, (1UL << 14), 0);  // test end flag

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 2);  // UART0 TX  // unconnect to GND
    while(1)
    {
        DELAY_MS(1000);
        SEGGER_RTT_printf(0, "cnt=%d usb_bist_ok=%d \n", cnt, GPIO_PinRead(gGpioADev, (1UL << 13)));
        VIDEO_LOG("cnt=%d state0=%d state1=%d", cnt, state0, state1);
    }
}


/* DM and DM 15K¦¸ pull-down */
void test_usb_lsbist(void)
{
    unsigned int cnt = 0;
    void *gGpioDev = NULL;
    unsigned char state0 = 0;
    unsigned char state1 = 0;

    SEGGER_RTT_printf(0, "[%s:%d] \n", __func__, __LINE__);
    //VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    dvp_reset();

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 0);   // GPIOB_07 connect to GPIOB_06  usb_bist_ok
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 8, 0);   // GPIOB_08 connect to GPIOA_02  usb_ponrst

    gGpioDev = GPIOB();
    GPIO_Initialize(gGpioDev, NULL, NULL);
    GPIO_SetDir(gGpioDev, (1UL << 7), CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(gGpioDev, (1UL << 8), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gGpioDev, (1UL << 8), 1);
    DELAY_MS(1000);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  4, 16);  // vic_clk_out connect to GPIOA_07
    DVP_EnableClockout(DVP0(), 10000000);
    DELAY_MS(100);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 26);  // GPIOA_03  usb_vcontrol0  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, 26);  // GPIOA_04  usb_vcontrol1  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, 26);  // GPIOA_05  usb_vcontrol2  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, 26);  // GPIOA_06  usb_vcontrol3  = 1  connect to 3V3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  2, 26);  // GPIOA_02  usb_ponrst
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, 26);  // GPIOA_07  usb_coreclkin
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  6, 26);  // GPIOB_06  usb_bist_ok
    DELAY_MS(100);

    IP_SYSCTRL->REG_USB_CTRL1.all = 0x158110;  //bit4 USBPHY_LS_EN = 1;
    DELAY_MS(100);

    state0 = GPIO_PinRead(gGpioDev, (1UL << 7));  // read usb_bist_ok
    SEGGER_RTT_printf(0, "usb_bist_ok=%d \n", state0);

    GPIO_PinWrite(gGpioDev, (1UL << 8), 0);  // rst
    DELAY_US(1);
    GPIO_PinWrite(gGpioDev, (1UL << 8), 1);

    while(1)
    {
        state1 = GPIO_PinRead(gGpioDev, (1UL << 7));
        if(state1)
        {
            break;
        }

        cnt++;
        //DELAY_US(1);
    }

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 2);  // UART0 TX  // unconnect to GND
    while(1)
    {
        DELAY_MS(1000);
        SEGGER_RTT_printf(0, "cnt=%d state0=%d state1=%d usb_bist_ok=%d \n", cnt, state0, state1, GPIO_PinRead(gGpioDev, (1UL << 7)));
        VIDEO_LOG("cnt=%d state0=%d state1=%d usb_bist_ok=%d ", cnt, state0, state1, GPIO_PinRead(gGpioDev, (1UL << 7)));
    }
}


/* DM and DM 15K¦¸ pull-down */
void test_usb_fsbist(void)
{
    unsigned int cnt = 0;
    void *gGpioDev = NULL;
    unsigned char state0 = 0;
    unsigned char state1 = 0;

    SEGGER_RTT_printf(0, "[%s:%d] \n", __func__, __LINE__);
    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    mmio_write32(0x46000000,0x1200001b);//utmi_clk   GPIOA26
    //mmio_write32(0x47500068,0x12000012);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 18);

    while(0)
    {
        DELAY_MS(1000);
        VIDEO_LOG("[%s:%d] ", __func__, __LINE__);
    }

    dvp_reset();

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 0);   // GPIOB_07 connect to GPIOB_06  usb_bist_ok
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 8, 0);   // GPIOB_08 connect to GPIOA_02  usb_ponrst

    gGpioDev = GPIOB();
    GPIO_Initialize(gGpioDev, NULL, NULL);
    GPIO_SetDir(gGpioDev, (1UL << 7), CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(gGpioDev, (1UL << 8), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gGpioDev, (1UL << 8), 1);
    DELAY_MS(1000);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, 16);  // vic_clk_out connect to GPIOA_07
    DVP_EnableClockout(DVP0(), 10000000);
    DELAY_MS(100);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 26);  // GPIOA_03  usb_vcontrol0  = 1  connect to 3V3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, 26);  // GPIOA_04  usb_vcontrol1  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, 26);  // GPIOA_05  usb_vcontrol2  = 1  connect to 3V3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, 26);  // GPIOA_06  usb_vcontrol3  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  2, 26);  // GPIOA_02  usb_ponrst
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, 26);  // GPIOA_07  usb_coreclkin
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  6, 26);  // GPIOB_06  usb_bist_ok
    DELAY_MS(100);

    IP_SYSCTRL->REG_USB_CTRL1.all = 0x158100;
    DELAY_MS(100);

    state0 = GPIO_PinRead(gGpioDev, (1UL << 7));  // read usb_bist_ok
    SEGGER_RTT_printf(0, "usb_bist_ok=%d \n", state0);

    GPIO_PinWrite(gGpioDev, (1UL << 8), 0);  // rst
    DELAY_US(10);
    GPIO_PinWrite(gGpioDev, (1UL << 8), 1);

    while(1)
    {
        state1 = GPIO_PinRead(gGpioDev, (1UL << 7));
        if(state1)
        {
            break;
        }

        cnt++;
        //DELAY_US(1);
    }

    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 2);  // UART0 TX  // unconnect to GND
    while(1)
    {
        DELAY_MS(1000);
        SEGGER_RTT_printf(0, "cnt=%d state0=%d state1=%d usb_bist_ok=%d \n", cnt, state0, state1, GPIO_PinRead(gGpioDev, (1UL << 7)));
        //VIDEO_LOG("cnt=%d state0=%d state1=%d usb_bist_ok=%d ", cnt, state0, state1, GPIO_PinRead(gGpioDev, (1UL << 7)));

        if(GPIO_PinRead(gGpioDev, (1UL << 7))) {
            DELAY_US(1);
        } else {
            DELAY_US(10);
        }
    }
}


/* DM and DM 45¦¸ pull-down */
/*
 * usb_vcontrol0 :  GPIOA_03  <-->  3V3
 * usb_vcontrol1 :  GPIOA_04  <-->  GND
 * usb_vcontrol2 :  GPIOA_05  <-->  GND
 * usb_vcontrol3 :  GPIOA_06  <-->  GND
 * usb_ponrst    :  GPIOA_02  <-->  GPIOB_08
 * usb_coreclkin :  GPIOA_07  <-->  GPIOA_26
 * usb_bist_ok   :  GPIOB_06  <-->  GPIOB_07
 */
void test_usb_hsbist(void)
{
    unsigned int cnt = 0;
    void *gGpioDev = NULL;
    unsigned char state0 = 0;
    unsigned char state1 = 0;

    //SEGGER_RTT_printf(0, "[%s:%d] \n", __func__, __LINE__);
    //VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    /* DVP clk enable */
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;    // bit21
    IP_AP_CFG->REG_DMA_SEL.bit.SEL_DVP_QSPI = 1;      // bit1  0:qspi_in  1:dvp
    IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 1;        // bit9

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 0);   // GPIOB_07 connect to GPIOB_06  usb_bist_ok
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 8, 0);   // GPIOB_08 connect to GPIOA_02  usb_ponrst

    gGpioDev = GPIOB();
    GPIO_Initialize(gGpioDev, NULL, NULL);
    GPIO_SetDir(gGpioDev, (1UL << 7), CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(gGpioDev, (1UL << 8), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gGpioDev, (1UL << 8), 1);
    DELAY_MS(1000);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  26, 16);  // vic_clk_out connect to GPIOA_07
    DVP_EnableClockout(DVP0(), 10000000);
    DELAY_MS(100);

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 26);  // GPIOA_03  usb_vcontrol0  = 1  connect to 3V3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  4, 26);  // GPIOA_04  usb_vcontrol1  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  5, 26);  // GPIOA_05  usb_vcontrol2  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  6, 26);  // GPIOA_06  usb_vcontrol3  = 0  connect to GND
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  2, 26);  // GPIOA_02  usb_ponrst
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  7, 26);  // GPIOA_07  usb_coreclkin
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  6, 26);  // GPIOB_06  usb_bist_ok
    DELAY_MS(100);

    IP_SYSCTRL->REG_USB_CTRL1.all = 0x158000;
    DELAY_MS(100);

    state0 = GPIO_PinRead(gGpioDev, (1UL << 7));  // read usb_bist_ok
    //SEGGER_RTT_printf(0, "usb_bist_ok=%d \n", state0);

    GPIO_PinWrite(gGpioDev, (1UL << 8), 0);  // rst
    DELAY_MS(1);
    GPIO_PinWrite(gGpioDev, (1UL << 8), 1);

    while(1)
    {
        state1 = GPIO_PinRead(gGpioDev, (1UL << 7));
        if(state1)
        {
            break;
        }

        cnt++;
        //DELAY_US(1);
    }

    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A,  3, 2);  // UART0 TX  // unconnect to GND
    while(1)
    {
        DELAY_MS(1000);
        //SEGGER_RTT_printf(0, "cnt=%d state0=%d state1=%d usb_bist_ok=%d \n", cnt, state0, state1, GPIO_PinRead(gGpioDev, (1UL << 7)));
        //VIDEO_LOG("cnt=%d state0=%d state1=%d usb_bist_ok=%d ", cnt, state0, state1, GPIO_PinRead(gGpioDev, (1UL << 7)));

        if(GPIO_PinRead(gGpioDev, (1UL << 7))) {
            DELAY_US(1);
        } else {
            DELAY_US(10);
        }
    }
}



