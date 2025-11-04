#define HS_SEL 28
#define M2P_PRE_FETCH 23
#define SRC_BURST_LEN 14
#define DST_BURST_LEN 16
#define AUTO_TFR 13
#define DST_INC 10
#define SRC_INC 9
#define TFR_MODE 4
#define CH_START 1
#define CH_ENABLE 0


extern void apc_i2s_gpio_cfg(int i2s_sel,int iocfg);
extern void APC_Tx_Ch_R_Enable(int ch_num,int enable);
extern void APC_Tx_Ch_L_Enable(int ch_num,int enable);
extern void APC_Rx_Ch_R_Enable(int ch_num,int enable);
extern void APC_Rx_Ch_L_Enable(int ch_num,int enable);
extern void APC_Enable(int enable);
extern void APC_I2S0_Enable(int enable);
extern void APC_I2S1_Enable(int enable);
extern uint32_t APC_Rx_Ch_R_Data( void );
extern void APC_Tx_Ch_L_DMA_Req_Mask( void );
extern void APC_Tx_Ch_R_FIFO_Overflow_Mask( void );
extern void APC_Tx_Ch_R_FIFO_Underflow_Mask( void );
extern void APC_Tx_Ch_R_FIFO_Full_Mask( void );
extern void APC_Tx_Ch_R_FIFO_Empty_Mask( void );
extern void APC_Tx_Ch_L_FIFO_Overflow_Mask( void );
extern void APC_Tx_Ch_L_FIFO_Underflow_Mask( void );
extern void APC_Tx_Ch_L_FIFO_Full_Mask( void );
extern void APC_Tx_Ch_L_FIFO_Empty_Mask( void );
extern void APC_Rx_Ch_L_DMA_Req_Mask( void );
extern void APC_Rx_Ch_R_FIFO_Overflow_Mask( void );
extern void APC_Rx_Ch_R_FIFO_Underflow_Mask( void );
extern void APC_Rx_Ch_R_FIFO_Full_Mask( void );
extern void APC_Rx_Ch_R_FIFO_Empty_Mask( void );
extern void APC_Rx_Ch_L_FIFO_Overflow_Mask( void );
extern void APC_Rx_Ch_L_FIFO_Underflow_Mask( void );
extern void APC_Rx_Ch_L_FIFO_Full_Mask( void );
extern void APC_Rx_Ch_L_FIFO_Empty_Mask( void );

extern void APC_Tx_Ch_L_DMA_Req_Clear( void );
extern void APC_Tx_Ch_R_FIFO_Overflow_Clear( void );
extern void APC_Tx_Ch_R_FIFO_Underflow_Clear( void );
extern void APC_Tx_Ch_R_FIFO_Full_Clear( void );
extern void APC_Tx_Ch_R_FIFO_Empty_Clear( void );
extern void APC_Tx_Ch_L_FIFO_Overflow_Clear( void );
extern void APC_Tx_Ch_L_FIFO_Underflow_Clear( void );
extern void APC_Tx_Ch_L_FIFO_Full_Clear( void );
extern void APC_Tx_Ch_L_FIFO_Empty_Clear( void );
extern void APC_Rx_Ch_L_DMA_Req_Clear( void );
extern void APC_Rx_Ch_R_FIFO_Overflow_Clear( void );
extern void APC_Rx_Ch_R_FIFO_Underflow_Clear( void );
extern void APC_Rx_Ch_R_FIFO_Full_Clear( void );
extern void APC_Rx_Ch_R_FIFO_Empty_Clear( void );
extern void APC_Rx_Ch_L_FIFO_Overflow_Clear( void );
extern void APC_Rx_Ch_L_FIFO_Underflow_Clear( void );
extern void APC_Rx_Ch_L_FIFO_Full_Clear( void );
extern void APC_Rx_Ch_L_FIFO_Empty_Clear( void );

