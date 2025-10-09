#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chip.h"
#include "log_print.h"
#include "PSRAMManager.h"
#include "nmsis_core.h"

#include "test_case.h"
#include "csk_driver.h"
#include "config.h"

#define HOUR    ((__TIME__[0]-'0')*10+(__TIME__[1]-'0'))
#define MINUTE  ((__TIME__[3]-'0')*10+(__TIME__[4]-'0'))
#define SECOND  ((__TIME__[6]-'0')*10+(__TIME__[7]-'0'))

extern void BootClock_Init();

int main()
{
    //BootClock_Init();

#if ICACHE_ENABLE
    EnableICache();
#else
    DisableDCache();
#endif

#if DCACHE_ENABLE
    EnableDCache();
#else
    DisableDCache();
#endif
    __RWMB();
    __FENCE_I();

#if UART_ENABLE
    logInit(0, 115200); // uart0, baudrate=115200
#endif

    //csk_timer_start();
    VIDEO_LOG("[%s:%d] %s %s", __func__, __LINE__, __DATE__, __TIME__);

    /* Set random seed using current time's hour, minute, and second values */
    srand((HOUR * 3600) + (MINUTE * 60) + SECOND);

    gpio_init();

    PSRAM_Initialize(NULL, NULL, 1);        // PSRAM_BASE_ADDRESS

    test_dvp();
    //test_rgb();
    //test_qspi_in();
    //test_qspi_out();
    //test_blender();
    //test_jpeg();
    //test_gpdma2d();
    //test_gpdma_cpdma();
    //test_2d();
    //test_pipe();
    //test_lvgl_dma2d_port();
    //test_lvgl_dma2d_port2();
    //test_lvgl_dma2d_port3();
    //test_rgb888_rgb565();

    //test_uart();
    //test_sw_uart();
    //test_big_little_endian();
    //test_jlink();
    //test_timer();
    //test_i2c_detect();
    //test_uart_tx();
    //test_gpio_irq();
    //test_img_converter();
    //test_gpdma_regctrl();
    //test_psram();
    //test_segger_rtt();
    //test_cp_run_in_flash();
    //test_memcpy();
    //test_usb_bist();
    //test_usb_lsbist();
    //test_usb_fsbist();
    //test_usb_hsbist();

    //test_dvp_xclk();
    //test_dvp_api();
    //test_camera_i2c();
    //test_camera_dvp_pin();
    //test_camera_dvp_vs_hs();
    //test_ov5640_jpeg();
    //test_camera_dvp();

    //test_camera_spi_pin();
    //test_camera_qspi_waveform();
    //test_camera_spi0_rx();
    //test_camera_qspi_in();

    //test_lcd_vs_hs();
    //test_lcd_color();
    //test_lcd_display();

    //test_lvgl_text();
    //test_lvgl_demos();
    //test_touch_i2c();
    //test_touch();
    //test_lcd_touch();

    while(1);

    return 0;
}



