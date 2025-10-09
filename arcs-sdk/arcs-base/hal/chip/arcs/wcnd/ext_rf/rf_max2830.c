/*
 * max2830_test.c
 *
 *  Created on:
 *
 *
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include "bt_config.h"
#if (RF_MAX2830_SUPPORT)
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include <string.h>
#include <stdio.h>

#include "nos_timer.h"

#include "spi.h"
#include "Driver_GPIO.h"
#include "Driver_SPI.h"
#include "Driver_UART.h"

#include "rf_max2830.h"
#include "bt_drv.h"
#include "log_print.h"
#include "clock_config.h"



#define SPI0_CLK_PIN            21
#define SPI0_CS_PIN             18
#define SPI0_MISO_PIN           19
#define SPI0_MOSI_PIN           20

#define SPI1_HOLD_PIN           16
#define SPI1_CLK_PIN            17
#define SPI1_WP_PIN             18
#define SPI1_CS_PIN             19
#define SPI1_MISO_PIN           20
#define SPI1_MOSI_PIN           21


#define BT_CTRL_INT_bt_dac_trig_stat_bit                0x10
#define BT_CTRL_INT_rxen_off_stat_bit                   0x08
#define BT_CTRL_INT_txen_off_stat_bit                   0x04
#define BT_CTRL_INT_rxen_on_stat_bit                    0x02
#define BT_CTRL_INT_txen_on_stat_bit                    0x01

#define RF_TX_TEST             1   // 1:TX;  0: RX
#define RF_RX_TEST             0   // 1:TX;  0: RX
//#define ZERO_MIDDLE_FREQ     0

#define RF_RXHP_PIN             0
#define RF_RXTX_PIN             1
#define RF_SHDNB_PIN            2


/*
 0 middle frequency
*/
uint8_t freq_int_0M_table[80]=
{
//2402
120,120,120,120,120,120,120,120,120,120,
120,120,120,120,120,120,120,120,121,121,
121,121,121,121,121,121,121,121,121,121,
121,121,121,121,121,121,121,121,122,122,
122,122,122,122,122,122,122,122,122,122,
122,122,122,122,122,122,122,122,123,123,
123,123,123,123,123,123,123,123,123,123,
123,123,123,123,123,123,123,123,124, //2480
};

/*
 1.5MHZ middle frequency
*/
uint8_t freq_int_1P5M_table[80]=
{
//2402
120,120,120,120,120,120,120,120,120,120,
120,120,120,120,120,120,120,120,120,120,
121,121,121,121,121,121,121,121,121,121,
121,121,121,121,121,121,121,121,121,121,
122,122,122,122,122,122,122,122,122,122,
122,122,122,122,122,122,122,122,122,122,
123,123,123,123,123,123,123,123,123,123,
123,123,123,123,123,123,123,123,123, //2480

};
/*
 0HZ middle frequency
*/    
int tx_freq_fac_table[80]=
{
//2402
104857,157286,209715,262144,314572,367001,419430,471859,524288,576716,
629145,681574,734003,786431,838860,891289,943718,996146,0     ,52429 ,
104857,157286,209715,262144,314572,367001,419430,471859,524288,576716,
629145,681574,734003,786431,838860,891289,943718,996146,0     ,52429 ,
104857,157286,209715,262144,314572,367001,419430,471859,524288,576716,
629145,681574,734003,786431,838860,891289,943718,996146,0     ,52429 ,
104857,157286,209715,262144,314572,367001,419430,471859,524288,576716,
629145,681574,734003,786431,838860,891289,943718,996146,0     , //2480
};

/*
 1.5MHZ middle frequency
*/

int rx_freq_fac_table[80]=
{
//2402
26214  ,78643  ,131072 ,183501 ,235929 ,288358 ,340787 ,393216 ,445644 ,498073 ,
550502 ,602931 ,655359 ,707788 ,760217 ,812646 ,865074 ,917503 ,969932 ,1022361,
26214  ,78643  ,131072 ,183501 ,235929 ,288358 ,340787 ,393216 ,445644 ,498073 ,
550502 ,602931 ,655359 ,707788 ,760217 ,812646 ,865074 ,917503 ,969932 ,1022361,
26214  ,78643  ,131072 ,183501 ,235929 ,288358 ,340787 ,393216 ,445644 ,498073 ,
550502 ,602931 ,655359 ,707788 ,760217 ,812646 ,865074 ,917503 ,969932 ,1022361,
26214  ,78643  ,131072 ,183501 ,235929 ,288358 ,340787 ,393216 ,445644 ,498073 ,
550502 ,602931 ,655359 ,707788 ,760217 ,812646 ,865074 ,917503 ,969932 , //2480
};




