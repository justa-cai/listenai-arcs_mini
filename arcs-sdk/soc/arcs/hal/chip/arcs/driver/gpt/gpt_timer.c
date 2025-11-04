/**
  ******************************************************************************
  * @file    gpt_timer.c
  * @author  ListenAI Application Team
  * @brief   GPT TIMER HAL module driver.
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
#include "Driver_GPT_TIMER.h"
#include "gpt.h"


/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @defgroup GPT_TIMER GPT_TIMER
  * @brief GPT_TIMER HAL module driver
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
typedef struct _GPT_TIMER_INFO
{
    uint32_t period_count[GPT_NUMBER_OF_CHANNELS]; // channel period count
}GPT_TIMER_INFO;


typedef struct _GPT_TIMER_RESOURCES
{
	GPT_RESOURCES *gpt_resources;
    GPT_TIMER_INFO *timer_info;
}GPT_TIMER_RESOURCES;


/* Private define ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern GPT_RESOURCES gpt0_resources; 

GPT_TIMER_INFO	gpt0_timer_info = {0};

static GPT_TIMER_RESOURCES gpt0_timer_resources ={
	&gpt0_resources,
	&gpt0_timer_info
};



/* Private functions ---------------------------------------------------------*/




/** @defgroup GPT_TIMER_Exported_Functions GPT_TIMER Exported Functions
  * @{
  */

/** @defgroup GPT_TIMER_Exported_Functions_Group1 Initialization and de-initialization functions
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
 \fn          void* GPT0_TIMER(void)
 \brief		  Get GPT timer instance
 \return      \ref gpt0_timer_resources
 */
void* GPT0_TIMER(void)
{
	return &gpt0_timer_resources;
}



/**
 \fn          int32_t HAL_GPT_TimerInitialize(void)
 \brief		  Initialize GPT timer interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_TimerInitialize(void *pGpt, void *user_param)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;
	uint32_t ch, status_Temp;

	status_Temp = GPT_Initialize(pGptTimer->gpt_resources, user_param);
	if(status_Temp != CSK_DRIVER_OK)
		return status_Temp;

	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGptTimer->timer_info->period_count[ch] = 0;
	}

	return CSK_DRIVER_OK;
}



/**
 \fn          int32_t HAL_GPT_TimerUninitialize(void)
 \brief		  De-initialize GPT timer interface
 \return      \ref execution_status
 */
int32_t HAL_GPT_TimerUninitialize(void *pGpt)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;
	uint32_t ch, status_Temp;

	status_Temp = GPT_Uninitialize(pGptTimer->gpt_resources);
	if(status_Temp != CSK_DRIVER_OK)
		return status_Temp;

	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGptTimer->timer_info->period_count[ch] = 0;
	}

	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t HAL_GPT_TimerPowerControl(CSK_POWER_STATE state)
 \brief		  GPT power control
 \param[in]   state: Power state \ref enum CSK_POWER_STATE
 \return      \ref execution_status
 */
int32_t HAL_GPT_TimerPowerControl(void *pGpt, CSK_POWER_STATE state)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;

	return(GPT_PowerControl(pGptTimer->gpt_resources, state));
}


/**
 \fn          int32_t HAL_GPT_TimerControl(void *pGpt, uint32_t control, GPT_CHANNEL_TYPE channel)
 \brief		  Control GPT interface
 \param[in]   control: Operation
 \param[in]   channel: Timer channel number
 \return      \ref execution_status
 */	
int32_t HAL_GPT_TimerControl(void *pGpt, uint32_t control, GPT_CHANNEL_TYPE channel)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;
	uint32_t tmp;
	
	if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_IDLE)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}
	else
	{
		pGptTimer->gpt_resources->hardware->channel_stat[channel] = HARDWARE_CHANNEL_STAT_USED_BY_TIM;
	}

  /* TODO: hardware layer */
	//clock gate disable
	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_GATE);
	pGptTimer->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_CNT_GATE);
	pGptTimer->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CLK_SAMPLE_GATE);

	//clock source select
	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_SEL_Msk);
	tmp = (control & CSK_GPT_TIMER_CLKSRC_Msk) >> CSK_GPT_TIMER_CLKSRC_Pos;
	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] |= tmp << GPT_CHx_CLK_CTRL_CLK_SEL_Pos;
	
	//clock divider setting
	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] &= ~(GPT_CHx_CLK_CTRL_CLK_DIV_Msk);
	tmp = (control & CSK_GPT_TIMER_CLKDIV_Msk) >> CSK_GPT_TIMER_CLKDIV_Pos;
	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] |= tmp << GPT_CHx_CLK_CTRL_CLK_DIV_Pos;		
	
	//divider load enable, pulse: W1S
	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] |= GPT_CHx_CLK_CTRL_CLK_DIV_LD;	
	
