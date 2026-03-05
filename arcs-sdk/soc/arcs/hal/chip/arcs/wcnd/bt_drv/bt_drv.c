
#include <stdio.h>
#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition
#include "patch.h"
#include "dbg_assert.h"
#include "bt_drv.h"
#include "rf_cali.h"


/*
 * VARIABLES DEFINITIONS
 *****************************************************************************************
 */

uint8_t RXPWR_INC = 15; //should less 25

volatile CMN_BUSCFG_RegDef *CMN_SYS_NODFT_P = CMN_SYS_NODFT;
volatile BT_CTRL_TOP_RegDef *BT_CNTL_P  = IP_BT_CTRL;
volatile AON_IOMUX_RegDef *AON_IOMUX_P  = IP_AON_IOMUX;
volatile AON_CTRL_RegDef *AON_CTRL_P    = IP_AON_CTRL;
volatile BT_MODEM_RegDef *BT_MODEM_P    = IP_BTMODEM;
volatile CMN_SYSCFG_RegDef *CMN_SYS_P   = IP_CMN_SYS;
volatile BT_DM_RegDef *BT_DM_P          = IP_BT_DM;
volatile RFIF_RegDef *RFIF_P            = IP_RFIF;
volatile BLE_RegDef *BT_BLE_P           = IP_BLE;
volatile BT_RegDef *BT_BT_P             = IP_BT;



/*
 * FUNCTIONS DECLARATION
 ****************************************************************************************
 */

extern uint16_t plf_get_feat(uint8_t feat_idx);
extern uint32_t CRM_GetHclkFreq();

/*
 *  FUNCTIONS DEFINITION
 *****************************************************************************************
 */
/// register type  0:modem 1: rfif  2:ble linklayer  3:aon  4: other
void ls_reg_init_cfg (uint8_t reg_type, struct BT_REG_INIT_ITEM reg_cfg_table[], uint32_t talbe_length)
{
	uint32_t i =0;
    uint32_t reg_type_baseaddr =0;

    switch(reg_type)
    {
        case REG_TYPE_BT_MODEM:
            reg_type_baseaddr = BT_MODEM_BASE;
            break;
        case REG_TYPE_BT_RF_IF:
            reg_type_baseaddr = RF_IF_BASE;
            break;
        case REG_TYPE_BT_BLE:
            reg_type_baseaddr = BASE_ADDR_BT_BLE;
            break;
        case REG_TYPE_AON_CTRL:
            reg_type_baseaddr = AON_CTRL_BASE;
            break;
        case REG_TYPE_BT_DM:
            reg_type_baseaddr = BASE_ADDR_BT_DM;
            break;
        case REG_TYPE_BT_BT:
            reg_type_baseaddr = BASE_ADDR_BT_BT;
            break;
        default:
        	ASSERT_ERR(reg_type <= BASE_ADDR_BT_BT);
        break;

    }

    //ASSERT_ERR(modem_int_cfg_table);
    for(i=0; i<talbe_length; i++)
    {
        LS_SETF_REG(reg_type_baseaddr + reg_cfg_table[i].reg_offset, reg_cfg_table[i].start_bit_pos, reg_cfg_table[i].bits_width, reg_cfg_table[i].bits_value);
    }
}

void Bt_BootClock_Init(void)
{
    uint32_t hclk_value = CRM_GetHclkFreq()/1000000; //CRM_IpCore_300MHz, CRM_IpCore_240MHz, CRM_IpCore_200MHz, CRM_IpCore_150MHz
    uint32_t bt_clk_div = 1;
    uint32_t bt_master_clk = 24;
    
    if(hclk_value <= 40)  //16M CLK < bt_master_clk < 40M CLK
    {
       bt_clk_div = 1; 
    }
    else
    {
        if((hclk_value == 48) || (hclk_value == 72)||(hclk_value == 96) || (hclk_value == 120)||(hclk_value == 240))
        {
            bt_clk_div = hclk_value/24;
        }
        else 
        {
            for(bt_clk_div=2; ; bt_clk_div++)
            {
               if(((hclk_value % bt_clk_div)==0) && (hclk_value/bt_clk_div <= 40)&& (hclk_value/bt_clk_div%2==0))
                {
                   break;
                }
            }
        
        }
    }

    bt_master_clk = hclk_value/bt_clk_div;// bt_master_clk should be equal even number, bt_master_clk should be lower than 40M clk, should be greater than 16M clk

    ASSERT_ERR(((bt_master_clk%2)==0) && (bt_master_clk <=40) && (bt_master_clk>=16)); // bt_master_clk should be equal even number, bt_master_clk should be lower than 40M clk, should be greater than 16M clk

    //CMN_SYS_P->REG_CLK_CTRL.bit.ENA_BT_CLK = 0x1;//ENABLE BT CLK
    //CMN_SYS_P->REG_PERI_CLK_CFG6.bit.ENA_BT_HCLK = 0x1;
    //__HAL_CRM_BT_CLK_ENABLE();

    
    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.MASTER_CLK_DIV      = bt_clk_div;  // for 24M: 0X2  for 48M: 0X0    asic no need +1 again ,so, soft no need -1;
    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.MASTER_CLKSEL       = bt_master_clk;   // MASTER_CLKSEL= (HCLK / (MASTER_CLK_DIV + 1) //hclk:0~96M,   in fpga the high clk 24M,  in asic the high clk 32M

    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.MASTER_CLK_DIV_LD   = 0x1;
    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.MASTER_CLK_EN       = 0x1;
#if 1
    //the follow is default value
    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.BT_PCLK_DIV_M       = bt_clk_div; //CRM_GetHclkFreq()/24000000;
    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.BT_PCLK_DIV_N       = 0x1;
    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.BT_PCLK_DIV_LD      = 0x1;
#endif
}

void bt_disable_bt_clock(void)
{
    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.MASTER_CLK_EN = 0x0;
}

void freq_table_init(void)
{
    uint32_t i;
    volatile uint8_t * freq_table_addr = (uint8_t *)FREQ_TABLE_EM;

    for(i=0; i<40; i++)
    {
        *(freq_table_addr + i) = 2*i;
    }
    for(i=40; i<80; i++)
    {
        *(freq_table_addr + i) = 2*(i-40)+1;
    }  
}