extern void test_debug_tx_test_tone_mode(uint8_t enable);
extern void test_debug_force_gpio_trx(uint8_t trx);

/*
    freq_int = INT(expected_freq/20.0)   //20MHZ bandwidth
    freq_frac = INT((expected_freq - freq_int*1048575+0.5)  // (2^20-1)=1048575
*/
static int rf_calc_freq(uint8_t tx_or_rx, uint8_t desired, uint8_t* pData)
{
    int frac= 0.0;
    uint8_t freq_int = 0;

    if (tx_or_rx ==1)
    {
        frac = tx_freq_fac_table[desired];
        freq_int = freq_int_0M_table[desired];
    }
    else
    {
        frac = rx_freq_fac_table[desired];
        freq_int = freq_int_1P5M_table[desired];
    }
	// register 3
	pData[0] = (uint8_t)((frac & 0x0000003F) >> 4);
	pData[1] = (uint8_t)(((frac & 0x0000003F) << 4) | ((freq_int & 0x000000FF) >> 4));
	pData[2] = (uint8_t)(((freq_int & 0x000000FF) << 4) | 0x03);

	// register 4
	pData[3] = (uint8_t)((frac & 0x000FFFC0) >> 18);
	pData[4] = (uint8_t)((frac & 0x000FFFC0) >> 10);
	pData[5] = (uint8_t)(((frac & 0x000003C0) >> 2) | 0x04);

	return 0;

}

void spi_tx_done(void *spi_dev)
{
    SPI_DEV *pDev = (SPI_DEV *)(spi_dev);
    int flag = 0;
    // if tx fifo empty
    flag = (pDev->reg->STATUS & (1 << 22));
    while(!flag)
        flag = (pDev->reg->STATUS & (1 << 22));

    pDev->info->status.bit.busy = 0;

    // several ms delay
    for(int i = 0; i < 10000; i++);

}

int rf_set_freq(uint8_t tx_or_rx, uint8_t freq)
{
    void* spi_dev = SPI1();  
    uint8_t spidata[2*3];
    
    if(0 != rf_calc_freq(tx_or_rx, freq, spidata))
        return -1;
 
    for(int i=0; i < 2; i++)
    {
        SPI_Send(spi_dev, &spidata[i*3], 3);
        spi_tx_done(spi_dev);
    }

    return 0;
}


static void config_spi0_iomux()
{
    //SPI0 pin configuration
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI0_CLK_PIN, CSK_IOMUX_FUNC_ALTER5);  // CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI0_CS_PIN, CSK_IOMUX_FUNC_ALTER5);  // CS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI0_MOSI_PIN, CSK_IOMUX_FUNC_ALTER5);  // MOSI
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI0_MISO_PIN, CSK_IOMUX_FUNC_ALTER5);  // MISO
}
static void config_spi1_iomux()
{
    //SPI0 pin configuration
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI1_HOLD_PIN, CSK_IOMUX_FUNC_ALTER6);  // HOLD
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI1_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);   // CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI1_WP_PIN, CSK_IOMUX_FUNC_ALTER6);    // WP
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI1_CS_PIN, CSK_IOMUX_FUNC_ALTER6);    // CS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI1_MOSI_PIN, CSK_IOMUX_FUNC_ALTER6);  // MOSI
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI1_MISO_PIN, CSK_IOMUX_FUNC_ALTER6);  // MISO
}


void spi_initial_clock(int spi_num)
{
    if (spi_num == 0)
    {
        CMN_SYS_P->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_LD = 1;
        CMN_SYS_P->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_N = 1;
        CMN_SYS_P->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_M = 1;
        CMN_SYS_P->REG_PERI_CLK_CFG1.bit.ENA_SPI0_CLK = 1;
    }
    else if (spi_num == 1)
    {
        CMN_SYS_P->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_LD = 1;
        CMN_SYS_P->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_N = 1;
        CMN_SYS_P->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_M = 1;
        CMN_SYS_P->REG_PERI_CLK_CFG2.bit.ENA_SPI1_CLK = 1;
    }
}

