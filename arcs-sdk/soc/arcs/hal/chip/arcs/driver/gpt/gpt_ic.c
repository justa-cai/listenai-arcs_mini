/**
  ******************************************************************************
  * @file    gpt_ic.c
  * @author  ListenAI Application Team
  * @brief   GPT IC HAL module driver.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Driver_GPT_IC.h"
#include "gpt.h"
#include "dma.h"

/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @defgroup GPT_IC GPT_IC
  * @brief GPT_IC HAL module driver
  * @{
  */


/* Private typedef -----------------------------------------------------------*/
typedef struct _GPT_IC_INFO
{
    uint32_t trigger_source[GPT_NUMBER_OF_CHANNELS]; // trigger source: sftware/external
    uint32_t edge_select[GPT_NUMBER_OF_CHANNELS];    // edge selection: rising/falling /both
}GPT_IC_INFO;

//GPT IC DMA
typedef struct _GPT_IC_DMA
{
    uint8_t channel;       			// DMA Channel
    uint8_t reqsel;        			// DMA request selection
    DMA_SignalEvent_t cb_event;     // DMA Event callback
} GPT_IC_DMA;

typedef struct _GPT_IC_RESOURCES
{
	GPT_RESOURCES *gpt_resources;
	GPT_IC_INFO *ic_info;
	GPT_IC_DMA *dma_rx;
	uint8_t dma_channel[GPT_NUMBER_OF_CHANNELS];
}GPT_IC_RESOURCES;


/* Private define ------------------------------------------------------------*/
#define AP_GPT_RX0_REQINDEX (12)
#define AP_GPT_RX1_REQINDEX (14)
#define AP_GPT_RX2_REQINDEX (2)
#define AP_GPT_RX3_REQINDEX (12)

/* Private function prototypes -----------------------------------------------*/
static void GPT_Ic_Channnel_Dma_Rx_Handle(uint32_t event_info, uint32_t xfer_bytes, void *pGpt);


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern GPT_RESOURCES gpt0_resources; 

GPT_IC_INFO	gpt0_ic_info = {0};	
GPT_IC_DMA	gpt0_ic_dma = {0,0,NULL};

static GPT_IC_RESOURCES gpt0_ic_resources ={
	&gpt0_resources,
	&gpt0_ic_info,
	&gpt0_ic_dma,
	{0}
};



/* Private functions ---------------------------------------------------------*/
/*!
 \fn         void GPT_Ic_Channnel_Dma_Rx_Handle(uint32_t event_info, uint32_t xfer_bytes, void *pGpt)
 \brief		 Input capture Dma mode interrupt handle
 \param[in]  event_info: CSK_GPT_EVENT_INPUTCAPTURE
 \param[in]	 xfer_bytes: dma transfer bytes
 \param[in]	 *pGpt: user parameter, it could be module instance
 \return      \ref execution_status
 */
static void GPT_Ic_Channnel_Dma_Rx_Handle(uint32_t event_info, uint32_t xfer_bytes, void *pGpt)
{
	GPT_IC_RESOURCES * pGptIc = (GPT_IC_RESOURCES *)pGpt;
	uint32_t channelinfo, eventinfo;
	channelinfo = (event_info & 0xFF00) >> 8;
	eventinfo = event_info & 0xFF;
	switch (eventinfo) {
		case DMA_EVENT_TRANSFER_COMPLETE:
			for(uint8_t ch=0; ch<8; ch++){
				//ledc channel dma funciton is enabled and dma_channel is same as dma interrupt channel
				if(pGptIc->gpt_resources->ic_capture_info->capture_mode[ch] == DMA_MODE) {
					if((pGptIc->dma_channel[ch]) == channelinfo){
						pGptIc->gpt_resources->ic_capture_info->ic_cnt[ch] = pGptIc->gpt_resources->ic_capture_info->ic_num[ch];
						//call interrupt mode user callback funciton
						pGptIc->gpt_resources->info->cb_event[ch](CSK_GPT_EVENT_INPUTCAPTURE, pGptIc->gpt_resources->user_param);
					}
				}
			}
			break;
		case DMA_EVENT_BLOCK_COMPLETE:
			break;
		case DMA_EVENT_ERROR:
			break;
		default:
			break;
	}
}






