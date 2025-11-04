#include "arcs_ap.h"

#include "log_print.h"

void apc_i2s_gpio_cfg(int i2s_sel,int iocfg){
  if(i2s_sel == 0){
    switch(iocfg)
    {
      case 1:{
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_00.bit.PAD_GPIOA_00_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_01.bit.PAD_GPIOA_01_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_02.bit.PAD_GPIOA_02_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_03.bit.PAD_GPIOA_03_FSEL = 10;
      }break;                                       
      case 2:{                                      
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_08.bit.PAD_GPIOA_08_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_09.bit.PAD_GPIOA_09_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_10.bit.PAD_GPIOA_10_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_11.bit.PAD_GPIOA_11_FSEL = 10;
      }break;                                       
      case 3:{                                      
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_17.bit.PAD_GPIOA_17_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_18.bit.PAD_GPIOA_18_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_19.bit.PAD_GPIOA_19_FSEL = 10;
      }break;                                       
      case 4:{                                      
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_00.bit.PAD_GPIOB_00_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_01.bit.PAD_GPIOB_01_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_02.bit.PAD_GPIOB_02_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_03.bit.PAD_GPIOB_03_FSEL = 10;
    	  IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.bit.PAD_AON_GPIOB_00_FSEL = 5;
    	  IP_AON_IOMUX->REG_PAD_AON_GPIOB_01.bit.PAD_AON_GPIOB_01_FSEL = 5;
    	  IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_FSEL = 5;
    	  IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_FSEL = 5;
      }break;                                       
	  default:{
	    CLOGD("No specify iocfg\n");
	    //c_fail();
	  }
    }
  }else{
    switch(iocfg)
    {
      case 1:{
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_04.bit.PAD_GPIOA_04_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_05.bit.PAD_GPIOA_05_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_FSEL = 10;
      }break;                                       
      case 2:{                                      
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_12.bit.PAD_GPIOA_12_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_14.bit.PAD_GPIOA_14_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_15.bit.PAD_GPIOA_15_FSEL = 10;
      }break;                                       
      case 3:{                                      
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_20.bit.PAD_GPIOA_20_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_21.bit.PAD_GPIOA_21_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_22.bit.PAD_GPIOA_22_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 10;
      }break;                                       
      case 4:{                                      
    	  IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_FSEL = 10;
          IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_FSEL = 10;
          IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_FSEL = 10;
          IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 10;
      }break;                                       
      case 5:{                                      
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_04.bit.PAD_GPIOB_04_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_05.bit.PAD_GPIOB_05_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_06.bit.PAD_GPIOB_06_FSEL = 10;
    	  IP_CMN_IOMUX->REG_PAD_GPIOB_07.bit.PAD_GPIOB_07_FSEL = 10;
    	  IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_FSEL = 5;
    	  IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_FSEL = 5;
    	  //IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_FSEL = 5;
    	  //IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_FSEL = 5;
      }break;                                       
	  default:{
	    CLOGD("No specify iocfg\n");
	    //c_fail();
	  }
    }

  }
}
void APC_Tx_Ch_R_Enable(int ch_num,int enable){
  if(ch_num){
    if(enable)
    	IP_AUDIO_APC->REG_APC_TX_CH1_CFG.bit.TX_CH1_R_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_TX_CH1_CFG.bit.TX_CH1_R_EN = 0x0;
  }else{
    if(enable)
    	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_EN = 0x0;
  }
}
void APC_Tx_Ch_L_Enable(int ch_num,int enable){
  if(ch_num){
    if(enable)
    	IP_AUDIO_APC->REG_APC_TX_CH1_CFG.bit.TX_CH1_L_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_TX_CH1_CFG.bit.TX_CH1_L_EN = 0x0;
  }else{
    if(enable)
    	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_EN = 0x0;
  }
}
void APC_Rx_Ch_R_Enable(int ch_num,int enable){
  if(ch_num){
    if(enable)
    	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.RX_CH1_R_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.RX_CH1_R_EN = 0x0;
  }else{
    if(enable)
    	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_EN = 0x0;
  }
}
void APC_Rx_Ch_L_Enable(int ch_num,int enable){
  if(ch_num){
    if(enable)
    	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.RX_CH1_L_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.RX_CH1_L_EN = 0x0;
  }else{
    if(enable)
    	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_EN = 0x1;
    else
    	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_EN = 0x0;
  }
}
void APC_Enable(int enable){
  if(enable)
	  IP_AUDIO_APC->REG_APC_CFG.bit.APC_ENABLE = 0x1;
  else
	  IP_AUDIO_APC->REG_APC_CFG.bit.APC_ENABLE = 0x0;
}
void APC_I2S0_Enable(int enable){
  if(enable)
	  IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_ENABLE = 0x1;
  else
	  IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_ENABLE = 0x0;
}
void APC_I2S1_Enable(int enable){
  if(enable)
	  IP_AUDIO_APC->REG_APC_I2S1_CFG0.bit.I2S1_ENABLE = 0x1;
  else
	  IP_AUDIO_APC->REG_APC_I2S1_CFG0.bit.I2S1_ENABLE = 0x0;
}
uint32_t APC_Rx_Ch_R_Data( void ){
    return IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all;
}

