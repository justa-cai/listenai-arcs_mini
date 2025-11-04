/*
 * i2s_tx_rx.c
 *
 *  Created on: Apr. 25, 2024 for MARS
 *      Author: bauldeng
 */

#include "main.h"

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

#define XFER_FLAG_SLV_RX_DONE     (0x1 << 0)
#define XFER_FLAG_SLV_TX_DONE     (0x1 << 1)
#define XFER_FLAG_SLV_BOTH_DONE   (XFER_FLAG_SLV_RX_DONE | XFER_FLAG_SLV_TX_DONE)

#define XFER_FLAG_MST_RX_DONE     (0x1 << 2) // (0x1 << 0)
#define XFER_FLAG_MST_TX_DONE     (0x1 << 3) // (0x1 << 1)
#define XFER_FLAG_MST_BOTH_DONE   (XFER_FLAG_MST_RX_DONE | XFER_FLAG_MST_TX_DONE)

// RX/TX done flag on slave/master side
static volatile uint8_t s_xfer_flag = 0;
static volatile uint32_t s_rx_cnt = 0; // RX sample count

uint32_t g_rx_buf[MAX_XFER_WORD];
uint32_t g_tx_buf[MAX_XFER_WORD];

extern I2S_TEST_CASE g_cur_tcase;

//------------------------------------------------------------------
//  RX/TX buffer functions
//------------------------------------------------------------------
void init_tx_buf(uint32_t total_bytes)
{
    uint32_t i;
    uint8_t *pTx = (uint8_t *)&g_tx_buf[0];
    if (total_bytes > TX_SAMP_CNT * 4)
        total_bytes = TX_SAMP_CNT * 4;
    for (i = 0; i < total_bytes; i++)
        pTx[i] = ((i << 1) + 1) & 0xFF; // odd numbers
}

void init_rx_buf()
{
    memset(g_rx_buf, 0xAA, sizeof(g_rx_buf));
}

static int memcmp_bits (const void *a1, const void *a2, uint32_t size, int8_t ch_bits)
{
    if (ch_bits == 0 || ch_bits > 32 || (ch_bits < 0 && ch_bits >= -16) || ch_bits <= -32) {
        LOGD("%s: ch_bits = %d, ERROR!!\n", __func__, ch_bits);
        return -1;
    }

    if (ch_bits == 8 || ch_bits == 16 || ch_bits == 32)
        return memcmp (a1, a2, size);

    uint32_t count, i;
    if (ch_bits < 8 && ch_bits > 0) { // 1 ~ 7 bits
        uint8_t mask = (1 << ch_bits) - 1;
        uint8_t *b1 = (uint8_t *)a1;
        uint8_t *b2 = (uint8_t *)a2;
        count = size;
        for (i = 0; i < count; i++, b1++, b2++) {
            if ((*b1 ^ *b2) & mask)
                return 1; // different, which is bigger? assume the former...
        } // end for
        return 0; // same

    } else if (ch_bits < 16 && ch_bits > 8) { // 9 ~ 15 bits
        uint16_t mask = (1 << ch_bits) - 1;
        uint16_t *b1 = (uint16_t *)a1;
        uint16_t *b2 = (uint16_t *)a2;
        count = (size + 1) / 2;
        for (i = 0; i < count; i++, b1++, b2++) {
            if ((*b1 ^ *b2) & mask)
                return 1; // different, which is bigger? assume the former...
        } // end for
        return 0; // same

    } else if (ch_bits < 32 || ch_bits > -32) { // 17 ~ 31 bits High @WORD or Low @WORD
        uint32_t mask;
        if (ch_bits > 0) // High n bits @ WORD
            mask = ~((1 << (32 - ch_bits)) - 1);
        else
            mask = (1 << (0 - ch_bits)) - 1;
        uint32_t *b1 = (uint32_t *)a1;
        uint32_t *b2 = (uint32_t *)a2;
        count = (size + 3) / 4;
        for (i = 0; i < count; i++, b1++, b2++) {
            if ((*b1 ^ *b2) & mask)
                return 1; // different, which is bigger? assume the former...
        } // end for
        return 0; // same
    }

    return -1;
}