void ble_linklayer_init( void )
{
    //BT_BLE_P->REG_BLE_RADIOCNTL1.bit.BLE_XRFSEL    =2;// self radio select
    //BT_BLE_P->REG_BLE_RADIOCNTL1.bit.BLE_JEF_SELECT =1; // no used again

    BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_ADVERTFILT_EN     = 0x1;
    BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_LSBLE_EN          = 0x1;

#if (RF_MAX2830_SUPPORT)
    // ble uncoded 1m
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN0.bit.BLE_RXPWRUP0      = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN0.bit.BLE_TXPWRUP0      = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    ///BT_BLE_P->REG_BLE_RADIOPWRUPDN0.bit.BLE_TXPWRDN0      = 0x4;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN0.all                   = ((0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US)<<16) + (0x4<<8) + (0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US);
    BT_BLE_P->REG_BLE_RADIOTXRXTIM0.bit.BLE_TXPATHDLY0    = 0x3;
    BT_BLE_P->REG_BLE_RADIOTXRXTIM0.bit.BLE_RXPATHDLY0    = 0x17;
    BT_BLE_P->REG_BLE_RADIOTXRXTIM0.bit.BLE_RFRXTMDA0     = 0x17;

    // ble uncoded 2m
    //// ble uncoded 2m   // fixed 2M PHY sync bug
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN1.bit.BLE_RXPWRUP1      = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN1.bit.BLE_TXPWRUP1      = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN1.bit.BLE_TXPWRDN1      = 0x4;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN1.all                   = ((0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US)<<16) + (0x4<<8) + (0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US);
    BT_BLE_P->REG_BLE_RADIOTXRXTIM1.bit.BLE_TXPATHDLY1    = 0x3;
    BT_BLE_P->REG_BLE_RADIOTXRXTIM1.bit.BLE_RXPATHDLY1    = 0xA;
    BT_BLE_P->REG_BLE_RADIOTXRXTIM1.bit.BLE_RFRXTMDA1     = 0x9; 

    //// ble coded s8
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN2.bit.BLE_RXPWRUP2      = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN2.bit.BLE_TXPWRUP2      = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN2.bit.BLE_TXPWRDN2      = 0x5;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN2.all                   = ((0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US)<<16) + (0x5<<8) + (0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US);
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_TXPATHDLY2    = 0x3;
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RXPATHDLY2    = 0xA;
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RFRXTMDA2     = 0x40;
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RXFLUSHPATHDLY2 = 0x25;

    BT_BLE_P->REG_BLE_RADIOTXRXTIM2.all                     = (0x25 << 24) + (0x40 << 16) + (0xA << 8) + 0x3;
    BT_BLE_P->REG_BLE_RADIOCNTL2.bit.BLE_PHYMSK           = 0x2;
    BT_BLE_P->REG_BLE_RADIOCNTL2.bit.BLE_RXCITERMBYPASS   = 0x1;	

    //// ble coded s2
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN3.bit.BLE_TXPWRUP3      = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN3.bit.BLE_TXPWRDN3      = 0x5;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN3.all                   = (0x5 << 8) + (0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US);
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM3.bit.BLE_TXPATHDLY3    = 0x3;
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM3.bit.BLE_RFRXTMDA3     = 0x3;
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM3.bit.BLE_RXFLUSHPATHDLY3 = 0x24;
    BT_BLE_P->REG_BLE_RADIOTXRXTIM3.all                     = (0x27 << 24) + (0x3 << 16) + 0x3;

    BT_BLE_P->REG_BLE_TIMGENCNTL.bit.BLE_PREFETCH_TIME    = (IP_PREFETCH_TIME_US)<<1;
#else
        
    // ble uncoded 1m
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN0.bit.BLE_RXPWRUP0      = 0x46 + RXPWR_INC;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN0.bit.BLE_TXPWRUP0      = 0x5a; //0x46;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN0.bit.BLE_TXPWRDN0      = 0x4;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN0.all                   = ((0x46 + RXPWR_INC)<<16) + (0x9<<8) + (0x5a);
    BT_BLE_P->REG_BLE_RADIOTXRXTIM0.bit.BLE_TXPATHDLY0    = 0x3;   //arcs_c actual value 4
    BT_BLE_P->REG_BLE_RADIOTXRXTIM0.bit.BLE_RXPATHDLY0    = 0x1b;  //actual value 0x18
    BT_BLE_P->REG_BLE_RADIOTXRXTIM0.bit.BLE_RFRXTMDA0     = 0x1C;  //actual value 0x1B   simulate value 0x15

    // ble uncoded 2m
    //// ble uncoded 2m   // fixed 2M PHY sync bug
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN1.bit.BLE_RXPWRUP1      = 0x46 + RXPWR_INC;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN1.bit.BLE_TXPWRUP1      = 0x5a; //0x46;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN1.bit.BLE_TXPWRDN1      = 0x4;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN1.all                   = ((0x46 + RXPWR_INC)<<16) + (0x4<<8) + (0x5a);
    BT_BLE_P->REG_BLE_RADIOTXRXTIM1.bit.BLE_TXPATHDLY1    = 0x2;  //arcs_c actual value 2.57
    BT_BLE_P->REG_BLE_RADIOTXRXTIM1.bit.BLE_RXPATHDLY1    = 0xe;  //arcs_c actual value 0xc
    BT_BLE_P->REG_BLE_RADIOTXRXTIM1.bit.BLE_RFRXTMDA1     = 0xe;  //arcs_c actual value 0xe  simulate value 0xb

    //// ble coded s8
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN2.bit.BLE_RXPWRUP2      = 0x46 + RXPWR_INC;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN2.bit.BLE_TXPWRUP2      = 0x5a; //0x46;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN2.bit.BLE_TXPWRDN2      = 0x5;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN2.all                   = ((0x46 + RXPWR_INC)<<16) + (0x7<<8) + (0x5a);
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_TXPATHDLY2    = 0x4;   //arcs_c actual value 3.9
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RXPATHDLY2    = 0xa;   //arcs_c actual value 0x16
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RFRXTMDA2     = 0xa0;    //arcs_c actual value 0xa0  simulate value 0x97
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM2.bit.BLE_RXFLUSHPATHDLY2 = 0x27;  //arcs_c actual value 0x27
    BT_BLE_P->REG_BLE_RADIOTXRXTIM2.all                     = (0x2A << 24) + (0x9c << 16) + (0x16 << 8) + 0x4;
    //BT_BLE_P->REG_BLE_RADIOCNTL2.bit.BLE_PHYMSK           = 0x2;
    BT_BLE_P->REG_BLE_RADIOCNTL2.bit.BLE_RXCITERMBYPASS   = 0x1;   

    //// ble coded s2
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN3.bit.BLE_TXPWRUP3      = 90; //0x46;
    //BT_BLE_P->REG_BLE_RADIOPWRUPDN3.bit.BLE_TXPWRDN3      = 0x5;
    BT_BLE_P->REG_BLE_RADIOPWRUPDN3.all                   = (0x7 << 8) + (90);
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM3.bit.BLE_TXPATHDLY3    = 0x3;   //arcs_c actual value 3.6
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM3.bit.BLE_RFRXTMDA3     = 0x3B;  //arcs_c actual value 0x3B  simulate value 0x37
    //BT_BLE_P->REG_BLE_RADIOTXRXTIM3.bit.BLE_RXFLUSHPATHDLY3 = 0x26; //arcs_c actual value 0x26
    BT_BLE_P->REG_BLE_RADIOTXRXTIM3.all                     = (0x29 << 24) + (0x3B << 16) + 0x3;

    BT_BLE_P->REG_BLE_TIMGENCNTL.bit.BLE_PREFETCH_TIME    = (IP_PREFETCH_TIME_US)<<1;
#endif

    /*
    1Mbps that is mandatory
    Bit 0 indicates 2Mbps support when set
    Bit 1 indicates Coded HPY support when set
    */	
    BT_BLE_P->REG_BLE_RADIOCNTL2.bit.BLE_PHYMSK           = 0x3; 

    //BT_BLE_P->REG_BLE_RADIOCNTL1.bit.BLE_SYNC_PULSE_SRC  =1;


}

void bt_linklayer_init( void )
{
    //BT_BT_P->REG_RADIOCNTL1.bit.XRFSEL                = 0x2;// self radio select
    //BT_BT_P->REG_RADIOCNTL1.bit.JEF_SELECT            = 0x1; // no used again

    BT_BT_P->REG_LSBTCNTL.bit.LSBTEN                      = 0x1;
#if (RF_MAX2830_SUPPORT)
    //BT_BT_P->REG_RADIOPWRUPDN.bit.RXPWRUPCT               = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;
    //BT_BT_P->REG_RADIOPWRUPDN.bit.TXPWRUPCT               = 0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US;

    BT_BT_P->REG_RADIOPWRUPDN.all                         = ((0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US) << 16) + (0x10 << 8) + (0x46 + MAX2830_WRITE_SPI_CONSUME_TIME_US);
#else
    //BT_BT_P->REG_RADIOPWRUPDN.bit.RXPWRUPCT               = 0x46 + RXPWR_INC;
    //BT_BT_P->REG_RADIOPWRUPDN.bit.TXPWRUPCT               = 0x58; //0x46;
    // modify TXPWRDNCT from 0xa to 0xc, for tapadlen=0,  add for CMW500 crc_err
    BT_BT_P->REG_RADIOPWRUPDN.all                         = ((0x46 + RXPWR_INC) << 16) + (0xc << 8) + 0x5a;
#endif

    //BT_BT_P->REG_RADIOPWRUPDN.bit.TXPWRDNCT               = 0xA;
    //BT_BT_P->REG_RADIOTXRXTIM.bit.TXPATHDLY               = 0x4;
    //BT_BT_P->REG_RADIOTXRXTIM.bit.RXPATHDLY               = 0xB; //0x2;
    //BT_BT_P->REG_RADIOTXRXTIM.bit.SYNC_POSITION           = 0xF;
    BT_BT_P->REG_RADIOTXRXTIM.all                         = (0xD<<8) + (0x4);
    BT_BT_P->REG_RADIOCNTL3.bit.RXRATE0CFG                = 0x0;
    BT_BT_P->REG_RADIOCNTL3.bit.RXRATE1CFG                = 0x1;
    BT_BT_P->REG_RADIOCNTL3.bit.RXRATE2CFG                = 0x2;
    BT_BT_P->REG_RADIOCNTL3.bit.TXRATE0CFG                = 0x0;
    BT_BT_P->REG_RADIOCNTL3.bit.TXRATE1CFG                = 0x1;
    BT_BT_P->REG_RADIOCNTL3.bit.TXRATE2CFG                = 0x2;
    BT_BT_P->REG_RADIOCNTL2.bit.TRAILER_GATING_VAL        = 0x0;

    // edr
    BT_BT_P->REG_EDRCNTL.bit.GUARD_BAND_TIME              = 0x3;
    BT_BT_P->REG_EDRCNTL.bit.RXGRD_TIMEOUT                = 0x16;

    // esco
    BT_BT_P->REG_ESCOCHANCNTL0.all                        = 0xC010;//SWEN=1, CHANEN=1, TeSCO=0x10
    BT_BT_P->REG_ESCOMUTECNTL0.all                        = 0xAAAAA;//INVL0_0=2, INVL0_1=2, MUTEPATT0=0xAAAA
    BT_BT_P->REG_ESCOCURRENTTXPTR0.all                    = 0x010000C0;
    BT_BT_P->REG_ESCOCURRENTRXPTR0.all                    = 0x01800140;
    BT_BT_P->REG_ESCOLTCNTL0.all                          = 0x0003003A;//ReTxNB0=3, eSCOEDRRX0=1, eSCOEDRTX0=1, SYNTYPE0=1, SYNLTADDR0=2
    BT_BT_P->REG_ESCOTRCNTL0.all                          = 0x03C603C6;//TXSEQN0=0, TXLEN0=0xA, TXTYPE0=6, RXLEN0 =0xA, RXTYPE0=6

}

void dm_linklayer_init( void ){
    BT_DM_P->REG_DM_RADIOCNTL1.bit.DM_XRFSEL              = 0x2; //extrc
    BT_DM_P->REG_DM_RADIOCNTL1.bit.DM_SYNC_PULSE_SRC      = 0x0; //sync pulse gen internal
    //BT_DM_P->REG_DM_RADIOCNTL1.bit.DM_JEF_SELECT          = 0x1; // no used again
    BT_DM_P->REG_DM_TIMGENCNTL.bit.DM_PREFETCH_TIME       = IP_PREFETCH_TIME_US<<1; //0xF0;
}

void linklayer_init(void)
{
    freq_table_init();

#if (CHIP == arcs)
    //BT_CNTL_P->REG_BT_CTRL_TWS_PWRUP.bit.REG_LOAD_VALID            = 1;   //bt and ble all need this
#endif

#if (BLE_EMB_PRESENT)
    //if(plf_get_feat(PLF_FEAT_CORE)&PLF_CORE_LE)
    {
        ble_linklayer_init();
    }
#endif
        
#if (BT_EMB_PRESENT)
    //if(plf_get_feat(PLF_FEAT_CORE)&PLF_CORE_BT)
    {
        bt_linklayer_init();
    }
#endif
    
#if (BT_EMB_PRESENT & BLE_EMB_PRESENT) || (SINGLE_RUN_ON_DUAL)
    //if(plf_get_feat(PLF_FEAT_CORE)&(PLF_CORE_BT|PLF_CORE_LE))
    {
        dm_linklayer_init();
    }
#endif

}

