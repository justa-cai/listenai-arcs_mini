/*
 * spi_spi_nos_chk.c
 *
 *  Created on: Aug 21, 2020
 *
 */

#include "arcs_ap.h"
#include "log_print.h"
#include "Driver_SPI.h"
#include "IOMuxManager.h"

#include "ClockManager.h" // for BootClock_Init etc.

#include <stdbool.h>
#include <string.h>
#include "nos_timer.h"
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

#define ARRAY_SIZE(array) \
    (sizeof(array) / sizeof((array)[0]))

#define DEBUG_LOG   1 // 1
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#define HEXP                hexPrint
#define vprintf             tfp_vprintf
#else
#define LOGD(format, ...)   ((void)0)
#define HEXP(...)           ((void)0)
#endif // DEBUG_LOG

#define NO_SPI_CS           0 // 1: no cs signal

//#define SPI_Send            SPI_Send_NEnd
//#define SPI_Receive         SPI_Receive_NEnd
//#define SPI_Transfer        SPI_Transfer_NEnd

#define SPI0_CLK_PIN          CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER5
#define SPI0_CS_PIN           CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER5
#define SPI0_MISO_PIN         CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER5
#define SPI0_MOSI_PIN         CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER5

#define SPI1_CLK_PIN          CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER6
#define SPI1_CS_PIN           CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER6
//#define SPI1_MISO_PIN         CSK_IOMUX_PAD_B, 6, CSK_IOMUX_FUNC_ALTER6
#define SPI1_MISO_PIN         CSK_IOMUX_PAD_B, 8, CSK_IOMUX_FUNC_ALTER6
#define SPI1_MOSI_PIN         CSK_IOMUX_PAD_B, 7, CSK_IOMUX_FUNC_ALTER6

#define SPI2_CLK_PIN          CSK_IOMUX_PAD_B, 0, CSK_IOMUX_FUNC_ALTER7
#define SPI2_CS_PIN           CSK_IOMUX_PAD_B, 1, CSK_IOMUX_FUNC_ALTER7
#define SPI2_MISO_PIN         CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_ALTER7
#define SPI2_MOSI_PIN         CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_ALTER7

#define SPI0_CS_PAD_NUM       CSK_IOMUX_PAD_A, 6
#define SPI1_CS_PAD_NUM       CSK_IOMUX_PAD_A, 7
#define SPI2_CS_PAD_NUM       CSK_IOMUX_PAD_B, 1

#if NO_SPI_CS

void init_spi_cs(uint32_t index)
{
    if (index == 0) {
        IOMuxManager_ModeConfigure(SPI0_CS_PAD_NUM, HAL_IOMUX_NONE_MODE);
    } else if (index == 1) {
        IOMuxManager_ModeConfigure(SPI1_CS_PAD_NUM, HAL_IOMUX_NONE_MODE);
    }
}

/*
void lower_spi_cs(uint8_t index)
{
    volatile uint32_t i = 3500; //3000; //2500; //1000; //2000; //5000;
    if (index == 0) {
        //IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI0_CS_PIN, HAL_IOMUX_NONE_MODE);
        //while(i > 0)    i--;
        IOMuxManager_ModeConfigure(SPI0_CS_PAD_NUM, HAL_IOMUX_PULLDOWN_MODE);
        while(i > 0)    i--;
    } else if (index == 1) {
        //IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI1_CS_PIN, HAL_IOMUX_NONE_MODE);
        //while(i > 0)    i--;
        IOMuxManager_ModeConfigure(SPI1_CS_PAD_NUM, HAL_IOMUX_PULLDOWN_MODE);
        while(i > 0)    i--;
    }
}

void raise_spi_cs(uint8_t index)
{
    volatile uint32_t i = 1000;
    if (index == 0) {
        IOMuxManager_ModeConfigure(SPI0_CS_PAD_NUM, HAL_IOMUX_PULLUP_MODE);
        while(i > 0)    i--;
        IOMuxManager_ModeConfigure(SPI0_CS_PAD_NUM, HAL_IOMUX_NONE_MODE);
    } else if (index == 1) {
        IOMuxManager_ModeConfigure(SPI1_CS_PAD_NUM, HAL_IOMUX_PULLUP_MODE);
        while(i-- > 0);
        IOMuxManager_ModeConfigure(SPI1_CS_PAD_NUM, HAL_IOMUX_NONE_MODE);
    }
}

//void set_spi_cs(uint8_t index, uint8_t level)
void set_spi_cs(void *spi_dev, uint8_t level)
{
    uint8_t index = SPI_Index(spi_dev);
    if (level == 0)
        lower_spi_cs(index);
    else
        raise_spi_cs(index);
}


void lower2_spi_cs(uint8_t index)
{
    CORE_IOMUX_RegDef * iom = (CORE_IOMUX_RegDef*)CMN_IOMUX_BASE;
    if (index == 0) { // SPI0_CS_PIN @ PAD_GPIOA16
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OUT_REG = 0; // output low??
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OEN_REG = 0; // OUT direction
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OUT_FRC = 1; // force using OUT_VAL
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OEN_FRC = 1; // force using OEN_VAL
    } else if (index == 1) { // SPI1_CS_PIN @ PAD_GPIOA13
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OUT_REG = 0; // output low??
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OEN_REG = 0; // OUT direction
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OUT_FRC = 1; // force using OUT_VAL
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OEN_FRC = 1; // force using OEN_VAL
   }
}

void raise2_spi_cs(uint8_t index)
{
	CORE_IOMUX_RegDef* iom = (CORE_IOMUX_RegDef*)CMN_IOMUX_BASE;
    if (index == 0) { // SPI0_CS_PIN @ PAD_GPIOA16
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OUT_REG = 1; // output high??
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OEN_REG = 0; // OUT direction
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OUT_FRC = 1; // force using OUT_VAL
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OEN_FRC = 1; // force using OEN_VAL
    } else if (index == 1) { // SPI1_CS_PIN @ PAD_GPIOA13
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OUT_REG = 1; // output high??
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OEN_REG = 0; // OUT direction
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OUT_FRC = 1; // force using OUT_VAL
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OEN_FRC = 1; // force using OEN_VAL
    }
}

//void set2_spi_cs(uint8_t index, uint8_t level)
void set2_spi_cs(void *spi_dev, uint8_t level)
{
	CORE_IOMUX_RegDef* iom = (CORE_IOMUX_RegDef*)CMN_IOMUX_BASE;
    uint8_t index = SPI_Index(spi_dev);
    if (index == 0) { // SPI0_CS_PIN @ PAD_GPIOA16
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OUT_REG = (level==0 ? 0 : 1); //FIXME:
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OEN_REG = 0; // OUT direction
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OUT_FRC = 1; // force using OUT_VAL
        iom->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_OEN_FRC = 1; // force using OEN_VAL
    } else if (index == 1) { // SPI1_CS_PIN @ PAD_GPIOA13
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OUT_REG = (level==0 ? 0 : 1); //FIXME:
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OEN_REG = 0; // OUT direction
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OUT_FRC = 1; // force using OUT_VAL
        iom->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_OEN_FRC = 1; // force using OEN_VAL
    }
}
*/

// NOT WORK on ARCS D0 FPGA...
void lower3_spi_cs(uint8_t index)
{
	CORE_IOMUX_RegDef* iom = (CORE_IOMUX_RegDef*)CMN_IOMUX_BASE;
    if (index == 0) { // SPI0_CS_PIN @ PAD_GPIOA06
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_FRC = 0; // normal function, NOT force
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_UP = 0; // disable pullup
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_DN = 1; // enable pulldown
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_FRC = 1; // force using pullup/pulldown
    } else if (index == 1) { // SPI1_CS_PIN @ PAD_GPIOA07
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_FRC = 0; // normal function, NOT force
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_UP = 0; // disable pullup
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_DN = 1; // enable pulldown
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_FRC = 1; // force using pullup/pulldown
    }
}