void APC_Tx_Ch_L_DMA_Req_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_L_DMA_REQ_MSK = 0x1;
}

void APC_Tx_Ch_R_FIFO_Overflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_R_FIFO_OVFLOW_MSK = 0x1;
}

void APC_Tx_Ch_R_FIFO_Underflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_R_FIFO_UNFLOW_MSK = 0x1;
}

void APC_Tx_Ch_R_FIFO_Full_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_R_FIFO_FUL_MSK = 0x1;
}

void APC_Tx_Ch_R_FIFO_Empty_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_R_FIFO_EMP_MSK = 0x1;
}

void APC_Tx_Ch_L_FIFO_Overflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_L_FIFO_OVFLOW_MSK = 0x1;
}

void APC_Tx_Ch_L_FIFO_Underflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_L_FIFO_UNFLOW_MSK = 0x1;
}

void APC_Tx_Ch_L_FIFO_Full_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_L_FIFO_FUL_MSK = 0x1;
}

void APC_Tx_Ch_L_FIFO_Empty_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_L_FIFO_EMP_MSK = 0x1;
}

void APC_Rx_Ch_L_DMA_Req_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_L_DMA_REQ_MSK = 0x1;
}

void APC_Rx_Ch_R_FIFO_Overflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_R_FIFO_OVFLOW_MSK = 0x1;
}

void APC_Rx_Ch_R_FIFO_Underflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_R_FIFO_UNFLOW_MSK = 0x1;
}

void APC_Rx_Ch_R_FIFO_Full_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_R_FIFO_FUL_MSK = 0x1;
}

void APC_Rx_Ch_R_FIFO_Empty_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_R_FIFO_EMP_MSK = 0x1;
}

void APC_Rx_Ch_L_FIFO_Overflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_L_FIFO_OVFLOW_MSK = 0x1;
}

void APC_Rx_Ch_L_FIFO_Underflow_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_L_FIFO_UNFLOW_MSK = 0x1;
}

void APC_Rx_Ch_L_FIFO_Full_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_L_FIFO_FUL_MSK = 0x1;
}

void APC_Rx_Ch_L_FIFO_Empty_Mask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_MSK.bit.RX_CH0_L_FIFO_EMP_MSK = 0x1;
}

void APC_Tx_Ch_L_DMA_Req_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_DMA_REQ_CLR = 0x1;
}

void APC_Tx_Ch_R_FIFO_Overflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_R_FIFO_OVFLOW_CLR = 0x1;
}

void APC_Tx_Ch_R_FIFO_Underflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_R_FIFO_UNFLOW_CLR = 0x1;
}

void APC_Tx_Ch_R_FIFO_Full_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_R_FIFO_FUL_CLR = 0x1;
}