extern int APC_Tx_Ch_L_DMA_Req_Status( void );
extern int APC_Tx_Ch_R_FIFO_Overflow_Status( void );
extern int APC_Tx_Ch_R_FIFO_Underflow_Status( void );
extern int APC_Tx_Ch_R_FIFO_Full_Status( void );
extern int APC_Tx_Ch_R_FIFO_Empty_Status( void );
extern int APC_Tx_Ch_L_FIFO_Overflow_Status( void );
extern int APC_Tx_Ch_L_FIFO_Underflow_Status( void );
extern int APC_Tx_Ch_L_FIFO_Full_Status( void );
extern int APC_Tx_Ch_L_FIFO_Empty_Status( void );
extern int APC_Rx_Ch_L_DMA_Req_Status( void );
extern int APC_Rx_Ch_R_FIFO_Overflow_Status( void );
extern int APC_Rx_Ch_R_FIFO_Underflow_Status( void );
extern int APC_Rx_Ch_R_FIFO_Full_Status( void );
extern int APC_Rx_Ch_R_FIFO_Empty_Status( void );
extern int APC_Rx_Ch_L_FIFO_Overflow_Status( void );
extern int APC_Rx_Ch_L_FIFO_Underflow_Status( void );
extern int APC_Rx_Ch_L_FIFO_Full_Status( void );
extern int APC_Rx_Ch_L_FIFO_Empty_Status( void );
extern void APC_Tx_Ch_L_FIFO_Overflow_Unmask( void ) ;
extern void APC_Tx_Ch_R_FIFO_Overflow_Unmask( void );
extern void APC_Tx_Ch_L_Data( uint32_t value );
extern void APC_Tx_Ch_R_Data( uint32_t value );
extern void APC_Rx_Ch_FIFO_Flush( void );
extern void APC_Tx_Ch_FIFO_Flush( void );
extern void APC_EQ_Clear( void );
extern int APC_EQ_Clear_Done( void );
extern void APC_EQ_Clear_Disable( void );
extern void APC_EQ_Not_Bypass( void );
extern void APC_EQ_Ch_LR_Enable( void );
extern void APC_EQ_Stage( uint8_t value );
extern void APC_EQCoef_0( uint32_t value );
extern void APC_EQCoef_1( uint32_t value );
extern void APC_EQCoef_2( uint32_t value );
extern void APC_EQCoef_3( uint32_t value );
extern void APC_EQCoef_4( uint32_t value );
extern void APC_EQCoef_5( uint32_t value );
extern void APC_EQCoef_6( uint32_t value );
extern void APC_EQCoef_7( uint32_t value );
extern void APC_EQCoef_8( uint32_t value );
extern void APC_EQCoef_9( uint32_t value );
extern void APC_EQCoef_10( uint32_t value );
extern void APC_EQCoef_11( uint32_t value );
extern void APC_EQCoef_12( uint32_t value );
extern void APC_EQCoef_13( uint32_t value );
extern void APC_EQCoef_14( uint32_t value );
extern void APC_EQCoef_15( uint32_t value );
extern void APC_EQCoef_16( uint32_t value );
extern void APC_EQCoef_17( uint32_t value );
extern void APC_EQCoef_18( uint32_t value );
extern void APC_EQCoef_19( uint32_t value );
extern void APC_EQCoef_20( uint32_t value );
extern void APC_EQCoef_21( uint32_t value );
extern void APC_EQCoef_22( uint32_t value );
extern void APC_EQCoef_23( uint32_t value );
extern void APC_EQCoef_24( uint32_t value );
extern void APC_EQCoef_25( uint32_t value );
extern void APC_EQCoef_26( uint32_t value );
extern void APC_EQCoef_27( uint32_t value );
extern void APC_EQCoef_28( uint32_t value );
extern void APC_EQCoef_29( uint32_t value );
extern void APC_EQCoef_30( uint32_t value );
extern void APC_EQCoef_31( uint32_t value );
extern void APC_EQCoef_32( uint32_t value );
extern void APC_EQCoef_33( uint32_t value );
extern void APC_EQCoef_34( uint32_t value );
extern void APC_EQCoef_35( uint32_t value );
extern void APC_EQCoef_36( uint32_t value );
extern void APC_EQCoef_37( uint32_t value );
extern void APC_EQCoef_38( uint32_t value );
extern void APC_EQCoef_39( uint32_t value );
extern void APC_EQCoef_40( uint32_t value );
extern void APC_EQCoef_41( uint32_t value );
extern void APC_EQCoef_42( uint32_t value );
extern void APC_EQCoef_43( uint32_t value );
extern void APC_EQCoef_44( uint32_t value );
extern void APC_EQCoef_45( uint32_t value );
extern void APC_EQCoef_46( uint32_t value );
extern void APC_EQCoef_47( uint32_t value );
extern void APC_EQCoef_48( uint32_t value );
extern void APC_EQCoef_49( uint32_t value );
extern void APC_SRC_Clear( void );
extern void APC_SRC_Mode_8K( void );
extern void APC_SRC_Mode_16K( void );

extern void ADC_ENABLE_SETTING( void );
extern void Codec_IO_Cfg(int iocfg);
extern void DMIC_ENABLE_SETTING( void );
extern void DAC_ENABLE_SETTING( void );
extern void ADC_IO_CFG(void);