int rf_init_cfg(void *spi_dev)
{
    uint8_t spidata[19*3];

    int i = 0;
	// reg from 0 ~ 15
    spidata[i] = 0x0; i++; spidata[i] = 0x74; i++; spidata[i] = 0x00; i++;//0 test #1
    spidata[i] = 0x1; i++; spidata[i] = 0x19; i++; spidata[i] = 0xA1; i++;//1 test #2
    spidata[i] = 0x1; i++; spidata[i] = 0x00; i++; spidata[i] = 0x32; i++;//2 standby

   // for tx inter 
    spidata[i] = 0x1; i++; spidata[i] = 0xA7; i++; spidata[i] = 0x83; i++;//3 main div int
    spidata[i] = 0x0; i++; spidata[i] = 0x66; i++; spidata[i] = 0x64; i++; //4 main div frac
    
    spidata[i] = 0x0; i++; spidata[i] = 0x0E; i++; spidata[i] = 0x45; i++;//5 miscellaneous
    spidata[i] = 0x0; i++; spidata[i] = 0x06; i++; spidata[i] = 0x06; i++;// 6 // TXRX MODE CONTROL
    spidata[i] = 0x1; i++; spidata[i] = 0x02; i++; spidata[i] = 0x37; i++;// 7 RX/TX LPF freq  
    spidata[i] = 0x3; i++; spidata[i] = 0x02; i++; spidata[i] = 0x18; i++;// 8// rx gain rssi  BW
    spidata[i] = 0x0; i++; spidata[i] = 0x4B; i++; spidata[i] = 0x69; i++; // 9 tx gain
    spidata[i] = 0x1; i++; spidata[i] = 0xd2; i++; spidata[i] = 0x1A; i++; // 10 PA DAC
    spidata[i] = 0x0; i++; spidata[i] = 0x04; i++; spidata[i] = 0x6B; i++; // 11  RX LNA GAIN  //0x046b
    spidata[i] = 0x0; i++; spidata[i] = 0x15; i++; spidata[i] = 0xaC; i++; // 12  TX VGA GAIN
    spidata[i] = 0x0; i++; spidata[i] = 0xE9; i++; spidata[i] = 0x2D; i++;// 13  bias
    spidata[i] = 0x0; i++; spidata[i] = 0x51; i++; spidata[i] = 0xaE; i++; // 14  xtal1
    spidata[i] = 0x0; i++; spidata[i] = 0x14; i++; spidata[i] = 0x5F; i++; // 15  xtal2
	// rf frequence
    spidata[i] = 0x1; i++; spidata[i] = 0xA7; i++; spidata[i] = 0xC3; i++;
    spidata[i] = 0x0; i++; spidata[i] = 0x07; i++; spidata[i] = 0xC3; i++;
    spidata[i] = 0x0; i++; spidata[i] = 0x00; i++; spidata[i] = 0x04; i++;

    for(i = 0; i < 19; i++)
    {
        SPI_Send(spi_dev, &spidata[i*3], 3);
        spi_tx_done(spi_dev);
    }

	return 0;
}


void rf_spi_init()
{
    spi_initial_clock(1);

    config_spi1_iomux();

    void* spi_dev = SPI1();
    uint32_t control = 0;

    // master
    control = CSK_SPI_MODE_MASTER;

    control |= CSK_SPI_TXIO_PIO;

    // frame format
    control |= CSK_SPI_CPOL0_CPHA0;// | CSK_SPI_RXIO_PIO;
    control |= CSK_SPI_DATA_BITS( 8 );

    // bit order
    control |= CSK_SPI_MSB_LSB;

    // call SPI_Initialize
    SPI_Initialize(spi_dev, NULL, 0);

    SPI_PowerControl (spi_dev, CSK_POWER_FULL);

    uint32_t bus_speed = IC_BOARD_FPGA_FIX_FREQ;

    // call SPI_Control, and arg = bus_speed when as master
    SPI_Control(spi_dev, control, bus_speed);


    bus_speed = SPI_Control(spi_dev, CSK_SPI_GET_BUS_SPEED, 0);

    // divided by 2 if bus speed is greater than 100MHz
    if (bus_speed > 100000000) 
    { 
        // 100MHz
        bus_speed /= 2;
        SPI_Control(spi_dev, CSK_SPI_SET_BUS_SPEED, bus_speed);
    }
}