/** @defgroup GPT_IC_Exported_Functions GPT_IC Exported Functions
  * @{
  */

/** @defgroup GPT_IC_Exported_Functions_Group1 Initialization and de-initialization functions
  *  @brief    Initialization and de-initialization functions
  *
@verbatim
 ===============================================================================
              ##### Initialization and de-initialization functions #####
 ===============================================================================
    [..]
		add description

@endverbatim
  * @{
  */

/**
  * @brief add description.
  * @note add description
  * @retval None
  */
void* GPT0_IC(void)
{
	return &gpt0_ic_resources;
}


/**
 \fn          int32_t HAL_GPT_IcInitialize(void)
 \brief		  Initialize GPT Input capture interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcInitialize(void *pGpt, void *user_param)
{
	GPT_IC_RESOURCES * pGptIc = (GPT_IC_RESOURCES *)pGpt;
	uint32_t ch, status;

	status = GPT_Initialize(pGptIc->gpt_resources, user_param);
	if(status != CSK_DRIVER_OK)
		return status;
	
	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGptIc->ic_info->edge_select[ch] = 0;
		pGptIc->ic_info->trigger_source[ch] = 0;
		pGptIc->dma_rx->channel = 0;	
		pGptIc->dma_rx->reqsel = 0; //DMA_HSID_UART1_RX;//AP_GPT_RX2
		pGptIc->dma_rx->cb_event = NULL;
		pGptIc->gpt_resources->ic_capture_info->ic_buf[ch] = NULL;
		pGptIc->gpt_resources->ic_capture_info->ic_cnt[ch] = 0;
		pGptIc->gpt_resources->ic_capture_info->ic_num[ch] = 0;
		pGptIc->gpt_resources->ic_capture_info->capture_mode[ch] = 0;
	}

	return CSK_DRIVER_OK;
}



/**
 \fn          int32_t HAL_GPT_IcUninitialize(void)
 \brief		  De-initialize GPT Input capture interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcUninitialize(void *pGpt)
{
	GPT_IC_RESOURCES * pGptIc = (GPT_IC_RESOURCES *)pGpt;
	uint32_t ch, status;

	status = GPT_Uninitialize(pGptIc->gpt_resources);
	if(status != CSK_DRIVER_OK)
		return status;

	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGptIc->ic_info->edge_select[ch] = 0;
		pGptIc->ic_info->trigger_source[ch] = 0;
		pGptIc->dma_rx->channel = 0;
		pGptIc->dma_rx->reqsel = 0;
		pGptIc->dma_rx->cb_event = NULL;
		pGptIc->gpt_resources->ic_capture_info->ic_buf[ch] = NULL;
		pGptIc->gpt_resources->ic_capture_info->ic_cnt[ch] = 0;
		pGptIc->gpt_resources->ic_capture_info->ic_num[ch] = 0;		
	}

	return CSK_DRIVER_OK;
}



/**
 \fn          int32_t HAL_GPT_IcPowerControl(CSK_POWER_STATE state)
 \brief		  GPT Input Capture power control
 \param[in]   state: Power state \ref enum CSK_POWER_STATE
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcPowerControl(void *pGpt, CSK_POWER_STATE state)
{
	GPT_IC_RESOURCES * pGptIc = (GPT_IC_RESOURCES *)pGpt;

	return(GPT_PowerControl(pGptIc->gpt_resources, state));
}



/**
 \fn          int32_t HAL_GPT_IcControl(void *pGpt, uint32_t control, uint32_t channel)
 \brief		  Control GPT input capture interface
 \param[in]   control: Operation
 \param[in]   channel: Input capture channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_IcControl(void *pGpt, uint32_t control, uint32_t channel)
{
	GPT_IC_RESOURCES * pGptIc = (GPT_IC_RESOURCES *)pGpt;
	uint32_t tmp;
	
	if((pGptIc->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	pGptIc->gpt_resources->hardware->channel_stat[channel] = HARDWARE_CHANNEL_STAT_USED_BY_IC;

	//clock gate disable
	pGptIc->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_GATE);
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_CNT_GATE);
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_SAMPLE_GATE);

	//clock source select
	pGptIc->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_SEL_Msk);
	tmp = (control & CSK_GPT_IC_CLKSRC_Msk) >> CSK_GPT_IC_CLKSRC_Pos;
	pGptIc->gpt_resources->reg->CHx_CLK_CTRL[channel] |= (tmp << GPT_CHx_CLK_CTRL_CLK_SEL_Pos);		
	
	//clock divider setting
	pGptIc->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_DIV_Msk);
	tmp = (control & CSK_GPT_IC_CLKDIV_Msk) >> CSK_GPT_IC_CLKDIV_Pos;
	pGptIc->gpt_resources->reg->CHx_CLK_CTRL[channel] |= (tmp << GPT_CHx_CLK_CTRL_CLK_DIV_Pos);		
	pGptIc->gpt_resources->reg->CHx_CLK_CTRL[channel] |= GPT_CHx_CLK_CTRL_CLK_DIV_LD;
	pGptIc->gpt_resources->reg->CHx_CLK_CTRL[channel] |= GPT_CHx_CLK_CTRL_CLK_PREDIV_LD;

	//set chx_ch_mode: 32bit timer for input capture mode
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CH_MODE_Msk);
	tmp = 0x01;
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= tmp << GPT_CHx_CTRL_CH_MODE_Pos;

	//set input capture mode
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CH_OPERATION_Msk);
	tmp = (control & CSK_GPT_IC_CONTROL_Msk) >> CSK_GPT_IC_CONTROL_Pos;
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= (tmp << GPT_CHx_CTRL_CH_OPERATION_Pos);
	
	//set trigger source
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_IC_SOURCE_Msk);
	tmp = (control & CSK_GPT_IC_TRIGGER_Msk) >> CSK_GPT_IC_TRIGGER_Pos;
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= (tmp << GPT_CHx_CTRL_IC_SOURCE_Pos);		
	pGptIc->ic_info->trigger_source[channel] = tmp;
	
	//set trigger edge
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_IC_EDGE_Msk);
	tmp = (control & CSK_GPT_IC_EDGE_Msk) >> CSK_GPT_IC_EDGE_Pos;
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= (tmp << GPT_CHx_CTRL_IC_EDGE_Pos);			
	pGptIc->ic_info->edge_select[channel] = tmp;

	//trigger reset counter enable/disable
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CHTRIG_RESET_CNT_EN_Msk);
	tmp = (control & CSK_GPT_IC_TRIGGER_RESET_Msk) >> CSK_GPT_IC_TRIGGER_RESET_Pos;
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= (tmp << GPT_CHx_CHTRIG_RESET_CNT_EN_Pos);

	//set filter threshold
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_IC_FILTER_Msk);
	tmp = (control & CSK_GPT_IC_FILTER_Msk) >> CSK_GPT_IC_FILTER_Pos;
	pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= (tmp << GPT_CHx_CTRL_IC_FILTER_Pos);		
	
	if(channel < 4)
	{
		//set timer run mode, time0, free running mode
		pGptIc->gpt_resources->reg->RUN_MODE_CTRL_0 &= ~(0x03<<(channel<<3));
		tmp = 0x02;
		pGptIc->gpt_resources->reg->RUN_MODE_CTRL_0 |= (tmp<<(channel<<3));

		//set timer counter mode, up
		pGptIc->gpt_resources->reg->CNT_MODE_CTRL_0 &= ~(0x03<<(channel<<3));
		tmp = 0x00;
		pGptIc->gpt_resources->reg->CNT_MODE_CTRL_0 |= (tmp<<(channel<<3));
	}
	else
	{
		//set timer run mode, time0
		pGptIc->gpt_resources->reg->RUN_MODE_CTRL_1 &= ~(0x03<<((channel-4)<<3));
		tmp = 0x02;
		pGptIc->gpt_resources->reg->RUN_MODE_CTRL_1 |= (tmp<<((channel-4)<<3));

		//set timer counter mode
		pGptIc->gpt_resources->reg->CNT_MODE_CTRL_1 &= ~(0x03<<((channel-4)<<3));
		tmp = 0x00;
		pGptIc->gpt_resources->reg->CNT_MODE_CTRL_1 |= (tmp<<((channel-4)<<3));
	}

	//set DMA enable flag: 	1:dma enable 	0:dma disable
	tmp = control & CSK_GPT_IC_TRANSFER_MODE_Msk;
	if(tmp == CSK_GPT_IC_TRANSFER_MODE_POLLING){
		pGptIc->gpt_resources->ic_capture_info->capture_mode[channel] = POLLING_MODE;
	}else if(tmp == CSK_GPT_IC_TRANSFER_MODE_INTERRUPT){
		pGptIc->gpt_resources->ic_capture_info->capture_mode[channel] = INTERRUPT_MODE;
	}else if(tmp == CSK_GPT_IC_TRANSFER_MODE_DMA){
		pGptIc->gpt_resources->ic_capture_info->capture_mode[channel] = DMA_MODE;

		dma_initialize();
	}

	
	//arg will pass which channel is to be activated
	pGptIc->gpt_resources->info->flags  |= GPT_FLAG_CHANNEL_ACTIVED(channel);
	return CSK_DRIVER_OK;
}

/**
  * @}
  */



