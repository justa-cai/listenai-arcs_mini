/**
  ******************************************************************************
  * @file    gpt.c
  * @author  ListenAI Application Team
  * @brief   GPT HAL module driver.
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
#include "gpt.h"
#include "PowerManager.h"
#include "ClockManager.h"

/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @defgroup GPT GPT
  * @brief GPT HAL module driver
  * @{
  */


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CSK_GPT_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,00)
CSK_DRIVER_VERSION gpt_driver_version = {CSK_GPT_API_VERSION, CSK_GPT_DRV_VERSION};

/* Private function prototypes -----------------------------------------------*/
void gpt0_irq_handler(void);


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
GPT_INFO gpt0_info = {0};
GPT_IC_CAPTURE_INFO gpt0_ic_capture_info = {0};
GPT_LEDC_TRANSMIT_INFO gpt0_ledc_transmit_info = {0};
GPT_HARDWARE gpt0_hardware = {0};

GPT_RESOURCES gpt0_resources ={
		{
				8
		},
		IP_GPT,
		IRQ_GPT_VECTOR,
		gpt0_irq_handler,
		&gpt0_info,
		&gpt0_ic_capture_info,
		&gpt0_ledc_transmit_info,
		&gpt0_hardware,
		NULL
};


/**
 \fn          void* GPT0 (void)
 \brief       Get resources.
 \return      \ref &gpt0_resources
 */
void* GPT0(void)
{
	return &gpt0_resources;
}


/* Private functions ---------------------------------------------------------*/
/**
  \fn          void gpt_irq_handler (void)
  \brief       GPT interrupt handler
*/
static void gpt_irq_handler (void *pGpt) {

	GPT_RESOURCES * pGpt_Temp = (GPT_RESOURCES *)pGpt;
	uint32_t ch, intflag_timer, intflag_ch;

	//get interrupt status and clear interrupt flag
	intflag_timer = pGpt_Temp->reg->ISR_TIMER;
	//wirte 1 to clear interrupt flag
	pGpt_Temp->reg->ICR_TIMER = intflag_timer;

	//input capture and ledc interrupt status
	intflag_ch = pGpt_Temp->reg->ISR_CH;
	pGpt_Temp->reg->ICR_CH = intflag_ch;

	//get interrupt channel and service interrupt route pGpt->info->cb_event
	for(ch = 0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		//timer overflow interrupt handle
		if((pGpt_Temp->hardware->channel_stat[ch] == HARDWARE_CHANNEL_STAT_USED_BY_TIM)
			&&(intflag_timer & (0x01010101 << ch)))
		{
			pGpt_Temp->info->cb_event[ch](CSK_GPT_EVENT_OVERFLOW, pGpt_Temp->user_param);
		}

		//input capture rx interrupt handle
		if((pGpt_Temp->hardware->channel_stat[ch] == HARDWARE_CHANNEL_STAT_USED_BY_IC)
			&&(intflag_ch & (0x1 << ch)))
		{
			//rx fifo is not empty
			while((pGpt_Temp->reg->FIFO_STATUS_1 & (0x01<<ch)) == 0){
				*pGpt_Temp->ic_capture_info->ic_buf[ch]++ = pGpt_Temp->reg->CHx_RX_FIFO[ch];
				pGpt_Temp->ic_capture_info->ic_cnt[ch]++;
				if(pGpt_Temp->ic_capture_info->ic_cnt[ch] >= pGpt_Temp->ic_capture_info->ic_num[ch]){
					//input capture interrupt disable
					pGpt_Temp->reg->IMR_CH |= 0x01 << ch;
					pGpt_Temp->ic_capture_info->ic_cnt[ch] = 0;
					pGpt_Temp->info->cb_event[ch](CSK_GPT_EVENT_INPUTCAPTURE, pGpt_Temp->user_param);
					break;
				}
			}
		}

		//ledc tx interrupt handle
		if((pGpt_Temp->hardware->channel_stat[ch] == HARDWARE_CHANNEL_STAT_USED_BY_LEDC)
			&&(intflag_ch & (0x100 << ch))){
			//write fifo is empty and data count is equal to data number total number datas transmitted
			if((pGpt_Temp->ledc_transmit_info->ledc_tx_cnt[ch] == pGpt_Temp->ledc_transmit_info->ledc_tx_num[ch]) &&
				 (pGpt_Temp->reg->FIFO_STATUS_2 & (0x01<<16<<ch))){
				//disable tx done interrupt
				pGpt_Temp->reg->IMR_CH |= 0x01 << 8 << ch;
				pGpt_Temp->info->cb_event[ch](CSK_GPT_EVENT_LEDC_TX_DONE, pGpt_Temp->user_param);
			}

			//transimit through : tx fifo is not full flag, full flag is clear and cnt != num
			while(((pGpt_Temp->reg->FIFO_STATUS_2 & (0x01<<24<<ch)) == 0) &&
						 (pGpt_Temp->ledc_transmit_info->ledc_tx_cnt[ch] != pGpt_Temp->ledc_transmit_info->ledc_tx_num[ch])){
					pGpt_Temp->reg->CHxLEDC_TX_FIFO[ch] = *pGpt_Temp->ledc_transmit_info->ledc_tx_buf[ch]++;
					pGpt_Temp->ledc_transmit_info->ledc_tx_cnt[ch]++;
			}
		}
	}
}