//	//clock pre divider setting
//	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] |= 0x05;
//	//divider load enable, pulse: W1S
//	pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] |= 1<<23;

	//set timer mode
	pGptTimer->gpt_resources->reg->CHx_CTRL[channel] &= ~(GPT_CHx_CTRL_CH_MODE_Msk);
	tmp = (control & CSK_GPT_TIMER_CONTROL_Msk) >> CSK_GPT_TIMER_CONTROL_Pos;
	pGptTimer->gpt_resources->reg->CHx_CTRL[channel] |= tmp << GPT_CHx_CTRL_CH_MODE_Pos;	
		
	//enable peripheral's single interrupt	
	switch(control & CSK_GPT_TIMER_CONTROL_Msk){
		default:
			break;
		case CSK_GPT_TIMER_32_BIT_TIMER:

			if(channel < 4)
			{
				//set timer run mode, time0				
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_0 &= ~(0x03<<(channel<<3));
				tmp = (control & CSK_GPT_TIMER_RUNMODE_Msk) >> CSK_GPT_TIMER_RUNMODE_Pos;
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_0 |= tmp<<(channel<<3);
	
				//set timer counter mode	
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_0 &= ~(0x03<<(channel<<3));
				tmp = (control & CSK_GPT_TIMER_COUNTERMODE_Msk) >> CSK_GPT_TIMER_COUNTERMODE_Pos;
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_0 |= tmp<<(channel<<3);
			}
			else
			{
				//set timer run mode, time0						
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_1 &= ~(0x03<<((channel-4)<<3));
				tmp = (control & CSK_GPT_TIMER_RUNMODE_Msk) >> CSK_GPT_TIMER_RUNMODE_Pos;
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_1 |= tmp<<((channel-4)<<3);

				//set timer counter mode
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_1 &= ~(0x03<<((channel-4)<<3));
				tmp = (control & CSK_GPT_TIMER_COUNTERMODE_Msk) >> CSK_GPT_TIMER_COUNTERMODE_Pos;
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_1 |= tmp<<((channel-4)<<3);
			}
		
			//channelx timer0 interrupt mask
			pGptTimer->gpt_resources->reg->IMR_TIMER &= ~(1<<channel); 			
			break;
		case CSK_GPT_TIMER_16_BIT_TIMER:
			if(channel < 4)
			{
				//set timer run mode, time0&time1						
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_0 &= ~(0x0F<<(channel<<3));
				tmp = (control & CSK_GPT_TIMER_RUNMODE_Msk) >> CSK_GPT_TIMER_RUNMODE_Pos;
				tmp |= (tmp<<2);
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_0 |= tmp<<(channel<<3);

				//set timer counter mode, time0&time1	
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_0 &= ~(0x0F<<(channel<<3));
				tmp = (control & CSK_GPT_TIMER_COUNTERMODE_Msk) >> CSK_GPT_TIMER_COUNTERMODE_Pos;
				tmp |= (tmp<<2);
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_0 |= tmp<<(channel<<3);
				
			}
			else
			{
				//set timer run mode, time0&time1						
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_1 &= ~(0x0F<<((channel-4)<<3));
				tmp = (control & CSK_GPT_TIMER_RUNMODE_Msk) >> CSK_GPT_TIMER_RUNMODE_Pos;
				tmp |= (tmp<<2);
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_1 |= tmp<<((channel-4)<<3);

				//set timer counter mode, time0&time1	
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_1 &= ~(0x03<<((channel-4)<<3));
				tmp = (control & CSK_GPT_TIMER_COUNTERMODE_Msk) >> CSK_GPT_TIMER_COUNTERMODE_Pos;
				tmp |= (tmp<<2);
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_1 |= tmp<<((channel-4)<<3);
			}		
		
			//channelx timer0&timer1 interrupt mask
			pGptTimer->gpt_resources->reg->IMR_TIMER &= ~((1<<channel) | ((0x01<<8)<<channel)); 
			break;
		case CSK_GPT_TIMER_8_BIT_TIMER:	
			if(channel < 4)
			{
				//set timer run mode, time0&time1&timer2&timer3						
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_0 &= ~(0xFF<<(channel<<3));
				tmp = (control & CSK_GPT_TIMER_RUNMODE_Msk) >> CSK_GPT_TIMER_RUNMODE_Pos;
				tmp |= (tmp<<2);
				tmp |= (tmp<<4);
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_0 |= tmp<<(channel<<3);

				//set timer counter mode, time0&time1&timer2&timer3	
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_0 &= ~(0xFF<<(channel<<3));
				tmp = (control & CSK_GPT_TIMER_COUNTERMODE_Msk) >> CSK_GPT_TIMER_COUNTERMODE_Pos;
				tmp |= (tmp<<2);
				tmp |= (tmp<<4);
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_0 |= tmp<<(channel<<3);
			}
			else
			{
				//set timer run mode, time0&time1&timer2&timer3					
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_1 &= ~(0xFF<<((channel-4)<<3));
				tmp = (control & CSK_GPT_TIMER_RUNMODE_Msk) >> CSK_GPT_TIMER_RUNMODE_Pos;
				tmp |= (tmp<<2);
				tmp |= (tmp<<4);
				pGptTimer->gpt_resources->reg->RUN_MODE_CTRL_1 |= tmp<<((channel-4)<<3);

				//set timer counter mode, time0&time1&timer2&timer3	
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_1 &= ~(0xFF<<(channel<<3));
				tmp = (control & CSK_GPT_TIMER_COUNTERMODE_Msk) >> CSK_GPT_TIMER_COUNTERMODE_Pos;
				tmp |= (tmp<<2);
				tmp |= (tmp<<4);
				pGptTimer->gpt_resources->reg->CNT_MODE_CTRL_1 |= tmp<<(channel<<3);
			}			
		
			//channelx timer0&timer1&timer2&timer3 interrupt mask
			pGptTimer->gpt_resources->reg->IMR_TIMER &= ~((0x01<<channel) | ((0x01<<8)<<channel) | ((0x01<<16)<<channel) | ((0x01<<24)<<channel));
			break;
	}
	
	//arg will pass which channel is to be activated
	pGptTimer->gpt_resources->info->flags  |= GPT_FLAG_CHANNEL_ACTIVED(channel);
	return CSK_DRIVER_OK;
}