/** @defgroup GPT_IC_Exported_Functions_Group2 control functions
  *  @brief    Interrupt enable/disable functions
  *
@verbatim
 ===============================================================================
              ##### Interrupt Enable and disable functions #####
 ===============================================================================
    [..]
		add description

@endverbatim
  * @{
  */

/**
 \fn          HAL_GPT_GetIcData(void *pGpt, uint32_t channel, uint32_t *capdata, uint32_t length)
 \brief		  Get input capture data
 \param[in]   channel: Input capture channel number
 \param[in]   *capdata: data point for input capture data
 \param[in]   length: capture length
 \return      \ref execution_status
 */
int32_t HAL_GPT_GetIcData(void *pGpt, uint32_t channel, uint32_t *capdata, uint32_t length)
{
	GPT_IC_RESOURCES * pGptIc = (GPT_IC_RESOURCES *)pGpt;
	uint32_t state;
	
	if((pGptIc->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}	
	
	if((capdata == NULL) || (length == 0)){
		return CSK_DRIVER_ERROR_PARAMETER;
	}
	
	if(pGptIc->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_IC)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

	pGptIc->gpt_resources->ic_capture_info->ic_num[channel] = length;
	pGptIc->gpt_resources->ic_capture_info->ic_buf[channel] = capdata;
	pGptIc->gpt_resources->ic_capture_info->ic_cnt[channel] = 0;
	
	if(pGptIc->gpt_resources->ic_capture_info->capture_mode[channel] == POLLING_MODE){
		//input capture interrupt disable
		pGptIc->gpt_resources->reg->IMR_CH |= (0x01 << channel);

		//channel start
		pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_START;
		//channelx timer0 enable, for input capture
		pGptIc->gpt_resources->reg->CH_TIMER_ENABLE |= 0x01<<channel;
		//counter start
		pGptIc->gpt_resources->reg->CH_CNT_EN |= 1<<channel;

		//polling the input capture data
		uint32_t intflag_ch;
		while(pGptIc->gpt_resources->ic_capture_info->ic_cnt[channel] < pGptIc->gpt_resources->ic_capture_info->ic_num[channel]){
			//read input capture raw interrupt status and polling
			intflag_ch = pGptIc->gpt_resources->reg->IRSR_CH;
			while((intflag_ch & (0x01<<channel)) == 0){
				intflag_ch = pGptIc->gpt_resources->reg->IRSR_CH;
			}
			pGptIc->gpt_resources->reg->ICR_CH = intflag_ch;
			//read fifo empty status and polling
            while((pGptIc->gpt_resources->reg->FIFO_STATUS_1 & (0x01<<channel)) == 0){
                *pGptIc->gpt_resources->ic_capture_info->ic_buf[channel]++ = pGptIc->gpt_resources->reg->CHx_RX_FIFO[channel];
                pGptIc->gpt_resources->ic_capture_info->ic_cnt[channel]++;
                if(pGptIc->gpt_resources->ic_capture_info->ic_cnt[channel] >= length)
                    break;
            }
		}
	}
	else if(pGptIc->gpt_resources->ic_capture_info->capture_mode[channel] == INTERRUPT_MODE){
		//input capture interrupt enable
		pGptIc->gpt_resources->reg->IMR_CH &= ~(0x01 << channel);

		//channel start
		pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_START;
		//channelx timer0 enable, for input capture
		pGptIc->gpt_resources->reg->CH_TIMER_ENABLE |= 0x01<<channel;
		//counter start
		pGptIc->gpt_resources->reg->CH_CNT_EN |= 1<<channel;
	}
	else if(pGptIc->gpt_resources->ic_capture_info->capture_mode[channel] == DMA_MODE){

//		//channel0 => CFG0, channel1-channel3 => CFG1
//		IP_SYSCTRL->REG_AP_CTRL1.bit.AP_DMA_HS_SEL &= ~(channel==0 ? AP_GPT_RX0:(channel==1 ? AP_GPT_RX1:\
//													   (channel==2 ? AP_GPT_RX2:(channel==3 ? AP_GPT_RX3 : 0x0))));
//		IP_SYSCTRL->REG_AP_CTRL1.bit.AP_DMA_HS_SEL |= (channel==1 ? AP_GPT_RX1:
//													  (channel==2 ? AP_GPT_RX2:(channel==3 ? AP_GPT_RX3 : 0x0)));
//

//		//DMA request select GPT channel 0/1/2/3
		switch (channel) {
			case 0:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_12 = 0x0;
				break;
			case 1:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_14 = 0x0;
				break;
			case 2:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_02 = 0x1;
				break;
			case 3:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_12 = 0x1;
				break;
			default:
				break;
		}

		pGptIc->dma_rx->reqsel = (channel==0 ? AP_GPT_RX0_REQINDEX:(channel==1 ? AP_GPT_RX1_REQINDEX:\
				   	   	   	   	 (channel==2 ? AP_GPT_RX2_REQINDEX:(channel==3 ? AP_GPT_RX3_REQINDEX : 0x0))));

		//GPT module DMA enable
		pGptIc->gpt_resources->reg->DMA_CTRL &= (~(0xFF<<16));
		pGptIc->gpt_resources->reg->DMA_CTRL |= 0x01<<4<<channel;

		//get capture data in dma
		pGptIc->dma_rx->cb_event = (DMA_SignalEvent_t)GPT_Ic_Channnel_Dma_Rx_Handle;

		//DMA transmit
		dma_channel_select(
				&pGptIc->dma_rx->channel,
				pGptIc->dma_rx->cb_event,
				(uint32_t)pGptIc,
				DMA_CACHE_SYNC_DST);
		if(pGptIc->dma_rx->channel == DMA_CHANNEL_ANY)
			return CSK_DRIVER_ERROR;

		pGptIc->dma_channel[channel] = pGptIc->dma_rx->channel;

		state = dma_channel_configure (pGptIc->dma_rx->channel,
					(uint32_t) (&(pGptIc->gpt_resources->reg->CHx_RX_FIFO[channel])),
					(uint32_t) capdata,
					length,
					DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_WORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_WORD) |\
					DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
					DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
					DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
					DMA_CH_CFGL_CH_PRIOR(1),
					DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(pGptIc->dma_rx->reqsel), // config_high
					0, 0);
		if(state == -1)
			return CSK_DRIVER_ERROR;

		//channel start
		pGptIc->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_START;
		//channelx timer0 enable, for input capture
		pGptIc->gpt_resources->reg->CH_TIMER_ENABLE |= 0x01<<channel;
		//counter start
		pGptIc->gpt_resources->reg->CH_CNT_EN |= 1<<channel;

	}
	
	return CSK_DRIVER_OK;
}



/*!
 \fn         int32_t HAL_GPT_RegisterIcCallback(uint32_t channel, CSK_GPT_SignalEvent_t cb_event)
 \brief		 Sets the user callback when input capture generate
 \param[in]  channel: Input capture channel number
 \param[in]	 cb_event: User callback
 \return      \ref execution_status
 */
int32_t HAL_GPT_RegisterIcCallback(void *pGpt, uint32_t channel, CSK_GPT_SignalEvent_t cb_event)
{
	GPT_IC_RESOURCES * pGptIc = (GPT_IC_RESOURCES *)pGpt;

	if((pGptIc->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptIc->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_IC)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

	pGptIc->gpt_resources->info->cb_event[channel] = cb_event;

	return CSK_DRIVER_OK;
}

/**
  * @}
  */


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */



