
#include <stdio.h>
#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition

#include "ble_drv.h"
#include "bt_drv.h"



extern void sch_alarm_set(struct sch_alarm_tag* elt);


/*
 * VARIABLES DEFINITIONS
 *****************************************************************************************
 */

struct lsip_modem_env_api lsip_modem_api =
{
    .modem_fsm_reset = modem_reset,
    .modem_config_phy = NULL,
    .modem_config_cbk_phy =NULL,
    .modem_at_eof_update_phy = NULL,
    .modem_at_startup_update_phy = NULL,
};
struct lsip_modem_param_tag lsip_modem_param_env[MODEM_ALARM_LENGTH] = {0};
struct sch_alarm_tag lsip_modem_alarm_arr[MODEM_ALARM_LENGTH] = {0};
uint8_t lsip_modem_alarm_set_idx = 0;
uint8_t lsip_modem_alarm_hdl_idx = 0;
uint8_t modem_alarm_start_flag = 0;






/*
 *  FUNCTIONS DEFINITION
 *****************************************************************************************
 */
 
void lsip_modem_env_init(void)
{
    if (HAL_PMU_Is_PowerOn())
    {
        
        lsip_modem_api.modem_fsm_reset = modem_reset;
        /// modem config phy will be excuted at the 1.5 slot later
        lsip_modem_api.modem_config_cbk_phy = NULL;
        
        lsip_modem_alarm_hdl_idx = 0;
        lsip_modem_alarm_set_idx = 0;
        modem_alarm_start_flag = 0;
        memset(lsip_modem_param_env, 0, sizeof(struct lsip_modem_param_tag) * MODEM_ALARM_LENGTH);
        memset(lsip_modem_alarm_arr, 0, sizeof(struct sch_alarm_tag) * MODEM_ALARM_LENGTH);
    }

    return;
}

void modem_cfg_phy_alarm_clean(uint8_t et_idx)
{
    uint8_t i;

    for (i = 0; i < MODEM_ALARM_LENGTH; i++)
    {
        if ((lsip_modem_param_env[i].et_idx == et_idx) && (lsip_modem_param_env[i].valid))
        {
            lsip_modem_param_env[i].valid = false;
            break;
        }
    }

    return;
}

void linklayer_cfg_ble_1m( void ){
    //ble uncoded 1m
    //BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_RXWINSZDEF        = BLE_NORMAL_WIN_SIZE+1;

}
void linklayer_cfg_ble_2m( void ){
    //ble uncoded 2m
    //BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_RXWINSZDEF        = BLE_NORMAL_WIN_SIZE+1;

}
void linklayer_cfg_ble_lr125( void ){

    //BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_RXWINSZDEF        = BLE_NORMAL_WIN_SIZE+1;
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RXPATHDLY2    = 0xE;

    LS_SETF_REG(&(IP_BLE->REG_BLE_RADIOTXRXTIM2), 8, 8, 0xE);
}
void linklayer_cfg_ble_lr500( void ){

    //BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_RXWINSZDEF        = BLE_NORMAL_WIN_SIZE+1;
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RXPATHDLY2    = 0xA;

    LS_SETF_REG(&(IP_BLE->REG_BLE_RADIOTXRXTIM2), 8, 8, 0xA);
}

void modem_cfg_ble_coded ( void ){

    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY  =  10;
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY =  1110;
}

void modem_cfg_ble_1m ( void ){
    
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY  =  10;
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY =  1110;
}

void modem_cfg_ble_2m ( void ){
    
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY  =  10;
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY =  1110;
}

void modem_cfg_br( void ){
    //BT_MODEM_P->REG_TX_CFG5.bit.TX_BT_CLK_SRC   =  0; // 1 fpga should be 0, chip can be 1
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY  =  50;
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY =  470;    
}

void modem_cfg_edr( void ){
    //BT_MODEM_P->REG_TX_CFG5.bit.TX_BT_CLK_SRC   =  0;
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY  =  50;
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY =  470;    
}

void modem_cfg_phy_alarm_cbk(struct sch_alarm_tag *elt)
{
    if (lsip_modem_param_env[lsip_modem_alarm_hdl_idx].valid != true)
    {
        //if et is skiped, do nothing, add by jiangsheng.shi, 2022.07.07
    }
    else if(lsip_modem_param_env[lsip_modem_alarm_hdl_idx].current_mode == SCHEDULE_BLE)
    {
#if 0 // useless
        BT_MODEM_P->REG_TX_CFG0.bit.TX_MODULATIONINDEX_BLE             = 16384;
        BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY  =  10;
        BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY =  1110;
        BT_MODEM_P->REG_BT_RX_SYNC_CODED_CFG.bit.RX_SYNC_SAMP_OFFSET            = 0x0;

        if(lsip_modem_param_env[lsip_modem_alarm_hdl_idx].rx_rate == BLE_RATE_1MBPS)
        {
            modem_cfg_ble_1m();
            linklayer_cfg_ble_1m();
        }
        else if (lsip_modem_param_env[lsip_modem_alarm_hdl_idx].rx_rate == BLE_RATE_2MBPS)
        {
            modem_cfg_ble_2m();
            linklayer_cfg_ble_2m();
        }
        else if (lsip_modem_param_env[lsip_modem_alarm_hdl_idx].rx_rate == BLE_RATE_125KBPS)
        {
            modem_cfg_ble_coded();
            linklayer_cfg_ble_lr125();
        }
        else if (lsip_modem_param_env[lsip_modem_alarm_hdl_idx].rx_rate == BLE_RATE_500KBPS)
        {
            modem_cfg_ble_coded();
            linklayer_cfg_ble_lr500();
        }
#endif
    }
    else   // (rwip_modem_param_env.current_mode == SCH_PROG_BT)
    {
#if 0 //useless
        BT_MODEM_P->REG_TX_CFG0.bit.TX_MODULATIONINDEX_BT              = 11469;
        BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY            =  50;  //ble & bt not same
        BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY           =  470;  //ble & bt not same
        BT_MODEM_P->REG_BT_RX_SYNC_CODED_CFG.bit.RX_SYNC_SAMP_OFFSET= 0x1;

        if(lsip_modem_param_env[lsip_modem_alarm_hdl_idx].edr_en == 0)
        {
            modem_cfg_br();
        }
        else //if (lsip_modem_param_env.rx_rate == 1)
        {
            modem_cfg_edr();
        }
#endif
    }

    lsip_modem_param_env[lsip_modem_alarm_hdl_idx].valid = false;
    lsip_modem_param_env[lsip_modem_alarm_hdl_idx].et_idx = 0xff;
    lsip_modem_alarm_hdl_idx++;

    if (lsip_modem_alarm_hdl_idx >= MODEM_ALARM_LENGTH)
    {
        lsip_modem_alarm_hdl_idx = 0;
    }

    return;
}

/// modem config phy will be excuted at the 1.5 slot later
void modem_cfg_phy_prog_cbk(uint8_t mode, uint8_t tx_rate, uint8_t rx_rate, uint8_t aux_rate, uint8_t edr, lsip_time_t time, uint8_t et_idx)
{
    bool change_phy = false;
    uint8_t last_index = (lsip_modem_alarm_set_idx==0) ? 4 : (lsip_modem_alarm_set_idx-1);

    if(mode == SCHEDULE_BLE)
    {
        if (modem_alarm_start_flag && (tx_rate == lsip_modem_param_env[last_index].tx_rate) && (rx_rate == lsip_modem_param_env[last_index].rx_rate))
        {
            return;
        }
        lsip_modem_param_env[lsip_modem_alarm_set_idx].current_mode = SCHEDULE_BLE;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].tx_rate = tx_rate;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].rx_rate = rx_rate;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].aux_rate = aux_rate;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].et_idx = et_idx;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].valid = true;
        change_phy = true;
    }
    else   // (mode == SCHEDULE_BLE)
    {
        if (edr == lsip_modem_param_env[last_index].edr_en) 
        {
            return;
        }
        lsip_modem_param_env[lsip_modem_alarm_set_idx].current_mode = SCHEDULE_BT;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].edr_en = edr;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].et_idx = et_idx;
        lsip_modem_param_env[lsip_modem_alarm_set_idx].valid = true;
        change_phy = true;
    }   
    
    if(change_phy)
    {
        if(time.hus>=TIME_TO_WRITE_MODEM_REG)
        {
            time.hus -= TIME_TO_WRITE_MODEM_REG;
        }
        else
        {
            time.hus = HALF_SLOT_SIZE - TIME_TO_WRITE_MODEM_REG + time.hus;
            time.hs = CLK_SUB(time.hs,1);
        }

        lsip_modem_alarm_arr[lsip_modem_alarm_set_idx].time.hs = time.hs;
        lsip_modem_alarm_arr[lsip_modem_alarm_set_idx].time.hus = time.hus;
        lsip_modem_alarm_arr[lsip_modem_alarm_set_idx].cb_alarm = modem_cfg_phy_alarm_cbk;
        sch_alarm_set(&lsip_modem_alarm_arr[lsip_modem_alarm_set_idx]);
        lsip_modem_alarm_set_idx++;
        if (lsip_modem_alarm_set_idx >= MODEM_ALARM_LENGTH)
        {
            lsip_modem_alarm_set_idx = 0;
        }
        modem_alarm_start_flag = true;

    }
    return;
}


