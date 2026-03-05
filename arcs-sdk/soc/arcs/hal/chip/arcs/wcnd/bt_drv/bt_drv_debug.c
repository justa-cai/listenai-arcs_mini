
#include <stdio.h>
#include <string.h>         // for memcpy
#include <stdlib.h>         // standard lib functions
#include <stddef.h>         // standard definitions
#include <stdint.h>         // standard integer definition
#include <stdbool.h>        // boolean definition

#include "ble_drv.h"
#include "bt_drv.h"
#include "IOMuxManager.h"


/*
* DEFINES
*****************************************************************************************
*/

#define LINKLAYER_DEBUG_EN          (0)
#define CLOCK_DEBUG_EN              (0)
#define MODEM_DEBUG_EN              (0)
#define RXIQ_DEBUG_EN               (0)
#define TXIQ_DEBUG_EN               (0)
#define BBPLL_FORCE_DEBUG_EN        (0)
#define FIXED_GAIN_DEBUG_EN         (0)
#define DCOC_DEBUG_EN               (0)
#define SX_DEBUG_DEBUG_EN           (0)

#define DEBUG_CONFIG_EN (LINKLAYER_DEBUG_EN || CLOCK_DEBUG_EN || MODEM_DEBUG_EN || RXIQ_DEBUG_EN || TXIQ_DEBUG_EN || BBPLL_FORCE_DEBUG_EN || FIXED_GAIN_DEBUG_EN || DCOC_DEBUG_EN || SX_DEBUG_DEBUG_EN)
 

/*
 * ENUMERATION DEFINITION
 *****************************************************************************************
 */
#if (DEBUG_CONFIG_EN)
extern int32_t IOMuxManager_PinConfigure (uint8_t pad, uint8_t pin_num, uint32_t pin_cfg);

void debug_out_en()
{
    //output bt dbg signal
    CMN_SYS_P->REG_TEST_CTRL.bit.DBG_OUT_EN =  1;
    CMN_SYS_P->REG_TEST_CTRL.bit.DBG_OUT_SEL = 9;

    return;
}

void debug_clk_en()
{
    //output clk signal
    CMN_SYS_P->REG_TEST_CTRL.bit.DBG_CLK_EN =   1;
    CMN_SYS_P->REG_TEST_CTRL.bit.DBG_CLK_SEL =  16; // 16 HCLK, 3 XO24M

    return;
}


#if (LINKLAYER_DEBUG_EN)
extern uint32_t g_top_diag_port_sel;
void linklayer_debug_config(void)
{


    BT_CNTL_P->REG_BT_CTRL_DBG_MUX.all = g_top_diag_port_sel;

    debug_out_en();

#if (IC_BOARD == 1)  
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER19); //dbg0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER19); //dbg1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER19); //dbg2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER19); //dbg3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER19); //dbg4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21,  CSK_IOMUX_FUNC_ALTER19); //dbg5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6,  CSK_IOMUX_FUNC_ALTER19);  //dbg6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7,  CSK_IOMUX_FUNC_ALTER19);  //dbg7 
//    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 8,  CSK_IOMUX_FUNC_ALTER19);  //dbg8
//    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 9,  CSK_IOMUX_FUNC_ALTER19);  //dbg9 
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10,  CSK_IOMUX_FUNC_ALTER19);  //dbg10
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11,  CSK_IOMUX_FUNC_ALTER19);  //dbg11 
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12,  CSK_IOMUX_FUNC_ALTER19);  //dbg12
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13,  CSK_IOMUX_FUNC_ALTER19);  //dbg13 
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14,  CSK_IOMUX_FUNC_ALTER19);  //dbg14
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15,  CSK_IOMUX_FUNC_ALTER19);  //dbg15 
#endif
    return;
}
#endif

#if (CLOCK_DEBUG_EN)
void clock_debug_config(void)
{
    debug_clk_en();
#if 0  //arcs a0
        // bt link layer dbg     
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER18); //dbg0
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER18); //dbg1
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, CSK_IOMUX_FUNC_ALTER18); //dbg2
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 3, CSK_IOMUX_FUNC_ALTER18); //dbg3
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER18); //dbg4
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5,  CSK_IOMUX_FUNC_ALTER18); //dbg5
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6,  CSK_IOMUX_FUNC_ALTER18);  //dbg6
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7,  CSK_IOMUX_FUNC_ALTER18);  //dbg7 
#endif
#if 0  //arcs c0   
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER18); //dbg0
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER18); //dbg1
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER18); //dbg2
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER18); //dbg3
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER18); //dbg4
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21,  CSK_IOMUX_FUNC_ALTER18); //dbg5
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22,  CSK_IOMUX_FUNC_ALTER18);  //dbg6
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7,  CSK_IOMUX_FUNC_ALTER18);  //dbg7 
#endif