// NOT WORK on ARCS D0 FPGA...
void raise3_spi_cs(uint8_t index)
{
	CORE_IOMUX_RegDef* iom = (CORE_IOMUX_RegDef*)CMN_IOMUX_BASE;
    if (index == 0) { // SPI0_CS_PIN @ PAD_GPIOA06
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_FRC = 0; // normal function, NOT force
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_DN = 0; // disable pulldown
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_UP = 1; // enable pullup
        iom->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_PULL_FRC = 1; // force using pullup/pulldown
    } else if (index == 1) { // SPI1_CS_PIN @ PAD_GPIOA07
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_FRC = 0; // normal function, NOT force
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_DN = 0; // disable pulldown
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_UP = 1; // enable pullup
        iom->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_PULL_FRC = 1; // force using pullup/pulldown
    }
}

//void set3_spi_cs(uint8_t index, uint8_t level)
void set3_spi_cs(void *spi_dev, uint8_t level)
{
    uint8_t index = SPI_Index(spi_dev);
    if (level == 0)
        lower3_spi_cs(index);
    else
        raise3_spi_cs(index);
}

#endif // NO_SPI_CS


uint32_t TC_total = 0, TC_pass = 0, TC_fail = 0;

//SPI state
typedef enum {
    SPI_STATE_UNKNOWN = 0,
    SPI_STATE_INITIALIZED,
    SPI_STATE_SENDING,      // send is ongoing
    SPI_STATE_SEND_DONE,    // send is done
    SPI_STATE_RECVING,      // receive is ongoing
    SPI_STATE_RECV_DONE,    // receive is done
    SPI_STATE_XFERING,      // xfering = sending & receiving
    SPI_STATE_XFER_DONE,    // xfer is done
    SPI_STATE_COUNT
} SPI_STATE ;

//SPI master/slave state
static SPI_STATE spi_state_m = SPI_STATE_UNKNOWN;
static SPI_STATE spi_state_s = SPI_STATE_UNKNOWN;


//#define DATA_BYTES(dbits)          (((dbits) + 7) / 8)
#define TO_BYTES(bits)              (((bits) + 7) >> 3)

//#define DATA_BYTES(dbits)           (uint32_t dbytes = TO_BYTES(dbits), \
//                                    (dbytes & 0x3) == 0x3 ? dbytes+1 : dbytes)
static inline uint32_t DATA_BYTES(uint32_t dbits) {
    uint32_t dbytes = TO_BYTES(dbits);
    return ((dbytes & 0x3) == 0x3 ? dbytes+1 : dbytes);
}

#define BYTE_CNT(items, dbits)      (items * (DATA_BYTES(dbits)))

static inline uint32_t DATA_CNT(uint32_t bytes, uint32_t dbits) {
    uint32_t dbytes = TO_BYTES(dbits);
    return (bytes + dbytes - 1) / dbytes;
}


#define SLV_CMD_PHA_EN      0 // 1 = enable command phase
#define BUF_SIZE            (512+16) // in word(4 bytes)
// one cycle: master send & slave receive, and then master receive & slave send
#define TEST_CYCLES         1 //3 //100 //
#define TEST_VAR_CYCLES     1 //5 //10 //


// _DMA_PARAM indicates aligned(32), without the indicator, error may occur in DMA transfer.
// Several received ending bytes will differ from the sent ones, are usually 0.
// The potential risk of DMA transfer when start address or size are not aligned on cache line size(32):
// access vars (in the same cache line where ending bytes locate) => DMA transfer of ending bytes =>
// flush the cache line where vars and ending bytes locate => ending bytes are overwritten...
static _DMA_PRAM uint32_t buf_send_m[BUF_SIZE]; // buffer for master send
static _DMA_PRAM uint32_t buf_recv_m[BUF_SIZE]; // buffer for master receive
static _DMA_PRAM uint32_t buf_send_s[BUF_SIZE]; // buffer for slave send
static _DMA_PRAM uint32_t buf_recv_s[BUF_SIZE]; // buffer for slave receive
//__attribute__((__used__)) static uint32_t pads[5] = { 0 };

static void hexPrint(const uint8_t *buf, uint32_t len)
{
    int i = 0;
    while (i < len) {
        logDbg("%02x ", buf[i]);
        i++;
        if (i % 16 == 0) {
            logDbg("\r\n");
        } else if (i % 4 == 0) {
            logDbg(" ");
        }
    }
    logDbg("\r\n\r\n");
    //pads[0] = 1;
}


//static void *g_spi_m = NULL, *g_spi_s = NULL;
static volatile uint32_t g_run_flag = 0;

static inline void INIT_RUN_FLAG()  { g_run_flag = 0; }

static inline void SET_RECV_DONE() { g_run_flag |= 1; }
static inline void SET_SEND_DONE() { g_run_flag |= 2; }
static inline void SET_M_XFER_DONE() { g_run_flag |= 4; } // for Master
static inline void SET_S_XFER_DONE() { g_run_flag |= 8; } // for Slave

static inline void CLR_RECV_DONE() { g_run_flag &= ~0x1UL; }
static inline void CLR_SEND_DONE() { g_run_flag &= ~0x2UL; }
static inline void CLR_SEND_RECV_DONE() { g_run_flag &= ~0x3UL; } // for Slave
//static inline void CLR_M_XFER_DONE() { g_run_flag &= ~0x4UL; } // for Master
//static inline void CLR_S_XFER_DONE() { g_run_flag &= ~0x8UL; } // for Slave
static inline void CLR_BOTH_XFER_DONE() { g_run_flag &= ~0xcUL; } // for Slave

static inline bool GET_RECV_DONE() { return (g_run_flag & 0x1UL); }
static inline bool GET_SEND_DONE() { return (g_run_flag & 0x2UL); }
static inline bool GET_SEND_RECV_DONE() { return ((g_run_flag & 0x3UL) == 0x3); }
//static inline bool GET_M_XFER_DONE() { return (g_run_flag & 0x4UL); } // for Master
//static inline bool GET_S_XFER_DONE() { return (g_run_flag & 0x8UL); } // for Slave
static inline bool GET_BOTH_XFER_DONE() { return ((g_run_flag & 0xcUL) == 0xcUL); } // for both Master & Slave

// wait receive done with timeout
static bool wait_recv_done_timeout(uint32_t max_wait_ms)
{
    bool ret = false;

    nos_timer_start();
    while(nos_timer_elapsed() < max_wait_ms){
        if(GET_RECV_DONE()){
            CLR_RECV_DONE();
            ret = true;
            break;
        }
    }
    nos_timer_stop();
    return ret;
}

// wait both send & receive done with timeout
static bool wait_send_recv_done_timeout(uint32_t max_wait_ms)
{
    bool ret = false;

    nos_timer_start();
    while(nos_timer_elapsed() < max_wait_ms){
        if(GET_SEND_RECV_DONE()){
            CLR_SEND_RECV_DONE();
            ret = true;
            break;
        }
    }
    nos_timer_stop();
    return ret;
}

// wait duplex transfer done with timeout
static bool wait_xfer_done_timeout(uint32_t max_wait_ms)
{
    bool ret = false;

    nos_timer_start();
    while(nos_timer_elapsed() < max_wait_ms){
        if(GET_BOTH_XFER_DONE()){
            CLR_BOTH_XFER_DONE();
            ret = true;
            break;
        }
    }
    nos_timer_stop();
    return ret;
}

//SPI master event callback
static void SPI_DrvEvent_m (uint32_t event, uint32_t usr_param)
{
    void *spi_dev = (void *)usr_param;
    assert(spi_dev != NULL);

    if( !(event & CSK_SPI_EVENT_TRANSFER_COMPLETE) ) {
        CSK_SPI_STATUS status;
        status.all = 0;
        SPI_GetStatus(spi_dev, &status);
        LOGD("%s: event = 0x%x (status = 0x%x), other than TRANSFER_COMPLETE!\r\n",
                __func__, event, status.all);
        return;
    }

    if (spi_state_m == SPI_STATE_SENDING) {
        LOGD("%s: Master Send Completed (%d items)!\r\n",
                __func__, SPI_GetDataCount(spi_dev));
        spi_state_m = SPI_STATE_SEND_DONE;

        SET_SEND_DONE();

    } else if (spi_state_m == SPI_STATE_RECVING) {
        CLOGD("%s: Master Receive Completed (%d items)!\r\n",
                __func__, SPI_GetDataCount(spi_dev));
        spi_state_m = SPI_STATE_RECV_DONE;

        //signal semCanXfer to notify xfer task to start next operation
//        BaseType_t xHPWoken = pdFALSE;
//        xSemaphoreGiveFromISR(semCanXfer, &xHPWoken);
//        if( xHPWoken ) {
//            portYIELD_FROM_ISR();
//        }
        SET_RECV_DONE();

    } else if (spi_state_m == SPI_STATE_XFERING) {
        LOGD("%s: Master duplex Xfer Completed (%d items)!\r\n",
                __func__, SPI_GetDataCount(spi_dev));
        spi_state_m = SPI_STATE_XFER_DONE;

        SET_M_XFER_DONE();
        //TODO:
    }

    return;
}

//SPI slave event callback
static void SPI_DrvEvent_s (uint32_t event, uint32_t usr_param)
{
    void *spi_dev = (void *)usr_param;
    assert(spi_dev != NULL);

//#if NO_SPI_CS
//    if (event & CSK_SPI_EVENT_TX_DRAIN) {
//        raise_spi_cs(SPI_Index(spi_dev));
//    }
//#endif

    if( !(event & (CSK_SPI_EVENT_TRANSFER_COMPLETE)) ) { // | CSK_SPI_EVENT_TX_DRAIN
        CSK_SPI_STATUS status;
        status.all = 0;
        SPI_GetStatus(spi_dev, &status);
//        LOGD("%s: event = 0x%x (status = 0x%x), neither TRANSFER_COMPLETE nor TX_DRAIN!\r\n",
//                __func__, event, status.all);
        LOGD("%s: event = 0x%x (status = 0x%x), NOT TRANSFER_COMPLETE!\r\n",
                __func__, event, status.all);
        return;
    }

//#if NO_SPI_CS
//    raise_spi_cs(SPI_Index(spi_dev));
//#endif

    if (spi_state_s == SPI_STATE_SENDING) {
        LOGD("%s: Slave Send Completed! (%d items)\r\n",
                __func__, SPI_GetDataCount(spi_dev));
        spi_state_s = SPI_STATE_SEND_DONE;

        SET_SEND_DONE();

    } else if (spi_state_s == SPI_STATE_RECVING) {
        LOGD("%s: Slave Receive Completed! (%d items)\r\n",
                __func__, SPI_GetDataCount(spi_dev));
        spi_state_s = SPI_STATE_RECV_DONE;

        //signal semCanXfer to notify xfer task to start next operation
//        BaseType_t xHPWoken = pdFALSE;
//        xSemaphoreGiveFromISR(semCanXfer, &xHPWoken);
//        if( xHPWoken ) {
//            portYIELD_FROM_ISR();
//        }
        SET_RECV_DONE();

//        #if NO_SPI_CS
//            raise_spi_cs(SPI_Index(spi_dev));
//        #endif

    } else if (spi_state_s == SPI_STATE_XFERING) {
        LOGD("%s: Slave duplex Xfer Completed (%d items)!\r\n",
                __func__, SPI_GetDataCount(spi_dev));
        spi_state_s = SPI_STATE_XFER_DONE;

        SET_S_XFER_DONE();
        //TODO:
    }


    //TODO: spi_state_s == SPI_STATE_XFERING?
    return;
}


// return opened SPI device pointer if OK, or NULL if failed.
static void * open_spi(uint32_t index, bool as_master, emIOMode txmode, emIOMode rxmode,
        uint32_t frm_fmt, uint32_t data_bits, bool msb_order, uint32_t bus_speed)
{
    int32_t ret;
    void* spi_dev = NULL;
    uint32_t control = 0;

    //NOTE: set interrupt priority of SPI slave to higher, and set SPI master to default,
    // so that interrupts of SPI slave can be serviced first, to prevent data overflow or underflow...
    if (index == 0) {
        spi_dev = SPI0();
        //NVIC_SetPriority(IRQ_SPI0_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY : DEF_INTERRUPT_PRIORITY - 1);
/*
        if (txmode == PIO_IO || rxmode == PIO_IO)
//            NVIC_SetPriority(IRQ_SPI0_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY - 1 : DEF_INTERRUPT_PRIORITY - 2);
        	ECLIC_SetPriorityIRQ(IRQ_SPI0_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY : DEF_INTERRUPT_PRIORITY + 1);
*/
    }
    else if (index == 1) {
        spi_dev = SPI1();
        //NVIC_SetPriority(IRQ_SPI1_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY : DEF_INTERRUPT_PRIORITY - 1);
/*
        if (txmode == PIO_IO || rxmode == PIO_IO)
//            NVIC_SetPriority(IRQ_SPI1_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY - 1 : DEF_INTERRUPT_PRIORITY - 2);
        	ECLIC_SetPriorityIRQ(IRQ_SPI1_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY : DEF_INTERRUPT_PRIORITY + 1);
*/
    }
    else if (index == 2) {
        spi_dev = SPI2();
        //NVIC_SetPriority(IRQ_SPI1_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY : DEF_INTERRUPT_PRIORITY - 1);
/*
        if (txmode == PIO_IO || rxmode == PIO_IO)
//            NVIC_SetPriority(IRQ_SPI2_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY - 1 : DEF_INTERRUPT_PRIORITY - 2);
        	ECLIC_SetPriorityIRQ(IRQ_SPI2_VECTOR, as_master ? DEF_INTERRUPT_PRIORITY : DEF_INTERRUPT_PRIORITY + 1);
*/
    }else {
        LOGD("%s: illegal SPI device (index = %d)!\r\n", __func__, index);
        return NULL;
    }

    // master or slave
    control = (as_master ? CSK_SPI_MODE_MASTER : CSK_SPI_MODE_SLAVE);

    // tx mode
    switch (txmode) {
    case DMA_IO:
        control |= CSK_SPI_TXIO_DMA;
        break;
    case PIO_IO:
        control |= CSK_SPI_TXIO_PIO;
        break;
    case NO_IO:
    default:
        control |= CSK_SPI_TXIO_AUTO;
    }

    // rx mode
    switch (rxmode) {
    case DMA_IO:
        control |= CSK_SPI_RXIO_DMA;
        break;
    case PIO_IO:
        control |= CSK_SPI_RXIO_PIO;
        break;
    case NO_IO:
    default:
        control |= CSK_SPI_RXIO_AUTO;
    }

    // frame format
    frm_fmt &= CSK_SPI_FRAME_FORMAT_Msk;
    if (frm_fmt == 0 || frm_fmt > CSK_SPI_CPOL1_CPHA1) {
        LOGD("%s: illegal frame format!\r\n", __func__);
        return NULL;
    }
    control |= frm_fmt;

    // data bits
    if (data_bits != 40 && (data_bits == 0 || data_bits > 32)) {
        LOGD("%s: illegal data_bits (%d)!\r\n", __func__, data_bits);
        return NULL;
    }
    control |= CSK_SPI_DATA_BITS( data_bits );

    // bit order
    control |= (msb_order ? CSK_SPI_MSB_LSB : CSK_SPI_LSB_MSB);

    // call SPI_Initialize
#if NO_SPI_CS
//    ret = SPI_Initialize_NCS(spi_dev, (as_master ? SPI_DrvEvent_m : SPI_DrvEvent_s), (uint32_t)spi_dev, set_spi_cs); // GPIO pulldown/pullup
//    ret = SPI_Initialize_NCS(spi_dev, (as_master ? SPI_DrvEvent_m : SPI_DrvEvent_s), (uint32_t)spi_dev, set2_spi_cs); // GPIO oen_frc
//    ret = SPI_Initialize_NCS(spi_dev, (as_master ? SPI_DrvEvent_m : SPI_DrvEvent_s), (uint32_t)spi_dev, set3_spi_cs); // GPIO pull_frc
    ret = SPI_Initialize_NCS(spi_dev, (as_master ? SPI_DrvEvent_m : SPI_DrvEvent_s), (uint32_t)spi_dev, SPI_Pull_CS); // internal REG control
#else
    ret = SPI_Initialize(spi_dev, (as_master ? SPI_DrvEvent_m : SPI_DrvEvent_s), (uint32_t)spi_dev);
#endif
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: SPI_Initialize (spi_no = %d) call failed!!\r\n", __func__, index);
        return NULL;
    }

    // call SPI_PowerControl
    ret = SPI_PowerControl(spi_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: SPI_PowerControl (spi_no = %d) call failed!!\r\n", __func__, index);
        return NULL;
    }

    // call SPI_Control, and arg = bus_speed when as master
    ret = SPI_Control(spi_dev, control, bus_speed);
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: SPI_Control (spi_no = %d) call failed!!\r\n", __func__, index);
        return NULL;
    }

    if (as_master)
        spi_state_m = SPI_STATE_INITIALIZED;
    else {
        spi_state_s = SPI_STATE_INITIALIZED;

    #if NO_SPI_CS
        if (!as_master) {
        //raise_spi_cs(index);
        //raise2_spi_cs(index);
        //raise3_spi_cs(index);
        SPI_Enable_Pull_CS(spi_dev, 1);
        SPI_Pull_CS(spi_dev, 1);
        }
    #endif
    }

    return spi_dev;
}


static void close_spi(void *spi_dev)
{
//#if NO_SPI_CS
//    raise_spi_cs(SPI_Index(spi_dev));
//    //init_spi_cs(SPI_Index(spi_dev));
//#endif

    if (spi_dev != NULL) {
        SPI_PowerControl (spi_dev, CSK_POWER_OFF);
        SPI_Uninitialize(spi_dev);
    }
}

int memcmp_bits (const void *a1, const void *a2, size_t size, uint32_t data_bits)
{
    if (data_bits == 0 || data_bits > 32) {
        LOGD("%s: data_bits = %d, ERROR!!\n", __func__, data_bits);
        return -1;
    }

    if (data_bits == 8 || data_bits == 16 || data_bits == 32)
        return memcmp (a1, a2, size);

    uint32_t count, i;
    if (data_bits < 8) {
        uint8_t mask = (1 << data_bits) - 1;
        uint8_t *b1 = (uint8_t *)a1;
        uint8_t *b2 = (uint8_t *)a2;
        count = size;
        for (i = 0; i < count; i++, b1++, b2++) {
            if ((*b1 ^ *b2) & mask)
                return 1; // different, which is bigger? assume the former...
        } // end for
        return 0; // same

    } else if (data_bits < 16) {
        uint16_t mask = (1 << data_bits) - 1;
        uint16_t *b1 = (uint16_t *)a1;
        uint16_t *b2 = (uint16_t *)a2;
        count = (size + 1) / 2;
        for (i = 0; i < count; i++, b1++, b2++) {
            if ((*b1 ^ *b2) & mask)
                return 1; // different, which is bigger? assume the former...
        } // end for
        return 0; // same

    } else if (data_bits < 32) {
        uint32_t mask = (1 << data_bits) - 1;
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

static bool
check_spim_spis_xfer(void *spi_m, void *spi_s, uint32_t data_bits, uint32_t count)
{
    bool ret1 = true, ret2 = true;
    bool xret = true;
    uint32_t i, send_cnt, recv_cnt;
    uint32_t uint_bytes, total_bytes;
    uint8_t *buf8;

    //assert(spi_m != NULL && spi_s != NULL);
    if (spi_m == NULL || spi_s == NULL)
        return false;

    assert(data_bits > 0 && data_bits <= 32);
    uint_bytes = DATA_BYTES(data_bits);

    // initialize all buffers
    buf8 = (uint8_t*)buf_send_m; // buffer for master send
    buf8[0] = 0xa5; //0xbd; //0x5a; //0xff; //
    for (i=1; i<sizeof(buf_send_m); i++) {
        buf8[i] = (i*2+1) & 0xFF;
    }

    buf8 = (uint8_t*)buf_send_s; // buffer for slave send
    buf8[0] = 0xbd; //0x5a; //0xff; //0xa5; //0xaf; //0xb3; //
    for (i=1; i<sizeof(buf_send_s); i++) {
        buf8[i] = (i*2+1) & 0xFF;
    }

    //Create binary semaphore
//    semCanXfer = xSemaphoreCreateBinary(); // initially non-signaled

    for (i=0; i<TEST_CYCLES; i++) {
        //count is the data number of VALID data sent by SPI Master (or Slave), while
        //BYTE_CNT(count) is the bytes number of VALID data sent by SPI Master (or Slave)

        INIT_RUN_FLAG();

        send_cnt = count;
        memset(buf_recv_m, 0, sizeof(buf_recv_m)); // buffer for master receive
        memset(buf_recv_s, 0, sizeof(buf_recv_s)); // buffer for slave receive

        //---------------------------------------
        // SPI Master send, SPI Slave receive
        //---------------------------------------

        // slave is about to receive data
        spi_state_s = SPI_STATE_RECVING;
//        SPI_Receive(spi_s, buf_recv_s, BUF_SIZE);
        SPI_Receive(spi_s, buf_recv_s, count);
//#if NO_SPI_CS
//        lower_spi_cs(SPI_Index(spi_s));
//#endif

        //vTaskDelay(1);
        //nos_delay_ms(1); //10
        //__ISB();

        // master send data initially
        spi_state_m = SPI_STATE_SENDING;
        SPI_Send(spi_m, buf_send_m, send_cnt);

        // wait xfer complete
//        xSemaphoreTake(semCanXfer, portMAX_DELAY);

        ret1 = wait_send_recv_done_timeout(5000);
        recv_cnt = SPI_GetDataCount(spi_s);
        if (!ret1) { // 5000ms = 5s
            SPI_Control(spi_s, CSK_SPI_ABORT_TRANSFER, 0);
            LOGD("[Warning] SPI slave receive timeout!!\r\n");
        }

        total_bytes = send_cnt * uint_bytes;
#if SLV_CMD_PHA_EN
        total_bytes -= 2; // remove 2 bytes: cmd + dummy
#endif

#if SLV_CMD_PHA_EN
        if (recv_cnt != DATA_CNT((BYTE_CNT(send_cnt, data_bits) - 2), data_bits)) {
#else
        if (recv_cnt != send_cnt) {
#endif
            LOGD("[Warning] Master sent %d (valid %d) data, but slave received %d data!\n",
                    send_cnt, count, recv_cnt);
            ret1 = false;
#if SLV_CMD_PHA_EN
//        } else if ( memcmp(buf_recv_s, ((uint8_t*)buf_send_m)+2, total_bytes) ) {
        } else if ( memcmp_bits(buf_recv_s, ((uint8_t*)buf_send_m)+2, total_bytes, data_bits) ) {
#else
//        } else if ( memcmp(buf_recv_s, buf_send_m, total_bytes) ) {
        } else if ( memcmp_bits(buf_recv_s, buf_send_m, total_bytes, data_bits) ) {
#endif
            LOGD("[Warning] Data sent by master differed from the one received by slave!\n");
            LOGD("buf_recv_s start = 0x%x, end = 0x%x\n", (uint32_t)buf_recv_s, (uint32_t)buf_recv_s+BUF_SIZE*4);
            ret1 = false;
        } else {
            ret1 = true;
            LOGD("@@@@@@@ DATA is same (Master Sent == Slave Received, %d bytes)\n", total_bytes);
        }

        if (!ret1) {
            LOGD("  Master Send (head bytes, sent_bytes = %d):", total_bytes);
            HEXP((uint8_t *)buf_send_m, total_bytes < 16 ? total_bytes : 16);
            if (total_bytes > 16) {
                uint8_t *d = (uint8_t *)buf_send_m;
                d += total_bytes - 16;
                LOGD("  Master Send (tail bytes):");
                HEXP(d, 16);
            }

            total_bytes = recv_cnt * uint_bytes;
            LOGD("  Slave Receive (head bytes, recv_bytes = %d):", total_bytes);
            HEXP((uint8_t *)buf_recv_s, total_bytes < 16 ? total_bytes : 16);
            if (total_bytes > 16) {
                uint8_t *d = (uint8_t *)buf_recv_s;
                d += total_bytes - 16;
                LOGD("  Slave Receive (tail bytes):");
                HEXP(d, 16);
            }
        } // end if !ret1

        nos_delay_ms(100);
/**/

        //---------------------------------------
        // SPI Slave Send, SPI Master Receive
        //---------------------------------------

        // slave is about to send data
        spi_state_s = SPI_STATE_SENDING;
        SPI_Send(spi_s, buf_send_s, send_cnt);
//#if NO_SPI_CS
//        lower_spi_cs(SPI_Index(spi_s));
//#endif

        // make sure there is at least 1 data in spi slave FIFO, ready for slave sent!
        nos_delay_ms(10); //100 //1000
        //vTaskDelay(1);

        // master receive data initially
        spi_state_m = SPI_STATE_RECVING;
        SPI_Receive(spi_m, buf_recv_m, send_cnt);

        // wait xfer complete
//        xSemaphoreTake(semCanXfer, portMAX_DELAY);

        ret2 = wait_send_recv_done_timeout(5000);
        recv_cnt = SPI_GetDataCount(spi_m);
        if (!ret2) { // 5000ms = 5s
            SPI_Control(spi_m, CSK_SPI_ABORT_TRANSFER, 0);
            LOGD("[Warning] SPI master RX or slave TX timeout!!\r\n");
        }

        total_bytes = send_cnt * uint_bytes;

#if SLV_CMD_PHA_EN
        if (DATA_CNT((BYTE_CNT(recv_cnt, data_bits) - 2), data_bits) != send_cnt) {
#else
        if (recv_cnt != send_cnt) {
#endif
            LOGD("[Warning] Slave sent %d data, but master received %d data!\n", send_cnt, recv_cnt);
            ret2 = false;
#if SLV_CMD_PHA_EN
//        } else if ( memcmp(((uint8_t*)buf_recv_m)+2, buf_send_s, total_bytes) ) {
        } else if ( memcmp_bits(((uint8_t*)buf_recv_m)+2, buf_send_s, total_bytes, data_bits) ) {
#else
//        } else if ( memcmp(buf_recv_m, buf_send_s, total_bytes) ) {
        } else if ( memcmp_bits(buf_recv_m, buf_send_s, total_bytes, data_bits) ) {
#endif
            LOGD("[Warning] Data sent by slave differed from the one received by master!\n");
            ret2 = false;
        } else {
            ret2 = true;
            LOGD("###### DATA is same (Slave Sent := Master Received, %d bytes)\n", total_bytes);
        }

        if (!ret2) {
            LOGD("  Slave Send (head bytes, sent_bytes = %d):", total_bytes);
            HEXP((uint8_t *)buf_send_s, total_bytes < 16 ? total_bytes : 16);
            if (total_bytes > 16) {
                uint8_t *d = (uint8_t *)buf_send_s;
                d += total_bytes - 16;
                LOGD("  Slave Send (tail bytes):");
                HEXP(d, 16);
            }

            total_bytes = recv_cnt * uint_bytes;
            LOGD("  Master Receive (head bytes, recv_bytes = %d):", total_bytes);
            HEXP((uint8_t *)buf_recv_m, total_bytes < 16 ? total_bytes : 16);
            if (total_bytes > 16) {
                uint8_t *d = (uint8_t *)buf_recv_m;
                d += total_bytes - 16;
                LOGD("  Master Receive (tail bytes):");
                HEXP(d, 16);
            }
        } // end if !ret2

        nos_delay_ms(100);
/**/
/*
        //---------------------------------------
        // SPI Slave <--> SPI Master duplex transfer
        //---------------------------------------

        //TODO: bi-direction transfer check operation?
        memset(buf_recv_m, 0, sizeof(buf_recv_m)); // buffer for master receive
        memset(buf_recv_s, 0, sizeof(buf_recv_s)); // buffer for slave receive

        // master / slave is about to transfer data
        spi_state_s = SPI_STATE_XFERING;
        spi_state_m = SPI_STATE_XFERING;
        SPI_Transfer(spi_s, buf_send_s, buf_recv_s, count);
        // make sure there is at least 1 data in spi slave FIFO, ready for slave sent!
        nos_delay_ms(20); //100 //200
        //vTaskDelay(1);
        SPI_Transfer(spi_m, buf_send_m, buf_recv_m, count);

        // wait xfer complete
        xret = wait_xfer_done_timeout(5000);
        send_cnt = SPI_GetDataCount(spi_m);
        recv_cnt = SPI_GetDataCount(spi_s);
        if (!xret) { // 5000ms = 5s
            SPI_Control(spi_s, CSK_SPI_ABORT_TRANSFER, 0);
            SPI_Control(spi_m, CSK_SPI_ABORT_TRANSFER, 0);
            LOGD("[Warning] SPI master/slave transfer timeout!!\r\n");
        }

        total_bytes = send_cnt * uint_bytes;
#if SLV_CMD_PHA_EN
        total_bytes -= 2; // remove 2 bytes: cmd + dummy
#endif

        xret = true;
        if (recv_cnt != send_cnt) {
            LOGD("[Warning] Master XFER %d data, but slave received %d data!\n",
                    send_cnt, recv_cnt);
            xret = false;
        } else {

#if SLV_CMD_PHA_EN
//            if ( memcmp(buf_recv_s, ((uint8_t*)buf_send_m)+2, total_bytes) ) {
            if ( memcmp_bits(buf_recv_s, ((uint8_t*)buf_send_m)+2, total_bytes, data_bits) ) {
#else
//            if ( memcmp(buf_recv_s, buf_send_m, total_bytes) ) {
            if ( memcmp_bits(buf_recv_s, buf_send_m, total_bytes, data_bits) ) {
#endif
                LOGD("[Warning] Data XFER by master differed from the one received by slave!\n");
                xret = false;
                LOGD("  Master Send (head bytes, sent_bytes = %d):", total_bytes);
                HEXP((uint8_t *)buf_send_m, total_bytes < 16 ? total_bytes : 16);
                LOGD("  Slave Receive (head bytes, recv_bytes = %d):", total_bytes);
                HEXP((uint8_t *)buf_recv_s, total_bytes < 16 ? total_bytes : 16);
            } else {
                //xret = true;
                LOGD("@@@@@@@ DATA is same (Master XFER == Slave Received, %d bytes)\n", total_bytes);
            }

#if SLV_CMD_PHA_EN
//            if ( memcmp(((uint8_t*)buf_recv_m)+2, buf_send_s, total_bytes) ) {
            if ( memcmp_bits(((uint8_t*)buf_recv_m)+2, buf_send_s, total_bytes, data_bits) ) {
#else
//            if ( memcmp(buf_recv_m, buf_send_s, total_bytes) ) {
            if ( memcmp_bits(buf_recv_m, buf_send_s, total_bytes, data_bits) ) {
#endif
                LOGD("[Warning] Data XFER by slave differed from the one received by master!\n");
                xret = false;
                LOGD("  Slave Send (head bytes, sent_bytes = %d):", total_bytes);
                HEXP((uint8_t *)buf_send_s, total_bytes < 16 ? total_bytes : 16);
                LOGD("  Master Receive (head bytes, recv_bytes = %d):", total_bytes);
                HEXP((uint8_t *)buf_recv_m, total_bytes < 16 ? total_bytes : 16);
            } else {
                //xret = true;
                LOGD("@@@@@@@ DATA is same (Slave XFER == Master Received, %d bytes)\n", total_bytes);
            }
        }
/**/
    } // end for i

    return (ret1 && ret2 && xret);
}


//void record_result(bool expected, bool returned, char * hint, ...)
//{
//    bool OK = (returned == expected);
//
//    va_list ap;
//
//    va_start(ap, hint);
//
//    LOGD("----------------------------------");
//    TC_total++;
//    if (OK) {
//        TC_pass++;
//        LOGD("PASSED: %s", hint, ap);
//    } else {
//        TC_fail++;
//        LOGD("FAILED: %s", hint, ap);
//    }
//    LOGD("----------------------------------\r\n\r\n");
//    va_end(ap);
//}

void record_result(bool expected, bool returned, char * hint, ...)
{
    bool OK = (returned == expected);

    va_list ap;
    va_start(ap, hint);

    logDbg("----------------------------------\r\n");

    TC_total++;
    if (OK) {
        TC_pass++;
        logDbg("PASSED: ");
        vprintf(hint, ap);
        logDbg("\r\n");
    } else {
        TC_fail++;
        logDbg("FAILED: ");
        vprintf(hint, ap);
        logDbg("\r\n");
    }

    va_end(ap);
    logDbg("----------------------------------\r\n\r\n");
}


#if (IC_BOARD == 0)
//#define DEF_BUS_SPEED   250000 // default set to 500KHz on FPGA
//#define DEF_BUS_SPEED   500000 // default set to 500KHz on FPGA
//#define DEF_BUS_SPEED   1000000 // default set to 1MHz on FPGA
#define DEF_BUS_SPEED   2000000; // default set to 2MHz on FPGA
//#define DEF_BUS_SPEED   10000000; // default set to 10MHz on FPGA
//#define DEF_BUS_SPEED   100000; // default set to 100KHz on FPGA
//#define DEF_BUS_SPEED   50000; // default set to 100KHz on FPGA
//#define MAX_SCLK_FREQ    (DEF_MAIN_FREQUENCE / 2) // SPI module input frequency, NOT SPI CLK
#define MAX_SCLK_FREQ    (IC_BOARD_FPGA_FIX_FREQ / 2) // SPI module input frequency, NOT SPI CLK
#else
//#define DEF_BUS_SPEED   30000000 // default set to 30MHz on ASIC
//#define DEF_BUS_SPEED   20000000 // default set to 20MHz on ASIC

//#define DEF_BUS_SPEED   15000000 // default set to 15MHz on ASIC
//#define DEF_BUS_SPEED   10000000 // default set to 10MHz on ASIC

//#define DEF_BUS_SPEED   6000000 // default set to 6MHz on ASIC
#define DEF_BUS_SPEED   10000000 // default set to 10MHz on ASIC
//#define DEF_BUS_SPEED   2000000 // default set to 2MHz on ASIC
#define MAX_SCLK_FREQ    PCLKFREQ() //FIXME: SPI module input frequency, NOT SPI CLK
#endif

#define DEF_XFER_CNT    (BUF_SIZE) //(BUF_SIZE-1) //32 //16 //1 //

void check_spi_xfer_cases(uint32_t idx_m, uint32_t idx_s)
{
    bool ret;
    void *spi_m = NULL, *spi_s = NULL;
    uint32_t data_bits = 8;
    uint32_t bus_speed = DEF_BUS_SPEED;
    uint32_t count = DEF_XFER_CNT;


/*
    // SPI-SPI XFER 1: CPOL0_CPHA0
    LOGD("Check SPI-SPI XFER: PIO, CPOL0_CPHA0, 8, MSB");
    count = 520; //32; //16; //7; //1; //2; //4; //
    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "PIO, CPOL0_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;
    return;
*/

//#define SYSCTRL_CFG    ((SYSCFG_RegDef* ) CMN_SYSCTRL_BASE)

//    int i;
//    for(i=0; i<4; i++) {
    // SPI-SPI XFER 2: CPOL0_CPHA1
//    SYSCTRL_CFG->REG_SW_RESET2.all = 0x30;
//    nos_delay_ms(100);

//    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA1, 8, MSB");
//    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
//    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
//    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
//    record_result(true, ret, "DMA, CPOL0_CPHA1, 8, MSB");

//    LOGD("Check SPI-SPI XFER: PIO, CPOL0_CPHA0, 8, MSB");
//    //count = 16;//7; //512; //1; //2; //4; //
//    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
//    spi_s = open_spi(idx_s, false, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
//    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
//    record_result(true, ret, "PIO, CPOL0_CPHA0, 8, MSB");

//    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 8, LSB");
//    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, false, bus_speed);
//    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, false, bus_speed);
//    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
//    record_result(true, ret, "DMA, CPOL0_CPHA0, 8, LSB");

//    data_bits = 8; //32; // 1; //2; // 3; // 4; // 10; //
//    count = 50; //511; //512; //32; //1; //3; //7; //
//    //count = 8; //4; //50; //511; //512; //
//    LOGD("Check SPI-SPI XFER: DMA, CPOL1_CPHA1, %d, MSB", data_bits);
//    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits, true, bus_speed);
//    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, 40/*data_bits*/, true, bus_speed);
//    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
//    record_result(true, ret, "DMA, CPOL1_CPHA1, %d, MSB", data_bits);

//    LOGD("Check SPI-SPI XFER: master PIO, CPOL0_CPHA0, 8, MSB");
//    //count = 16;//7; //512; //1; //2; //4; //
//    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
//    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
//    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
//    record_result(true, ret, "master PIO, CPOL0_CPHA0, 8, MSB");

//    close_spi(spi_m); spi_m = NULL;
//    close_spi(spi_s); spi_s = NULL;

//    }
//    return;


    // SPI-SPI XFER 1: CPOL0_CPHA0
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 8, MSB");
    //LOGD("Check SPI-SPI XFER: PIO, CPOL0_CPHA0, 8, MSB");
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    //spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    //spi_s = open_spi(idx_s, false, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    //record_result(true, ret, "PIO, CPOL0_CPHA0, 8, MSB");
    record_result(true, ret, "DMA, CPOL0_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 2: CPOL0_CPHA1
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA1, 8, MSB");
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA1, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;


    // SPI-SPI XFER 3: CPOL1_CPHA0
    LOGD("Check SPI-SPI XFER: DMA, CPOL1_CPHA0, 8, MSB");
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL1_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 4: CPOL1_CPHA1
    LOGD("Check SPI-SPI XFER: DMA, CPOL1_CPHA1, 8, MSB");
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL1_CPHA1, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 5: data_bits = 16
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 16, MSB");
    data_bits = 16;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 16, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 6: data_bits = 32
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 32, MSB");
    data_bits = 32;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 32, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 7: data_bits = 1 (data_bits < 8)
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 1, MSB");
    data_bits = 1;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 1, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 8: data_bits = 10 (8 < data_bits < 16)
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 10, MSB");
    data_bits = 10;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 10, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 9: data_bits = 19 (16 < data_bits < 24)
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 19, MSB");
    data_bits = 19;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 19, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 10: data_bits = 30 (24 < data_bits < 32)
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 30, MSB");
    data_bits = 30;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 30, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 11: LSB, 32bits
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 32, LSB");
    data_bits = 32;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, false, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, false, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 32, LSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    data_bits = 8;

    // SPI-SPI XFER 12: LSB, 8bits
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 8, LSB");
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, false, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, false, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 8, LSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 13: MAX_SCLK_FREQ / 2
    bus_speed = MAX_SCLK_FREQ / 2;
    //LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 8, MSB, %d", bus_speed);
    //spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    //spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA1, 8, MSB, %d", bus_speed);
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    //record_result(true, ret, "DMA, CPOL0_CPHA0, 8, MSB, %d", bus_speed);
    record_result(true, ret, "DMA, CPOL0_CPHA1, 8, MSB, %d", bus_speed);
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 14: MAX_SCLK_FREQ / 4
    bus_speed = MAX_SCLK_FREQ / 4;
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA1, 8, MSB, %d", bus_speed);
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA1, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA1, 8, MSB, %d", bus_speed);
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 15: MAX_SCLK_FREQ / 256 (Min. freq by SPI internal divider)
    //bus_speed = MAX_SCLK_FREQ / 256;
    bus_speed = MAX_SCLK_FREQ / 255;
    LOGD("Check SPI-SPI XFER: DMA, CPOL0_CPHA0, 8, MSB, Min:%d", bus_speed);
    data_bits = 8;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "DMA, CPOL0_CPHA0, 8, MSB, Min:%d", bus_speed);
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    bus_speed = DEF_BUS_SPEED;

    // SPI-SPI XFER 16: master PIO
    LOGD("Check SPI-SPI XFER: master PIO, CPOL0_CPHA0, 8, MSB");
    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "master PIO, CPOL0_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 17: slave PIO
    LOGD("Check SPI-SPI XFER: slave PIO, CPOL0_CPHA0, 8, MSB");
    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "slave PIO, CPOL0_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 18: master/slave PIO
    LOGD("Check SPI-SPI XFER: master/slave PIO, CPOL0_CPHA0, 8, MSB");
    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "master/slave PIO, CPOL0_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // new added after Sept.3

    // SPI-SPI XFER 19: 1 BYTE
    LOGD("Check SPI-SPI XFER: 1 BYTE, DMA, CPOL0_CPHA0, 8, MSB");
    count = 1;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "1 BYTE, DMA, CPOL0_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    // SPI-SPI XFER 20: 7 BYTES
    LOGD("Check SPI-SPI XFER: 7 BYTES, DMA, CPOL0_CPHA0, 8, MSB");
    count = 7;
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL0_CPHA0, data_bits, true, bus_speed);
    ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count);
    record_result(true, ret, "7 BYTES, DMA, CPOL0_CPHA0, 8, MSB");
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;


    count = DEF_XFER_CNT;
}


void check_spi_master_8bit_merge_cases(uint32_t idx_m, uint32_t idx_s)
{
    bool ret;
    void *spi_m = NULL, *spi_s = NULL;
    uint32_t data_bits = 8;
    uint32_t bus_speed = DEF_BUS_SPEED;
    uint32_t count = DEF_XFER_CNT;

    uint32_t i;
    //uint32_t count_list[] = { 32 }; // 1, 3, 5, 20
    uint32_t count_list[] = { 512, 511, 50, 20, 1, 3, 7 }; // with CS, open OR close EndInt
    //uint32_t count_list[] = { 512, 511, 50, 20, 8, 4 }; // no CS
    data_bits = 8;

    LOGD("((((((((((((((  DMA )))))))))))))))\r\n");
    LOGD("Check SPI-SPI XFER: DMA RX, CPOL1_CPHA1, %dbit DataMerge, MSB", data_bits);
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits+32, true, bus_speed); //
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits, true, bus_speed);
    for (i=0; i<ARRAY_SIZE(count_list); i++) {
        ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count_list[i]);
        record_result(true, ret, "DMA RX, CPOL1_CPHA1, %dbit DataMerge, MSB, %d items", data_bits, count_list[i]);
    }
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    LOGD("((((((((((((((  PIO )))))))))))))))\r\n");
    LOGD("Check SPI-SPI XFER: PIO RX, CPOL1_CPHA1, %dbit DataMerge, MSB", data_bits);
    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL1_CPHA1, data_bits+32, true, bus_speed); //
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits, true, bus_speed);
    for (i=0; i<ARRAY_SIZE(count_list); i++) {
        ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count_list[i]);
        record_result(true, ret, "PIO RX, CPOL1_CPHA1, %dbit DataMerge, MSB, %d items", data_bits, count_list[i]);
    }
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;
    return;
}

void check_spi_slave_8bit_merge_cases(uint32_t idx_m, uint32_t idx_s)
{
    bool ret;
    void *spi_m = NULL, *spi_s = NULL;
    uint32_t data_bits = 8;
    uint32_t bus_speed = DEF_BUS_SPEED;
    uint32_t count = DEF_XFER_CNT;

    uint32_t i;
    //uint32_t count_list[] = { 20 }; // 1, 3, 5
    //uint32_t count_list[] = { 512, 511, 50, 20, 1, 3, 7 }; // with CS, open OR close EndInt
    uint32_t count_list[] = { 512, 511, 50, 20, 8, 4 }; // no CS
    data_bits = 8;

    LOGD("((((((((((((((  DMA )))))))))))))))\r\n");
    LOGD("Check SPI-SPI XFER: DMA RX, CPOL1_CPHA1, %dbit DataMerge, MSB", data_bits);
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits+32, true, bus_speed); //
    for (i=0; i<ARRAY_SIZE(count_list); i++) {
        ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count_list[i]);
        record_result(true, ret, "DMA RX, CPOL1_CPHA1, %dbit DataMerge, MSB, %d items", data_bits, count_list[i]);
    }
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;

    LOGD("((((((((((((((  PIO )))))))))))))))\r\n");
    LOGD("Check SPI-SPI XFER: PIO RX, CPOL1_CPHA1, %dbit DataMerge, MSB", data_bits);
    spi_m = open_spi(idx_m, true, DMA_IO, DMA_IO, CSK_SPI_CPOL1_CPHA1, data_bits, true, bus_speed);
    spi_s = open_spi(idx_s, false, PIO_IO, PIO_IO, CSK_SPI_CPOL1_CPHA1, data_bits+32, true, bus_speed); //
    for (i=0; i<ARRAY_SIZE(count_list); i++) {
        ret = check_spim_spis_xfer(spi_m, spi_s, data_bits, count_list[i]);
        record_result(true, ret, "PIO RX, CPOL1_CPHA1, %dbit DataMerge, MSB, %d items", data_bits, count_list[i]);
    }
    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;
    return;
}


static bool
check_spim_spis_var_xfer(void *spi_m, void *spi_s, uint32_t mbits, uint32_t sbits, uint32_t mcount)
{
    bool ret1 = true;
    uint32_t i, scount, send_cnt, recv_cnt;
    uint32_t muint_bytes, suint_bytes, mbytes, sbytes;
    uint8_t *buf8;

    if (spi_m == NULL || spi_s == NULL)
        return false;

    assert(mbits > 0 && mbits <= 32 && sbits > 0 && sbits <= 32);
    muint_bytes = DATA_BYTES(mbits);
    suint_bytes = DATA_BYTES(sbits);
    scount = mcount * muint_bytes / suint_bytes;

    // initialize all buffers
    buf8 = (uint8_t*)buf_send_m; // buffer for master send
    buf8[0] = 0xf5; //0xa5; //0xbd; //0x5a; //0xff; //
    for (i=1; i<sizeof(buf_send_m); i++) {
        buf8[i] = (i*2+1) & 0xFF;
    }

    buf8 = (uint8_t*)buf_send_s; // buffer for slave send
    buf8[0] = 0xbd; //0x5a; //0xff; //0xa5; //0xaf; //0xb3; //
    for (i=1; i<sizeof(buf_send_s); i++) {
        buf8[i] = (i*2+1) & 0xFF;
    }

    for (i=0; i<TEST_VAR_CYCLES; i++) {
        //count is the data number of VALID data sent by SPI Master (or Slave), while
        //BYTE_CNT(count) is the bytes number of VALID data sent by SPI Master (or Slave)

        INIT_RUN_FLAG();

        send_cnt = mcount;
        memset(buf_recv_m, 0, sizeof(buf_recv_m)); // buffer for master receive
        memset(buf_recv_s, 0, sizeof(buf_recv_s)); // buffer for slave receive

        //---------------------------------------
        // SPI Master send, SPI Slave receive
        //---------------------------------------

        // slave is about to receive data
        spi_state_s = SPI_STATE_RECVING;
        //SPI_Receive(spi_s, buf_recv_s, BUF_SIZE);
        SPI_Receive(spi_s, buf_recv_s, scount);

        //nos_delay_ms(1); //10

        // master send data initially
        spi_state_m = SPI_STATE_SENDING;
        SPI_Send(spi_m, buf_send_m, send_cnt);

        ret1 = wait_send_recv_done_timeout(5000);
        recv_cnt = SPI_GetDataCount(spi_s);
        if (!ret1) { // 5000ms = 5s
            SPI_Control(spi_s, CSK_SPI_ABORT_TRANSFER, 0);
            LOGD("[Warning] SPI slave receive timeout!!\r\n");
        }

        mbytes = send_cnt * muint_bytes;
        sbytes = recv_cnt * suint_bytes;

#if SLV_CMD_PHA_EN
        mbytes -= 2; // remove 2 bytes: cmd + dummy
#endif

       if (sbytes != mbytes) {
            LOGD("[Warning] Master sent %d bytes (valid %d), but slave received %d bytes!\n", mbytes, mcount, sbytes);
            ret1 = false;
#if SLV_CMD_PHA_EN
        } else if ( memcmp_bits(buf_recv_s, ((uint8_t*)buf_send_m)+2, mbytes, mbits) ) {
#else
        } else if ( memcmp_bits(buf_recv_s, buf_send_m, mbytes, mbits) ) {
#endif
            LOGD("[Warning] Data sent by master differed from the one received by slave!\n");
            ret1 = false;
        } else {
            ret1 = true;
            LOGD("@@@@@@@ DATA is same (Master Sent == Slave Received, %d bytes)\n", mbytes);
        }

        if (!ret1) {
            LOGD("  Master Send (head bytes, sent_bytes = %d):", mbytes);
            HEXP((uint8_t *)buf_send_m, mbytes < 16 ? mbytes : 16);
            if (mbytes > 16) {
                uint8_t *d = (uint8_t *)buf_send_m;
                d += mbytes - 16;
                LOGD("  Master Send (tail bytes):");
                HEXP(d, 16);
            }

            sbytes = recv_cnt * suint_bytes;
            LOGD("  Slave Receive (head bytes, recv_bytes = %d):", sbytes);
            HEXP((uint8_t *)buf_recv_s, sbytes < 16 ? sbytes : 16);
            if (sbytes > 16) {
                uint8_t *d = (uint8_t *)buf_recv_s;
                d += sbytes - 16;
                LOGD("  Slave Receive (tail bytes):");
                HEXP(d, 16);
            }
        } // end if !ret1

        //nos_delay_ms(100);

    } // end for i

    return ret1;
}

// spi master send data with data_bits = 8, LSB_MSB order (for SPI slave is little endian!!)
// spi slave receive data with data_bits = 8, 16, 32 or 40 (8bit DATA MERGE)
void check_spi_var_xfer_cases(uint32_t idx_m, uint32_t idx_s)
{
    bool ret;
    void *spi_m = NULL, *spi_s = NULL;
    uint32_t i, mbits=8;
    uint32_t bus_speed = DEF_BUS_SPEED;
    uint32_t count = DEF_XFER_CNT;

#if SUPPORT_8BIT_DATA_MERGE
    uint32_t sbits_array[] = { 16, 32, 40 };
#else
    uint32_t sbits_array[] = { 16, 32};
#endif

    LOGD("Check SPI-SPI VAR_XFER: DMA, CPOL0_CPHA1, 8, LSB");
    count = 512; //1; //2; //4; //16;

    spi_m = open_spi(idx_m, true, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA1, mbits, false, bus_speed);
    spi_s = open_spi(idx_s, false, PIO_IO, PIO_IO, CSK_SPI_CPOL0_CPHA1, mbits, false, bus_speed);
    ret = check_spim_spis_var_xfer(spi_m, spi_s, mbits, mbits, count);
    record_result(true, ret, "mbits = %d, sbits = %d", mbits, mbits);

    for(i = 0; i < ARRAY_SIZE(sbits_array); i++) {
        ret = SPI_Control(spi_s, CSK_SPI_DATA_BITS(sbits_array[i]), 0);
        if (ret != CSK_DRIVER_OK) {
            LOGD("%s: SPI_Control: failed to set data_bits to %d!!\r\n", __func__, sbits_array[i]);
            continue;
        }

        ret = check_spim_spis_var_xfer(spi_m, spi_s, mbits, (sbits_array[i] == 40 ? 8 : sbits_array[i]), count);
        record_result(true, ret, "mbits = %d, sbits = %d", mbits, sbits_array[i]);
    } // end for

    close_spi(spi_m); spi_m = NULL;
    close_spi(spi_s); spi_s = NULL;
}

