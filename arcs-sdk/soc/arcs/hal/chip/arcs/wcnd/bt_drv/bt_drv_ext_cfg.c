
#include <stdio.h>
#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition

#include "ble_drv.h"
#include "bt_drv.h"
#include "log_print.h"



/*
 * DEFINES
 *****************************************************************************************
 */
        
#define APB_MODEM_RSSI_BUG_WORKAROUND           (1) //modem rssi bug workaround
    
#define APB_CONFIG_EN                           (APB_MODEM_RSSI_BUG_WORKAROUND) // APB_Function


#define CHANGE_MIDDLE_FREQ                		(1)
#define DUMP_DATA_FUNC_EN                       (1)
#define BR_EDR_MODEM_BUG                        (1)


#if (APB_CONFIG_EN)

/*
 * ENUMERATION DEFINITION
 *****************************************************************************************
 */

enum trx_on_off_t
{
    APB_BLE_TX_ON,
    APB_BLE_TX_OFF,
    APB_BLE_RX_ON,
    APB_BLE_RX_OFF,
    APB_BT_TX_ON,
    APB_BT_TX_OFF,
    APB_BT_RX_ON,
    APB_BT_RX_OFF,
};

#define APB_WRITE       (1<<15)
#define APB_READ        (0<<15)
#define APB_MODEM       (0<<31)
#define APB_RF          (1<<31)



extern uint32_t lsip_get_em_base_addr(void);


/*
 * LOCAL FUNCTIONS DEFINITION
 *****************************************************************************************
 */

void apb_write32p(void *ptr32, uint32_t value)
{
    uint8_t *ptr = (uint8_t *)ptr32;

    *ptr++ = (uint8_t)(value&0xff);
    *ptr++ = (uint8_t)((value&0xff00)>>8);
    *ptr++ = (uint8_t)((value&0xff0000)>>16);
    *ptr   = (uint8_t)((value&0xff000000)>>24);
}

void apb_em_wr32p(uint16_t em_addr, uint32_t value)
{
    apb_write32p((void *)(em_addr + lsip_get_em_base_addr()), value);
}

void apb_trx_on_off_enable(uint8_t trx_on_off, uint16_t em_addr)
{
    switch(trx_on_off)
    {
        case APB_BLE_TX_ON:
        {
            BT_BLE_P->REG_BLE_SPIPTRCNTL0.bit.BLE_TXONPTR = em_addr >> 2;
            break;
        }
        case APB_BLE_TX_OFF:
        {
            BT_BLE_P->REG_BLE_SPIPTRCNTL0.bit.BLE_TXOFFPTR = em_addr >> 2;
            break;
        }
        case APB_BLE_RX_ON:
        {
            BT_BLE_P->REG_BLE_SPIPTRCNTL1.bit.BLE_RXONPTR = em_addr >> 2;
            break;
        }
        case APB_BLE_RX_OFF:
        {
            BT_BLE_P->REG_BLE_SPIPTRCNTL1.bit.BLE_RXOFFPTR = em_addr >> 2;
            break;
        }
        case APB_BT_TX_ON:
        {
            BT_BT_P->REG_SPIPTRCNTL0.bit.TXONPTR = em_addr >> 2;
            break;
        }
        case APB_BT_TX_OFF:
        {
            BT_BT_P->REG_SPIPTRCNTL0.bit.TXOFFPTR = em_addr >> 2;
            break;
        }
        case APB_BT_RX_ON:
        {
            BT_BT_P->REG_SPIPTRCNTL1.bit.RXONPTR = em_addr >> 2;
            break;
        }
        case APB_BT_RX_OFF:
        {
            BT_BT_P->REG_SPIPTRCNTL1.bit.RXOFFPTR = em_addr >> 2;
            break;
        }
        default:
            break;
    }
}

void apb_em_config(uint16_t em_addr, uint32_t apb_config, uint32_t apb_value)
{
    memset((uint8_t *)(lsip_get_em_base_addr() + em_addr), 0, 8);
    apb_em_wr32p(em_addr, apb_config);
    apb_em_wr32p(em_addr+4, apb_value);
}

#if (APB_MODEM_RSSI_BUG_WORKAROUND)
/*
* bug description: linklayer request rssi_result from modem before frame end,
*                because of clock sync issue, modem can not get rssi_req singnal from linklayer, 
*                then linklayer can not get rssi_result, and can not end the frame;
*
* workaround: disable rssi function, and reset modem state machine while rx_off; (Use APB tool auto reset modem)
*/
extern uint16_t lsip_get_em_bt_end_addr(void);

#define EM_ADDR_MODEM_RSSI_FIX_BLE      (lsip_get_em_bt_end_addr())//(0x5274)
#define EM_ADDR_MODEM_RSSI_FIX_BT       (EM_ADDR_MODEM_RSSI_FIX_BLE+16)
#define EM_ADDR_MODEM_RSSI_FIX_END      (EM_ADDR_MODEM_RSSI_FIX_BT+16)

#define CONFIG_MODEM_REG_OFFSET     (0x100 << 16)   //modem register address offset = 0x100

void apb_modem_rssi_bug_workaround(void)
{
    // for ble rx
    uint16_t em_addr = EM_ADDR_MODEM_RSSI_FIX_BLE;
    uint32_t le_config_1 = APB_MODEM | CONFIG_MODEM_REG_OFFSET | APB_WRITE | ((em_addr+8)>>2);
    uint32_t le_config_2 = APB_MODEM | CONFIG_MODEM_REG_OFFSET | APB_WRITE;
    uint32_t le_value_1  = BT_MODEM_P->REG_TOP_CFG0.all & (~0x01);
    uint32_t le_value_2  = BT_MODEM_P->REG_TOP_CFG0.all;
    // for bt rx
    uint16_t em_addr_2 = EM_ADDR_MODEM_RSSI_FIX_BT;
    uint32_t bt_config_1 = APB_MODEM | CONFIG_MODEM_REG_OFFSET | APB_WRITE | ((em_addr_2+8)>>2);
    uint32_t bt_config_2 = APB_MODEM | CONFIG_MODEM_REG_OFFSET | APB_WRITE;
    uint32_t bt_value_1  = BT_MODEM_P->REG_TOP_CFG0.all & (~0x01);
    uint32_t bt_value_2  = BT_MODEM_P->REG_TOP_CFG0.all;


    //bug desc  : coex issue, linklayer request rssi_result from modem before frame end, because of clock sync issue
    //            modem can not get rssi_req singnal from linklayer, then linklayer can not get rssi_result, so bt frame can not end
    //workaround: disable rssi function, rssi value of ID packet will be abnormal, other packets are not affected
    //disable rssi function
    BT_CNTL_P->REG_BT_CTRL_RADIO.bit.RSSI_SEL = 0;


    //every packet rx end off should reset modem state machine, if not,  C/I -6M will fail

    //select ble rx window off, to config modem register(reset modem state machine)
    apb_trx_on_off_enable(APB_BLE_RX_OFF, em_addr);
    //BT_MODEM, top_Normal_Work_En=0
    apb_em_config(em_addr, le_config_1, le_value_1);
    //BT_MODEM, top_Normal_Work_En=1
    apb_em_config(em_addr+8, le_config_2, le_value_2);
    
    //select bt rx window off, to config modem register(reset modem state machine)
    apb_trx_on_off_enable(APB_BT_RX_OFF, em_addr_2);
    //BT_MODEM, top_Normal_Work_En=0
    apb_em_config(em_addr_2, bt_config_1, bt_value_1);
    //BT_MODEM, top_Normal_Work_En=1
    apb_em_config(em_addr_2+8, bt_config_2, bt_value_2);
}
#else
#define EM_ADDR_MODEM_RSSI_FIX_END      (lsip_get_em_bt_end_addr())
#endif

#endif

void apb_config(void)
{
#if APB_MODEM_RSSI_BUG_WORKAROUND
    apb_modem_rssi_bug_workaround();
#endif
}

#if (DUMP_DATA_FUNC_EN)
#define ADC_RAW_DATA_TYPE       1
#define RSSIADC_RAW_DATA_TYPE   2
#define NOTCH_RAW_DATA_TYPE     3