#if 1  //arcs d0   
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER18); //dbg0
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER18); //dbg1
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER18); //dbg2
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, CSK_IOMUX_FUNC_ALTER18); //dbg3
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER18); //dbg4
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21,  CSK_IOMUX_FUNC_ALTER18); //dbg5
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6,  CSK_IOMUX_FUNC_ALTER18);  //dbg6
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7,  CSK_IOMUX_FUNC_ALTER18);  //dbg7 
    
#endif

    return;
}
#endif


#if (MODEM_DEBUG_EN)
void modem_debug_config(void)
{
    //1 step config output modem signal
    /*
    debug mode 
    1:  debug_out[0]      clk_adc_x1
        debug_out[25]     top2rx_modem_en
        debug_out[24~22]  iqadc_i[10:8]
        debug_out[21~19]  iqadc_q[10:8]
        debug_out[18~10]  iqadc_i[11], iqadc_i[7:0]
        debug_out[9~1]    iqadc_q[11], iqadc_q[7:0]

    2:  debug_out[0]      clk_rssi_adc
        debug_out[25]     top2rx_modem_en
        debug_out[15~10]  rssuadc_i[5:0]
        debug_out[6~1]    rssiadc_q[5:0]

    3:  debug_out[0]      clk_dac_x4
        debug_out[25]     dac_iq_out_en_tx
        debug_out[23]     clk_dac
        debug_out[22~22]  iqdac_i[8]
        debug_out[19~19]  iqdac_q[8]
        debug_out[18~10]  iqdac_i[9], iqdac_i[7:0]
        debug_out[9~1]    iqdac_q[9], iqdac_q[7:0]

    4:  debug_out[0]      clk_adc_x1
        debug_out[1]      top2rx_modem_en
        debug_out[13~2]   iqadc_i

    5:  debug_out[0]      clk_adc_x1
        debug_out[1]      top2rx_modem_en
        debug_out[13~2]   iqadc_Q

    6:  debug_out[3:0]     agc2debug_cstate
        debug_out[7:4]     agc2rf_LnaGain_Idx
        debug_out[11:8]    agc2rf_AbbGain_Idx

    7:  debug_out[0]     clk_adc_x1
        debug_out[16:1]  corr_value
        debug_out[17]    top2rx_modem_en       
    */
    BT_MODEM_P->REG_TOP_CFG6.bit.TOP_DEBUG_MODE = 4;//  1:ADC  2: RSSI ADC  3: DAC_X4  4: ADC_X1   MODEM EN 5:ADC   6:AGC  7: corr_value

    // 2 step: cfg top IOMUX
    debug_out_en();

    // 3 modem dbg GPIO CONFIG     
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, CSK_IOMUX_FUNC_ALTER23); //modem_dbg0  uart0 tx
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 3, CSK_IOMUX_FUNC_ALTER23); //modem_dbg1  uart0 rx
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER23); //modem_dbg2   uart1 tx
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5,  CSK_IOMUX_FUNC_ALTER23); //modem_dbg3     uart1 rx
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 8,  CSK_IOMUX_FUNC_ALTER23);  //modem_dbg4  jtag
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 9,  CSK_IOMUX_FUNC_ALTER23);  //modem_dbg5  jtag
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10,  CSK_IOMUX_FUNC_ALTER23);  //modem_dbg6
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11,  CSK_IOMUX_FUNC_ALTER24);  //dbg7

    return;
}
#endif

#if (RXIQ_DEBUG_EN)
void rxiq_debug_config(void)
{
    debug_out_en();

    AON_IOMUX_P->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 11;

    AON_IOMUX_P->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 11;

    AON_IOMUX_P->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 11;

    AON_IOMUX_P->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = 11;

    return;
}
#endif