void gpt0_irq_handler(void)
{
	gpt_irq_handler(GPT0());
}



/** @defgroup GPT_Exported_Functions GPT Exported Functions
  * @{
  */

/** @defgroup GPT_Exported_Functions_Group1 Initialization and de-initialization functions
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
 \fn          CSK_DRIVER_VERSION GPT_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
 */
CSK_DRIVER_VERSION GPT_GetVersion(void)
{
    return gpt_driver_version;
}


/**
 \fn          CSK_GPT_CAPABILITIES GPT_GetCapabilities(void)
 \brief       Get driver capabilities
 \return      \ref CSK_GPT_CAPABILITIES
 */
CSK_GPT_CAPABILITIES GPT_GetCapabilities(void *pGpt)
{
	GPT_RESOURCES * pGpt_Temp = (GPT_RESOURCES *)pGpt;

    return pGpt_Temp->capabilities;
}



static void GPT_ResetRegisters(void *pGpt)
{
	__HAL_PMU_GPT_RST_ENABLE();
}


/**
 \fn          int32_t GPT_Initialize(void)
 \brief		  Initialize GPT interface
 \return      \ref execution_status
 */
int32_t GPT_Initialize(void *pGpt, void *user_param)
{
	GPT_RESOURCES * pGpt_Temp = (GPT_RESOURCES *)pGpt;
	uint32_t ch;
	
	if((pGpt_Temp->info->flags&GPT_FLAG_INITIALIZED) != GPT_FLAG_INITIALIZED){
	pGpt_Temp->info->flags |= GPT_FLAG_INITIALIZED;

	//GPT module clock enable
    __HAL_CRM_GPT_T0_CLK_ENABLE();
    __HAL_CRM_GPT_S_CLK_ENABLE();

	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGpt_Temp->info->cb_event[ch] = NULL;
		pGpt_Temp->info->clk_source[ch] = 0;
		pGpt_Temp->info->origin_IRQ_handler = NULL;
		pGpt_Temp->ic_capture_info->ic_num[ch] = 0;
		pGpt_Temp->ic_capture_info->ic_buf[ch] = NULL;
		pGpt_Temp->ic_capture_info->ic_cnt[ch] = 0;
	        pGpt_Temp->ledc_transmit_info->ledc_tx_num[ch] = 0;
	        pGpt_Temp->ledc_transmit_info->ledc_tx_buf[ch] = NULL;
	        pGpt_Temp->ledc_transmit_info->ledc_tx_cnt[ch] = 0;
		pGpt_Temp->hardware->channel_stat[ch] = HARDWARE_CHANNEL_STAT_IDLE;
	}
	pGpt_Temp->user_param = user_param;
	}

	return CSK_DRIVER_OK;
}