void APC_Tx_Ch_R_FIFO_Empty_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_R_FIFO_EMP_CLR = 0x1;
}

void APC_Tx_Ch_L_FIFO_Overflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_FIFO_OVFLOW_CLR = 0x1;
}

void APC_Tx_Ch_L_FIFO_Underflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_FIFO_UNFLOW_CLR = 0x1;
}

void APC_Tx_Ch_L_FIFO_Full_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_FIFO_FUL_CLR = 0x1;
}

void APC_Tx_Ch_L_FIFO_Empty_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_FIFO_EMP_CLR = 0x1;
}

void APC_Rx_Ch_L_DMA_Req_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_L_DMA_REQ_CLR = 0x1;
}

void APC_Rx_Ch_R_FIFO_Overflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_R_FIFO_OVFLOW_CLR = 0x1;
}

void APC_Rx_Ch_R_FIFO_Underflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_R_FIFO_UNFLOW_CLR = 0x1;
}

void APC_Rx_Ch_R_FIFO_Full_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_R_FIFO_FUL_CLR = 0x1;
}

void APC_Rx_Ch_R_FIFO_Empty_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_R_FIFO_EMP_CLR = 0x1;
}

void APC_Rx_Ch_L_FIFO_Overflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_L_FIFO_OVFLOW_CLR = 0x1;
}

void APC_Rx_Ch_L_FIFO_Underflow_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_L_FIFO_UNFLOW_CLR = 0x1;
}

void APC_Rx_Ch_L_FIFO_Full_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_L_FIFO_FUL_CLR = 0x1;
}

void APC_Rx_Ch_L_FIFO_Empty_Clear( void ) {
	IP_AUDIO_APC->REG_APC_INTR_RX_CLR.bit.RX_CH0_L_FIFO_EMP_CLR = 0x1;
}

int APC_Tx_Ch_L_DMA_Req_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_L_DMA_REQ_ISR ;
}

int APC_Tx_Ch_R_FIFO_Overflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_R_FIFO_OVFLOW_ISR ;
}

int APC_Tx_Ch_R_FIFO_Underflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_R_FIFO_UNFLOW_ISR ;
}

int APC_Tx_Ch_R_FIFO_Full_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_R_FIFO_FUL_ISR ;
}

int APC_Tx_Ch_R_FIFO_Empty_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_R_FIFO_EMP_ISR ;
}

int APC_Tx_Ch_L_FIFO_Overflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_L_FIFO_OVFLOW_ISR ;
}

int APC_Tx_Ch_L_FIFO_Underflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_L_FIFO_UNFLOW_ISR ;
}

int APC_Tx_Ch_L_FIFO_Full_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_L_FIFO_FUL_ISR ;
}

int APC_Tx_Ch_L_FIFO_Empty_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_TX_ISR.bit.TX_CH0_L_FIFO_EMP_ISR ;
}

int APC_Rx_Ch_L_DMA_Req_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_L_DMA_REQ_ISR ;
}

int APC_Rx_Ch_R_FIFO_Overflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_R_FIFO_OVFLOW_ISR ;
}

int APC_Rx_Ch_R_FIFO_Underflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_R_FIFO_UNFLOW_ISR ;
}

int APC_Rx_Ch_R_FIFO_Full_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_R_FIFO_FUL_ISR ;
}

int APC_Rx_Ch_R_FIFO_Empty_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_R_FIFO_EMP_ISR ;
}

int APC_Rx_Ch_L_FIFO_Overflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_L_FIFO_OVFLOW_ISR ;
}

int APC_Rx_Ch_L_FIFO_Underflow_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_L_FIFO_UNFLOW_ISR ;
}

int APC_Rx_Ch_L_FIFO_Full_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_L_FIFO_FUL_ISR ;
}