/*
only In arcs_c chip HCLK should be equal to modem_pclk, this is a asic bug
if SEL_HCLK select 0 CRM_IpSrcXtalClk, HCLK ==CRM_IpSrcXtalClk*N/M  24M  modem_pclk =24M
if SEL_HCLK select 1 CRM_IpSrcCoreClk, HCLK ==CRM_IpSrcXtalClk*N/M  240M modem_pclk =240M
if SEL_HCLK select 2 BBLPP_lClk        HCLK ==96M BBLPP_lClk        96M  modem_pclk =96M
*/
//#define BT_MODEM_USED_CLK    (CRM_GetHclkFreq()/1000000)  //24   24 or 96  MHZ CLK VALUE

void modem_init( void )
{
    modem_reset();

    // agc
    BT_MODEM_P->REG_BT_AGC_CFG0.bit.AGC_PARAM_AGC_EN                        = 0x1;  // bt&ble common
    BT_MODEM_P->REG_BT_CFG7.bit.AGC_ABBINT                                  = 0xb;  // bt&ble common
    BT_MODEM_P->REG_BT_CFG7.bit.AGC_LNAINIT                                 = 0x7;  // bt&ble common
    BT_MODEM_P->REG_BT_CFG9.bit.AGC_RSSIPOWDCCANCELEN                       = 0x0;  // bt&ble common
    BT_MODEM_P->REG_BT_CFG6.bit.AGC_THABBGAINLINEAR                         = 52752; // bt&ble common
    BT_MODEM_P->REG_BT_AGC_CFG0.bit.AGC_CORRLENRSSI_SELC                    = 0x1;  // bt&ble common
    BT_MODEM_P->REG_BT_AGC_CFG0.bit.AGC_CORRLENIQ_SELC                      = 0x1;  // bt&ble common
    BT_MODEM_P->REG_BT_AGC_CFG0.bit.AGC_CORRLENRSSI_DPC_SELC                = 0x1;  // bt&ble common
    BT_MODEM_P->REG_BT_AGC_CFG0.bit.AGC_CORRLENIQ_DPC_SELC                  = 0x1;  // bt&ble common
    BT_MODEM_P->REG_BT_CFG1.bit.AGC_ABBPOWDETTHLINEAR                       = 51324;
    BT_MODEM_P->REG_BT_CFG2.bit.AGC_LNAPOWDETTHLINEAR                       = 8;
    BT_MODEM_P->REG_BT_CFG6.bit.AGC_LNASETTLETIME                           = 0x20; // bt&ble common, set to 0x20 for -46dbm edr rx BER too high(maybe for fpga only)
    BT_MODEM_P->REG_BT_CFG7.bit.AGC_SIGTIMINGSURE                           = 0; // bt&ble common
    BT_MODEM_P->REG_BT_CFG7.bit.AGC_ABBMID                                  = 8;
    BT_MODEM_P->REG_BT_CFG8.bit.AGC_TIMEOUTCONFIG                           = 0XFFFFFF; // bt&ble common
    BT_MODEM_P->REG_BT_CFG8.bit.AGC_ABBSETTLETIME                           = 0X10; // bt&ble common

    // notch
    //BT_MODEM_P->REG_BT_RX_DC_NOTCH.bit.RX_DC_CANCEL_EN                      = 0x1;   // DC remove
    //BT_MODEM_P->REG_BT_RX_DC_NOTCH.bit.RX_DC_SHIFT                          = 0x1;
    //BT_MODEM_P->REG_BT_RX_DC_NOTCH.bit.RX_NOTCH_SPUR_EN                     = 0x0;    // 24MHZ clk harmonic enable, current not used, instead of using switch to 32M clk to avoid harmoni
    //BT_MODEM_P->REG_BT_RX_DC_NOTCH.bit.RX_NOTCH_SHIFT_A                     = 0x0;    // 24MHZ harmonic enable
    //BT_MODEM_P->REG_BT_RX_DC_NOTCH.bit.RX_NOTCH_COEF_B                      = 0x3537; // 24MHZ harmonic enable
    //BT_MODEM_P->REG_BT_RX_CORDIC_LPF.bit.RX_LPF_SHIFT_BIT                   = 0x1; // bt&ble common

    // middle freq, switch in rx_isr
#if (ZERO_MIDDLE_FREQ == 1)
    BT_MODEM_P->REG_BT_RX_CORDIC_LPF.bit.RX_FRQ_SHIFT                       = MIDDLE_FREQ_1MPHY_0_KHZ; //MIDDLE FREQENCE: 1.5M  0x1a00 ; 750K 0x1D000 750k ; 0  0HZ middle interfer
    BT_MODEM_P->REG_BT_RX_CORDIC_2M.bit.RX_FRQ_SHIFT_2M                     = MIDDLE_FREQ_2MPHY_0_KHZ;//0x1D800  1.25m  ?
    BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_1M                         = 0;// 0M MF
    BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_2M                         = 0;// 0M MF
#elif (MIDDLE_FREQ_1P5_M == 1)
    BT_MODEM_P->REG_BT_RX_CORDIC_LPF.bit.RX_FRQ_SHIFT                       = MIDDLE_FREQ_1MPHY_1P5_MHZ; //MIDDLE FREQENCE: 1.5M  0x1a00 ; 750K 0x1D000 750k ; 0  0HZ middle interfer
    BT_MODEM_P->REG_BT_RX_CORDIC_2M.bit.RX_FRQ_SHIFT_2M                     = MIDDLE_FREQ_2MPHY_1P5_MHZ;//0x1D800  1.25m  ?
    BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_1M                         = 0x600;// 1.5M MF
    BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_2M                         = 0x600;// 1.5M MF
#elif (MIDDLE_FREQ_750_K == 1)
    BT_MODEM_P->REG_BT_RX_CORDIC_LPF.bit.RX_FRQ_SHIFT                       = MIDDLE_FREQ_1MPHY_750_KHZ;
    BT_MODEM_P->REG_BT_RX_CORDIC_2M.bit.RX_FRQ_SHIFT_2M                     = MIDDLE_FREQ_2MPHY_1P25_MHZ; //temp change to 1.5M IF //MIDDLE_FREQ_2MPHY_1P25_MHZ;
    BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_1M                         = 0x300;// 750K MF   0.75*1024
    BT_MODEM_P->REG_CRM_CFG0.bit.RX_PLL_FREQOFFS_2M                         = 0x500;// 1.25M MF  1.25*1024
    #endif

    //
    //BT_MODEM_P->REG_BT_RX_SYNC_CONST.bit.RX_CONSTVALUE                      = 0x1800; // internal sync threshold
    //BT_MODEM_P->REG_BT_RX_SYNC_G.bit.RX_G_COEF_SYNC                         = 0x0ea16; // BT   only,  GUASS filter sync threshold
    //BT_MODEM_P->REG_BT_RX_SYNC_G_LE.bit.RX_G_COEF_SYNC_LE                   = 0x16028; // ble  only   GUASS filter sync threshold
    BT_MODEM_P->REG_BT_RX_SYNC_CORR.bit.RX_COARSE_CORR_THD                    = 0x1240;//0x1000 0x1400; // bt only
    //BT_MODEM_P->REG_BT_RX_SYNC_CORR.bit.RX_COARSE_CORR_THD_LE               = 0x1000; //ble only
    //BT_MODEM_P->REG_BT_RX_SYNC_CORR_CODED.bit.RX_COARSE_CORR_THD_CODED      = 0x1600; //ble coded only
    //BT_MODEM_P->REG_BT_RX_SYNC_CORR_CODED.bit.RX_COARSE_CORR_THD_CODED_DC   = 0x1800; //ble coded only
    //BT_MODEM_P->REG_BT_RX_SYNC_GFSK_TH1.bit.RX_ADJUST_TH1                   = 0x400; // bt &ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_GFSK_TH2.bit.RX_ADJUST_TH2                   = 0x4000;// bt &ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_CODED_TH1.bit.RX_ADJUST_TH1_CODED            = 0x800; // ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_CODED_TH2.bit.RX_ADJUST_TH2_CODED            = 0x1000;// ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_CODED_CFG.bit.RX_CORR_SUM_ALPHA              = 0x2;// ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_CODED_CFG.bit.RX_COEF_CR_LOW_FO              = 0x733;// ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_CODED_CFG.bit.RX_COEF_CR_HIGH_FO             = 0x380;// ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_THD.bit.RX_COARSE_CR_THD_LOW                 = 0x800;// ble uncoded
    //BT_MODEM_P->REG_BT_RX_SYNC_THD.bit.RX_COARSE_WINDOW_THD                 = 0x1980;// ble uncoded    
    // use default value
    //BT_MODEM_P->REG_BT_RX_SYNC_CODED_CFG.bit.RX_LOCAL_START_POS             = 0x2;                // bt only br&edr, not coded reference
    BT_MODEM_P->REG_BT_RX_SYNC_CODED_CFG.bit.RX_SYNC_SAMP_OFFSET            = 0x0;                // bt&ble common,  not coded reference     sync position start offset
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_1.bit.RX_GFSK_U_ERR                    = 0x14; // bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_1.bit.RX_GFSK_U_SAM                    = 0x6;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_1.bit.RX_GFSK_TIME_TRACK_THD           = 0x0;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_1.bit.RX_GFSK_U_DC                     = 0x40;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_1.bit.RX_GFSK_SAMPLE_TRACK_ON          = 0x1;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_2.bit.RX_TH11                          = 0x29;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_2.bit.RX_TH22                          = 0xA;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_2.bit.RX_U_CUR_ERR                     = 0x2;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_GFSK_2.bit.RX_DC_ADAPT_ON                   = 0x1;// bt&ble uncoded common
    //BT_MODEM_P->REG_BT_RX_DEMOD_G.bit.RX_G_COEF_DEMOD                       = 0xf214;  // bt only
    //BT_MODEM_P->REG_BT_RX_DEMOD_G_LE.bit.RX_G_COEF_DEMOD_LE                 = 0x17023;   // ble only
    BT_MODEM_P->REG_BT_RX_DEMOD_DPSK.bit.RX_DPSK_U_ERR                      = 0x2e; // bt edr
    BT_MODEM_P->REG_BT_RX_DEMOD_DPSK.bit.RX_DPSK_U_SAM                      = 0x4;// bt edr
    //BT_MODEM_P->REG_BT_RX_DEMOD_DPSK.bit.RX_DPSK_TIME_TRACK_THD             = 0x3;// bt edr
    BT_MODEM_P->REG_BT_RX_DEMOD_DPSK.bit.RX_DPSK_U_DC                       = 0x26;// bt edr
    //BT_MODEM_P->REG_BT_RX_DEMOD_DPSK.bit.RX_DPSK_SAMPLE_TRACK_ON            = 0x1;// bt edr
    //BT_MODEM_P->REG_BT_RX_DEMOD_DPSK.bit.RX_SEL_AFC_SEEK                    = 0x1;// bt edr
    BT_MODEM_P->REG_BT_RX_DEMOD_VITERBI.bit.RX_DEMOD_IQ                     = 0x1;// ble coded only
    //BT_MODEM_P->REG_BT_RX_DEMOD_VITERBI.bit.RX_VITERBI_U_ERR                = 0x14;// ble coded only
    //BT_MODEM_P->REG_BT_RX_DEMOD_VITERBI.bit.RX_VITERBI_U_SAM                = 0x6;// ble coded only
    //BT_MODEM_P->REG_BT_RX_DEMOD_VITERBI.bit.RX_VITERBI_TIME_TRACK_THD       = 0x0;// ble coded only
    //BT_MODEM_P->REG_BT_RX_DEMOD_VITERBI.bit.RX_VITERBI_U_DC                 = 0x40;// ble coded only
    //BT_MODEM_P->REG_BT_RX_DEMOD_VITERBI.bit.RX_VITERBI_SAMPLE_TRACK_ON      = 0x1;// ble coded only
    //BT_MODEM_P->REG_TOP_CFG0.bit.TOP_CALIBR_WORK_EN                         = 0x0; // bt&ble common
    //BT_MODEM_P->REG_TOP_CFG0.bit.TOP_NORMAL_WORK_EN                         = 0x1; // bt&ble common
    BT_MODEM_P->REG_TX_CFG0.bit.TX_MODULATIONINDEX_BT                         = 10486; // jjsu@2024-4-8 //ble & bt not same
    //BT_MODEM_P->REG_TX_CFG7.bit.TX_GUARDTIMEMASK_END_POINT                  = 88;
    //BT_MODEM_P->REG_TX_CFG7.bit.TX_GUARDTIMEMASK_START_POINT                = 51;
    //BT_MODEM_P->REG_TX_CFG1.bit.TX_RAMPFASTEN                               = 0;// bt&ble common
    //BT_MODEM_P->REG_TX_CFG1.bit.TX_TXIQCOMPEN                               = 0; // // bt&ble common
    //BT_MODEM_P->REG_TX_CFG1.bit.TX_TXIQCOMPSHIFTCONTROL                     = 0;// bt&ble common
    //BT_MODEM_P->REG_DPD_CFG0.bit.DPD_TXDPDAMEN                              = 0; // bt&ble common
    //BT_MODEM_P->REG_DPD_CFG0.bit.DPD_TXDPDPMEN                              = 0;// bt&ble common
    //BT_MODEM_P->REG_TOP_CFG0.bit.TOP_CALIBR_WORK_EN                         = 0x0; // bt&ble common
    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_NORMAL_WORK_EN                         = 0x1; // bt&ble common
    //BT_MODEM_P->REG_TOP_CFG1.bit.TOP_TX_CLK_EN_DELAY                        = 50;
    //BT_MODEM_P->REG_TOP_CFG1.bit.TOP_TX_RST_RLS_DELAY                       = 1100;
    //BT_MODEM_P->REG_TOP_CFG2.bit.TOP_TX_RF_EN_DELAY                         = 120;
    //BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY                        = 10;
    //BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY                       = 1000;
#if (RF_MAX2830_SUPPORT)
    BT_MODEM_P->REG_TOP_CFG4.bit.TOP_RX_MODEM_EN_DELAY                      = 1720 + MAX2830_WRITE_SPI_CONSUME_TIME_US *24;
    BT_MODEM_P->REG_TOP_CFG2.bit.TOP_TX_MODEM_EN_DELAY                      = 1720 + (MAX2830_WRITE_SPI_CONSUME_TIME_US+1) *24;
#else
    uint32_t modem_used_pclk=    BT_CNTL_P->REG_BT_CTRL_CLK_CTRL.bit.MASTER_CLKSEL; //(CRM_GetHclkFreq()/1000000) ;

    BT_MODEM_P->REG_TOP_CFG4.bit.TOP_RX_MODEM_EN_DELAY                      = 0x47*modem_used_pclk + RXPWR_INC*modem_used_pclk; // (BLE_RXPWRUP0  - modem fifo deepth(8) + 1us)* CLK    rx modem first receive then bt link receive
    // (0x5a+(2-TX_PADBITLEN))*BT_MODEM_USED_CLK,  add for CMW500 crc_err
    BT_MODEM_P->REG_TOP_CFG2.bit.TOP_TX_MODEM_EN_DELAY                      = (0x5a+2)*modem_used_pclk; //2184;//1720; // (BLE_TXPWRUP0  + modem fifo deepth(8) + 1us )* CLK   tx bt link send first then modem send
#endif

    BT_MODEM_P->REG_TOP_CFG4.bit.TOP_RX_RF_EN_DELAY                      = 120 + 5*modem_used_pclk;

#if 0  // the follow value can be fixed at default value
#if (BT_MODEM_USED_CLK == 24)
    BT_MODEM_P->REG_TOP_CFG4.bit.TOP_RX_RF_EN_DELAY                         = 50;// default value, no need change
    BT_MODEM_P->REG_TOP_CFG2.bit.TOP_TX_RF_EN_DELAY                         = 120;// default value, no need change
#else 
    BT_MODEM_P->REG_TOP_CFG4.bit.TOP_RX_RF_EN_DELAY                         = 2*BT_MODEM_USED_CLK; // 96/BT_MODEM_USED_CLK
    BT_MODEM_P->REG_TOP_CFG2.bit.TOP_TX_RF_EN_DELAY                         = 5*BT_MODEM_USED_CLK;
#endif
#endif 

    //bt edr 
    //BT_MODEM_P->REG_TX_CFG4.bit.TX_DELTAPOWERGFSKDPSK                       = 5696;
    //BT_MODEM_P->REG_DPD_CFG0.bit.TX_GUARDTIME                               = 0x3;

    // modem tx 2 bit before linklayer tx start
    // remove for CMW500 crc_err
    //BT_MODEM_P->REG_TX_CFG1.bit.TX_PADBITLEN                                  = 2;// bt&ble common  4  extend 4bit preamble
    //ramp
    // br ramp en
    BT_MODEM_P->REG_TX_CFG1.bit.TX_RAMPUPEN                                 = 1;// bt&ble common
    BT_MODEM_P->REG_TX_CFG1.bit.TX_RAMPDOWNEN                               = 1; // bt&ble common
    // edr ramp en
    BT_MODEM_P->REG_TX_CFG1.bit.TX_RAMPUP4EDREN                             = 1;
    BT_MODEM_P->REG_TX_CFG1.bit.TX_RAMPDOWN4EDREN                           = 1;
    // ble ramp en
    BT_MODEM_P->REG_TX_CFG1.bit.TX_RAMPUP4BLEEN                             = 1;
    BT_MODEM_P->REG_TX_CFG1.bit.TX_RAMPDOWN4BLEEN                           = 1;

    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_CLK_EN_DELAY                        = 10 + 5*modem_used_pclk;
    BT_MODEM_P->REG_TOP_CFG3.bit.TOP_RX_RST_RLS_DELAY                       = 470;
    // add for CMW500 crc_err
    BT_MODEM_P->REG_TX_CFG7.bit.TX_GUARDBLEND_TIME                          = 0x55;
    // add for (DPSK_POWER - GFSK_POWER) in CMW500
    BT_MODEM_P->REG_TX_CFG4.bit.TX_DELTAPOWERGFSKDPSK                       = 0xa3;

#if 1 // (BT_WIFI_COEX == 1)
    // RF_SX_DOUBLER_EN is used for calculating frequence. // close RF_SX_DOUBLER_EN  or open RF_SX_DOUBLER_EN  is both ok for BT, but wifi should close RF_SX_DOUBLER_EN 
    //RFIF_P->REG_SX_REG0.bit.RF_SX_DOUBLER_EN                    = 1; // RF_SX_DOUBLER_EN default is 1
    BT_MODEM_P->REG_DOUBLER_EN.bit.DOUBLER_EN = 1;
#else
    // close RF_SX_DOUBLER_EN  or open RF_SX_DOUBLER_EN  is both ok for BT, but wifi should close RF_SX_DOUBLER_EN 
    RFIF_P->REG_SX_REG0.bit.RF_SX_DOUBLER_EN                    = 0;
    BT_MODEM_P->REG_DOUBLER_EN.bit.DOUBLER_EN = 0;
#endif
}

// modem reset fsm
void modem_reset(void)
{
    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_NORMAL_WORK_EN = 0;
    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_NORMAL_WORK_EN = 1;

    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_RESET_RX = 1;
    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_RESET_RX = 0; 

    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_RESET_TX = 1;
    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_RESET_TX = 0; 

    return;
}



void rfif_dac_clk_init(void)
{
#if 0
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_EN_FORCE             = 1; // debug to force open rfdac
    RFIF_P->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_EN               = 0;
    RFIF_P->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_EN               = 1;
    //RFIF_P->REG_TX_PPA_GAIN_BT_GRP1.bit.REG_RF_TX_PPA_GAIN_BT_7 = 25;
    //RFIF_P->REG_SX_REG0.bit.RF_SX_DOUBLER_EN                    = 0;
    //RFIF_P->REG_RX_LOGIC2.bit.REG_RF_RX_ABB_BUF_GC              = 0;
#endif
    //CMN_SYS_P->REG_PERI_CLK_CFG4.bit.ENA_RFIF_CLK           = 0x1;

#if !(IC_BOARD)
#if (RF_MAX2830_SUPPORT)
    // swap iq
    //BT_MODEM_P->REG_TOP_CFG0.bit.TOP_ADC_SWAP =1;
    BT_MODEM_P->REG_TOP_CFG0.bit.TOP_DAC_SWAP = 1;
#elif (RF_ARCS_B0_SUPPORT)
    // swap iq
    //BT_MODEM_P->REG_TOP_CFG0.bit.TOP_ADC_SWAP =1;
    //BT_MODEM_P->REG_TOP_CFG0.bit.TOP_DAC_SWAP = 1;
#endif
#endif

#if (CHIP == arcs)    // arcs adc bugfixed
   //AON_CTRL_P->REG_AON_LDO_CTRL4.bit.SEL_RXADC_VREF        = 2;// ref=2
#endif
 
#if 0 //(BT_WIFI_COEX == 0)
    CMN_SYS_NODFT_P->REG_BBPLL_LOGIC0.bit.REG_BBPLL_POSTDIV_SEL = 0;  // 0:bbpll96M  1:BBPLL160m
#endif 

    return;
}

