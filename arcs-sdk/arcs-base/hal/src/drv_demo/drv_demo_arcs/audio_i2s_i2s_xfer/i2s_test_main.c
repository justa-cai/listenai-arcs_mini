/*
 * main.h
 *
 *  Created on: Apr. 9, 2024 for MARS/ARCS
 *      Author: bauldeng
 */

#include "main.h"
#include "core_iomux_reg.h"
#include "ClockManager.h"

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
//#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#define HEXP                hexPrint
#else
#define LOGD(format, ...)   ((void)0)
#define HEXP(...)           ((void)0)
#endif // DEBUG_LOG

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof(a[0]))
#endif

#define LOG_SPLIT_LINE1()  \
    LOGD("\r\n==============================================================")

#define LOG_SPLIT_LINE2()  \
    LOGD("==============================================================\r\n")

//------------------------------------------------------------------
static void hexPrint(const void *buf, uint32_t len)
{
    int i = 0;
    const uint8_t *data = (const uint8_t *)buf;
    while (i < len) {
        logDbg("%02x ", data[i]);
        i++;
        if (i % 16 == 0) {
            logDbg("\r\n");
        } else if (i % 4 == 0) {
            logDbg(" ");
        }
    }
    logDbg("\r\n\r\n");
}


I2S_TCASE_CONFIG g_case_cfg[] = {
// lr_bmp, alt_apc_ch, use_tdm, ch_cnt, ch_bits, i2s_prt, SampFreq, bck_slave, req_cnt
/**/
    { 0x3, 0, 0, 2, 32, I2S_PROTO_PHILIPS, 16000, 0, 256, "Standard, 2ch,32bit,16K" },
    { 0x3, 0, 0, 2, 32, I2S_PROTO_PHILIPS, 48000, 0, 256, "Standard, 2ch,32bit,48K" },
    { 0x3, 0, 0, 2, 24, I2S_PROTO_PHILIPS, 16000, 0, 256, "Standard, 2ch,24bit_H,16K" },
    { 0x3, 0, 0, 2, 24, I2S_PROTO_PHILIPS, 16000, 0, 4, "Standard, 2ch,24bit_H,16K, 4Samps" },
    { 0x3, 0, 0, 2, 24, I2S_PROTO_PHILIPS, 48000, 0, 256, "Standard, 2ch,24bit_H,48K" },
    { 0x3, 0, 0, 2, -24, I2S_PROTO_PHILIPS, 16000, 0, 256, "Standard, 2ch,24bit_L,16K" },
    { 0x3, 0, 0, 2, -24, I2S_PROTO_PHILIPS, 16000, 0, 4, "Standard, 2ch,24bit_L,16K, 4Samps" },
    { 0x3, 0, 0, 2, -24, I2S_PROTO_PHILIPS, 48000, 0, 256, "Standard, 2ch,24bit_L,48K" },
    { 0x3, 0, 0, 2, 20, I2S_PROTO_PHILIPS, 16000, 0, 256, "Standard, 2ch,20bit_H,16K" },
    { 0x3, 0, 0, 2, 20, I2S_PROTO_PHILIPS, 48000, 0, 256, "Standard, 2ch,20bit_H,48K" },
    { 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 16000, 0, 256, "Standard, 2ch,dual_16bit,16K" },
    { 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 16000, 0, 10, "Standard, 2ch,dual_16bit,16K, 10Samps" },
    { 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 48000, 0, 256, "Standard, 2ch,dual_16bit,48K" },

    { 0x3, 0, 0, 2, 16, I2S_PROTO_LEFT, 16000, 0, 256, "Left, 2ch,dual_16bit,16K" },
    { 0x3, 0, 0, 2, 16, I2S_PROTO_LEFT, 48000, 0, 256, "Left, 2ch,dual_16bit,48K" },

    //BSD NOTE: For RJ, Slave TX & Master RX can PASS, Slave RX & Master TX FAILED!!
    { 0x3, 0, 0, 2, 16, I2S_PROTO_RIGHT, 16000, 800000, 256, "Right, 2ch,dual_16bit,16K" }, //bck_slv = 800K
    //{ 0x3, 0, 0, 2, 16, I2S_PROTO_RIGHT, 16000, 960000, 256, "Right, 2ch,dual_16bit,16K" }, //bck_slv = 960K
    { 0x3, 0, 0, 2, 16, I2S_PROTO_RIGHT, 48000, 2400000, 256, "Right, 2ch,dual_16bit,48K" }, //bck_slv = 2.4M
    //{ 0x3, 0, 0, 2, 16, I2S_PROTO_RIGHT, 48000, 4800000, 256, "Right, 2ch,dual_16bit,48K" }, //bck_slv = 4.8M

    { 0x3, 0, 1, 2, 16, I2S_PROTO_PCMMODE_0, 16000, 0, 4, "PCM 0, 2ch,dual_16bit,16K, 4Samps" },
    { 0x3, 0, 1, 2, 16, I2S_PROTO_PCMMODE_0, 16000, 0, 256, "PCM 0, 2ch,dual_16bit,16K" },
    { 0x3, 0, 1, 2, 16, I2S_PROTO_PCMMODE_0, 48000, 0, 256, "PCM 0, 2ch,dual_16bit,48K" },
    { 0x3, 0, 1, 4, 16, I2S_PROTO_PCMMODE_0, 16000, 0, 256, "PCM 0, 4ch,dual_16bit,16K" },
    { 0x3, 0, 1, 4, 16, I2S_PROTO_PCMMODE_0, 48000, 0, 256, "PCM 0, 4ch,dual_16bit,48K" },
    { 0x3, 0, 1, 4, 16, I2S_PROTO_PCMMODE_1, 16000, 0, 256, "PCM 1, 4ch,dual_16bit,16K" },
    { 0x3, 0, 1, 4, 16, I2S_PROTO_PCMMODE_1, 48000, 0, 256, "PCM 1, 4ch,dual_16bit,48K" },

    { 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 8000, 0, 256, "Standard, 2ch,dual_16bit,8K" }, //OK
    { 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 24000, 0, 256, "Standard, 2ch,dual_16bit,24K" }, //OK
    //BSD NOTE: The following SR (32K/96K/192K) CANNOT be generated via frequency division from 24MHz!!
    //{ 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 32000, 0, 256, "Standard, 2ch,dual_16bit,32K" }, //NOK, BYPASS
    //{ 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 96000, 0, 256, "Standard, 2ch,dual_16bit,96K" }, //NOK, BYPASS
    //{ 0x3, 0, 0, 2, 16, I2S_PROTO_PHILIPS, 192000, 0, 256, "Standard, 2ch,dual_16bit,192K" }, //NOK, BYPASS

    { 0x1, 0, 0, 1, 16, I2S_PROTO_PHILIPS, 16000, 0, 256, "Standard, 1ch_L,dual_16bit,16K" }, //OK
    { 0x1, 0, 0, 1, 16, I2S_PROTO_PHILIPS, 48000, 0, 256, "Standard, 1ch_L,dual_16bit,48K" }, //OK
    //{ 0x1, 0, 0, 1, 16, I2S_PROTO_PHILIPS, 48000, 0, 4, "Standard, 1ch_L,dual_16bit,48K, 4Samps" }, //OK
    { 0x2, 0, 0, 1, 16, I2S_PROTO_PHILIPS, 16000, 0, 256, "Standard, 1ch_R,dual_16bit,16K" }, //OK
    { 0x2, 0, 0, 1, 16, I2S_PROTO_PHILIPS, 48000, 0, 256, "Standard, 1ch_R,dual_16bit,48K" }, //OK

//TODO: any legal settings of I2S configuration ...

};

