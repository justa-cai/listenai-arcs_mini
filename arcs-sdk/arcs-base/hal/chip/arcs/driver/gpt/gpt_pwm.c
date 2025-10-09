/**
  ******************************************************************************
  * @file    gpt_pwm.c
  * @author  ListenAI Application Team
  * @brief   GPT PWM HAL module driver.
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
#include "Driver_GPT_PWM.h"
#include "ClockManager.h"
#include "gpt.h"
#include "dma.h"


typedef void (*DMA_SignalEvent_t) (uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param);
/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @defgroup GPT_PWM GPT_PWM
  * @brief GPT_PWM HAL module driver
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
typedef struct _GPT_PWM_INFO
{
    uint32_t freq[GPT_NUMBER_OF_CHANNELS];       // PWM channel frequency
    uint32_t duty[GPT_NUMBER_OF_CHANNELS];   	 	 // PWM channel duty cycle (0 ~ 255)
    uint32_t polarity[GPT_NUMBER_OF_CHANNELS];
}GPT_PWM_INFO;


// GPT_LEDC DMA
typedef struct _GPT_LEDC_DMA
{
    uint8_t channel;                       // DMA Channel
    uint8_t reqsel;                        // DMA request selection
    DMA_SignalEvent_t cb_event;            // DMA Event callback
} GPT_LEDC_DMA;

typedef struct _GPT_PWM_RESOURCES
{
	GPT_RESOURCES *gpt_resources;
	GPT_PWM_INFO *pwm_info;	
	GPT_LEDC_DMA *dma_tx;
	uint8_t dma_channel[GPT_NUMBER_OF_CHANNELS];	
}GPT_PWM_RESOURCES;


/* Private function prototypes -----------------------------------------------*/
static void GPT_LEDC_Channel_Dma_Tx_Handle(uint32_t event_info, uint32_t xfer_bytes, void *pGpt);


/* Private macro -------------------------------------------------------------*/
#define AP_GPT_TX0_REQINDEX (13)
#define AP_GPT_TX1_REQINDEX (15)
#define AP_GPT_TX2_REQINDEX (3)
#define AP_GPT_TX3_REQINDEX (14)


/* Private variables ---------------------------------------------------------*/
extern GPT_RESOURCES gpt0_resources; 

GPT_PWM_INFO	gpt0_pwm_info = {0};	
GPT_LEDC_DMA	gpt0_ledc_dma = {0,0,NULL};

static GPT_PWM_RESOURCES gpt0_pwm_resources ={
	&gpt0_resources,
	&gpt0_pwm_info,
	&gpt0_ledc_dma,
	{0}
};


/* Private functions ---------------------------------------------------------*/
/*!
 \fn         void GPT_LEDC_Channel_Dma_Tx_Handle(uint32_t event_info, uint32_t xfer_bytes, void *pGpt)
 \brief		 Input capture Dma mode interrupt handle
 \param[in]  event_info: DMA_EVENT_TRANSFER_COMPLETE
 \param[in]	 xfer_bytes: dma transfer bytes
 \param[in]	 *pGpt: user parameter, it could be module instance
 \return      \ref execution_status
 */