/**
  * @}
  */


/** @defgroup GPT_TIMER_Exported_Functions_Group2 control functions
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

/*!
 \fn         int32_t HAL_GPT_RegisterTimerCallback(uint32_t channel, CSK_GPT_SignalEvent_t cb_event)
 \brief		 Sets the user callback when the timer time out
 \param[in]  channel: Timer channel number
 \param[in]	 cb_event: User callback
 \return      \ref execution_status
 */
int32_t HAL_GPT_RegisterTimerCallback(void *pGpt, GPT_CHANNEL_TYPE channel, CSK_GPT_SignalEvent_t cb_event)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;

	if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

	pGptTimer->gpt_resources->info->cb_event[channel] = cb_event;

	return CSK_DRIVER_OK;
}


/*!
 \fn         int32_t GPT_SetTimerPeriodByCount(uint32_t channel, uint32_t count)
 \brief		 Sets the timer period in units of count.
 \param[in]  channel: Timer channel number
 \param[in]	 count: Timer period in units of count
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetTimerPeriodByCount(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t count)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;

	if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

	//set timer reload value in hardware and update gpt->info
	pGptTimer->timer_info->period_count[channel] = count;
	pGptTimer->gpt_resources->reg->CHx_RELOAD[channel] = count;
	pGptTimer->gpt_resources->reg->SHADOW_SYNC &= ~((1<<8)<<channel);
	pGptTimer->gpt_resources->reg->SHADOW_LOAD |= (1<<8)<<channel;
	return CSK_DRIVER_OK;
}


/*!
 \fn         int32_t HAL_GPT_SetTimerPeriodDirectByCount(uint32_t channel, uint32_t count)
 \brief      Sets the timer period in units of count directly(reload value update immediately).
 \param[in]  channel: Timer channel number
 \param[in]  count: Timer period in units of count
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetTimerPeriodDirectByCount(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t count)
{
    GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;

    if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
    {
        //gpt is not powered
        return CSK_DRIVER_ERROR;
    }

    if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
    {
        return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
    }

    //set timer reload value in hardware and update gpt->info
    pGptTimer->timer_info->period_count[channel] = count;
    pGptTimer->gpt_resources->reg->CHx_RELOAD[channel] = count;
    pGptTimer->gpt_resources->reg->SHADOW_SYNC |= (1<<8)<<channel;
    pGptTimer->gpt_resources->reg->SHADOW_LOAD |= (1<<8)<<channel;
	return CSK_DRIVER_OK;
}




/*!
 \fn         int32_t GPT_SetTimerPeriodByMs(uint32_t channel, uint32_t ms)
 \brief		 Sets the timer period in units of ms.
 \param[in]  channel: Timer channel number
 \param[in]	 count: Timer period in units of ms
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetTimerPeriodByMs(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t ms)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;

	if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

	uint32_t clocksrc = (pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_SEL) >> GPT_CHx_CLK_CTRL_CLK_SEL_Pos;
	uint32_t cntdiv = (pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_DIV) >> GPT_CHx_CLK_CTRL_CLK_DIV_Pos;
	uint32_t clockfreq, cntval;
	switch(clocksrc)
	{
		case 0x00:
		//24M crystal
			clockfreq = 24000000>>cntdiv;
		break;
		case 0x01:
		//external unknow
			clockfreq = 10000000>>cntdiv;
		break;
		case 0x02:
		//PCLK
//			clockfreq = CRM_GetApapbFreq()>>cntdiv;
			clockfreq = 24000000>>cntdiv;
		break;
		default:
//			clockfreq = CRM_GetApapbFreq()>>cntdiv;
			clockfreq = 24000000>>cntdiv;
		break;
	}

	cntval = clockfreq/1000 * ms;

	//set timer reload value in hardware and update gpt->info
	pGptTimer->timer_info->period_count[channel] = cntval;
	pGptTimer->gpt_resources->reg->CHx_RELOAD[channel] = cntval;
	pGptTimer->gpt_resources->reg->SHADOW_SYNC &= ~((1<<8)<<channel);
	pGptTimer->gpt_resources->reg->SHADOW_LOAD |= (1<<8)<<channel;
	return CSK_DRIVER_OK;
}


/*!
 \fn         int32_t HAL_GPT_SetTimerPeriodDirectByMs(uint32_t channel, uint32_t ms)
 \brief      Sets the timer period in units of ms directly(reload value update immediately)..
 \param[in]  channel: Timer channel number
 \param[in]  count: Timer period in units of ms
 \return      \ref execution_status
 */