uint32_t TC_total = 0, TC_pass = 0, TC_fail = 0;

I2S_TEST_CASE g_cur_tcase = { 0 };

//------------------------------------------------------------------

/*
void __log_io_config(int dbg){
#define UART0_TX_IO_MUX_FUNC_SEL               (0x2)
#define UART1_TX_IO_MUX_FUNC_SEL               (0x3)

    switch (dbg){
    case 0:
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, UART0_TX_IO_MUX_FUNC_SEL); // A04
        break;
    case 1:
    default:
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, UART1_TX_IO_MUX_FUNC_SEL); // A05
        break;
    }
}
*/

static uint8_t i2s_channel_bytes(int32_t ch_bits)
{
    if (ch_bits < 0)
        //ch_bits += 256;
        ch_bits = 0 - ch_bits;

    if (ch_bits == 20 || ch_bits == 24)
        ch_bits = 32;
    return (ch_bits + 7) >> 3; // /8
}

static void record_result(bool expected, bool returned, uint32_t case_idx, const char *tag)
{
    bool OK = (returned == expected);
    const char * hint = g_case_cfg[case_idx].hint;

    TC_total++;
    if (OK) {
        TC_pass++;
        LOGD("case %d PASSED: %s, %s\r\n", case_idx, tag, hint);
    } else {
        TC_fail++;
        LOGD("case %d FAILED: %s, %s\r\n\r\n", case_idx, tag, hint);
    }

}

