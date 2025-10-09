/**
 ****************************************************************************************
 *
 * @file ble_drv.h
 *
 * @brief Header file - BLE driver API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BLE_DRV_H_
#define BLE_DRV_H_

#include "chip_id.h"
/**
 ****************************************************************************************
 * @addtogroup BLE_DRV
 * @ingroup
 *
 * @brief ble driver interface.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */


/*
 * DEFINES
 ****************************************************************************************
 */

enum plf_feat_idx {
    /// hcit feature
    PLF_FEAT_HCIT  = 0,
    /// core feature
    PLF_FEAT_CORE  = 1,
    /// stack feature
    PLF_FEAT_STACK = 2,
};


enum plf_hcit_feat {
    /// none of hcit support
    PLF_HCIT_NONE  = 0,
    /// uart hcit support
    PLF_HCIT_UART  = 1,
    /// usb hcit support
    PLF_HCIT_USB   = 2,

    /// aud hcit support
    PLF_HCIT_AUD   = 4,

    /// ahi hcit support
    PLF_HCIT_AHI   = 8,
};

enum plf_core_feat {
    /// none of core support
    PLF_CORE_NONE  = 0,
    /// core le support
    PLF_CORE_LE    = 1,
    /// core classic bt support
    PLF_CORE_BT    = 2,
    /// core le audio support
    PLF_CORE_ISO   = 4,
};

enum plf_stack_feat {
    /// none of stack support
    PLF_STACK_NONE  = 0,
    /// le stack support
    PLF_STACK_LE    = 1,
    /// classic bt stack support
    PLF_STACK_BT    = 2,
    /// mesh stack support
    PLF_STACK_MESH  = 4,
    /// le audio stack support
    PLF_STACK_LEA   = 8,
};


/// result of sleep state.
enum lsip_sleep_state
{
    /// Some activity pending, can not enter in sleep state
    LSIP_ACTIVE    = 0,
    /// CPU can be put in sleep state
    LSIP_CPU_SLEEP,
    /// IP could enter in deep sleep
    LSIP_DEEP_SLEEP,
};

/*
 * MACROS
 ****************************************************************************************
 */

#define MIN_POWER_LEVEL  (0)
#define MAX_POWER_LEVEL  (7)

/**
 * Prefetch time (in us)
 *  - Radio power up: 60us (worst case)
 *  - EM fetch: 30us (worst case at 26Mhz)
 *  - HW logic: 10us (worst case at 26Mhz)
 */
#if (RF_MAX2830_SUPPORT == 1)
#if (CHIP == vega || CHIP == vegap || CHIP == arcs || CHIP == vegah) 
#define MAX2830_WRITE_SPI_CONSUME_TIME_US 30
#define IP_PREFETCH_TIME_US       (100) //+32  max2830_write_spi_time_us
#else
#define MAX2830_WRITE_SPI_CONSUME_TIME_US 0
#define IP_PREFETCH_TIME_US       (100+MAX2830_WRITE_SPI_CONSUME_TIME_US)
#endif
#else //(RF_MAX2830_SUPPORT == 1)
#define MAX2830_WRITE_SPI_CONSUME_TIME_US 0
#if (SINGLE_RUN_ON_DUAL)                                                                                                                                                        
#define IP_PREFETCH_TIME_US       (110)
#else
#define IP_PREFETCH_TIME_US       (100)
#endif
#endif //(RF_MAX2830_SUPPORT == 1)


/**
 * Prefetch Abort time (in us)
 *
 * - EM fetch:
 *    - HW CS Update is 18 access
 *    - HW Tx Desc Update is 1 access
 *    - HW Rx Desc Update is 5 access
 *        => EM update at 26MHz Tx, Rx and CS is (18+1+5)*0.04*4 = 4us
 * - HW logic: 10us (worst case)
 * - Radio power down: 26 us for Ripple
 *
 * Prefetch abort time = prefetch time + 4 + 10 + 26
 */
#if (CHIP == vega || CHIP == vegap)// || CHIP == vegah)
#define IP_PREFETCHABORT_TIME_US  (171)//(140) // modify by jiangsheng.shi, 2022.07.07
#else
#define IP_PREFETCHABORT_TIME_US  (140)
#endif




 
/// UART
#define PLF_UART             1

/// UART 2
#define PLF_UART2            0

/*
 * USB
 ****************************************************************************************
 */