bool is_rx_eq_tx(uint32_t ch_bytes, int8_t ch_bits)
{
    uint32_t total_bytes = s_rx_cnt * ch_bytes;
    if (total_bytes > MAX_XFER_WORD * 4)
        return false;
     return (memcmp_bits(g_rx_buf, g_tx_buf, total_bytes, ch_bits) == 0);
}

//------------------------------------------------------------------
//  I2S Callback functions
//------------------------------------------------------------------
void cb_i2s_slave_rx_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint16_t event = event_info & CSK_I2S_EVENT_MASK; //use 13bits event type?

    if(event & CSK_I2S_EVENT_RECEIVE_COMPLETE) {
        s_xfer_flag |= XFER_FLAG_SLV_RX_DONE;
        s_rx_cnt = I2S_GetRxCount(i2s_dev, g_cur_tcase.tcase_cfg->lr_bmp);
    }
    if (event & CSK_I2S_EVENT_RX_FIFO_OVERRUN) {
        I2S_Abort_Channels(i2s_dev, CH_BMP_STEREO, 0, 0);
        LOGD("SLV RX Overflow!!");
    }
    if (event & CSK_I2S_EVENT_CLOCK_ERROR) {
        LOGD("SLV CLK GLITCH!!");
    }
}

void cb_i2s_slave_tx_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint16_t event = event_info & CSK_I2S_EVENT_MASK; //use 13bits event type?
    if(event & CSK_I2S_EVENT_TRANSMIT_COMPLETE) {
        s_xfer_flag |= XFER_FLAG_SLV_TX_DONE;
    }
    if (event & CSK_I2S_EVENT_TX_FIFO_UNDERRUN) {
        I2S_Abort_Channels(i2s_dev, 0, CH_BMP_STEREO, 0);
        LOGD("SLV TX Underflow!!");
    }
    if (event & CSK_I2S_EVENT_CLOCK_ERROR) {
        LOGD("SLV CLK GLITCH!!");
    }
}

void cb_i2s_master_rx_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint16_t event = event_info & CSK_I2S_EVENT_MASK; //use 13bits event type?

    if(event & CSK_I2S_EVENT_RECEIVE_COMPLETE) {
        s_xfer_flag |= XFER_FLAG_MST_RX_DONE;
        s_rx_cnt = I2S_GetRxCount(i2s_dev, g_cur_tcase.tcase_cfg->lr_bmp);
    }
    if (event & CSK_I2S_EVENT_RX_FIFO_OVERRUN) {
        I2S_Abort_Channels(i2s_dev, CH_BMP_STEREO, 0, 0);
        LOGD("MST RX Overflow!!");
    }
    if (event & CSK_I2S_EVENT_CLOCK_ERROR) {
        LOGD("MST CLK GLITCH!!");
    }
}

void cb_i2s_master_tx_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint16_t event = event_info & CSK_I2S_EVENT_MASK; //use 13bits event type?
    if(event & CSK_I2S_EVENT_TRANSMIT_COMPLETE) {
        s_xfer_flag |= XFER_FLAG_MST_TX_DONE;
    }
    if (event & CSK_I2S_EVENT_TX_FIFO_UNDERRUN) {
        I2S_Abort_Channels(i2s_dev, 0, CH_BMP_STEREO, 0);
        LOGD("MST TX Underflow!!");
    }
    if (event & CSK_I2S_EVENT_CLOCK_ERROR) {
        LOGD("MST CLK GLITCH!!");
    }
}

//------------------------------------------------------------------
//  I2S wait done functions
//------------------------------------------------------------------

// wait transfer done with timeout
static bool wait_xfer_done_timeout(volatile uint8_t *xfer_flag_p, uint8_t done_bits, uint32_t max_wait_ms)
{
    bool ret = false;
    nos_timer_start();
    while(nos_timer_elapsed() < max_wait_ms) {
        if((*xfer_flag_p & done_bits) == done_bits) {
            *xfer_flag_p &= ~done_bits;
            ret = true;
            break;
        }
    }
    nos_timer_stop();
    return ret;
}

bool WAIT_SLV_RX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
    return wait_xfer_done_timeout(&s_xfer_flag, XFER_FLAG_SLV_RX_DONE, max_wait_ms);
}

bool WAIT_SLV_TX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
    return wait_xfer_done_timeout(&s_xfer_flag, XFER_FLAG_SLV_TX_DONE, max_wait_ms);
}

bool WAIT_MST_RX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
    return wait_xfer_done_timeout(&s_xfer_flag, XFER_FLAG_MST_RX_DONE, max_wait_ms);
}

bool WAIT_MST_TX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
    return wait_xfer_done_timeout(&s_xfer_flag, XFER_FLAG_MST_TX_DONE, max_wait_ms);
}

bool WAIT_SLV_RX_MST_TX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
    return wait_xfer_done_timeout(&s_xfer_flag,
            XFER_FLAG_SLV_RX_DONE | XFER_FLAG_MST_TX_DONE, max_wait_ms);
}

bool WAIT_SLV_TX_MST_RX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
//    nos_delay_ms(3);
    return wait_xfer_done_timeout(&s_xfer_flag,
            XFER_FLAG_SLV_TX_DONE | XFER_FLAG_MST_RX_DONE, max_wait_ms);
}


/*
bool WAIT_SLV_TXRX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
    return wait_xfer_done_timeout(&s_xfer_flag,
            XFER_FLAG_SLV_TX_DONE | XFER_FLAG_SLV_RX_DONE, max_wait_ms);
}

bool WAIT_MST_TXRX_DONE_TIMEOUT(uint32_t max_wait_ms)
{
    return wait_xfer_done_timeout(&s_xfer_flag,
            XFER_FLAG_MST_TX_DONE | XFER_FLAG_MST_RX_DONE, max_wait_ms);
}
*/


//------------------------------------------------------------------
//  I2S RX/TX case functions
//------------------------------------------------------------------

void * i2s_rx_case(uint8_t i2s_idx, bool as_master, bool start_rx, I2S_TCASE_CONFIG *cfg_p)
{
    int ret;
    void *i2s_dev = NULL;
    uint32_t dev_bmp_flag = 0;

    assert(i2s_idx < CHIP_I2S_DEV_CNT && cfg_p != NULL);
    if (cfg_p->lr_bmp == 0) {
        LOGD("%s: NO L/R channel is set for I2S%d", __func__, i2s_idx);
        return NULL;
    }

    dev_bmp_flag = cfg_p->lr_bmp << I2S_BMP_FLAG_IN_POS;
    //if (cfg_p->alt_apc_ch)
    //    dev_bmp_flag |= I2S_BMP_FLAG_USE_ALT_IN;
    i2s_dev = i2s_in_init(i2s_idx, dev_bmp_flag,
            as_master ? cb_i2s_master_rx_event : cb_i2s_slave_rx_event);
    if (i2s_dev == NULL) {
        LOGD("%s: Failed to call i2s_in_init for I2S%d", __func__, i2s_idx);
        return NULL;
    }

    ret = i2s_config(i2s_dev, as_master, cfg_p->use_tdm, cfg_p->ch_cnt, cfg_p->ch_bits,
            cfg_p->i2s_prt, cfg_p->sampling_freq, cfg_p->bck_slave);
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: Failed to call i2s_config for I2S%d", __func__, i2s_idx);
        goto ERR_EXIT;
    }

    if (start_rx) {
        ret = i2s_rx_case_start(i2s_dev, cfg_p);
        if (ret != CSK_DRIVER_OK) {
            LOGD("%s: Failed to call rx_start for I2S%d", __func__, i2s_idx);
            goto ERR_EXIT;
        }
    }