#if USE_ONE_I2S_PER_CHIP // 1 chip

// return false if error occurs and the test should exit
static bool test_i2s_slave_rx()
{
    bool ret = true;
    void *i2s_dev = NULL;
    uint32_t i;

    g_cur_tcase.test_type = I2S_TEST_SLAVE_RX;

    for (i = 0; i < ARRAY_SIZE(g_case_cfg); i++) {
        init_rx_buf();

        LOG_SPLIT_LINE1();
        LOGD("Slave RX case %d...", i);
        LOG_SPLIT_LINE2();
        i2s_dev = i2s_slave_rx_case(I2S_IDX0, true, &g_case_cfg[i]);
        assert(i2s_dev != NULL);
        g_cur_tcase.i2s_slv = i2s_dev;
        g_cur_tcase.i2s_mst = NULL;
        g_cur_tcase.tcase_cfg = &g_case_cfg[i];

        gpio_out_slv_notify();
        ret = WAIT_SLV_RX_DONE_TIMEOUT(500); //ms
        if (ret) { // RX done
            gpio_out_slv_restore();
            i2s_close(i2s_dev);
            if (is_rx_eq_tx(i2s_channel_bytes(g_case_cfg[i].ch_bits), g_case_cfg[i].ch_bits)) {
                LOGD("case %d: Slave RX PASSED!\r\n", i);
            } else {
                LOGD("case %d: Slave RX FAILED!\r\n\r\n", i);
            }
            // wait for 10 ms and then change to next test case
            nos_delay_ms(10); //ms

        } else { // RX timeout
            i2s_abort_rx(i2s_dev);
            i2s_close(i2s_dev);
            LOGD("case %d: Slave RX FAILED (TIMEOUT)!\r\n\r\n", i);
            break;
        }
    } // end for i

    return ret;
}


// return false if error occurs and the test should exit
static bool test_i2s_slave_tx()
{
    bool ret = true;
    uint8_t ch_bytes;
    void *i2s_dev = NULL;
    uint32_t i;

    g_cur_tcase.test_type = I2S_TEST_SLAVE_TX;

    for (i = 0; i < ARRAY_SIZE(g_case_cfg); i++) {
        ch_bytes = i2s_channel_bytes(g_case_cfg[i].ch_bits);
        init_tx_buf(TX_SAMP_CNT * ch_bytes);

        LOG_SPLIT_LINE1();
        LOGD("Slave TX case %d...", i);
        LOG_SPLIT_LINE2();
        i2s_dev = i2s_slave_tx_case(I2S_IDX0, true, &g_case_cfg[i]);
        assert(i2s_dev != NULL);
        g_cur_tcase.i2s_slv = i2s_dev;
        g_cur_tcase.i2s_mst = NULL;
        g_cur_tcase.tcase_cfg = &g_case_cfg[i];

        gpio_out_slv_notify();
        ret = WAIT_SLV_TX_DONE_TIMEOUT(500); //ms
        if (ret) { // TX done
            gpio_out_slv_restore();
            i2s_close(i2s_dev);
            LOGD("case %d: Slave TX PASSED (Data checked on Master)!\r\n", i);
            // wait for 10 ms and then change to next test case
            nos_delay_ms(10); //ms

        } else { // TX timeout
            i2s_abort_tx(i2s_dev);
            i2s_close(i2s_dev);
            LOGD("case %d: Slave TX FAILED (TIMEOUT)!\r\n\r\n", i);
            break;
        }
    } // end for i

    return ret;
}