/// USB
#define PLF_USB              0


/// Possible errors detected by FW
#define    RESET_NO_ERROR         0x00000000
#define    RESET_MEM_ALLOC_FAIL   0xF2F2F2F2

/// Reset platform and stay in ROM
#define    RESET_TO_ROM           0xA5A5A5A5
/// Reset platform to main function
#define    RESET_TO_MAIN          0xB4B4B4B4
/// Reset platform and reload FW
#define    RESET_AND_LOAD_FW      0xC3C3C3C3

/*
 * ENUMERATIONS
 ****************************************************************************************
 */


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Function called when packet transmission/reception is finished.

 * @param[in]  dummy  Dummy data pointer returned to callback when operation is over.
 * @param[in]  status Ok if action correctly performed, else reason status code.
 *****************************************************************************************
 */
typedef void (*ble_eif_callback) (void*, uint8_t);

/**
 * Transport layer communication interface.
 */
struct ble_eif_api
{
    /**
     *************************************************************************************
     * @brief Starts a data reception.
     *
     * @param[out] bufptr      Pointer to the RX buffer
     * @param[in]  size        Size of the expected reception
     * @param[in]  callback    Pointer to the function called back when transfer finished
     * @param[in]  dummy       Dummy data pointer returned to callback when reception is finished
     *************************************************************************************
     */
    void (*read) (uint8_t *bufptr, uint32_t size, ble_eif_callback callback, void* dummy);

    /**
     *************************************************************************************
     * @brief Starts a data transmission.
     *
     * @param[in]  bufptr      Pointer to the TX buffer
     * @param[in]  size        Size of the transmission
     * @param[in]  callback    Pointer to the function called back when transfer finished
     * @param[in]  dummy       Dummy data pointer returned to callback when transmission is finished
     *************************************************************************************
     */
    void (*write)(uint8_t *bufptr, uint32_t size, ble_eif_callback callback, void* dummy);

    /**
     *************************************************************************************
     * @brief Enable Interface flow.
     *************************************************************************************
     */
    void (*flow_on)(void);

    /**
     *************************************************************************************
     * @brief Disable Interface flow.
     *
     * @return True if flow has been disabled, False else.
     *************************************************************************************
     */
    bool (*flow_off)(void);
};

/// Time information
/*@TRACE*/
typedef struct
{
    /// Integer part of the time (in half-slot)
    uint32_t hs;
    /// Fractional part of the time (in half-us) (range: 0-624)
    uint16_t hus;
    /// Bluetooth timestamp value (in us) 32 bits counter
    uint32_t bts;
} lsip_time_t;


/// API functions of the RF driver that are used by the BLE or BT software
struct ble_rf_api
{
    /// Function called upon HCI reset command reception
    void (*reset)(void);
    /// Function called to enable/disable force AGC mechanism (true: en / false : dis)
    void (*force_agc_enable)(bool);
    /// Function called when TX power has to be decreased for a specific link id
    bool (*txpwr_dec)(uint8_t);
    /// Function called when TX power has to be increased for a specific link id
    bool (*txpwr_inc)(uint8_t);
    /// Function called when TX power has to be set to max for a specific link id
    void (*txpwr_max_set)(uint8_t);
    /// Function called to convert a TX power CS power field into the corresponding value in dBm
    int8_t (*txpwr_dbm_get)(uint8_t, uint8_t);
    /// Function called to convert a power in dBm into a control structure tx power field
    uint8_t (*txpwr_cs_get)(int8_t, uint8_t);
    /// Function called to convert the RSSI read from the control structure into a real RSSI
    int8_t (*rssi_convert)(uint16_t);
    /// Function used to read a RF register
    uint32_t (*reg_rd)(uint32_t);
    /// Function used to write a RF register
    void (*reg_wr)(uint32_t, uint32_t);
    /// Function called to put the RF in deep sleep mode
    void (*sleep)(void);
    /// Index of minimum TX power
    uint8_t txpwr_min;
    /// Index of maximum TX power
    uint8_t txpwr_max;
    /// RSSI high threshold ('real' signed value in dBm)
    int8_t rssi_high_thr;
    /// RSSI low threshold ('real' signed value in dBm)
    int8_t rssi_low_thr;
    /// interferer threshold ('real' signed value in dBm)
    int8_t rssi_interf_thr;
    /// RF wakeup delay (in slots)
    uint8_t wakeup_delay;
    /// RF do calibration
    int (*calibrate)(void *param);
};