int32_t HAL_GPT_SetTimerPeriodDirectByMs(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t ms)
{
    GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;

    if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
    {
        //gpt is not powered
        return CSK_DRIVER_ERROR;
    }

    if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
    {
        return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
    }

    uint32_t clocksrc = (pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_SEL) >> GPT_CHx_CLK_CTRL_CLK_SEL_Pos;
    uint32_t cntdiv = (pGptTimer->gpt_resources->reg->CHx_CLK_CTRL[channel] & GPT_CHx_CLK_CTRL_CLK_DIV) >> GPT_CHx_CLK_CTRL_CLK_DIV_Pos;
    uint32_t clockfreq, cntval;
    switch(clocksrc)
    {
        case 0x00:
        //24M crystal
            clockfreq = 24000000>>cntdiv;
        break;
        case 0x01:
        //external unknow
            clockfreq = 10000000>>cntdiv;
        break;
        case 0x02:
        //PCLK
//            clockfreq = CRM_GetApapbFreq()>>cntdiv;
        	clockfreq = 24000000>>cntdiv;
        break;
        default:
//            clockfreq = CRM_GetApapbFreq()>>cntdiv;
        	clockfreq = 24000000>>cntdiv;
        break;
    }

    cntval = clockfreq/1000 * ms;

    //set timer reload value in hardware and update gpt->info
    pGptTimer->timer_info->period_count[channel] = cntval;
    pGptTimer->gpt_resources->reg->CHx_RELOAD[channel] = cntval;
    pGptTimer->gpt_resources->reg->SHADOW_SYNC |= (1<<8)<<channel;
    pGptTimer->gpt_resources->reg->SHADOW_LOAD |= (1<<8)<<channel;
    return CSK_DRIVER_OK;
}


/*!
 \fn         int32_t HAL_GPT_StartTimer(void *pGpt, GPT_CHANNEL_TYPE channel)
 \brief		 Starts the timer counting.
 \param[in]  channel Timer channel number.
 \return      \ref execution_status
 */