void rf_gpio_init()
{


	void* GPIOB_Handler = GPIOB();
    GPIO_Initialize(GPIOB_Handler, NULL, NULL);
#if 1
    // GPIOA24-26: GPIOB00-02
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, RF_RXHP_PIN, CSK_AON_IOMUX_FUNC_NORMAL); //CSK_AON_IOMUX_FUNC_NORMAL
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, RF_RXTX_PIN, CSK_AON_IOMUX_FUNC_NORMAL); //CSK_AON_IOMUX_FUNC_NORMAL
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, RF_SHDNB_PIN,CSK_AON_IOMUX_FUNC_NORMAL); //CSK_AON_IOMUX_FUNC_NORMAL
    
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, RF_RXHP_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, RF_RXTX_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, RF_SHDNB_PIN, CSK_IOMUX_FUNC_DEFAULT);


    //GPIO_SetDir(gpio_B, CSK_GPIO_PIN2_DBG, CSK_GPIO_DIR_OUTPUT);
    //GPIO_PinWrite(gpio_B, CSK_GPIO_PIN2_DBG, 0); // LOW initially
    

    GPIO_SetDir(GPIOB_Handler, 1<<RF_RXHP_PIN, CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(GPIOB_Handler, 1<<RF_RXTX_PIN, CSK_GPIO_DIR_OUTPUT);
    GPIO_SetDir(GPIOB_Handler, 1<<RF_SHDNB_PIN, CSK_GPIO_DIR_OUTPUT);
#if (ZERO_MIDDLE_FREQ == 1)
    GPIO_PinWrite(GPIOB_Handler, 1<<RF_RXHP_PIN, 0); // RXHP_PIN    PIN24 IS GPIOB_0  0.750K high pass  use 1.5M middle freq   0:dc1OOHZ  1:600khz highpass   
#else
    GPIO_PinWrite(GPIOB_Handler, 1<<RF_RXHP_PIN, 1); // RXHP_PIN    PIN24 IS GPIOB_0  0.750K high pass  use 1.5M middle freq   0:dc1OOHZ  1:600khz highpass  
#endif
    GPIO_PinWrite(GPIOB_Handler, 1<<RF_SHDNB_PIN, 1); // SHDNB_PIN     PIN26 IS GPIOB_2

    //锟斤拷锟斤拷锟竭硷拷锟斤拷锟斤拷TXRX锟叫伙拷锟斤拷锟斤拷GPIB_01 xor TXRX状态锟斤拷锟狡ｏ拷默锟斤拷GPIO为0锟斤拷锟斤拷TXRX为0锟斤拷锟斤拷锟轿�0锟斤拷TXRX为1锟斤拷锟斤拷锟轿�1  ; // 锟斤拷锟角匡拷锟紾PIO为1锟斤拷锟斤拷TXRX锟斤拷锟斤拷锟斤拷锟斤拷锟洁反
    GPIO_PinWrite(GPIOB_Handler, 1<<RF_RXTX_PIN, (0)); // GPIOB01 xor trx_en:  set GPIOB01 0,then if trx_en==1:  output tx;    if trx_en==0:   output  rx
#endif

    ((volatile AON_IOMUX_RegDef *)IP_AON_IOMUX)->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_OEN_REG = 0;
    ((volatile AON_IOMUX_RegDef *)IP_AON_IOMUX)->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_OEN_FRC = 1;
    ((volatile AON_IOMUX_RegDef *)IP_AON_IOMUX)->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_OUT_REG = 1;
    ((volatile AON_IOMUX_RegDef *)IP_AON_IOMUX)->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_OUT_FRC = 1;


    //GPIO_Uninitialize(GPIOA_Handler);

    rf_spi_init();

    void* spi_dev = SPI1();

    rf_init_cfg(spi_dev);
    rf_set_freq(1, 0);

}

void trx_en_2830_isr(uint8_t stats)
{
    uint32_t  curr_freq=0;
    uint8_t channel_idx = 0;

    //DBG_SWDIAG_PORT0_vendor(vendor0, index0,1);

    channel_idx = BT_CNTL_P->REG_BT_CTRL_CHANNEL_RATE.bit.CHANNEL;
   
    curr_freq = channel_idx; //chnl2freq(channel_idx); // g_curr_channel

    if(stats & RXEN_ON_STAT_BIT)
    {
        BT_CNTL_P->REG_BT_CTRL_INT_CLR.bit.RXEN_ON_CLR = 1;

        rf_set_freq(ZERO_MIDDLE_FREQ, curr_freq); //+curr_freq  -1.5MHZ
    }
   
    else if (stats & TXEN_ON_STAT_BIT)
    {
        BT_CNTL_P->REG_BT_CTRL_INT_CLR.bit.TXEN_ON_CLR = 1;
        
        rf_set_freq(1, curr_freq);//+curr_freq
    }
}




#endif