int APC_Rx_Ch_L_FIFO_Empty_Status( void ) {
    return IP_AUDIO_APC->REG_APC_INTR_RX_ISR.bit.RX_CH0_L_FIFO_EMP_ISR ;
}
void APC_Tx_Ch_L_FIFO_Overflow_Unmask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_L_FIFO_OVFLOW_MSK = 0x0;
}
void APC_Tx_Ch_R_FIFO_Overflow_Unmask( void ) {
	IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_R_FIFO_OVFLOW_MSK = 0x0;
}
void APC_Tx_Ch_L_Data( uint32_t value ){
	IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all = value;
}

void APC_Tx_Ch_R_Data( uint32_t value ){
	IP_AUDIO_APC->REG_APC_TX_CH0_R_DATA.all = value;
}
void APC_Rx_Ch_FIFO_Flush( void ){
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_FIFO_FLUSH = 0x1;
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_FIFO_FLUSH = 0x1;
}

void APC_Tx_Ch_FIFO_Flush( void ){
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_FIFO_FLUSH = 0x1;
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_FIFO_FLUSH = 0x1;
}

void APC_EQ_Clear( void ){
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.EQ_CLR = 0x1;
}

int APC_EQ_Clear_Done( void ){
    return IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.EQ_CLR_DONE;
}

void APC_EQ_Clear_Disable( void ){
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.EQ_CLR = 0x0;
}

void APC_EQ_Not_Bypass( void ){
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.EQ_BYPASS_REG = 0x0;
}

void APC_EQ_Stage( uint8_t value ){
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.EQ_STAGE = value;
}

void APC_EQ_Ch_LR_Enable( void ){
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.EQ_CH_EN = 0x1;
}

void APC_EQCoef_0( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_0.bit.EQ_COEF_0 = value;
}

void APC_EQCoef_1( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_1.bit.EQ_COEF_1 = value;
}

void APC_EQCoef_2( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_2.bit.EQ_COEF_2 = value;
}

void APC_EQCoef_3( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_3.bit.EQ_COEF_3 = value;
}

void APC_EQCoef_4( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_4.bit.EQ_COEF_4 = value;
}

void APC_EQCoef_5( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_5.bit.EQ_COEF_5 = value;
}

void APC_EQCoef_6( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_6.bit.EQ_COEF_6 = value;
}

void APC_EQCoef_7( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_7.bit.EQ_COEF_7 = value;
}

void APC_EQCoef_8( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_8.bit.EQ_COEF_8 = value;
}

void APC_EQCoef_9( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_9.bit.EQ_COEF_9 = value;
}

void APC_EQCoef_10( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_10.bit.EQ_COEF_10 = value;
}

void APC_EQCoef_11( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_11.bit.EQ_COEF_11 = value;
}

void APC_EQCoef_12( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_12.bit.EQ_COEF_12 = value;
}

void APC_EQCoef_13( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_13.bit.EQ_COEF_13 = value;
}

void APC_EQCoef_14( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_14.bit.EQ_COEF_14 = value;
}

void APC_EQCoef_15( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_15.bit.EQ_COEF_15 = value;
}

void APC_EQCoef_16( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_16.bit.EQ_COEF_16 = value;
}

void APC_EQCoef_17( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_17.bit.EQ_COEF_17 = value;
}

void APC_EQCoef_18( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_18.bit.EQ_COEF_18 = value;
}

void APC_EQCoef_19( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_19.bit.EQ_COEF_19 = value;
}

void APC_EQCoef_20( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_20.bit.EQ_COEF_20 = value;
}

void APC_EQCoef_21( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_21.bit.EQ_COEF_21 = value;
}

void APC_EQCoef_22( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_22.bit.EQ_COEF_22 = value;
}

void APC_EQCoef_23( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_23.bit.EQ_COEF_23 = value;
}

void APC_EQCoef_24( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_24.bit.EQ_COEF_24 = value;
}

void APC_EQCoef_25( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_25.bit.EQ_COEF_25 = value;
}

void APC_EQCoef_26( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_26.bit.EQ_COEF_26 = value;
}

void APC_EQCoef_27( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_27.bit.EQ_COEF_27 = value;
}

void APC_EQCoef_28( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_28.bit.EQ_COEF_28 = value;
}

