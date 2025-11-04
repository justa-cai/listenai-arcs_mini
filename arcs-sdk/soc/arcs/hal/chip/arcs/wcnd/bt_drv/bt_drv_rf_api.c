/**
****************************************************************************************
*
* @file bt_drv_rf_api.c
*
* @brief VEGA radio initialization and specific functions
*
* Copyright (C)
*
* $Rev: $
*
****************************************************************************************
*/

/**
****************************************************************************************
* @addtogroup BT_RF_API
* @ingroup RF
* @brief VEGA Radio Driver
*
* This is the driver block for Vega radio
* @{
****************************************************************************************
*/

/**
 *****************************************************************************************
 * INCLUDE FILES
 *****************************************************************************************
 */

#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition
#include "ble_drv.h"

#include "ble_plf_config.h"


#if (RF_MAX2830_SUPPORT == 1)
#include "rf_max2830.h"     // SW configuration
#endif

#define __STATIC


extern uint8_t ld_acl_tx_power_level_get(uint8_t link_id);
extern void    ld_acl_tx_power_level_set(uint8_t link_id, uint8_t tx_pwr_index);

#define TX_PPA_GAIN_BT_0_DBM  (-14)
#define TX_PPA_GAIN_BT_1_DBM  (-10)
#define TX_PPA_GAIN_BT_2_DBM  (-6)
#define TX_PPA_GAIN_BT_3_DBM  (-2)
#define TX_PPA_GAIN_BT_4_DBM  (1)
#define TX_PPA_GAIN_BT_5_DBM  (4)
#define TX_PPA_GAIN_BT_6_DBM  (7)
#define TX_PPA_GAIN_BT_7_DBM  (10)


#if 1
/**
 ****************************************************************************************
 * RADIO FUNCTION INTERFACE
 ****************************************************************************************
 **/
__STATIC void bt_rf_spi_tf(void)
{

}

/**
 ****************************************************************************************
 * @brief BTIPT specific read access
 *
 * @param[in] addr	  register address
 *
 * @return uint32_t value
 *****************************************************************************************
 */
__STATIC uint32_t bt_rf_reg_rd (uint32_t addr)
{
	return 0;
}

/**
 ****************************************************************************************
 * @brief BTIPT specific write access
 *
 * @param[in] addr	  register address
 * @param[in] value   value to write
 *
 * @return uint32_t value
 ****************************************************************************************
 */
__STATIC void bt_rf_reg_wr (uint32_t addr, uint32_t value)
{

}

__STATIC void bt_rf_reg_wr_full (uint32_t addr,uint16_t header, uint16_t value)
{

}




/**
 ****************************************************************************************
 * @brief BTIPT specific read access
 *
 * @param[in] addr	  register address
 * @param[in] size	  transfer size
 * @param[in] data	  pointer to the data array
 *
 * @return uint32_t value
 ****************************************************************************************
 **/
__STATIC void bt_rf_reg_burst_wr (uint32_t addr, uint8_t size, uint8_t *data)
{

}




/**
 *****************************************************************************************
 * @brief Init RF sequence after reset.
 *****************************************************************************************
 */
__STATIC void bt_rf_reset(void)
{
}

/**
 ****************************************************************************************
 * @brief ISR to be called in BLE ISR routine when RF Interrupt occurs.
 *****************************************************************************************
 */
__STATIC void bt_rf_force_agc_enable(bool en)
{
}

/**
 *****************************************************************************************
 * @brief Get TX power in dBm from the index in the control structure
 *
 * @param[in] txpwr_idx  Index of the TX power in the control structure
 * @param[in] modulation Modulation: 1 or 2 or 3 MBPS
 *
 * @return The TX power in dBm
 *
 *****************************************************************************************
 */
__STATIC int8_t bt_rf_txpwr_dbm_get(uint8_t txpwr_idx, uint8_t modulation)
{
    /* // 3DH5   GFSK +3dbm
    RFIF_P->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_GAIN_BT_0 = 1;  // -17.23
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_1 = 3;  // -12.76
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_2 = 4;  // -8.67
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_3 = 7;  // -5.34
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_4 = 8;  // -2
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_5 = 14; // 1.4
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_6 = 18; // 4.4
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_7 = 24; // 7.4 */



    //if tx power config modify, this must be modified
    int8_t rf_txpwr_table[MAX_POWER_LEVEL+1]= {TX_PPA_GAIN_BT_0_DBM, TX_PPA_GAIN_BT_1_DBM, TX_PPA_GAIN_BT_2_DBM, TX_PPA_GAIN_BT_3_DBM,
                                               TX_PPA_GAIN_BT_4_DBM, TX_PPA_GAIN_BT_5_DBM, TX_PPA_GAIN_BT_6_DBM, TX_PPA_GAIN_BT_7_DBM};

    if (txpwr_idx > MAX_POWER_LEVEL)
    {
        return rf_txpwr_table[MAX_POWER_LEVEL];
    }


	return rf_txpwr_table[txpwr_idx];
}

/**
 *****************************************************************************************
 * @brief Sleep function for BTIPT RF.
 *****************************************************************************************
 */
__STATIC void bt_rf_sleep(void)
{

}

/**
 *****************************************************************************************
 * @brief Convert RSSI to dBm
 *
 * @param[in] rssi_reg RSSI read from the HW registers
 *
 * @return The converted RSSI
 *
 *****************************************************************************************
 */
__STATIC int8_t bt_rf_rssi_convert (uint16_t rssi_reg)
{

    int8_t RssidBm = (int8_t)(((rssi_reg & 0x3FF)*18)>>5) - 116;

    return RssidBm;
}