int32_t HAL_GPT_StartTimer(void *pGpt, GPT_CHANNEL_TYPE channel)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;
	uint32_t tmp;
	
	if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

	//get timer mode
	tmp =	(pGptTimer->gpt_resources->reg->CHx_CTRL[channel] & GPT_CHx_CTRL_CH_MODE_Msk) >> GPT_CHx_CTRL_CH_MODE_Pos;
	switch(tmp){
		default:
			break;
		case CSK_GPT_TIMER_32_BIT_TIMER:
			//cnt start
			pGptTimer->gpt_resources->reg->CH_CNT_EN |= 0x01<<channel;
            //channelx timer0 enable
            pGptTimer->gpt_resources->reg->CH_TIMER_ENABLE |= 0x01<<channel;
			break;
		case CSK_GPT_TIMER_16_BIT_TIMER:
			//cnt start
			pGptTimer->gpt_resources->reg->CH_CNT_EN |= 0x01<<channel;
			//channelx timer0&timer1 enable
			pGptTimer->gpt_resources->reg->CH_TIMER_ENABLE |= (0x01<<channel) | ((0x01<<8)<<channel);
			break;
		case CSK_GPT_TIMER_8_BIT_TIMER:
			//cnt start
			pGptTimer->gpt_resources->reg->CH_CNT_EN |= 0x01<<channel;
			//channelx timer0&timer1&timer2&timer3 enable	
			pGptTimer->gpt_resources->reg->CH_TIMER_ENABLE |= (0x01<<channel) | ((0x01<<8)<<channel) | ((0x01<<16)<<channel) | ((0x01<<24)<<channel);
			break;		
	}
	return CSK_DRIVER_OK;
}


/*!
 \fn         int32_t gpt_read_timer_count(uint32_t channel, uint32_t *count)
 \brief		 Reads the current timer counting value.
 \param[in]	 channel Timer channel number
 \param[out] *count 32-bit result
 \return      \ref execution_status
 */
int32_t HAL_GPT_ReadTimerCount(void *pGpt, GPT_CHANNEL_TYPE channel, uint32_t *count)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;

	if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

    /* TODO: hardware layer */
	//get timer counter and set counter value in *count
	*count = pGptTimer->gpt_resources->reg->CHx_CNT[channel];

	return CSK_DRIVER_OK;
}


/*!
 \fn         int32_t gpt_stop_timer(uint32_t channel)
 \brief		 Stops the timer counting. Timers reload their periods respectively after the next time they call the StartTimer.
 \param[in]  channel Timer channel number.
 \return      \ref execution_status
 */
int32_t HAL_GPT_StopTimer(void *pGpt, GPT_CHANNEL_TYPE channel)
{
	GPT_TIMER_RESOURCES * pGptTimer = (GPT_TIMER_RESOURCES *)pGpt;
	uint32_t tmp;
	
	if((pGptTimer->gpt_resources->info->flags&GPT_FLAG_POWERED) == 0)
	{
		//gpt is not powered
		return CSK_DRIVER_ERROR;
	}

	if(pGptTimer->gpt_resources->hardware->channel_stat[channel] != HARDWARE_CHANNEL_STAT_USED_BY_TIM)
	{
		return CSK_GPT_ERROR_HARDWARE_CONFLICTION;
	}

    /* TODO: hardware layer */
	//get timer mode
	tmp =	(pGptTimer->gpt_resources->reg->CHx_CTRL[channel] & GPT_CHx_CTRL_CH_MODE_Msk) >> GPT_CHx_CTRL_CH_MODE_Pos;
    //cnt stop
    pGptTimer->gpt_resources->reg->CH_CNT_EN |= (0x0100<<channel);
	switch(tmp){
		default:
			break;
		case CSK_GPT_TIMER_32_BIT_TIMER:
			//channelx timer0 disable
			pGptTimer->gpt_resources->reg->CH_TIMER_ENABLE &= ~(0x01<<channel); 			
			break;
		case CSK_GPT_TIMER_16_BIT_TIMER:
			//channelx timer0&timer1 disable
			pGptTimer->gpt_resources->reg->CH_TIMER_ENABLE &= ~((0x01<<channel) | ((0x01<<8)<<channel)); 		
			break;
		case CSK_GPT_TIMER_8_BIT_TIMER:
			//channelx timer0&timer1&timer2&timer3 disable
			pGptTimer->gpt_resources->reg->CH_TIMER_ENABLE &= ~((0x01<<channel) | ((0x01<<8)<<channel) | ((0x01<<16)<<channel) | ((0x01<<24)<<channel)); 		
			break;		
	}

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