void APC_EQCoef_29( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_29.bit.EQ_COEF_29 = value;
}

void APC_EQCoef_30( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_30.bit.EQ_COEF_30 = value;
}

void APC_EQCoef_31( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_31.bit.EQ_COEF_31 = value;
}

void APC_EQCoef_32( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_32.bit.EQ_COEF_32 = value;
}

void APC_EQCoef_33( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_33.bit.EQ_COEF_33 = value;
}

void APC_EQCoef_34( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_34.bit.EQ_COEF_34 = value;
}

void APC_EQCoef_35( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_35.bit.EQ_COEF_35 = value;
}

void APC_EQCoef_36( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_36.bit.EQ_COEF_36 = value;
}

void APC_EQCoef_37( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_37.bit.EQ_COEF_37 = value;
}

void APC_EQCoef_38( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_38.bit.EQ_COEF_38 = value;
}

void APC_EQCoef_39( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_39.bit.EQ_COEF_39 = value;
}

void APC_EQCoef_40( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_40.bit.EQ_COEF_40 = value;
}

void APC_EQCoef_41( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_41.bit.EQ_COEF_41 = value;
}

void APC_EQCoef_42( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_42.bit.EQ_COEF_42 = value;
}

void APC_EQCoef_43( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_43.bit.EQ_COEF_43 = value;
}

void APC_EQCoef_44( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_44.bit.EQ_COEF_44 = value;
}

void APC_EQCoef_45( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_45.bit.EQ_COEF_45 = value;
}

void APC_EQCoef_46( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_46.bit.EQ_COEF_46 = value;
}

void APC_EQCoef_47( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_47.bit.EQ_COEF_47 = value;
}

void APC_EQCoef_48( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_48.bit.EQ_COEF_48 = value;
}

void APC_EQCoef_49( uint32_t value ){
    IP_AUDIO_APC->REG_EQCOEF_49.bit.EQ_COEF_49 = value;
}

void APC_SRC_Enable( void ){
	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.SRC_CH_EN = 0x3;
}

void APC_SRC_Clear( void ){
	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.SRC_CLR = 0x1;
}

void APC_SRC_Mode_8K( void ){
	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.SRC_MODE = 0x0;
}

void APC_SRC_Mode_16K( void ){
	IP_AUDIO_APC->REG_APC_RX_CH1_CFG.bit.SRC_MODE = 0x1;
}

void ADC_ENABLE_SETTING( void ){
//    IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.ADC_RST   = 0;
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.AUD_EN_VMID = 1;
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.AUD_EN_IREF = 1;
//    AUDIO_CODEC->REG_AUD_R11_ADC_CTRL6.all = 0x21;      //enable adc_l, adc_r, set adc_idac_ctrl to 2'h3: x+13%, set adc_rc_ctrl to 5'h3: 10uA, 12MHz FS, disable loop path reset
	IP_AUDIO_CODEC->REG_AUD_R11_ADC_CTRL6.bit.REG_AUD_EN_ADC0 = 1;
	IP_AUDIO_CODEC->REG_AUD_R11_ADC_CTRL6.bit.REG_AUD_EN_ADC1 = 1;
	IP_AUDIO_CODEC->REG_AUD_R11_ADC_CTRL6.bit.ANA_ADC_RST_REG = 0;
	IP_AUDIO_CODEC->REG_AUD_R11_ADC_CTRL6.bit.AUD_ADC_IB_CTRL = 3;		//3uA
//    IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.all = 0x403;    //enable vref, lr pga, lrpga_zcen, pga_buffer, set lr pga to differetial mode, disable lrpga_mute
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_PGA1_MUTE = 0;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_PGA0_MUTE = 0;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.LPGA_ZCEN_REG = 1;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.RPGA_ZCEN_REG = 1;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.REG_AUD_EN_PGA0 = 1;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.REG_AUD_EN_PGA1 = 1;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.RPGA_ZCEN_REG = 1;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_EN_ADC0_VREF = 1;
	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_EN_ADC1_VREF = 1;
	IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 1;
//	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_EN_PGA0_VCMBUF = 1;
//	IP_AUDIO_CODEC->REG_AUD_R12_ADC_CTRL7.bit.AUD_EN_PGA1_VCMBUF = 1;
}