#if (BT_EMB_PRESENT)
/**
 *****************************************************************************************
 * @brief Decrease the TX power by one step
 *
 * @param[in] link_id Link ID for which the TX power has to be decreased
 *
 * @return true when minimum power is reached, false otherwise
 *****************************************************************************************
 */
__STATIC bool bt_rf_txpwr_dec(uint8_t link_id)
{
	bool boMinpow = false;
    uint8_t tx_pwr_index;
    tx_pwr_index = ld_acl_tx_power_level_get(link_id);

    if (tx_pwr_index > MIN_POWER_LEVEL)
    {
        tx_pwr_index = tx_pwr_index - 1;
        ld_acl_tx_power_level_set(link_id, tx_pwr_index);
        if (tx_pwr_index == MIN_POWER_LEVEL)
        {
            boMinpow = true;
        }
    }
    else  //min pwr
    {
        boMinpow = true;
    }

	return(boMinpow);
}

/**
 *****************************************************************************************
 * @brief Increase the TX power by one step
 *
 * @param[in] link_id Link ID for which the TX power has to be increased
 *
 * @return true when maximum power is reached, false otherwise
 *****************************************************************************************
 */
__STATIC bool bt_rf_txpwr_inc(uint8_t link_id)
{
	bool boMaxpow = false;
    uint8_t tx_pwr_index;
    tx_pwr_index = ld_acl_tx_power_level_get(link_id);

    if (tx_pwr_index < MAX_POWER_LEVEL)
    {
        tx_pwr_index = tx_pwr_index + 1;
        ld_acl_tx_power_level_set(link_id, tx_pwr_index);
        if (tx_pwr_index == MAX_POWER_LEVEL)
        {
            boMaxpow = true;
        }
    }
    else
    {
        boMaxpow = true;
    }

	return(boMaxpow);
}

/**
 ****************************************************************************************
 * @brief Set the TX power to max
 *
 * @param[in] link_id	  Link Identifier
 ****************************************************************************************
 */
__STATIC void bt_rf_txpwr_max_set(uint8_t link_id)
{
    ld_acl_tx_power_level_set(link_id, MAX_POWER_LEVEL);
}
#endif // CFG_BT

/**
 *****************************************************************************************
 * @brief Get the TX power as control structure TX power field from a value in dBm.
 *
 * @param[in] txpwr_dbm   TX power in dBm
 * @param[in] option	  If TXPWR_CS_LOWER, return index equal to or lower than requested
 *						  If TXPWR_CS_HIGHER, return index equal to or higher than requested
 *						  If TXPWR_CS_NEAREST, return index nearest to the desired value
 *
 * @return The index of the TX power
 *
 *****************************************************************************************
 */
__STATIC uint8_t bt_rf_txpwr_cs_get (int8_t txpwr_dbm, uint8_t option)
{
    uint8_t power_index = 0, index=0;

    /* // 3DH5   GFSK +3dbm
    RFIF_P->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_GAIN_BT_0 = 1;  // -17.23
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_1 = 3;  // -12.76
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_2 = 4;  // -8.67
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_3 = 7;  // -5.34
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_4 = 8;  // -2
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_5 = 14; // 1.4
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_6 = 18; // 4.4
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_7 = 24; // 7.4 */



    //if tx power config modify, this must be modified
    int8_t rf_txpwr_table[MAX_POWER_LEVEL+1]= {TX_PPA_GAIN_BT_0_DBM, TX_PPA_GAIN_BT_1_DBM, TX_PPA_GAIN_BT_2_DBM, TX_PPA_GAIN_BT_3_DBM,
                                               TX_PPA_GAIN_BT_4_DBM, TX_PPA_GAIN_BT_5_DBM, TX_PPA_GAIN_BT_6_DBM, TX_PPA_GAIN_BT_7_DBM};

    if(txpwr_dbm > rf_txpwr_table[MAX_POWER_LEVEL])
    {
        return MAX_POWER_LEVEL; //maxpower is 12dbm
    }
    
    for(index=MIN_POWER_LEVEL; index<MAX_POWER_LEVEL+1; index++)
    {
        if(txpwr_dbm <= rf_txpwr_table[index] )
        {
           power_index = index;
           break;
        }
    }

	return power_index;
}




uint8_t bt_rf_api_init(void *api)
{
    if (NULL == api)
        return 1;

    struct ble_rf_api *rf_api_p = (struct ble_rf_api *)api;

    // Initialize the RF driver API structure
    rf_api_p->reg_rd = bt_rf_reg_rd;
    rf_api_p->reg_wr = bt_rf_reg_wr;
    rf_api_p->txpwr_dbm_get = bt_rf_txpwr_dbm_get;
    rf_api_p->txpwr_min = MIN_POWER_LEVEL;
    rf_api_p->txpwr_max = MAX_POWER_LEVEL;
    rf_api_p->sleep = bt_rf_sleep;
    rf_api_p->reset = bt_rf_reset;
    rf_api_p->rssi_convert = bt_rf_rssi_convert;
    rf_api_p->txpwr_cs_get = bt_rf_txpwr_cs_get;

    rf_api_p->rssi_high_thr = -10;
    rf_api_p->rssi_low_thr = -30;
    rf_api_p->rssi_interf_thr = -50;
//#if defined(BLE_EMB_PRESENT)
    rf_api_p->force_agc_enable = bt_rf_force_agc_enable;
//#endif //CFG_BLE

#if (BT_EMB_PRESENT)
    rf_api_p->txpwr_dec = bt_rf_txpwr_dec;
    rf_api_p->txpwr_inc = bt_rf_txpwr_inc;
    rf_api_p->txpwr_max_set = bt_rf_txpwr_max_set;
#endif //CFG_BT

    return 0;




};
///@} RF_ATLAS
#endif
