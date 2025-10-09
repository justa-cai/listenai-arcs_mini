/*
 * i2c_op.c
 *
 *  Created on: Nov. 6, 2020 for VENUS (CP)
 *  Ported on: Dec. 18, 2023 for ARCS (AP)
 *
 */

// NOTE: NO INTERRUPT & DMA SUPPORT!
// The demo is running on CP while interrupt & dma signals of I2C device are all communicated
// with AP, so we cannot utilize interrupt & dma operations, but only read/write I2C registers...

#include "main.h"
#include <assert.h>
#include "ClockManager.h"

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
//#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG


#define IDX_I2C0        0
#define IDX_I2C1        1
#define CUR_IDX_I2C     IDX_I2C0 // IDX_I2C1 // FIXME:

//----------------------------------------------------------------

// I2C0/1 address in CP is same as AP
//#define I2C0_BASE               0x45600000 // size=1MB
//#define I2C1_BASE               0x45700000 // size=1MB

typedef struct {
    I2C_RegDef * reg;
    int32_t     irq_no;
    uint32_t    fifo_size;
} I2C_DEV;

I2C_DEV i2c0_dev = {
        (I2C_RegDef *)I2C0_BASE,
        IRQ_I2C0_VECTOR,
        0
};

I2C_DEV i2c1_dev = {
        (I2C_RegDef *)I2C1_BASE,
		IRQ_I2C1_VECTOR,
        0
};

void * get_i2c_dev() {
#if (CUR_IDX_I2C == IDX_I2C0)
    return &i2c0_dev;
#elif (CUR_IDX_I2C == IDX_I2C1)
    return &i2c1_dev;
#endif // definition of CUR_I2C
}

//----------------------------------------------------------------
#define SYSCTRL_CFG    ((CMN_SYSCFG_RegDef *) CMN_SYS_BASE)

bool init_i2c_dev(void *i2c_dev)
{
#if (CUR_IDX_I2C == IDX_I2C0)
    //I2C0 pin configuration
    IOMuxManager_PinConfigure(I2C0_GROUP, I2C0_SCL, I2C0_FUNC);  // CLK
    IOMuxManager_PinConfigure(I2C0_GROUP, I2C0_SDA, I2C0_FUNC);  // DATA
    SYSCTRL_CFG->REG_PERI_CLK_CFG6.bit.ENA_I2C0_CLK = 0x1;
    SYSCTRL_CFG->REG_SW_RESET_CP2.bit.I2C0_RESET = 0x1;
#elif (CUR_IDX_I2C == IDX_I2C1)
    //I2C1 pin configuration
    IOMuxManager_PinConfigure(I2C1_GROUP, I2C1_SCL, I2C1_FUNC);  // CLK
    IOMuxManager_PinConfigure(I2C1_GROUP, I2C1_SDA, I2C1_FUNC);  // DATA
    SYSCTRL_CFG->REG_PERI_CLK_CFG6.bit.ENA_I2C1_CLK = 0x1;
    SYSCTRL_CFG->REG_SW_RESET_CP2.bit.I2C1_RESET = 0x1;
#endif

    uint32_t val;
    I2C_DEV *i2c = (I2C_DEV *)i2c_dev;
    assert(i2c != NULL);

    //disable I2C interrupt
    disable_IRQ(i2c->irq_no);

    //Get FIFO size
    val = i2c->reg->REG_CFG.bit.FIFOSIZE;
    i2c->fifo_size = 1 << (val + 1);

    //Reset I2C controller
    i2c->reg->REG_CMD.all = 0x5 ;
    nos_delay_ms(1);

    // Standard-mode (100Khz)
#define I2C_TIME_SP_MAX             50     // ns
#define I2C_TIME_SETUP_MIN          250    // ns
#define I2C_TIME_HOLD_MIN           300    // ns
#define I2C_TIME_HIGH_MIN           4700   // ns
#define I2C_TIME_RADIO_FIX          2

//    // Fast-mode (400Khz)
//#define I2C_TIME_SP_MAX             50     // ns
//#define I2C_TIME_SETUP_MIN          100    // ns
//#define I2C_TIME_HOLD_MIN           300    // ns
//#define I2C_TIME_HIGH_MIN           830 //700    // ns
//#define I2C_TIME_RADIO_FIX          2

//    // Fast-Plus mode (1Mhz)
//#define I2C_TIME_SP_MAX             50     // ns
//#define I2C_TIME_SETUP_MIN          50    // ns
//#define I2C_TIME_HOLD_MIN           150    // ns
//#define I2C_TIME_HIGH_MIN           300    // ns
//#define I2C_TIME_RADIO_FIX          2

    uint16_t sudat, sp, hddat, ratio, sclhi = 0;

//    val = CRM_GetAp_peri_pclkFreq() / 1000000; // PCLK cycles per ms
    val = CRM_GetCmn_peri_pclkFreq() / 1000000; // PCLK cycles per ms
    LOGD("%d APB cycles per ms\n", val);

    sp = I2C_TIME_SP_MAX * val / 1000; // spikes time
    sudat = (I2C_TIME_SETUP_MIN * val / 1000) - (4 + sp); // setup time
    hddat = (I2C_TIME_HOLD_MIN * val / 1000) - (4 + sp); // hold time
    // The T_SCLHi value must be greater than T_SP and T_HDDAT values.
    sclhi = (I2C_TIME_HIGH_MIN * val / 1000) - (4 + sp); // SCL high?
    ratio = I2C_TIME_RADIO_FIX - 1; // // ratio
    LOGD("sp=%d, sudat=%d, hddat=%d, hclhi=%d, ratio=%d\n",
            sp, sudat, hddat, sclhi, ratio);

    i2c->reg->REG_SETUP.all = (sudat << 24) |
            (sp << 21) |
            (hddat << 16) |
            (ratio << 13) |
            (sclhi << 4) |
            (0x1 << 2) | // master mode
            (0x1 << 0); // enable I2C

    return true;
}

// num SHOULD be no more than 256 (num=0 indicates 256)
// addr SHOULD be 7bits / 10bits
bool i2c_write(void *i2c_dev, uint32_t addr, const uint8_t* data, uint32_t num, uint32_t max_wait_ms)
{
    volatile union I2C_REG_STATUS sta;
    uint32_t i, bytes_left, bytes_toxfer;
    const uint8_t *cur;
    bool ret = true;

    I2C_DEV *i2c = (I2C_DEV *)i2c_dev;
    assert(i2c != NULL);

//    //Reset I2C controller
//    i2c->reg->REG_CMD.all = 0x5 ;

    //Step 2 set direction and count
//    i2c->reg->REG_CTRL.bit.DATACNT  = num & 0xFF;
    i2c->reg->REG_CTRL.all = (num & 0xFF) | // Data counts in bytes
                             (1 << 9) | (1 << 10) | // send STOP condition, data
                             (1 << 11) | (1 << 12); // send address, START

    //step 3 set slave address
    i2c->reg->REG_ADDR.all = addr & 0x3FF;

    //step 4 enable interrupt (NO interrupt is used!)
//    i2c->reg->REG_INTEN.all = 0;
    i2c->reg->REG_INTEN.bit.CMPL = 0x1;
    i2c->reg->REG_INTEN.bit.FIFOEMPTY = 0x1;

    // clear all status bits
    i2c->reg->REG_STATUS.all = 0x7F << 3;

    // pre-fill TX FIFO
    bytes_toxfer = i2c->fifo_size > num ? num : i2c->fifo_size;
    cur = &data[0];
    for (i=0; i<bytes_toxfer; i++) {
        i2c->reg->REG_DATA.all = *cur++;
    }
    bytes_left = num - bytes_toxfer;

    //step 5 command issue
    LOGD("Start I2C TX ...\n");
    i2c->reg->REG_CMD.all = 0x1 ;//issue transaction

    //wait until Transaction Completion
    nos_timer_start();
    //clock_t start_clk = clock();

    // Transaction Completion & data byte has been transmitted
    while (bytes_left || (sta.all = i2c->reg->REG_STATUS.all, !sta.bit.CMPL)) { // || !sta.bit.ACK
        if (bytes_left) {
            bytes_toxfer = 0;
            if (sta.bit.FIFO_EMPTY) { // if FIFO Empty
                bytes_toxfer = i2c->fifo_size < bytes_left ?
                        i2c->fifo_size : bytes_left;
            } else if (sta.bit.FIFOHALF) { // if FIFO Half
                bytes_toxfer = i2c->fifo_size/2 < bytes_left ?
                        i2c->fifo_size/2 : bytes_left;
            }
            if (bytes_toxfer != 0) {
                for (i=0; i<bytes_toxfer; i++)
                    i2c->reg->REG_DATA.all = *cur++;
                bytes_left -= bytes_toxfer;
            }
        }
        if ((i = nos_timer_elapsed()) >= max_wait_ms) {
            LOGD("I2C TX timeout (%d ms)!!\n", i);
            ret = false;
            break;
        }
    } // end while


/*
    nos_timer_start();

    // Transaction Completion & data byte has been transmitted
    while (sta.all = i2c->reg->REG_STATUS.all, (!sta.bit.CMPL || !sta.bit.BYTETRANS)) {
        if ((i = nos_timer_elapsed()) >= max_wait_ms) {
            LOGD("I2C TX timeout (%dms)!!\n", i);
            ret = false;
            break;
        }
        if (!sta.bit.FIFOHALF && !sta.bit.FIFO_EMPTY)
            continue;

        bytes_toxfer = 0;
        if (sta.bit.FIFOHALF) { // if FIFO Half
            bytes_toxfer = i2c->fifo_size/2 < bytes_left ?
                    i2c->fifo_size/2 : bytes_left;
        } else if (sta.bit.FIFO_EMPTY) { // if FIFO Empty
            bytes_toxfer = i2c->fifo_size < bytes_left ?
                    i2c->fifo_size : bytes_left;
        }
        if (bytes_toxfer != 0) {
            for (i=0; i<bytes_toxfer; i++) {
                i2c->reg->REG_DATA.all = *cur;
                cur++;
            }
            bytes_left -= bytes_toxfer;
        }
    } // end while !CMPL
*/

/*
    nos_timer_start();
    //clock_t start_clk = clock();

    // Transaction Completion & data byte has been transmitted
    sta.all = i2c->reg->REG_STATUS.all;
    while (true) {
        bytes_toxfer = 0;
        if (sta.bit.FIFO_EMPTY) { // if FIFO Empty
            bytes_toxfer = i2c->fifo_size < bytes_left ?
                    i2c->fifo_size : bytes_left;
        } else if (sta.bit.FIFOHALF) { // if FIFO Half
            bytes_toxfer = i2c->fifo_size/2 < bytes_left ?
                    i2c->fifo_size/2 : bytes_left;
        }
        if (bytes_toxfer != 0) {
            for (i=0; i<bytes_toxfer; i++) {
                i2c->reg->REG_DATA.all = *cur;
                cur++;
            }
            bytes_left -= bytes_toxfer;
        }

        sta.all = i2c->reg->REG_STATUS.all;
        if (sta.bit.CMPL) { // sta.bit.BYTETRANS?
            i2c->reg->REG_STATUS.all = sta.all;
            break;
        }

        //i = (clock() - start_clk) / CLKS_1MS;
        if ((i = nos_timer_elapsed()) >= max_wait_ms) {
        //if (i >= max_wait_ms) {
            LOGD("I2C TX timeout (%d ms)!!\n", i);
            ret = false;
            break;
        }
    } // end while
*/

    if (ret) {
        LOGD("I2C TX completed!\n");
    }

    nos_timer_stop();
    return ret;
}