void Codec_IO_Cfg(int iocfg){
  switch(iocfg)
  {
    case 1:{
    	//IP_CMN_IOMUX->REG_PAD_GPIOA_00.bit.PAD_GPIOA_00_FSEL = 24;
    	//IP_CMN_IOMUX->REG_PAD_GPIOA_01.bit.PAD_GPIOA_01_FSEL = 24;
    	//IP_CMN_IOMUX->REG_PAD_GPIOA_02.bit.PAD_GPIOA_02_FSEL = 24;
		IP_CMN_IOMUX->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_FSEL = 9; //lrck
		IP_CMN_IOMUX->REG_PAD_GPIOA_14.bit.PAD_GPIOA_14_FSEL = 9; //din
		IP_CMN_IOMUX->REG_PAD_GPIOA_15.bit.PAD_GPIOA_15_FSEL = 9; //dout
		IP_CMN_IOMUX->REG_PAD_GPIOA_24.bit.PAD_GPIOA_24_FSEL = 9; //bck
	}break;                                       
    case 2:{
    	IP_CMN_IOMUX->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_FSEL = 24;//clk
    	IP_CMN_IOMUX->REG_PAD_GPIOA_04.bit.PAD_GPIOA_04_FSEL = 24;//d0
    	IP_CMN_IOMUX->REG_PAD_GPIOA_05.bit.PAD_GPIOA_05_FSEL = 24;//d1
	}break;                                       
    case 3:{                                      
    	IP_CMN_IOMUX->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_FSEL = 25;
	  IP_CMN_IOMUX->REG_PAD_GPIOA_07.bit.PAD_GPIOA_07_FSEL = 25;
	  IP_CMN_IOMUX->REG_PAD_GPIOA_08.bit.PAD_GPIOA_08_FSEL = 25;
	}break;                                       
    case 4:{                                      
    	IP_CMN_IOMUX->REG_PAD_GPIOA_09.bit.PAD_GPIOA_09_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_10.bit.PAD_GPIOA_10_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_11.bit.PAD_GPIOA_11_FSEL = 25;
	}break;                                       
    case 5:{                                      
    	IP_CMN_IOMUX->REG_PAD_GPIOA_12.bit.PAD_GPIOA_12_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_13.bit.PAD_GPIOA_13_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_14.bit.PAD_GPIOA_14_FSEL = 25;
	}break;                                       
    case 6:{                                      
    	IP_CMN_IOMUX->REG_PAD_GPIOA_15.bit.PAD_GPIOA_15_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_17.bit.PAD_GPIOA_17_FSEL = 25;
	}break;                                       
    case 7:{                                      
    	IP_CMN_IOMUX->REG_PAD_GPIOA_18.bit.PAD_GPIOA_18_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_19.bit.PAD_GPIOA_19_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_20.bit.PAD_GPIOA_20_FSEL = 25;
	}break;                                       
    case 8:{                                      
    	IP_CMN_IOMUX->REG_PAD_GPIOA_21.bit.PAD_GPIOA_21_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_22.bit.PAD_GPIOA_22_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOA_23.bit.PAD_GPIOA_23_FSEL = 25;
	}break;                                       
    case 9:{                                      
    	IP_CMN_IOMUX->REG_PAD_GPIOA_24.bit.PAD_GPIOA_24_FSEL = 25;
	    IP_CMN_IOMUX->REG_PAD_GPIOA_25.bit.PAD_GPIOA_25_FSEL = 25;
	    IP_CMN_IOMUX->REG_PAD_GPIOA_26.bit.PAD_GPIOA_26_FSEL = 25;
	}break;                                       
	case 10:{                                      
		IP_CMN_IOMUX->REG_PAD_GPIOA_27.bit.PAD_GPIOA_27_FSEL = 25;
		IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_FSEL = 25;
		IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_FSEL = 25;
	}break;                                       
	case 11:{                                      
		IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_FSEL = 25;
		IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 25;
		IP_CMN_IOMUX->REG_PAD_GPIOB_00.bit.PAD_GPIOB_00_FSEL = 25;
	}break;
	case 12:{                                     
		IP_CMN_IOMUX->REG_PAD_GPIOB_01.bit.PAD_GPIOB_01_FSEL = 25;
		IP_CMN_IOMUX->REG_PAD_GPIOB_02.bit.PAD_GPIOB_02_FSEL = 25;
		IP_CMN_IOMUX->REG_PAD_GPIOB_03.bit.PAD_GPIOB_03_FSEL = 25;
		IP_AON_IOMUX->REG_PAD_AON_GPIOB_01.bit.PAD_AON_GPIOB_01_FSEL = 6;
		IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_FSEL = 6;
		IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_FSEL = 6;
	}break;                                       
    case 13:{                                     
    	IP_CMN_IOMUX->REG_PAD_GPIOB_04.bit.PAD_GPIOB_04_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOB_05.bit.PAD_GPIOB_05_FSEL = 25;
    	IP_CMN_IOMUX->REG_PAD_GPIOB_06.bit.PAD_GPIOB_06_FSEL = 25;
	    IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_FSEL = 6;
	    IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_FSEL = 6;
	  //IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_FSEL = 6;
    }break;
	default:{
	  CLOGD("No specify iocfg\n");
	  //c_fail();
	}
  }
}


void ADC_IO_CFG(void) {
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_FSEL = 21;
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_FSEL = 21;
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_FSEL = 21;
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 21;
}

void DMIC_ENABLE_SETTING( void ){
	IP_AUDIO_CODEC->REG_AUD_R9_ADC_CTRL4.bit.DMIC_ENABLE = 1;
	IP_AUDIO_CODEC->REG_AUD_R11_ADC_CTRL6.all =  0;
}

void DAC_ENABLE_SETTING( void ){
	//AON_ANALOGCTRL->REG_0X038.bit.LDO25VOSEL = 0x1;
	//AON_ANALOGCTRL->REG_0X038.bit.LDO25_EN = 0x1;
	//IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.all  = 0x564;       //enable vmid, ire7
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.AUD_EN_IREF = 1;
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.AUD_EN_VMID = 1;
	//IP_AUDIO_CODEC->REG_AUD_R17_DAC_CTRL2.all = 0x60;       //disable dac mute, softmute
	IP_AUDIO_CODEC->REG_AUD_R17_DAC_CTRL2.bit.SOFTMUTE_EN = 0;
	IP_AUDIO_CODEC->REG_AUD_R17_DAC_CTRL2.bit.DACMU = 0;
	//    IP_AUDIO_CODEC->REG_AUD_R18_DAC_CTRL3.all = 0x1B50;     //release dac sdm reset
	IP_AUDIO_CODEC->REG_AUD_R18_DAC_CTRL3.bit.DAC_SD_RSTN = 1;
	//IP_AUDIO_CODEC->REG_AUD_R21_DAC_CTRL6.all = 0x1F;       //release dac dct reset, enable lr dac
	IP_AUDIO_CODEC->REG_AUD_R21_DAC_CTRL6.bit.REG_AUD_EN_DAC = 1;
	//IP_AUDIO_CODEC->REG_AUD_R22_DAC_CTRL7.all = 0x990F;     //disable line out mute, enable line out
	IP_AUDIO_CODEC->REG_AUD_R22_DAC_CTRL7.bit.LOLP_MUTE_REG = 0;
	IP_AUDIO_CODEC->REG_AUD_R22_DAC_CTRL7.bit.LOLN_MUTE_REG = 0;
	IP_AUDIO_CODEC->REG_AUD_R22_DAC_CTRL7.bit.REG_AUD_EN_DAC_LOP = 1;
	IP_AUDIO_CODEC->REG_AUD_R22_DAC_CTRL7.bit.REG_AUD_EN_DAC_LON = 1;
}