// return false if error occurs and the test should exit
static bool test_i2s_master_rx()
{
    bool ret = true;
    void *i2s_dev = NULL;
    uint32_t i;

    g_cur_tcase.test_type = I2S_TEST_MASTER_RX;

    for (i = 0; i < ARRAY_SIZE(g_case_cfg); i++) {
        init_rx_buf();

        LOG_SPLIT_LINE1();
        LOGD("Master RX case %d...", i);
        LOG_SPLIT_LINE2();
        i2s_dev = i2s_master_rx_case(I2S_IDX0, false, &g_case_cfg[i]);
        assert(i2s_dev != NULL);
        g_cur_tcase.i2s_slv = NULL;
        g_cur_tcase.i2s_mst = i2s_dev;
        g_cur_tcase.tcase_cfg = &g_case_cfg[i];

        ret = gpio_int_mst_wait(500);
        gpio_int_mst_dis();
        if (!ret) {
            i2s_close(i2s_dev);
            LOGD("case %d: Master RX FAILED (TIMEOUT for GPIO notify)!\r\n\r\n", i);
            return ret;
        }

        ret = i2s_rx_case_start(i2s_dev, &g_case_cfg[i]);
        if (!ret) {
            i2s_close(i2s_dev);
            LOGD("case %d: Master RX FAILED (to start)!\r\n\r\n", i);
            return ret;
        }

        ret = WAIT_MST_RX_DONE_TIMEOUT(500); //ms
        if (ret) { // RX done
            i2s_close(i2s_dev);
            if (is_rx_eq_tx(i2s_channel_bytes(g_case_cfg[i].ch_bits), g_case_cfg[i].ch_bits)) {
                LOGD("case %d: Master RX PASSED!\r\n", i);
            } else {
                LOGD("case %d: Master RX FAILED!\r\n\r\n", i);
            }
            // wait for 10 ms and then change to next test case
            nos_delay_ms(10); //ms

        } else { // RX timeout
            i2s_abort_rx(i2s_dev);
            i2s_close(i2s_dev);
            LOGD("case %d: Master RX FAILED (TIMEOUT)!\r\n\r\n", i);
            break;
        }
    } // end for i

    return ret;
}


// return false if error occurs and the test should exit
static bool test_i2s_master_tx()
{
    bool ret = true;
    void *i2s_dev = NULL;
    uint8_t ch_bytes;
    uint32_t i;

    g_cur_tcase.test_type = I2S_TEST_MASTER_TX;

    for (i = 0; i < ARRAY_SIZE(g_case_cfg); i++) {
        ch_bytes = i2s_channel_bytes(g_case_cfg[i].ch_bits);
        init_tx_buf(TX_SAMP_CNT * ch_bytes);

        LOG_SPLIT_LINE1();
        LOGD("Master TX case %d...", i);
        LOG_SPLIT_LINE2();
        i2s_dev = i2s_master_tx_case(I2S_IDX0, false, &g_case_cfg[i]);
        assert(i2s_dev != NULL);
        g_cur_tcase.i2s_slv = NULL;
        g_cur_tcase.i2s_mst = i2s_dev;
        g_cur_tcase.tcase_cfg = &g_case_cfg[i];

        ret = gpio_int_mst_wait(500);
        gpio_int_mst_dis();
        if (!ret) {
            i2s_close(i2s_dev);
            LOGD("case %d: Master TX FAILED (TIMEOUT for GPIO notify)!\r\n\r\n", i);
            return ret;
        }

        ret = i2s_tx_case_start(i2s_dev, &g_case_cfg[i]);
        if (!ret) {
            i2s_close(i2s_dev);
            LOGD("case %d: Master TX FAILED (to start)!\r\n\r\n", i);
            return ret;
        }

        ret = WAIT_MST_TX_DONE_TIMEOUT(500); //ms
        if (ret) { // TX done
            i2s_close(i2s_dev);
            LOGD("case %d: Master TX PASSED (Data checked on Master)!\r\n", i);
            // wait for 10 ms and then change to next test case
            nos_delay_ms(10); //ms

        } else { // TX timeout
            i2s_abort_tx(i2s_dev);
            i2s_close(i2s_dev);
            LOGD("case %d: Master TX FAILED (TIMEOUT)!\r\n\r\n", i);
            break;
        }
    } // end for i

    return ret;
}

#else // 2 chip

#define TAG_SLV_RX_MST_TX   "Slave RX & Master TX"
#define TAG_SLV_RX_MST_TX_TO   TAG_SLV_RX_MST_TX" (TIMEOUT), Exit..."

// return false if error occurs and the test should exit
static bool test_i2s_slave_rx_master_tx()
{
    bool ret = true;
    uint8_t ch_bytes;
    void *i2s_slv = NULL, *i2s_mst = NULL;
    uint32_t i;

    g_cur_tcase.test_type = I2S_TEST_SLAVE_RX_MASTER_TX;

    for (i = 0; i < ARRAY_SIZE(g_case_cfg); i++) {
        ch_bytes = i2s_channel_bytes(g_case_cfg[i].ch_bits);
        init_rx_buf();
        init_tx_buf(TX_SAMP_CNT * ch_bytes);

        LOG_SPLIT_LINE1();
        LOGD(TAG_SLV_RX_MST_TX" case %d: %s", i, g_case_cfg[i].hint);
        LOG_SPLIT_LINE2();

        i2s_slv = i2s_slave_rx_case(I2S_IDX0, true, &g_case_cfg[i]);
        if(i2s_slv == NULL) {
            record_result(true, false, i, TAG_SLV_RX_MST_TX);
            continue;
        }

        nos_delay_ms(10); //ms

        i2s_mst = i2s_master_tx_case(I2S_IDX1, true, &g_case_cfg[i]);
        if(i2s_mst == NULL) {
            i2s_close(i2s_slv);
            record_result(true, false, i, TAG_SLV_RX_MST_TX);
            continue;
        }

        g_cur_tcase.i2s_slv = i2s_slv;
        g_cur_tcase.i2s_mst = i2s_mst;
        g_cur_tcase.tcase_cfg = &g_case_cfg[i];

        ret = WAIT_SLV_RX_MST_TX_DONE_TIMEOUT(1000); //ms
        if (ret) { // RX & TX done
            i2s_close(i2s_mst);
            i2s_close(i2s_slv);
            record_result(true, is_rx_eq_tx(ch_bytes, g_case_cfg[i].ch_bits), i, TAG_SLV_RX_MST_TX);

            // wait for 10 ms and then change to next test case
            nos_delay_ms(10); //ms

        } else { // RX & TX timeout
            i2s_abort_rx(i2s_mst);
            i2s_abort_rx(i2s_slv);
            i2s_close(i2s_mst);
            i2s_close(i2s_slv);
            record_result(true, ret, i, TAG_SLV_RX_MST_TX_TO);

            break;
        }
    } // end for i

    return ret;
}


#define TAG_SLV_TX_MST_RX   "Slave TX & Master RX"
#define TAG_SLV_TX_MST_RX_TO   TAG_SLV_TX_MST_RX" (TIMEOUT), Exit..."

// return false if error occurs and the test should exit
static bool test_i2s_slave_tx_master_rx()
{
    bool ret = true;
    uint8_t ch_bytes;
    void *i2s_slv = NULL, *i2s_mst = NULL;
    uint32_t i;

    g_cur_tcase.test_type = I2S_TEST_SLAVE_TX_MASTER_RX;

    for (i = 0; i < ARRAY_SIZE(g_case_cfg); i++) {
        ch_bytes = i2s_channel_bytes(g_case_cfg[i].ch_bits);
        init_rx_buf();
        init_tx_buf(TX_SAMP_CNT * ch_bytes);

        LOG_SPLIT_LINE1();
        LOGD(TAG_SLV_TX_MST_RX" case %d: %s", i, g_case_cfg[i].hint);
        LOG_SPLIT_LINE2();

        i2s_slv = i2s_slave_tx_case(I2S_IDX0, true, &g_case_cfg[i]);
        if(i2s_slv == NULL) {
            record_result(true, false, i, TAG_SLV_TX_MST_RX);
            continue;
        }

        nos_delay_ms(10); //ms

        i2s_mst = i2s_master_rx_case(I2S_IDX1, true, &g_case_cfg[i]);
        if(i2s_mst == NULL) {
            i2s_close(i2s_slv);
            record_result(true, false, i, TAG_SLV_TX_MST_RX);
            continue;
        }

        g_cur_tcase.i2s_slv = i2s_slv;
        g_cur_tcase.i2s_mst = i2s_mst;
        g_cur_tcase.tcase_cfg = &g_case_cfg[i];

        ret = WAIT_SLV_TX_MST_RX_DONE_TIMEOUT(1000); //ms
        if (ret) { // RX & TX done
            i2s_close(i2s_mst); // close i2 master first
            i2s_close(i2s_slv); // then close i2s slave

            record_result(true, is_rx_eq_tx(ch_bytes, g_case_cfg[i].ch_bits), i, TAG_SLV_TX_MST_RX);

            // wait for 10 ms and then change to next test case
            nos_delay_ms(10); //ms

        } else { // RX & TX timeout
            i2s_abort_rx(i2s_mst);
            i2s_abort_rx(i2s_slv);
            i2s_close(i2s_mst);
            i2s_close(i2s_slv);
            record_result(true, ret, i, TAG_SLV_TX_MST_RX_TO);
            break;
        }
    } // end for i

    return ret;
}

#endif // USE_ONE_I2S_PER_CHIP


int main()
{
    bool ret = true;
    uint32_t uart_idx = 0; //1;

    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 1); // enable JTAG TDI
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 1); // enable JTAG TDO

    logInit(uart_idx, 115200);
    LOGD("use UART%d as log print!\r\n", uart_idx);

//    IP_AP_CFG->REG_SW_RESET.bit.APC_RESET = 1;
//    IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 1;

//    // Disable D-Cache
//    DisableDCache();
//    __RWMB();
//    __FENCE_I();

    // enable global interrupts (all levels of interrupts)
    enable_GINT();

    nos_timer_init();
    TC_total = TC_pass = TC_fail = 0;

#if USE_ONE_I2S_PER_CHIP // 1 chip
    gpio_init();

#if TEST_ORDER_SLV_RX_FIRST // Slave RX first
    // initialize GPIO pin as output (for slave)
    gpio_out_slv_init();

    // Slave RX
    if (ret)
        ret = test_i2s_slave_rx();

    // Slave TX
    if (ret)
        ret = test_i2s_slave_tx();

    // initialize GPIO pin as interrupt (for master)
    gpio_int_mst_init();

    // Master RX
    if (ret)
        ret = test_i2s_master_rx();

    // Master TX
    if (ret)
        ret = test_i2s_master_tx();

#else // Master TX first
    // initialize GPIO pin as interrupt (for master)
    gpio_int_mst_init();

    // Master TX
    if (ret)
        ret = test_i2s_master_tx();

    // Master RX
    if (ret)
        ret = test_i2s_master_rx();

    // initialize GPIO pin as output (for slave)
    gpio_out_slv_init();

    // Slave TX
    if (ret)
        ret = test_i2s_slave_tx();

    // Slave RX
    if (ret)
        ret = test_i2s_slave_rx();

#endif // TEST_ORDER_SLV_RX_FIRST

    gpio_uninit();

#else // 2 chip

    // Slave RX & Master TX
    if (ret)
        ret = test_i2s_slave_rx_master_tx();

    // Slave TX & Master RX
    if (ret)
        ret = test_i2s_slave_tx_master_rx();

#endif // USE_ONE_I2S_PER_CHIP

    LOG_SPLIT_LINE1();
    LOGD("I2S Test cases: total = %d, passed = %d, failed = %d", TC_total, TC_pass, TC_fail);
    LOG_SPLIT_LINE2();

    while(1); // DON'T EXIT, or else it may enter exception handler...

    return 0;
}