#if (TXIQ_DEBUG_EN)
void txiq_debug_config(void)
{
    debug_out_en();

    //to disable PA out, and view TXIQ output
    AON_IOMUX_P->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = 9;

    AON_IOMUX_P->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 9;

    AON_IOMUX_P->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = 9;

    AON_IOMUX_P->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_FSEL = 3;
    AON_IOMUX_P->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = 9;

#if 0
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_EN_FORCE = 0x1; //# 1 bits
    RFIF_P->REG_TX_DAC_LOGIC0.bit.REG_RFDAC_EN = 0x1; //# 1 bits
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_SRC_10U_TRIM = 0x0; //# 5 bits
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_ISINK_I_TRIM = 0x0;// # 8 bits
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_ISINK_Q_TRIM = 0x0; //# 8 bits
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_ISINK_EN = 0x0; //# 1 bits
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_EDGE_LATCH_SEL = 0x0;// # 2 bits
    RFIF_P->REG_TX_DAC_LOGIC0.bit.RFDAC_TEST_SEL = 0x1; //# 3 bits

    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_PPA_EN_FORCE = 0x1;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_UPC_LO_EN_FORCE = 0x01; //# 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_PA_EN_FORCE = 0x1; //# 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_PA_DPD_EN_FORCE = 0x1;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_PA_TTG_EN_FORCE = 0x1;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_PA_X2_EN_FORCE = 0x01;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_ABB_EN_FORCE = 0x1; //# 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.RF_TX_ABB_EN_FORCE = 0x1;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_PPA_EN = 0x0;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_UPC_LO_EN = 0x0;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_PA_EN = 0x0;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_PA_DPD_EN = 0x0;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_PA_TTG_EN = 0x0;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_PA_X2_EN = 0x0;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_ABB_EN = 0x0;// # 1 bits
    RFIF_P->REG_TX_LOGIC0.bit.REG_RF_TX_ABB_EN = 0x0;// # 2 bits
#endif
    return;
}
#endif

#if (BBPLL_FORCE_DEBUG_EN)
void bbpll_force_config(void)
{
    debug_out_en();

    //RFIF_P->REG_RFBG_LOGIC.bit.RFBG_EN_FORCE = 1;

    RFIF_P->REG_BBPLL_LOGIC.all = 0x15ed79;   // default 0x2

    return;
}
#endif

#if (FIXED_GAIN_DEBUG_EN)
void fixed_gain_config(void)
{
    debug_out_en();

    BT_MODEM_P->REG_BT_AGC_CFG0.bit.AGC_PARAM_AGC_EN = 0;//closed agc
    BT_MODEM_P->REG_BT_CFG7.bit.AGC_ABBINT = 0xb;
    BT_MODEM_P->REG_BT_CFG7.bit.AGC_LNAINIT = 0x7;
    //BT_MODEM_P->REG_BT_CFG7.bit.AGC_LNAINIT = 4;

#if 0 
    //rf fix
    RFIF_P->REG_RX_LOGIC2.bit.RF_RX_LNA_GC_FORCE = 1;
    RFIF_P->REG_RX_LOGIC2.bit.RF_RX_ABB_BQ_GC_FORCE = 1;
    //RFIF_P->REG_RX_LOGIC2.bit.RF_RX_ABB_BUF_GC_FORCE = 1;
    RFIF_P->REG_RX_LOGIC2.bit.RF_RX_LNA_GC = 8; //48dbm
    RFIF_P->REG_RX_LOGIC2.bit.RF_RX_ABB_BQ_GC = 11;//22dbm
    //RFIF_P->REG_RX_LOGIC2.bit.RF_RX_ABB_BUF_GC = ?;
#endif

    return;
}
#endif

#if (DCOC_DEBUG_EN)
void dcoc_debug_config(void)
{
    debug_out_en();

    RFIF_P->REG_RX_ABB_DCOC_DAC_FORCE.bit.RF_RX_ABB_DCOC_DACQ_FORCE = 1;//
    RFIF_P->REG_RX_ABB_DCOC_DACQ_WF_GRP0.bit.REG_RF_RX_ABB_DCOC_DACQ_WF_00 = 0x20;//Q


    //RFIF_P->REG_RX_ABB_DCOC_DACI_WF_GRP0.bit.RF_RX_ABB_DCOC_DACI_WF_00 = 0xA;//I
    
    RFIF_P->REG_RX_LOGIC8.bit.REG_RF_RX_ABB_DCOC_DAC_SC_0 = 5;
    RFIF_P->REG_RX_LOGIC8.bit.REG_RF_RX_ABB_DCOC_DAC_SC_1 = 1;   // bt mode
    RFIF_P->REG_RX_LOGIC8.bit.RF_RX_ABB_DCOC_DAC_SC_FORCE = 1; // bt mode

    return;
}
#endif