static void GPT_LEDC_Channel_Dma_Tx_Handle(uint32_t event_info, uint32_t xfer_bytes, void *pGpt)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	uint32_t channelinfo, eventinfo;
	channelinfo = (event_info & 0xFF00) >> 8;
	eventinfo = event_info & 0xFF;
	switch (eventinfo) {
		case DMA_EVENT_TRANSFER_COMPLETE:
			for(uint8_t ch=0; ch<8; ch++){
				//ledc channel dma funciton is enabled and dma_channel is same as dma interrupt channel
				if(pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_mode[ch] == DMA_MODE) {
					if(pGptPWM->dma_channel[ch] == channelinfo){
						pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[ch] = pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_num[ch];
						//call interrupt mode user callback funciton
						//wait ledc data tranfer completed
						//fifo is not empty
						while((pGptPWM->gpt_resources->reg->FIFO_STATUS_2 & (0x01<<16<<ch)) == 0);
						//check raw status for sendout
						while((pGptPWM->gpt_resources->reg->IRSR_CH & (0x01<<8<<ch)) == 0);
						//clear raw interrupt flag
						pGptPWM->gpt_resources->reg->ICR_CH |= (0x01<<8<<ch);

						pGptPWM->gpt_resources->info->cb_event[ch](CSK_GPT_EVENT_LEDC_TX_DONE, pGptPWM->gpt_resources->user_param);
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


/** @defgroup GPT_PWM_Exported_Functions GPT_PWM Exported Functions
  * @{
  */

/** @defgroup GPT_PWM_Exported_Functions_Group1 Initialization and de-initialization functions
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
void* GPT0_PWM(void)
{
	return &gpt0_pwm_resources;
}


/**
 \fn          int32_t HAL_GPT_PWMInitialize(void)
 \brief		  Initialize GPT pwm interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMInitialize(void *pGpt, void *user_param)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	uint32_t ch, status;

	status = GPT_Initialize(pGptPWM->gpt_resources, user_param);
	if(status != CSK_DRIVER_OK)
		return status;

	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGptPWM->pwm_info->duty[ch] = 0;
		pGptPWM->pwm_info->freq[ch] = 0;
	}
	pGptPWM->dma_tx->channel = 0;
	pGptPWM->dma_tx->reqsel = 0;//DMA_HSID_UART1_TX;//share with AP_GPT_TX2
	pGptPWM->dma_tx->cb_event = NULL;

	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGptPWM->dma_channel[ch]= 0;
	}

	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_PWMUninitialize(void)
 \brief		  De-initialize GPT pwm interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMUninitialize(void *pGpt)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	uint32_t ch, status;

	status = GPT_Uninitialize(pGptPWM->gpt_resources);
	if(status != CSK_DRIVER_OK)
		return status;

	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGptPWM->pwm_info->duty[ch] = 0;
		pGptPWM->pwm_info->freq[ch] = 0;
	}

	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_PWMPowerControl(CSK_POWER_STATE state)
 \brief		  GPT power control
 \param[in]   state: Power state \ref enum CSK_POWER_STATE
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMPowerControl(void *pGpt, CSK_POWER_STATE state)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

	return(GPT_PowerControl(pGptPWM->gpt_resources, state));
}



/**
 \fn          int32_t HAL_GPT_PWMControl(void *pGpt, uint32_t control, GPT_CHANNEL_TYPE channel)
 \brief		  Control GPT Pwm interface
 \param[in]   control: Operation
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_PWMControl(void *pGpt, uint32_t control, GPT_CHANNEL_TYPE channel)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	uint8_t divider[8] = {0 ,1, 2, 3 ,4, 5, 6, 7};
	uint32_t clockfreq[3] = {CRM_GetSrcFreq(CRM_IpSrcXtalClk), CLOCK_EXT, CRM_GetSrcFreq(CRM_IpSrcPeriClk)};
	uint32_t tmp;
	
	if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_IDLE)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}
	else if((control & CSK_GPT_PWM_OPERATION_MODE_Msk) == CSK_GPT_PWM_OPERATION_MODE_LEDC){
		pGptPWM->gpt_resources->hardware->channel_stat[channel] = HARDWARE_CHANNEL_STAT_USED_BY_LEDC;
	}
	else{
		pGptPWM->gpt_resources->hardware->channel_stat[channel] = HARDWARE_CHANNEL_STAT_USED_BY_PWM;
	}

	//clock gate disable
	pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_GATE);
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_CNT_GATE);
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_SAMPLE_GATE);

	//clock source select
	pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_SEL_Msk);
	tmp = (control & CSK_GPT_PWM_CLKSRC_Msk) >> CSK_GPT_PWM_CLKSRC_Pos;
	pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] |= tmp << GPT_CHx_CLK_CTRL_CLK_SEL_Pos;	
	
	//clock divider setting
	pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_DIV_Msk);
	tmp = (control & CSK_GPT_PWM_CLKDIV_Msk) >> CSK_GPT_PWM_CLKDIV_Pos;
	pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] |= tmp << GPT_CHx_CLK_CTRL_CLK_DIV_Pos;			
	pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] |= GPT_CHx_CLK_CTRL_CLK_DIV_LD;
	pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] |= GPT_CHx_CLK_CTRL_CLK_PREDIV_LD;

	//set pwm mode
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CH_MODE_Msk);
	tmp = (control & CSK_GPT_PWM_CONTROL_Msk) >> CSK_GPT_PWM_CONTROL_Pos;
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= tmp << GPT_CHx_CTRL_CH_MODE_Pos;			
	
	//set pwm operation mode
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CH_OPERATION_Msk);
	tmp = (control & CSK_GPT_PWM_OPERATION_MODE_Msk) >> CSK_GPT_PWM_OPERATION_MODE_Pos;
	if((control & CSK_GPT_PWM_OPERATION_MODE_Msk) == CSK_GPT_PWM_OPERATION_MODE_LEDC_DMA){
		tmp &= 0x04;
	}
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= tmp << GPT_CHx_CTRL_CH_OPERATION_Pos;

	//set ledc duty cycle
	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] == HARDWARE_CHANNEL_STAT_USED_BY_LEDC)
	{
//		//configure ledc duty cycle as fixed 0.4us and 0.85us
		HAL_GPT_LEDCDutyCycle(pGpt, 0.8, 0.45, channel);

		tmp = control & CSK_GPT_LEDC_TRANSFER_MODE_Msk;
		if(tmp == CSK_GPT_LEDC_TRANSFER_MODE_POLLING){
			pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_mode[channel] = POLLING_MODE;
		}else if(tmp == CSK_GPT_LEDC_TRANSFER_MODE_INTERRUPT){
			pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_mode[channel] = INTERRUPT_MODE;
		}else if(tmp == CSK_GPT_LEDC_TRANSFER_MODE_DMA){
			pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_mode[channel] = DMA_MODE;

			//DMA request select GPT channel 0/1/2/3
			switch (channel) {
				case 0:
					IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_13 = 0x0;
					break;
				case 1:
					IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_15 = 0x0;
					break;
				case 2:
					IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_03 = 0x1;
					break;
				case 3:
					IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_14 = 0x1;
					break;
				default:
					break;
			}

			pGptPWM->dma_tx->reqsel = (channel==0 ? AP_GPT_TX0_REQINDEX:(channel==1 ? AP_GPT_TX1_REQINDEX:\
									  (channel==2 ? AP_GPT_TX2_REQINDEX:(channel==3 ? AP_GPT_TX3_REQINDEX : 0x0))));

			//GPT module DMA enable
			pGptPWM->gpt_resources->reg->DMA_CTRL &= (~(0xFF<<16));
			pGptPWM->gpt_resources->reg->DMA_CTRL |= 0x01<<channel;

			dma_initialize();

		}else{
			return CSK_DRIVER_ERROR_PARAMETER;
		}
	}
	
	//set pwm output mode
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_PWMOUT_MODE_Msk);
	tmp = (control & CSK_GPT_PWM_OUTMODE_Msk) >> CSK_GPT_PWM_OUTMODE_Pos;
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= tmp << GPT_CHx_CTRL_PWMOUT_MODE_Pos;		
	
	//set pwm output polarity
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_PWM_POLARITY_Msk);
	tmp = (control & CSK_GPT_PWM_OUTPOLARITY_Msk) >> CSK_GPT_PWM_OUTPOLARITY_Pos;
	//save pwm polarity
	pGptPWM->pwm_info->polarity[channel] = tmp;
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= tmp << GPT_CHx_CTRL_PWM_POLARITY_Pos;		

	//arg will pass which channel is to be activated
	pGptPWM->gpt_resources->info->flags  |= GPT_FLAG_CHANNEL_ACTIVED(channel);
	return CSK_DRIVER_OK;
}

/**
  * @}
  */

/** @defgroup GPT_PWM_Exported_Functions_Group2 control functions
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
 \fn          int32_t HAL_GPT_SetPWMFreqDuty(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t freq, uint8_t duty)
 \brief		  Set PWM output frequency and duty cycles
 \param[in]   channel: Pwm channel number
 \param[in]   freq: frequency setting
 \param[in]   duty: duty cycles setting(0<duty<100 represent 0% < duty cycle < 100%), duty should not be 0 or 100!!!!
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetPWMFreqDuty(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t freq, uint8_t duty)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	uint32_t tmp;
	uint8_t divider[8] = {0 ,1, 2, 3 ,4, 5, 6, 7};
    uint32_t clockfreq[3] = {CRM_GetSrcFreq(CRM_IpSrcXtalClk), CLOCK_EXT, CRM_GetSrcFreq(CRM_IpSrcPeriClk)};

	if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//PWM is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_PWM)
	{
		return CSK_DRIVER_ERROR;
	}
	
	if (channel >= pGptPWM->gpt_resources->capabilities.channels)
	{
    // PWM channel is not existed
		return CSK_DRIVER_ERROR;
	}
	
	pGptPWM->pwm_info->duty[channel] = duty;
	pGptPWM->pwm_info->freq[channel] = freq;
	
	uint32_t high16bit,low16bit;
	uint8_t freqpos, pos;
	pos = (pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_DIV) >> GPT_CHx_CLK_CTRL_CLK_DIV_Pos;
	freqpos = (pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_SEL) >> GPT_CHx_CLK_CTRL_CLK_SEL_Pos;
	if((pGptPWM->gpt_resources->reg->CHx_CTRL[channel] & GPT_CHx_CTRL_PWMOUT_MODE_Msk) == GPT_CHx_CTRL_PWMOUT_MODE){
		//pwmout mode1
		high16bit = ((clockfreq[freqpos] >> divider[pos])/freq)>>1;
		low16bit = high16bit*duty/100;
		if((high16bit > 0xFFFF) || (low16bit > 0xFFFF))
			return CSK_DRIVER_ERROR;
	}
	else{
		//pwmout mode0
		tmp = ((clockfreq[freqpos] >> divider[pos])/freq);
		high16bit = tmp*duty/100;
		low16bit = tmp - high16bit;

		if((high16bit > 0xFFFF) || (low16bit > 0xFFFF))
			return CSK_DRIVER_ERROR;
	}

	pGptPWM->gpt_resources->reg->CHx_RELOAD[channel] = (high16bit<<16) | low16bit;
	pGptPWM->gpt_resources->reg->SHADOW_LOAD = (0x01<<channel) | ((0x01<<GPT_NUMBER_OF_CHANNELS)<<channel);

	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_SetPWMFreqDuty_NoShadow(void *pGpt, uint32_t channel, uint32_t freq, uint8_t duty)
 \brief       Set PWM output frequency and duty cycles without shadow load, must call HAL_GPT_ShadowLoad later
 \param[in]   *pGpt: GPT0_PWM()
 \param[in]   channel: Pwm channel number
 \param[in]   freq: frequency setting
 \param[in]   duty: duty cycles setting(0<duty<100 represent 0%<duty<100%), duty should not be 0 or 100!!!!!!
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetPWMFreqDuty_NoShadow(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t freq, uint8_t duty)
{
    GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
    uint32_t tmp;
    uint8_t divider[8] = {0 ,1, 2, 3 ,4, 5, 6, 7};
//    uint32_t clockfreq[3] = {BOARD_H_XTAL_SRC_FREQ, CLOCK_EXT, BOARD_BOOTCLOCKRUN_AP_PCLK_CLK};
    uint32_t clockfreq[3] = {24000000, 24000000, 24000000};

    if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
    {
        //PWM is not powered
        return CSK_DRIVER_ERROR;
    }

    if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_PWM)
    {
        return CSK_DRIVER_ERROR;
    }

    if (channel >= pGptPWM->gpt_resources->capabilities.channels)
    {
    // PWM channel is not existed
        return CSK_DRIVER_ERROR;
    }

    pGptPWM->pwm_info->duty[channel] = duty;
    pGptPWM->pwm_info->freq[channel] = freq;

    if(duty == 0){
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_CNT_GATE);

        //counter stop
        pGptPWM->gpt_resources->reg->CH_CNT_EN |= 1<<8<<channel;
        //channelx timer3 disable, for pwm
        pGptPWM->gpt_resources->reg->CH_TIMER_ENABLE &= ~(0x01<<24<<channel);
        //channel stop
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_STOP;
        //polarity output low
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~GPT_CHx_CTRL_PWM_POLARITY;

        return CSK_DRIVER_OK;
    }else if(duty == 100){

        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_CNT_GATE);

        //counter stop
        pGptPWM->gpt_resources->reg->CH_CNT_EN |= 1<<8<<channel;
        //channelx timer3 disable, for pwm
        pGptPWM->gpt_resources->reg->CH_TIMER_ENABLE &= ~(0x01<<24<<channel);
        //channel stop
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_STOP;

        //polarity output high
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_PWM_POLARITY;


        return CSK_DRIVER_OK;
    }else{
        //set pwm output polarity
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_PWM_POLARITY_Msk);
        //restore pwm polarity
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= pGptPWM->pwm_info->polarity[channel] << GPT_CHx_CTRL_PWM_POLARITY_Pos;
        pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_START;
    }

    uint32_t high16bit,low16bit;
    uint8_t freqpos, pos;
    pos = (pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_DIV) >> GPT_CHx_CLK_CTRL_CLK_DIV_Pos;
    freqpos = (pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_SEL) >> GPT_CHx_CLK_CTRL_CLK_SEL_Pos;
    if((pGptPWM->gpt_resources->reg->CHx_CTRL[channel] & GPT_CHx_CTRL_PWMOUT_MODE_Msk) == GPT_CHx_CTRL_PWMOUT_MODE){
        //pwmout mode1, center aligned
        high16bit = ((clockfreq[freqpos] >> divider[pos])/freq)>>1;
        low16bit = high16bit*duty/100;
        if((high16bit > 0xFFFF) || (low16bit > 0xFFFF))
            return CSK_DRIVER_ERROR;
    }
    else{
        //pwmout mode0, edge aligned
        tmp = ((clockfreq[freqpos] >> divider[pos])/freq);
        high16bit = tmp*duty/100;
        low16bit = tmp - high16bit;

        high16bit = (high16bit > 1) ? (high16bit - 1) : (1);
        low16bit = (low16bit > 1) ? (low16bit - 1) : (1);
        if((high16bit > 0xFFFF) || (low16bit > 0xFFFF))
            return CSK_DRIVER_ERROR;
    }

    pGptPWM->gpt_resources->reg->CHx_RELOAD[channel] = (high16bit<<16) | low16bit;

    return CSK_DRIVER_OK;
}



/**
 \fn          int32_t HAL_GPT_SetPWMFreqDutyByCounter_NoShadow(void *pGpt, GPT_CHANNEL_TYPE channel, uint16_t high16bits, uint16_t low16bits)
 \brief       Set PWM output frequency and duty cycles without shadow load, must call HAL_GPT_ShadowLoad later,
 \brief       Edge mode: Frequency = PCLK/DIV/(high16bits+low16bits), Center mode: Frequency = PCLK/DIV/high16bits,  low16bits represents high duty cycle counter
 \brief       Example: Edge mode: 16000 = 100000000/1/(high16bits+low16bits), Center mode: 16000 = 100000000/1/high16bits,  low16bits = high16bits*30%
 \param[in]   *pGpt: GPT0_PWM()
 \param[in]   high16bits: high duty cycle
 \param[in]   low16bits: low duty cycle
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetPWMFreqDutyByCounter_NoShadow(void *pGpt, GPT_CHANNEL_TYPE channel, uint16_t high16bits, uint16_t low16bits)
{
    GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

    if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
    {
        //PWM is not powered
        return CSK_DRIVER_ERROR;
    }

    if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_PWM)
    {
        return CSK_DRIVER_ERROR;
    }

    if (channel >= pGptPWM->gpt_resources->capabilities.channels)
    {
    // PWM channel is not existed
        return CSK_DRIVER_ERROR;
    }

    pGptPWM->gpt_resources->reg->CHx_RELOAD[channel] = (high16bits<<16) | low16bits;

    return CSK_DRIVER_OK;
}





int32_t HAL_GPT_ShadowLoad(void *pGpt, GPT_CHANNEL_SYNC_TYPE channel)
{
    GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

    if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
    {
        //PWM is not powered
        return CSK_DRIVER_ERROR;
    }
    pGptPWM->gpt_resources->reg->SHADOW_SYNC = 0x00;
    pGptPWM->gpt_resources->reg->SHADOW_LOAD = (channel | (channel<<8));

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_EnablePWM(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		  	Enable PWM channel
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_EnablePWM(void *pGpt, GPT_CHANNEL_TYPE channel)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

	if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//PWM is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_PWM)
	{
		return CSK_DRIVER_ERROR;
	}	
	
	//counter start
	pGptPWM->gpt_resources->reg->CH_CNT_EN |= 1<<channel;
	//channelx timer3 enable, for pwm
	pGptPWM->gpt_resources->reg->CH_TIMER_ENABLE |= 0x01<<24<<channel;
	//channel start	
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_START;
	
	return CSK_DRIVER_OK;
}



int32_t HAL_GPT_EnablePWM_NoCntStart(void *pGpt, GPT_CHANNEL_TYPE channel)
{
    GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

    if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
    {
        //PWM is not powered
        return CSK_DRIVER_ERROR;
    }

    if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_PWM)
    {
        return CSK_DRIVER_ERROR;
    }

//    //counter start
//    pGptPWM->gpt_resources->reg->CH_CNT_EN |= 1<<channel;
    //channelx timer3 enable, for pwm
    pGptPWM->gpt_resources->reg->CH_TIMER_ENABLE |= 0x01<<24<<channel;
    //channel start
    pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_START;

    return CSK_DRIVER_OK;
}


int32_t HAL_GPT_CntStart(void *pGpt, GPT_CHANNEL_SYNC_TYPE channel)
{
    GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

    if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
    {
        //PWM is not powered
        return CSK_DRIVER_ERROR;
    }

    pGptPWM->gpt_resources->reg->CH_CNT_EN |= channel;

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_DisablePWM(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		  Disable PWM channel
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_DisablePWM(void *pGpt, GPT_CHANNEL_TYPE channel)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	
	if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//PWM is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_PWM)
	{
		return CSK_DRIVER_ERROR;
	}		
	
	//counter stop
	pGptPWM->gpt_resources->reg->CH_CNT_EN |= 1<<8<<channel;
	//channelx timer3 disable, for pwm
	pGptPWM->gpt_resources->reg->CH_TIMER_ENABLE &= ~(0x01<<24<<channel);
	//channel stop
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_STOP;	
	
	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_EnableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		  Enable LEDC channel
 \param[in]   channel: LEDC channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_EnableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

	if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//PWM is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_LEDC)
	{
		return CSK_DRIVER_ERROR;
	}		
	
	//counter start
	pGptPWM->gpt_resources->reg->CH_CNT_EN |= 1<<channel;
	//channelx timer3 enable, for ledc
	pGptPWM->gpt_resources->reg->CH_TIMER_ENABLE |= 0x01<<24<<channel;
	//channel start	
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_START;
	
	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_DisableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		  Disable LEDC channel
 \param[in]   channel: LEDC channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_DisableLEDC(void *pGpt, GPT_CHANNEL_TYPE channel)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	
	if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//PWM is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_LEDC)
	{
		return CSK_DRIVER_ERROR;
	}		
	
	//counter stop
	pGptPWM->gpt_resources->reg->CH_CNT_EN |= 1<<8<<channel;
	//channelx timer3 disable, for ledc
	pGptPWM->gpt_resources->reg->CH_TIMER_ENABLE &= ~(0x01<<24<<channel);
	//channel stop
	pGptPWM->gpt_resources->reg->CHx_CTRL[channel] |= GPT_CHx_CTRL_CH_STOP;	
	
	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_LEDCOut(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t *senddata, uint32_t length)
 \brief		  Set LEDC output
 \param[in]   channel: LEDC channel number
 \param[in]   senddata: data sent to LEDC
 \param[in]   length: data length
 \return      \ref execution_status
 */
int32_t HAL_GPT_LEDCOut(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t *senddata, uint32_t length)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
	uint32_t state;
	
	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_LEDC)
	{
		return CSK_DRIVER_ERROR;
	}		

	//clear fifo rd&wr pointer
	pGptPWM->gpt_resources->reg->FIFO_CONFIG = 0xFFFF0000;
	pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_buf[channel] = senddata;
	pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_num[channel] = length;
	pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[channel] = 0;
	uint32_t tmpFifoCnt = 0;

	if(pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_mode[channel] == POLLING_MODE)
	{
		//fill fifo
		pGptPWM->gpt_resources->reg->CHxLEDC_TX_FIFO[channel] = *pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_buf[channel]++;
		pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[channel]++;
		pGptPWM->gpt_resources->reg->CHxLEDC_TX_FIFO[channel] = *pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_buf[channel]++;
		pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[channel]++;
		HAL_GPT_EnableLEDC(pGpt, channel);
		//wait when fifo is full
		while(pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[channel] != pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_num[channel])
		{
			while((pGptPWM->gpt_resources->reg->FIFO_STATUS_2 & (0x01<<24<<channel)) != 0);
			pGptPWM->gpt_resources->reg->CHxLEDC_TX_FIFO[channel] = *pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_buf[channel]++;
			pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[channel]++;
		}
		//fifo is not empty
		while((pGptPWM->gpt_resources->reg->FIFO_STATUS_2 & (0x01<<16<<channel)) == 0);
		//check raw status for sendout
		while((pGptPWM->gpt_resources->reg->IRSR_CH & (0x01<<8<<channel)) == 0);
		//clear raw interrupt flag
		pGptPWM->gpt_resources->reg->ICR_CH |= (0x01<<8<<channel);
		HAL_GPT_DisableLEDC(pGpt, channel);
	}
	else if(pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_mode[channel] == INTERRUPT_MODE)
	{
		//LEDC mode, only channel0 -- channel4 support DMA
		if(channel >= 4)
			return CSK_DRIVER_ERROR_PARAMETER;

		//if transmit fifo is not full, get unused fifo data
		if((pGptPWM->gpt_resources->reg->FIFO_STATUS_2 & (0x01<<24<<channel)) == 0){
			tmpFifoCnt = (channel<4) ? pGptPWM->gpt_resources->reg->FIFO_STATUS_1 : pGptPWM->gpt_resources->reg->FIFO_STATUS_0;
			tmpFifoCnt = tmpFifoCnt & (0x07<<8<<channel);
			tmpFifoCnt = LEDC_FIFO_DEPTH - tmpFifoCnt;	//fifo unused depth for ledc
			
			while((tmpFifoCnt--) && (pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[channel] != pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_num[channel]))
			{
				pGptPWM->gpt_resources->reg->CHxLEDC_TX_FIFO[channel] = *pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_buf[channel]++;
				pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_cnt[channel]++;
			}
			
			//enable tx done interrupt
			pGptPWM->gpt_resources->reg->IMR_CH &= ~(0x01 << 8 << channel);
		}
		//start ledc transmit
		HAL_GPT_EnableLEDC(pGpt, channel);
	}
	else if(pGptPWM->gpt_resources->ledc_transmit_info->ledc_tx_mode[channel] == DMA_MODE)
	{
//		//DMA request select GPT channel 0/1/2/3
		switch (channel) {
			case 0:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_13 = 0x0;
				break;
			case 1:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_15 = 0x0;
				break;
			case 2:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_03 = 0x1;
				break;
			case 3:
				IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_14 = 0x1;
				break;
			default:
				break;
		}

		pGptPWM->dma_tx->reqsel = (channel==0 ? AP_GPT_TX0_REQINDEX:(channel==1 ? AP_GPT_TX1_REQINDEX:\
				   	   	   	   	  (channel==2 ? AP_GPT_TX2_REQINDEX:(channel==3 ? AP_GPT_TX3_REQINDEX : 0x0))));

		//GPT module DMA enable
		pGptPWM->gpt_resources->reg->DMA_CTRL &= (~(0xFF<<16));
		pGptPWM->gpt_resources->reg->DMA_CTRL |= 0x01<<channel;

		//DMA transmit
		pGptPWM->dma_tx->cb_event = (DMA_SignalEvent_t)GPT_LEDC_Channel_Dma_Tx_Handle;

		dma_channel_select(
				&pGptPWM->dma_tx->channel,
				pGptPWM->dma_tx->cb_event,
				(uint32_t)pGptPWM,
				DMA_CACHE_SYNC_SRC);
		if(pGptPWM->dma_tx->channel == DMA_CHANNEL_ANY)
				return CSK_DRIVER_ERROR;

		pGptPWM->dma_channel[channel] = pGptPWM->dma_tx->channel;

		state = dma_channel_configure (pGptPWM->dma_tx->channel,
										(uint32_t) senddata,
										(uint32_t) (&(pGptPWM->gpt_resources->reg->CHxLEDC_TX_FIFO[channel])),
										length,
										DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_WORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_WORD) |\
										DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
										DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |\
										DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN,
										DMA_CH_CFGL_CH_PRIOR(1),
										DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_DST_PER(pGptPWM->dma_tx->reqsel), // config_high
										0, 0);
		if(state == -1)
			return CSK_DRIVER_ERROR;
		//start ledc transmit
		HAL_GPT_EnableLEDC(pGpt, channel);
	}

	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_LEDCDutyCycle(void *pGpt, float highDutyCycle, float lowDutyCycle, GPT_CHANNEL_TYPE channel)
 \brief       Set LEDC duty cycle
 \param[in]   float highDutyCycle(us), Example 0.85
 \param[in]   float lowDutyCycle(us), Example 0.4
 \param[in]   channel: Pwm channel number
 \return      \ref execution_status
 */
int32_t HAL_GPT_LEDCDutyCycle(void *pGpt, float highDutyCycle, float lowDutyCycle, GPT_CHANNEL_TYPE channel)
{
    GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;
    uint8_t divider[8] = {0 ,1, 2, 3 ,4, 5, 6, 7};
//    uint32_t clockfreq[3] = {BOARD_H_XTAL_SRC_FREQ, CLOCK_EXT, BOARD_BOOTCLOCKRUN_AP_PCLK_CLK};
    uint32_t clockfreq[3] = {24000000, 24000000, 24000000};

    //configure ledc duty cycle as fixed 0.4us and 0.85us
    uint32_t high16bits, low16bits, sysclock;
    uint8_t pos, freqpos;
    pos = (pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_DIV) >> GPT_CHx_CLK_CTRL_CLK_DIV_Pos;
    freqpos = (pGptPWM->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_SEL) >> GPT_CHx_CLK_CTRL_CLK_SEL_Pos;
    //bit1 for RELAOD, bit0 for MATCH
    high16bits = highDutyCycle*(((clockfreq[freqpos] >> divider[pos])/1000000) & 0x00FF);
    low16bits = lowDutyCycle*(((clockfreq[freqpos] >> divider[pos])/1000000) & 0x00FF);
    pGptPWM->gpt_resources->reg->CHx_RELOAD[channel] = (high16bits<<16) | low16bits;
    high16bits = lowDutyCycle*(((clockfreq[freqpos] >> divider[pos])/1000000) & 0x00FF);
    low16bits = highDutyCycle*(((clockfreq[freqpos] >> divider[pos])/1000000) & 0x00FF);
    pGptPWM->gpt_resources->reg->CHx_MATCH_0[channel] = (high16bits<<16) | low16bits;
    pGptPWM->gpt_resources->reg->SHADOW_SYNC = 0x101<<channel;
    pGptPWM->gpt_resources->reg->SHADOW_LOAD = 0x101<<channel;

    return CSK_DRIVER_OK;
}


/*!
 \fn         int32_t HAL_GPT_RegisterLEDCCallback(GPT_CHANNEL_TYPE channel, CSK_GPT_SignalEvent_t cb_event)
 \brief		 Sets the user callback when the ledc data send finished
 \param[in]  channel: Timer channel number
 \param[in]	 cb_event: User callback
 \return      \ref execution_status
 */
int32_t HAL_GPT_RegisterLEDCCallback(void *pGpt, GPT_CHANNEL_TYPE channel, CSK_GPT_SignalEvent_t cb_event)
{
	GPT_PWM_RESOURCES * pGptPWM = (GPT_PWM_RESOURCES *)pGpt;

	if((pGptPWM->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptPWM->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_LEDC)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

	pGptPWM->gpt_resources->info->cb_event[channel] = cb_event;

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