#if ((CHIP != vega) && (CHIP != vegap))
struct lsip_modem_env_api
{
    /// modem fsm reset
    void (*modem_fsm_reset)(void);
    /// modem start config phy
    void (*modem_config_phy)(uint8_t mode, uint8_t tx_rate, uint8_t rx_rate);
    /// modem config phy will be excuted at the 1.5 slot later
    void (*modem_config_cbk_phy)(uint8_t mode, uint8_t tx_rate, uint8_t rx_rate, uint8_t aux_rate, uint8_t edr, lsip_time_t time, uint8_t et_idx);
    /// modem end of frame event update phy
    void (*modem_at_eof_update_phy)(uint8_t mode);
    /// modem at the tx_en/rx_en phy irq update phy. begin of the frame event
    void (*modem_at_startup_update_phy)(uint8_t mode);
};
#else
struct lsip_modem_env_api
{
    /// modem fsm reset
    void (*modem_fsm_reset)(void);
    /// modem start config phy
    void (*ble_modem_config_phy)(uint8_t mode, uint8_t tx_rate, uint8_t rx_rate);
    /// modem config phy will be excuted at the 1.5 slot later
    void (*ble_modem_config_cbk_phy)(uint8_t mode, uint8_t tx_rate, uint8_t rx_rate, uint8_t aux_rate, uint8_t edr, lsip_time_t time, uint8_t et_idx);
    /// modem end of frame event update phy
    void (*ble_modem_at_eof_update_phy)(uint8_t mode);
    /// modem at the tx_en/rx_en phy irq update phy. begin of the frame event
    void (*ble_modem_at_startup_update_phy)(uint8_t mode);
};
#endif

struct lsip_modem_param_tag
{
    ///
    uint8_t current_mode;
    ///
    uint8_t last_mode;
    ///
    uint8_t tx_rate;
    ///
    uint8_t rx_rate;
    ///
    uint8_t aux_rate;
    ///br or edr
    uint8_t edr_en;
    ///add by jiangsheng.shi, 2022.07.07
    uint8_t et_idx;
    ///add by jiangsheng.shi, 2022.07.07
    uint8_t valid;
};

struct bt_sleep_api_str
{
    void (*sleep_init)(void);
    void (*enter_sleep)(uint16_t);
    bool (*is_power_on)(void);
    bool (*is_wakeup)(void);
    uint32_t (*get_wakeup_state)(void);
    bool (*is_bt_wakeup)(void);
    void (*en_32kHZ)(void);
    void (*rc_cali_init)(void);
    void (*rc_cali_start)(void);
    void (*rc_cali_irq_handler)(void);
    void (*clean_bt_wakeup_singnal)(void);

    //31270; //(1<<LS_RCCALI_CYCLE_LENGTH)*1000; //250000;   // 2^rccal_length *24M XTAL /24k rc clk
    uint32_t  rc_cali_result;
    uint32_t  rc_init_stat;
    // 0: correct time before rc calibration, next time use the cali result
    // 1: correct time after rc calibration, current time use the cali result
    uint8_t  rc_result_position;
    //about256count*40.8us=10445us  // 32*30.517= 970us  ~ 2^5temp 1ms may 2ms
    uint8_t  rc_cali_cycle_length;
    // 0: no supported  1: 32000Hz  2:32768Hz clock  3:rc32k
    uint8_t  rc_clock_mod;
};

struct lsip_dma_api_str
{
    uint8_t dma_ch;
    uint32_t (*dma_init)(void);
    uint32_t (*dma_uninit)(void);
    int32_t (*dma_copy)(uint8_t channel, void* p_dst_addr, const void* p_src_addr, uint32_t size);
};

struct lsip_external_api_str
{
    // init bt rf api
    uint8_t (*rf_api_init)(void *api);
    // init bt link, bt modem reg
    void (*bt_drv_init)(uint8_t);
    // init bt sleep api
    uint8_t (*bt_sleep_api_init)(void **api);
    void (*bt_dma_api_init)(void *api);
    //init sleep wakeup reg
    void (*bt_sleep_wakeup_reg_init)(void);
};

/*
 * GLOBAL VARIABLE DECLARATION
 ****************************************************************************************
 */


/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/// @} BLE_DRV
///

#endif // BLE_DRV_H_