void rfif_tx_power_config(void)
{
    //RFIF_P->REG_TX_REG5.bit.RF_TX_ABB_TIA_RFB = 1; //set power level, wifi is set to 3, default is 2

    //improve 3DH5 7dbm acp  0.5-1 dbm
    RFIF_P->REG_TX_REG3.bit.RF_TX_ABB_TIA_RFB_BT = 5;

    //RFIF_P->REG_TX_LOGIC1.bit.RF_TX_PPA_GAIN_BT_FORCE = 1;

    /*// ble 1M 2M
    RFIF_P->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_GAIN_BT_0 = 1;  // -12
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_1 = 3;  // -7
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_2 = 4;  // -3
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_3 = 7;  // 0
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_4 = 8;  // 3
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_5 = 14; // 6
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_6 = 18; // 9
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_7 = 24; // 12 */
    // ble 1M, 2M
    RFIF_P->REG_TX_LOGIC1.bit.REG_RF_TX_PPA_GAIN_BT_0 = 0;  // -18.4
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_1 = 2;  // -10.7
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_2 = 3;  // -7.6
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_3 = 4;  // -3.5
    RFIF_P->REG_TX_LOGIC2.bit.REG_RF_TX_PPA_GAIN_BT_4 = 7;  // 0.0
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_5 = 10; // 3.5
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_6 = 14; // 6.3
    RFIF_P->REG_TX_LOGIC3.bit.REG_RF_TX_PPA_GAIN_BT_7 = 16; // 9.1

}

void rfif_init(void)
{
#if (BT_WIFI_COEX == 0)    
    RFIF_P->REG_CTRL0.bit.BT_WF_FORCE =1; // BIT4
    RFIF_P->REG_CTRL0.bit.REG_BT_WF = 1; //  BIT3 0:wifi  1: bt
#endif            

    rfif_dac_clk_init();

    rfif_tx_power_config();

#if 0 //this funciton is config   
    //if(CRM_GetCmn_peri_pclkFreq() >= 96000000)
    {
        // the default value is calculated by 24M cmn_peri_pclk, if cmn_peri_pclk change to 96M, the valued should *4;
        uint32_t multi = CRM_GetCmn_peri_pclkFreq() / CRM_GetSrcFreq(CRM_IpSrcXtalClk);
        RFIF_P->REG_DELAY_CTRL0.bit.DELAY1 = RFIF_DELAY1_DEF*multi;
        
        RFIF_P->REG_DELAY_CTRL1.bit.DELAY2 = RFIF_DELAY2_DEF*multi;
        RFIF_P->REG_DELAY_CTRL1.bit.DELAY3 = RFIF_DELAY3_DEF*multi;
        RFIF_P->REG_DELAY_CTRL1.bit.DELAY4 = RFIF_DELAY4_DEF*multi;
        RFIF_P->REG_DELAY_CTRL1.bit.DELAY5 = RFIF_DELAY5_DEF*multi;
        
        RFIF_P->REG_DELAY_CTRL2.bit.DELAY7 = RFIF_DELAY7_DEF*multi;
        RFIF_P->REG_DELAY_CTRL2.bit.DELAY8 = RFIF_DELAY8_DEF*multi;
        RFIF_P->REG_DELAY_CTRL2.bit.DELAY9 = RFIF_DELAY9_DEF*multi;    


    }
#endif    
}


void cmn_sys_ctrl_init(void)
{
	//CMN_SYS_P->REG_PERI_CLK_CFG1.bit.ENA_UART0_CLK = 1;				// for uart rx ok(maybe for fpga only)
    ///offset B8
    //CMN_SYS_P->REG_SYSPLL_CTRL.bit.SYSPLL_VCO_KVCO = 0x5;         //default value 6,  modified value 5
    //CMN_SYS_P->REG_SYSPLL_CTRL.bit.SYSPLL_VCO_IBIT = 0x5;         //default value 3,  modified value 5

    ///offset BC
    //CMN_SYS_P->REG_SYSPLL_CTRL1.bit.SYSPLL_VCO_LDO_OUT = 0x4;     //default value 3,  modified value 4
 #if 0 //(CHIP == arcs)
    //move to rf_por_config function
    CMN_SYS_NODFT_P->REG_BBPLL_REG0.bit.BBPLL_VCO_LDO_OUT = 0x5;   // change for bbpll voltage value
    CMN_SYS_NODFT_P->REG_BBPLL_REG0.bit.BBPLL_LDO_OUT = 0x4;       // change for bbpll voltage value
#endif    
}

void aon_ctrl_init(void)
{
#if (CHIP == arcs)
    //move to rf_por_config function
    //AON_CTRL_P->REG_AON_LDO_CTRL1.bit.TUNE_LDOCORE = 55;     //default value 0,  change for vdd core voltage value
#endif    
    AON_CTRL_P->REG_POWER_EXT_CTRL.bit.ENA_EN_LDO_CORE_EXT = 1;
}


void bt_drv_reg_init(uint8_t reset_state)
{
    PTCH(void, modem_rf_reg_init);

    if(reset_state ==0) // ==LSIP_INIT //only run onetime, hci reset no need process
    {
        lsip_modem_env_init();
        
        aon_ctrl_init();
        bt_nvs_init();
    }

    modem_init();

    linklayer_init();

    rfif_init();

    cmn_sys_ctrl_init();


    extra_cfg();

    debug_diag_config();

#if (RF_MAX2830_SUPPORT)
    rf_gpio_init();
#endif

    return;

}

void sleep_wakeup_reg_cfg(void)
{

    ///0x00, 0x00100100  // Turn off BLE Core
    BT_BLE_P->REG_BLE_LSBLECNTL.all = 0x00100400 | (BLE_NORMAL_WIN_SIZE+1);

    //BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_LSBLE_EN = 0;
    //BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_MD_DSB   = 1;
    //BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_ANONYMOUS_ADVERT_FILT_EN   = 1;

    ///0x18, 0x0000800e
    BT_BLE_P->REG_BLE_INTCNTL1.all = 0x0000800e;
    //BT_BLE_P->REG_BLE_INTCNTL1.bit.BLE_SLPINTMSK   = 1;
    //BT_BLE_P->REG_BLE_INTCNTL1.bit.BLE_CRYPTINTMSK = 1;
    //BT_BLE_P->REG_BLE_INTCNTL1.bit.BLE_SWINTMSK    = 1;
    //BT_BLE_P->REG_BLE_INTCNTL1.bit.BLE_FIFOINTMSK  = 1;

    ///0x20
    BT_BLE_P->REG_BLE_INTACK1.all  = 0xFFFFFFFF;

    ///0x28, 0x00000121
    //BT_BLE_P->REG_BLE_INTCNTL1.bit.BLE_FIFOINTMSK  = 1;

    ///0xE0, 0x011800C8
    BT_BLE_P->REG_BLE_TIMGENCNTL.all = 0x011800C8;
    //BT_BLE_P->REG_BLE_TIMGENCNTL.bit.BLE_PREFETCH_TIME          = (IP_PREFETCH_TIME_US + RXPWR_INC)<<1;
    //BT_BLE_P->REG_BLE_TIMGENCNTL.bit.BLE_PREFETCHABORT_TIME     = (IP_PREFETCHABORT_TIME_US + RXPWR_INC)<<1;

    //0x0C
    BT_BLE_P->REG_BLE_INTCNTL0.all = 0;
    ///0x0C, 0x0001001E
    BT_BLE_P->REG_BLE_INTCNTL0.all = 0x0001001E;
    //BT_BLE_P->REG_BLE_INTCNTL0.bit.BLE_ENDEVTINTMSK  = 1;
    //BT_BLE_P->REG_BLE_INTCNTL0.bit.BLE_SKIPEVTINTMSK = 1;
    //BT_BLE_P->REG_BLE_INTCNTL0.bit.BLE_TXINTMSK      = 1;
    //BT_BLE_P->REG_BLE_INTCNTL0.bit.BLE_RXINTMSK      = 1;
    //BT_BLE_P->REG_BLE_INTCNTL0.bit.BLE_ERRORINTMSK   = 1;

    BT_BLE_P->REG_BLE_INTACK0.all = 0xFFFFFFFF;

    ///0xD0
    BT_BLE_P->REG_BLE_RFTESTCNTL.bit.BLE_TXPLDSRC   = 0;

    ///0x3C, 0x14050014
    BT_BLE_P->REG_BLE_ENBPRESET.all = 0x14050014;
    //BT_BLE_P->REG_BLE_ENBPRESET.bit.BLE_TWRM   = 0x14;
    //BT_BLE_P->REG_BLE_ENBPRESET.bit.BLE_TWOSC  = 0x140;
    //BT_BLE_P->REG_BLE_ENBPRESET.bit.BLE_TWEXT  = 0xA0;

    ///0x130, 0xff9602d9
    BT_BLE_P->REG_BLE_ADVTIM.all = 0xFF9602D9;
    //BT_BLE_P->REG_BLE_ADVTIM.bit.BLE_ADVINT        = 0x2d9; //729
    //BT_BLE_P->REG_BLE_ADVTIM.bit.BLE_RX_AUXPTR_THR = 0x96;  //150
    //BT_BLE_P->REG_BLE_ADVTIM.bit.BLE_TX_AUXPTR_THR = 0xFF;

    ///0x140, 0x0x0003009E    activity = 5
    BT_BLE_P->REG_BLE_WPALCNTL.all = 0x00060161;
    //BT_BLE_P->REG_BLE_WPALCNTL.bit.BLE_WPALBASEPTR = 0x9E;
    //BT_BLE_P->REG_BLE_WPALCNTL.bit.BLE_WPALNBDEV   = 0x03;  //(BLE_ACTIVITY_MAX + 1) ??

    ///0x144, 0x0000009E      activity = 5
    BT_BLE_P->REG_BLE_WPALCURRENPTR.all = 0x00000161;
    //BT_BLE_P->REG_BLE_WPALCURRENPTR.bit.BLE_WPALCURRENPTR = 0x9E;

    ///0x170, 0x000300A7      activity = 5
    BT_BLE_P->REG_BLE_RALCNTL.all = 0x00030173;
    //BT_BLE_P->REG_BLE_RALCNTL.bit.BLE_RALBASEPTR = 0xA7;
    //BT_BLE_P->REG_BLE_RALCNTL.bit.BLE_RALNBDEV   = 0x03;

    ///0x174, 0x000000A7      activity = 5
    BT_BLE_P->REG_BLE_RALCURRENTPTR.bit.BLE_RALCURRENTPTR = 0x173;

    BT_BLE_P->REG_BLE_ETPTR.all = 0x00;  //EM_ET_OFFSET>>2

    // Enable off BLE Core
    BT_BLE_P->REG_BLE_LSBLECNTL.bit.BLE_LSBLE_EN = 1;
}

#if RFCALI_BT_EN
/// wakeup restore bt calibrate value
void bt_wakeup_calib_value_reinit(void)
{
    // bt rxdcoc calib value restore
    bt_set_dcoc_word(g_bt_rxdc_comp, 3, 3);
    // bt rxiq calib value restore
    bt_cali_rxiq_result(g_bt_rxiq_comp.c21, g_bt_rxiq_comp.c22);
    // bt rxrc calib value restore
    set_abb_cap_bt(g_bt_rxrc_comp);
    // bt txiq calib value restore
    bt_cali_txiq_set(g_bt_txiq_comp.c21, g_bt_txiq_comp.c22);
    // bt txdc calib value restore
    bt_cali_txdc_set(g_bt_txdc_comp.dac_i, g_bt_txdc_comp.dac_q);
    
}
#endif

void sleep_wakeup_reg_init(void)
{
    #if (REG_CFG_WITH_STRUCTER)
    sleep_wakeup_reg_cfg();
    #else
    //ls_reg_init_cfg2(BT_REG_ITEM_BLE_LINKLAYER, ble_link_init_cfg_table, sizeof(ble_link_init_cfg_table)/sizeof(struct BT_REG_INIT_ITEM));
    #endif
    #if RFCALI_BT_EN
    bt_wakeup_calib_value_reinit();
    #endif
}