/// dump adc data
#define DUMP_DATA_LEN           8191     // DATA LEN
#define DUMP_DELAY              20        // delay from modem_rx_en
#define DUMP_DELAY_RX_ISR_NUM   0        // start dump after num of rx_isr
uint8_t g_dump_flag;
// RAM DUMP ADDR: 0x88000-0x8fff
void bt_dump_trigger()
{
    BT_CNTL_P->REG_BT_CTRL_MODEM_CALIB.bit.CALIB_ACCESS_EM = 1;
    
    BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_RAM_DLY_LEN          = DUMP_DELAY;
    BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_RAM_DUMP_LEN         = DUMP_DATA_LEN;

    BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_ADC_RAW_DATA_FLG     = 1;

}
void trx_en_dump_isr(uint8_t stat)
{
    if (stat & RXEN_ON_STAT_BIT)
    {
        // dump start when dump trigger_bit have been set to 1(set in adc_dump.exe tool)
        if (CMN_SYS_P->REG_CMN_DUMMY_RSVD.all & 0x01)
        {
			if (g_dump_flag == 0xFF)
			{
				CLOGW("trx_en_dump_isr:dump adc data, soft run in while(1), please hardware reset!");
				BT_CNTL_P->REG_BT_CTRL_MODEM_CALIB.bit.CALIB_ACCESS_EM  = 0;
				//diable clock
				BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.MASTER_CLK_EN  = 0;
				while(1);
			}

			if (g_dump_flag >= DUMP_DELAY_RX_ISR_NUM)
			{
				uint8_t type = (CMN_SYS_P->REG_CMN_DUMMY_RSVD.all & 0x06)>>1;

				BT_CNTL_P->REG_BT_CTRL_MODEM_CALIB.bit.CALIB_ACCESS_EM = 1;

				BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_RAM_DLY_LEN          = DUMP_DELAY;
				BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_RAM_DUMP_LEN         = DUMP_DATA_LEN;

				if (type == NOTCH_RAW_DATA_TYPE)
				{
					BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_ADC_RAW_DATA_FLG     = 0;
				}
				else
				{
					BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_ADC_RAW_DATA_FLG     = 1;
				}

				BT_MODEM_P->REG_BT_RX_DUMP_CFG.bit.RX_RAM_DUMP_TRIG        = 1;

				g_dump_flag = 0xFF;
				return;
			}
			g_dump_flag++;
        }
        else if (CMN_SYS_P->REG_CMN_DUMMY_RSVD.all & 0x0100) // LA DUMP
        {
            // select modem debug port
            // rssi adc
            BT_MODEM_P->REG_BT_RX_CLK_DIV_SYNC.bit.REG_DIAG_SEL_BT_0 = 2;
            BT_MODEM_P->REG_BT_RX_CLK_DIV_SYNC.bit.REG_DIAG_SEL_BT_1 = 0;
            BT_MODEM_P->REG_BT_RX_CLK_DIV_SYNC.bit.REG_DIAG_SEL_BT_2 = 0;
        	// adc
            //BT_MODEM_P->REG_BT_RX_CLK_DIV_SYNC.bit.REG_DIAG_SEL_BT_0 = 0;
            //BT_MODEM_P->REG_BT_RX_CLK_DIV_SYNC.bit.REG_DIAG_SEL_BT_1 = 1;
            //BT_MODEM_P->REG_BT_RX_CLK_DIV_SYNC.bit.REG_DIAG_SEL_BT_2 = 0;

            // select wifi port for bt
            //WIFI SYSCTRL CFG, diag top
            *(uint32_t *)(0x4B100064) = 0;
            // debug port low
            *(uint32_t *)(0x4b100068) = 0x0d0d0d0d;
            // debug port mid
            *(uint32_t *)(0x4b100074) = 0x0e0e0e0e;
            // debug port high
            *(uint32_t *)(0x4b100078) = 0x0f0f0f0f;

            // LA select bt clk
            IP_WIFI_CRM->REG_CLKRST_CNTL.bit.LACLK_FREQ_SEL = 3;

            // set memory addr
            IP_WF_CTRL->REG_WIFI_CTRL_LA_DUMP_SADDR.bit.LA_DUMP_SADDR = 0x20090000;
            IP_WF_CTRL->REG_WIFI_CTRL_LA_DUMP_EADDR.bit.LA_DUMP_EADDR = 0x200B0000 - 4;

            // LA_SAMPLING_MSK0
            //*(uint32_t *)(0x4B500014) = 0xFFFFFFFF;
            IP_WF_LA->REG_SAMPLING_MASK0.all = 0xFFFFFFFF;
            // LA_SAMPLING_MSK1
            //*(uint32_t *)(0x4B500018) = 0xFFFFFFFF;
            IP_WF_LA->REG_SAMPLING_MASK1.all = 0xFFFFFFFF;

            // enable MPIP dynamic data masking
            *(uint32_t *)(0x4B10007C) = 0x3F1;

            // set trigger level
            //*(uint32_t *)(0x4B500038) = 0x100;
            IP_WF_LA->REG_TRIGGER_POINT.bit.TRIGGER_POINT = 0x100;

            // 64bit compress
            //IP_WF_CTRL->REG_WIFI_CTRL_LA_DUMP_CTRL.bit.LA_TRIG_MODE = 0;
            //IP_WF_CTRL->REG_WIFI_CTRL_LA_DUMP_CTRL.bit.LA_DUMP_MODE = 1;
        	// 32bit raw
            IP_WF_CTRL->REG_WIFI_CTRL_LA_DUMP_CTRL.bit.LA_TRIG_MODE = 1;
            IP_WF_CTRL->REG_WIFI_CTRL_LA_DUMP_CTRL.bit.LA_DUMP_MODE = 0;

            IP_WF_CTRL->REG_WIFI_CTRL_LA_DUMP_CTRL.bit.LA_DUMP_START = 1;

            // set trigger bit
            IP_WF_LA->REG_TRIGGER_MASK0.all = 0x40000000;
            IP_WF_LA->REG_TRIGGER_VALUE0.all = 0x40000000;

            // set bt linklayer err bit
            // 0 bt_crc_err, 1 bt_sync_err, 2 bt_hec_err, 3 bt_guard_err
            // 4 le_crc_err, 5 le_sync_err, 6 le_len_err, 7 le_type_err
            BT_CNTL_P->REG_BT_CTRL_DBG_MUX.all = 5;

            // mac_la_clk_gate_en
            *(uint32_t *)(0x4B1000E0) &= ~0x08;

            // wifi sub sate
            IP_WIFI_MAC_CORE->REG_STATECNTRLREG.bit.NEXTSTATE = 3;

            // LA start
            // release external trigger
            //WIFI SYSCTRL CFG
            *(uint32_t *)(0x4B100070) = 0;

            // start logic analyzer
            //*(uint32_t *)(0x4B50000C) |= 0x1;
            IP_WF_LA->REG_CNTRL.bit.START = 1;

            // clean LA trigger bit
            CMN_SYS_P->REG_CMN_DUMMY_RSVD.all &= ~0x0100;
        }
    }
    return;
}
#endif


#if (CHANGE_MIDDLE_FREQ)
 __attribute__ ((section (".ramcode"))) void trx_en_change_mid_freq(uint8_t stat)
{
    if (stat & RXEN_ON_STAT_BIT)
    {
    	// ble 2M, channel 2402, set middle freq to -1.25M
        if ((BT_CNTL_P->REG_BT_CTRL_CHANNEL_RATE.bit.CHANNEL == 0) && (BT_CNTL_P->REG_BT_CTRL_CHANNEL_RATE.bit.RATE_OUT == 1) && (BT_CNTL_P->REG_BT_CTRL_CHANNEL_RATE.bit.NBT_BLE == 1))
        {
            BT_MODEM_P->REG_BT_RX_CORDIC_2M.bit.RX_FRQ_SHIFT_2M = MIDDLE_FREQ_2MPHY_N1P25_MHZ;// -1.25M
            BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_2M     = 0xFB00;// -1.25*1024
        }
        else
        {
            BT_MODEM_P->REG_BT_RX_CORDIC_2M.bit.RX_FRQ_SHIFT_2M = MIDDLE_FREQ_2MPHY_1P25_MHZ;// -1.25M
            BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_2M     = 0x500;// -1.25*1024
        }
    }

    return;
}
#endif

#if (BR_EDR_MODEM_BUG)

void modem_br_set_deltapower4gfsk()
{
    BT_MODEM_P->REG_TX_CFG6.bit.TX_DELTAPOWER4GFSK = 0xE8;
}

void modem_edr_set_deltapower4gfsk()
{
    BT_MODEM_P->REG_TX_CFG6.bit.TX_DELTAPOWER4GFSK = 0x129;
}


  __attribute__ ((section (".ramcode"))) void trx_en_br_edr_modem_bug(uint8_t stat)
{
    if (stat & TXEN_ON_STAT_BIT)
    {
        if (BT_CNTL_P->REG_BT_CTRL_CHANNEL_RATE.bit.NBT_BLE == 0)     //bt
        {
            if (BT_CNTL_P->REG_BT_CTRL_CHANNEL_RATE.bit.RATE_OUT > 0) //edr
            {
                modem_edr_set_deltapower4gfsk();
            }
            else                                                      //br
            {
                modem_br_set_deltapower4gfsk();
            }
        }
    }
}

#endif

 __attribute__ ((section (".ramcode"))) void trx_en_isr()
{
    //GPIO_SET_OUTPUT(0, 20, 1);
    uint8_t stat = BT_CNTL_P->REG_BT_CTRL_INT_ISR.all & 0x0f;
    BT_CNTL_P->REG_BT_CTRL_INT_CLR.all = stat;

    #if (CHANGE_MIDDLE_FREQ)
    trx_en_change_mid_freq(stat);
    #endif
    //GPIO_SET_OUTPUT(0, 21, 1);

    #if (BR_EDR_MODEM_BUG)
    trx_en_br_edr_modem_bug(stat);
    #endif

    #if (RF_MAX2830_SUPPORT)
    trx_en_2830_isr(stat);
    #endif

    #if (DUMP_DATA_FUNC_EN)
    trx_en_dump_isr(stat);
    #endif
    //GPIO_SET_OUTPUT(0, 21, 0);
    //GPIO_SET_OUTPUT(0, 20, 0);
    //clean NVIC
    clear_IRQ(IRQ_BTTXRXEN_VECTOR);
    //CLOGD("ENTER TRX_EN_ISR");

    return;
}

 __attribute__ ((section (".ramcode"))) void txrx_int_enable()
{
    register_ISR(IRQ_BTTXRXEN_VECTOR, trx_en_isr, NULL);
    enable_IRQ(IRQ_BTTXRXEN_VECTOR);

    BT_CNTL_P->REG_BT_CTRL_INT_CLR.all     = 0x1f;
    BT_CNTL_P->REG_BT_CTRL_INT_MASK.all = 0x0;

    #if (RF_MAX2830_SUPPORT)
    BT_CNTL_P->REG_BT_CTRL_INT_MASK.bit.TXEN_ON_MASK =1;
    BT_CNTL_P->REG_BT_CTRL_INT_MASK.bit.RXEN_ON_MASK =1;
    #endif

    #if (DUMP_DATA_FUNC_EN)
    BT_CNTL_P->REG_BT_CTRL_INT_MASK.bit.RXEN_ON_MASK = 1;
    //clean dump trigger bit, bit only be set in adc_dump.exe tool while debug
    //BT_CNTL_P->REG_BT_CTRL_RSVD.all &= ~0x01;
    #endif

    #if (BR_EDR_MODEM_BUG)
    BT_CNTL_P->REG_BT_CTRL_INT_MASK.bit.TXEN_ON_MASK =1;
    #endif

    #if (CHANGE_MIDDLE_FREQ)
    BT_CNTL_P->REG_BT_CTRL_INT_MASK.bit.RXEN_ON_MASK =1;
    #endif

    return;
}

 void extra_cfg()
 {
     //PTCH(void, extra_config, void);

     apb_config();
 
     txrx_int_enable();
 }