/*
bool i2c_read(void *i2c_dev, uint32_t addr, uint8_t* data, uint32_t num, uint32_t max_wait_ms)
{
    union I2C_REG_STATUS sta;
    uint32_t i, bytes_left;
    uint8_t *cur;
    bool ret = true;

    I2C_DEV *i2c = (I2C_DEV *)i2c_dev;
    assert(i2c != NULL);

    //Step 2 set direction and count
//    i2c->reg->REG_CTRL.bit.DATACNT = num & 0xFF;
//    i2c->reg->REG_CTRL.bit.DIR = 0x1;
    i2c->reg->REG_CTRL.all = (num & 0xFF) | (1 << 8); // Data counts in bytes & Receiver

    //step 3 set slave address
    i2c->reg->REG_ADDR.all = addr & 0x3FF;

    //step 4 enable interrupt (NO interrupt is used!)
    i2c->reg->REG_INTEN.all = 0;
//    i2c->reg->REG_INTEN.bit.CMPL = 0x1;
//    i2c->reg->REG_INTEN.bit.FIFOFULL = 0x1;

    // clear all status bits
    i2c->reg->REG_STATUS.all = 0x7F << 3;

    //step 5 command issue
    //LOGD("Start I2C RX ...\n");
    i2c->reg->REG_CMD.all = 0x1 ;//issue transaction

    //wait until Transaction Completion
    bytes_left = num;
    cur = &data[0];

    nos_timer_start();

    // Transaction Completion & data byte has been received
    while (sta.all = i2c->reg->REG_STATUS.all, (!sta.bit.CMPL)) { // || !sta.bit.BYTEREVC
        if ((i = nos_timer_elapsed()) >= max_wait_ms) {
            LOGD("I2C RX timeout (%dms)!!\n", i);
            ret = false;
            break;
        }

        if (sta.bit.FIFO_EMPTY)
            continue;

        if (bytes_left == 0) {
            CLOGW("%s: All required are received, but data still is coming...\n", __func__);
            break;
        }
        *cur++ = i2c->reg->REG_DATA.all & 0xFF;
        bytes_left--;
    } // end while !CMPL

    if (ret) {
        //LOGD("I2C TX completed!\n");
    }
    nos_timer_stop();
    return ret;
}
*/