ERR_EXIT:
    if (ret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}


void * i2s_tx_case(uint8_t i2s_idx, bool as_master, bool start_tx, I2S_TCASE_CONFIG *cfg_p)
{
    int ret;
    void *i2s_dev = NULL;
    uint32_t dev_bmp_flag = 0;

    assert(i2s_idx < CHIP_I2S_DEV_CNT && cfg_p != NULL);
    if (cfg_p->lr_bmp == 0) {
        LOGD("%s: NO L/R channel is set for I2S%d", __func__, i2s_idx);
        return NULL;
    }

    dev_bmp_flag = cfg_p->lr_bmp << I2S_BMP_FLAG_OUT_POS;
    //if (cfg_p->alt_apc_ch)
    //    dev_bmp_flag |= I2S_BMP_FLAG_USE_ALT_OUT;
    i2s_dev = i2s_out_init(i2s_idx, dev_bmp_flag,
            as_master ? cb_i2s_master_tx_event : cb_i2s_slave_tx_event);
    if (i2s_dev == NULL) {
        LOGD("%s: Failed to call i2s_out_init for I2S%d", __func__, i2s_idx);
        return NULL;
    }

    ret = i2s_config(i2s_dev, as_master, cfg_p->use_tdm, cfg_p->ch_cnt, cfg_p->ch_bits,
            cfg_p->i2s_prt, cfg_p->sampling_freq, cfg_p->bck_slave);
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: Failed to call i2s_config for I2S%d", __func__, i2s_idx);
        goto ERR_EXIT;
    }

    if (start_tx) {
        ret = i2s_tx_case_start(i2s_dev, cfg_p);
        if (ret != CSK_DRIVER_OK) {
            LOGD("%s: Failed to call tx_start for I2S%d", __func__, i2s_idx);
            goto ERR_EXIT;
        }
    }

ERR_EXIT:
    if (ret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}


/*
void * i2s_slave_txrx_case(uint8_t i2s_idx, I2S_TCASE_CONFIG *cfg_p)
{
    //TODO:
    return NULL;
}

void * i2s_master_txrx_case(uint8_t i2s_idx, I2S_TCASE_CONFIG *cfg_p)
{
    //TODO:
    return NULL;
}
*/

int32_t i2s_rx_case_start(void *i2s_dev, I2S_TCASE_CONFIG *cfg_p)
{
    assert(i2s_dev != NULL && cfg_p != NULL);
    return I2S_Receive(i2s_dev, g_rx_buf, cfg_p->req_cnt, cfg_p->lr_bmp, I2S_RX_FLAG_START_NOW);
}


int32_t i2s_tx_case_start(void *i2s_dev, I2S_TCASE_CONFIG *cfg_p)
{
    assert(i2s_dev != NULL && cfg_p != NULL);
    return I2S_Send(i2s_dev, g_tx_buf, cfg_p->req_cnt, cfg_p->lr_bmp, I2S_TX_FLAG_START_NOW);
}


int32_t i2s_txrx_case_start(void *i2s_dev, I2S_TCASE_CONFIG *cfg_p)
{
    int32_t iret;
    assert(i2s_dev != NULL && cfg_p != NULL);
    iret = I2S_Receive(i2s_dev, g_rx_buf, cfg_p->req_cnt, cfg_p->lr_bmp, 0); //I2S_RX_FLAG_START_NOW
    if (iret != CSK_DRIVER_OK) {
        return false;
    }

    iret = I2S_Send(i2s_dev, g_tx_buf, cfg_p->req_cnt, cfg_p->lr_bmp, 0); //I2S_TX_FLAG_START_NOW
    if (iret != CSK_DRIVER_OK) {
        I2S_Abort_Channels(i2s_dev, cfg_p->lr_bmp, cfg_p->lr_bmp, 0);
        return false;
    }

    return I2S_Enable_Channels(i2s_dev, cfg_p->lr_bmp, cfg_p->lr_bmp);
}