/**
 \fn          int32_t GPT_Uninitialize(void)
 \brief		  De-initialize GPT interface
 \return      \ref execution_status
 */
int32_t GPT_Uninitialize(void *pGpt)
{
	GPT_RESOURCES * pGpt_Temp = (GPT_RESOURCES *)pGpt;
	uint32_t ch;
	
	//GPT module clock disable
	__HAL_CRM_GPT_T0_CLK_DISABLE();
    __HAL_CRM_GPT_S_CLK_DISABLE();

	pGpt_Temp->info->flags = 0;
	for(ch=0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		pGpt_Temp->info->cb_event[ch] = NULL;
		pGpt_Temp->info->clk_source[ch] = 0;
		pGpt_Temp->info->origin_IRQ_handler = NULL;
		pGpt_Temp->ic_capture_info->ic_num[ch] = 0;
		pGpt_Temp->ic_capture_info->ic_buf[ch] = NULL;
		pGpt_Temp->ic_capture_info->ic_cnt[ch] = 0;
		pGpt_Temp->ledc_transmit_info->ledc_tx_num[ch] = 0;
		pGpt_Temp->ledc_transmit_info->ledc_tx_buf[ch] = NULL;		
		pGpt_Temp->ledc_transmit_info->ledc_tx_cnt[ch] = 0;		
		pGpt_Temp->hardware->channel_stat[ch] = HARDWARE_CHANNEL_STAT_IDLE;
	}
	pGpt_Temp->user_param = NULL;
	
	return CSK_DRIVER_OK;
}



/**
 \fn          int32_t GPT_PowerControl(CSK_POWER_STATE state)
 \brief		  De-initialize GPT interface
 \param[in]   state: Power state \ref enum CSK_POWER_STATE
 \return      \ref execution_status
 */
int32_t GPT_PowerControl(void *pGpt, CSK_POWER_STATE state)
{
	GPT_RESOURCES * pGpt_Temp = (GPT_RESOURCES *)pGpt;
	uint32_t ch;

	switch(state){
	case CSK_POWER_OFF:
		//reset all GPT module
		GPT_ResetRegisters(pGpt);

		// disable interrupt
		disable_IRQ(pGpt_Temp->irq_num);
		// register original interrupt handle and interrupt number
		break;
	case CSK_POWER_LOW:
		break;
	case CSK_POWER_FULL:
		if((pGpt_Temp->info->flags&GPT_FLAG_INITIALIZED) == 0)
		{
			return CSK_DRIVER_ERROR;
		}
		if((pGpt_Temp->info->flags&GPT_FLAG_POWERED) == 0)
		{
			//gpt is not powered
			GPT_ResetRegisters(pGpt);

            // Maybe enable global peripheral interrupt, register interrupt handle and interrupt number
			register_ISR(pGpt_Temp->irq_num, pGpt_Temp->irq_handler, NULL);
	        enable_IRQ(pGpt_Temp->irq_num);

			pGpt_Temp->info->flags |= (GPT_FLAG_POWERED | GPT_FLAG_INITIALIZED);
		}
		else
		{
			//gpt has been powered before
			return CSK_DRIVER_OK;
		}
		break;
	default:
		return CSK_DRIVER_ERROR_UNSUPPORTED;
	}

	return CSK_DRIVER_OK;
}


/**
  * @}
  */


/** @defgroup GPT_Exported_Functions_Group2 control functions
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
 \fn         int32_t GPT_GetStatus(void)
 \brief		 Get GPT status.
 \return     GPT status \ref CSK_GPT_STATUS
 */
CSK_GPT_STATUS GPT_GetStatus(void *pGpt)
{
	GPT_RESOURCES * pGpt_Temp = (GPT_RESOURCES *)pGpt;
	CSK_GPT_STATUS status;

	//get timer status register and return
	status.configured = (pGpt_Temp->info->flags&0xFF)>>8;
	
	return status;
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