#if (SX_DEBUG_DEBUG_EN)
void sx_debug_config(void)
{
    debug_out_en();

    RFIF_P->REG_SX_LOGIC2.bit.REG_RF_SX_DIVN_INTEG = 135; // 2440.75@1.25   ROUNDDOWN((freq*2/0.75)/48, 0)
    RFIF_P->REG_SX_LOGIC2.bit.REG_RF_SX_DIVN_FRAC = 626233; //2440.75@1.25   ROUNDDOWN((freq*2/0.75)/48 - RF_SX_DIVN_INTEG)*2^20, 0)
    
    RFIF_P->REG_SX_LOGIC2.bit.RF_SX_DIVN_INTEG_FORCE = 1; 
    RFIF_P->REG_SX_LOGIC2.bit.RF_SX_DIVN_FRAC_FORCE = 1;

    return;
}
#endif

#endif//DEBUG_CONFIG_EN

void fpga_connect_remote_board_config(void)
{
    // JTAG CONFIG

    //UART0 CONFIG
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, CSK_IOMUX_FUNC_ALTER2); //dbg2   uart0_rxd
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 3, CSK_IOMUX_FUNC_ALTER2); //dbg3   uart0_txd
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11,  CSK_IOMUX_FUNC_ALTER2);  //dbg11 uart0_rts_n
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12,  CSK_IOMUX_FUNC_ALTER2);  //dbg12 uart0_cts_n 
    //UART1 CONFIG
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21,  CSK_IOMUX_FUNC_ALTER3);  //dbg9 uart1_txd
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22,  CSK_IOMUX_FUNC_ALTER3); //dbg10 uart1_rxd 
    //SPI CONFIG
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6,  CSK_IOMUX_FUNC_ALTER5);  //dbg6 spi_cs_n
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14,  CSK_IOMUX_FUNC_ALTER5);  //dbg14 spio_mosi
    //IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15,  CSK_IOMUX_FUNC_ALTER5);  //dbg15 spi_clk    

    return;
}

void GPIO_SET_OUTPUT(uint32_t A_OR_B, uint32_t GPIO_NUM, uint32_t high_or_low)
{
    if(A_OR_B == 0) //GPIOA
    {
        if (high_or_low)
        {
            (*((volatile uint32_t *)(CMN_IOMUX_BASE+ GPIO_NUM*4))) =  0x03700000; // GPIO HIGH  oen_reg 1:input 0:output out_reg 0:low 1:high
        }
        else
        {
            (*((volatile uint32_t *)(CMN_IOMUX_BASE+ GPIO_NUM*4))) =  0x03500000; // GPIO LOW  oen_reg 1:input 0:output out_reg 0:low 1:high
        }
    }
    else //GPIOB
    {
        if (high_or_low)
        {
            (*((volatile uint32_t *)(AON_IOMUX_BASE+ GPIO_NUM*4))) =  0x03700006; // GPIO HIGH
        }
        else
        {
            (*((volatile uint32_t *)(AON_IOMUX_BASE+ GPIO_NUM*4))) =  0x03500006; // GPIO LOW
        }
    }

    return;
}



void debug_diag_config(void)
{
#if LINKLAYER_DEBUG_EN
    linklayer_debug_config();
#endif

#if MODEM_DEBUG_EN
    modem_debug_config();
#endif

#if RXIQ_DEBUG_EN
    rxiq_debug_config();
#endif

#if TXIQ_DEBUG_EN
    txiq_debug_config();
#endif

#if BBPLL_FORCE_DEBUG_EN
    bbpll_force_config();
#endif

#if FIXED_GAIN_DEBUG_EN
    fixed_gain_config();
#endif

#if DCOC_DEBUG_EN
    dcoc_debug_config();
#endif

#if SX_DEBUG_DEBUG_EN
    sx_debug_config();
#endif

#if (CLOCK_DEBUG_EN)
    clock_debug_config();
#endif

#if (RF_MAX2830_SUPPORT)
    //fpga_connect_remote_board_config();
#endif

}