//#include "cmn_iomux_reg_venus.h" // for IOMux register map

// spi0 / spi1 IOMux
static void config_spi012_iomux()
{
//    CMN_IOMUX_RegDef* iom = (CMN_IOMUX_RegDef*)CMN_IOMUX_BASE;
//    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER1);  // MISO
//    iom->REG_PAD_GPIOA_21.bit.PAD_GPIOA_21_OEN_FRC = 1; // force using OEN_VAL
//    iom->REG_PAD_GPIOA_21.bit.PAD_GPIOA_21_OEN_VAL = 0; // OUT direction
//    iom->REG_PAD_GPIOA_21.bit.PAD_GPIOA_21_OUT_FRC = 1; // force using OUT_VAL
//    iom->REG_PAD_GPIOA_21.bit.PAD_GPIOA_21_OUT_VAL = 0; // output high

#if NO_SPI_CS
    init_spi_cs(0);
    init_spi_cs(1);
#endif //NO_SPI_CS

    //SPI0 pin configuration
    IOMuxManager_PinConfigure(SPI0_CLK_PIN);  // CLK
    IOMuxManager_PinConfigure(SPI0_CS_PIN);  // CS
    IOMuxManager_PinConfigure(SPI0_MOSI_PIN);  // MOSI
    IOMuxManager_PinConfigure(SPI0_MISO_PIN);  // MISO

   //SPI1 pin configuration
    IOMuxManager_PinConfigure(SPI1_CLK_PIN);  // CLK
    IOMuxManager_PinConfigure(SPI1_CS_PIN);  // CS
    IOMuxManager_PinConfigure(SPI1_MOSI_PIN);  // MOSI
    IOMuxManager_PinConfigure(SPI1_MISO_PIN);  //MISO


    //SPI2 pin configuration
    IOMuxManager_PinConfigure(SPI2_CLK_PIN);  // CLK
    IOMuxManager_PinConfigure(SPI2_CS_PIN);  // CS
    IOMuxManager_PinConfigure(SPI2_MOSI_PIN);  // MOSI
    IOMuxManager_PinConfigure(SPI2_MISO_PIN);  //MISO

}


//extern void BootClock_Init();
//extern void mpu_init( void );

int main()
{
    //BootClock_Init();

//    mpu_init();

    // Disable D-Cache
    DisableDCache();
    __RWMB();
    __FENCE_I();

    logInit(0, 115200); // uart0, baudrate=115200
    //logInit(2, 115200); // uart2, baudrate=115200

    // enable global interrupts (all levels of interrupts)
    enable_GINT();

    // init nos_timer
    nos_timer_init();

    // spi0 / spi1 / spi2iomux
    config_spi012_iomux();

    __HAL_CRM_SPI0_CLK_ENABLE();
    __HAL_CRM_SPI1_CLK_ENABLE();
    __HAL_CRM_SPI2_CLK_ENABLE();

//    // spi0 as master, spi1 as slave
//    LOGD("\r\n++++++++++ Master: SPI0,  Slave: SPI1 +++++++++++\r\n\r\n");
//    check_spi_xfer_cases(0, 1);
//
//    // spi1 as master, spi0 as slave
//    LOGD("\r\n++++++++++ Master: SPI1,  Slave: SPI0 +++++++++++\r\n\r\n");
//    check_spi_xfer_cases(1, 0);

//    // spi0 as master, spi2 as slave
//    LOGD("\r\n++++++++++ Master: SPI0,  Slave: SPI2 +++++++++++\r\n\r\n");
//    check_spi_xfer_cases(0,2);
//
//    // spi2 as master, spi0 as slave
//    LOGD("\r\n++++++++++ Master: SPI2,  Slave: SPI0 +++++++++++\r\n\r\n");
//    check_spi_xfer_cases(2,0);

    // spi1 as master, spi2 as slave
    LOGD("\r\n++++++++++ Master: SPI1,  Slave: SPI2 +++++++++++\r\n\r\n");
    check_spi_xfer_cases(1, 2);

    // spi2 as master, spi1 as slave
    LOGD("\r\n++++++++++ Master: SPI2,  Slave: SPI1 +++++++++++\r\n\r\n");
    check_spi_xfer_cases(2, 1);

//    // spi1 as master, spi2 as slave
//    LOGD("\r\n++++++++++ Master: SPI1,  Slave: SPI2 +++++++++++\r\n\r\n");
//    check_spi_var_xfer_cases(1,2);
//
//    // spi2 as master, spi1 as slave
//    LOGD("\r\n++++++++++ Master: SPI2,  Slave: SPI0 +++++++++++\r\n\r\n");
//    check_spi_var_xfer_cases(2,1);


#if SUPPORT_8BIT_DATA_MERGE
    // 8bit DATA MERGE cases (spi0 as master, spi1 as slave)
    check_spi_master_8bit_merge_cases(0, 1);
    check_spi_slave_8bit_merge_cases(0, 1);
#endif

    // change data_bits after CS is raised (spi0 as master, spi1 as slave)
    //check_spi_var_xfer_cases(0, 1);

    // show test statistics
    LOGD("\r\n===============================================\r\n");
    LOGD("===============================================\r\n");
    LOGD("===============================================\r\n");
    LOGD("SPI Test cases: total = %d, passed = %d, failed = %d\r\n", TC_total, TC_pass, TC_fail);

    return 0;
}
