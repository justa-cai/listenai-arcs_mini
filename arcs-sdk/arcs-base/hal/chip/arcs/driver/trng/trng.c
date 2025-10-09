/**
  ******************************************************************************
  * @file    trng.c
  * @author  ListenAI Application Team
  * @brief   TRNG HAL module driver.
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
/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @defgroup TRNG TRNG
  * @brief TRNG HAL module driver
  * @{
  */
#include "arcs_ap.h"
#include "trng_reg.h"
#include "Driver_TRNG.h"


/* Private typedef -----------------------------------------------------------*/
// TRNG flags
#define TRNG_FLAG_INITIALIZED          (1U << 0)
#define TRNG_FLAG_POWERED              (1U << 1)
#define TRNG_FLAG_CONFIGURED           (1U << 2)


#define TRNG_REG_PROTECK_KEY 0xF5000000

#define CSK_TRNG_CONIFG_REG_DELAYTIME_Pos            0
#define CSK_TRNG_CONIFG_REG_HOTTIME_Pos              4
#define CSK_TRNG_CONIFG_REG_COLDTIME_Pos             8

#define ENABLE 1
#define DISABLE 0


typedef struct _TRNG_INFO
{
	uint32_t flags;         		   	// TRNG driver flags
	CSK_TRNG_SignalEvent_t cb_event; 	// Event callback
	uint32_t clk_source[2];
	void (*origin_IRQ_handler)(void);
} TRNG_INFO;

// TRNG Resource Configuration
typedef const struct
{
    TRNG_RegDef* reg;                	  // TRNG register interface
    uint32_t irq_num;
    void (*irq_handler)(void);
    TRNG_INFO* info;               	  // Run-Time control information
    uint32_t user_param;
}TRNG_RESOURCES;


/* Private define ------------------------------------------------------------*/
#define CSK_TRNG_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

#define CHECK_RESOURCES(res)  do{\
        if(res != &trng_resources){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)


/* Private function prototypes -----------------------------------------------*/
static void TRNG0_IRQ_Handler(void);

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static const CSK_DRIVER_VERSION trng_driver_version = {
        CSK_TRNG_API_VERSION,
        CSK_TRNG_DRV_VERSION
};

TRNG_INFO info_trng = {
        0,
        (void*)0,
		{0},
		(void*)0,
};

static const TRNG_RESOURCES trng_resources = {
		IP_TRNG,
		IRQ_TRNG_VECTOR,
		TRNG0_IRQ_Handler,
        &info_trng,
};

/* Private functions ---------------------------------------------------------*/
static void
trng_irq_handler(TRNG_RESOURCES* htrng){

	uint32_t intflag, modeflag;

	intflag = htrng->reg->REG_TRNG_STATUS.bit.DREADY;

	if(intflag == 1){
		htrng->info->cb_event((void*)htrng->user_param);
	}
}

static void
TRNG0_IRQ_Handler(void)
{
	trng_irq_handler(TRNG());
}



/** @defgroup TRNG_Exported_Functions TRNG Exported Functions
  * @{
  */

/** @defgroup TRNG_Exported_Functions_Group1 Initialization and de-initialization functions
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
void* TRNG(void){
    return (void*)&trng_resources;
}

/**
  * @brief add description.
  * @note add description
  * @retval CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
HAL_TRNG_GetVersion(void){
    return trng_driver_version;
}

/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_Initialize(void *res){

    CHECK_RESOURCES(res);

	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

	//add variable initialize
    pTrng->info->flags |= (TRNG_FLAG_INITIALIZED);

    return CSK_DRIVER_OK;
}


/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_Uninitialize(void *res){
    CHECK_RESOURCES(res);

	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

    pTrng->info->flags &= ~(TRNG_FLAG_INITIALIZED);

    return CSK_DRIVER_OK;
}


/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t HAL_TRNG_PowerControl(void *res, CSK_POWER_STATE state)
{
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;
	uint32_t rddat;

	switch(state){
	case CSK_POWER_OFF:

		pTrng->reg->REG_TRNG_CTRL.all = TRNG_REG_PROTECK_KEY | 0;
		pTrng->reg->REG_TRNG_CONFIG.all = TRNG_REG_PROTECK_KEY | 0x223;

		//read to clear interrupt status
		rddat = pTrng->reg->REG_TRNG_DATA.all;

		pTrng->reg->REG_TRNG_INT_ENABLE.bit.DREADY = DISABLE;

	    pTrng->info->flags &= ~(TRNG_FLAG_POWERED);

	    // disable interrupt
		disable_IRQ(pTrng->irq_num);

		// register original interrupt handle and interrupt number
	    register_ISR(pTrng->irq_num, NULL, NULL);

		break;
	case CSK_POWER_LOW:
		break;
	case CSK_POWER_FULL:
		if((pTrng->info->flags&TRNG_FLAG_INITIALIZED) == 0)
		{
			return CSK_DRIVER_ERROR;
		}
		if((pTrng->info->flags&TRNG_FLAG_POWERED) == 0)
		{

			//read to clear interrupt status
			rddat = pTrng->reg->REG_TRNG_DATA.all;

            // Maybe enable global peripheral interrupt, register interrupt handle and interrupt number
			register_ISR(pTrng->irq_num, pTrng->irq_handler, NULL);
	        enable_IRQ(pTrng->irq_num);

	        pTrng->info->flags |= (TRNG_FLAG_POWERED);
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
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_Control(void* res, uint32_t control){
    CHECK_RESOURCES(res);

	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;
	uint32_t tmp, regValue=0;

    if ((pTrng->info->flags & TRNG_FLAG_POWERED) == 0U) {
        // UART not powered
        return CSK_DRIVER_ERROR;
    }

    tmp = (control & CSK_TRNG_DELAYTIME_CONTROL_Msk) >> CSK_TRNG_DELAYTIME_CONTROL_Pos;
    regValue |= tmp<<CSK_TRNG_CONIFG_REG_DELAYTIME_Pos;
    tmp = (control & CSK_TRNG_HOTTIME_CONTROL_Msk) >> CSK_TRNG_HOTTIME_CONTROL_Pos;
    regValue |= tmp<<CSK_TRNG_CONIFG_REG_HOTTIME_Pos;
    tmp = (control & CSK_TRNG_COLDTIME_CONTROL_Msk) >> CSK_TRNG_COLDTIME_CONTROL_Pos;
    regValue |= tmp<<CSK_TRNG_CONIFG_REG_COLDTIME_Pos;

    regValue |= TRNG_REG_PROTECK_KEY;

    pTrng->reg->REG_TRNG_CONFIG.all = regValue;

    return CSK_DRIVER_OK;
}

/**
  * @}
  */






/** @defgroup TRNG_Exported_Functions_Group2 control functions
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
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_InterruptEnable(void* res){

    CHECK_RESOURCES(res);
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

	pTrng->reg->REG_TRNG_INT_ENABLE.bit.DREADY = ENABLE;

    return CSK_DRIVER_OK;
}

/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_InterruptDisable(void* res){
    CHECK_RESOURCES(res);
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

	pTrng->reg->REG_TRNG_INT_ENABLE.bit.DREADY = DISABLE;

    return CSK_DRIVER_OK;
}


/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_Enable(void* res){

    CHECK_RESOURCES(res);
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

	pTrng->reg->REG_TRNG_CTRL.all = TRNG_REG_PROTECK_KEY | ENABLE;

    return CSK_DRIVER_OK;
}

/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_Disable(void* res){

    CHECK_RESOURCES(res);
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

	pTrng->reg->REG_TRNG_CTRL.all = TRNG_REG_PROTECK_KEY;

    return CSK_DRIVER_OK;
}

/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_GetData(void* res){

    CHECK_RESOURCES(res);
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;
	uint32_t rngdata;

	rngdata = pTrng->reg->REG_TRNG_DATA.all;

    return rngdata;
}

/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_GetDataReady(void* res){

    CHECK_RESOURCES(res);
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

    return pTrng->reg->REG_TRNG_STATUS.bit.DREADY;
}

/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  */
int32_t
HAL_TRNG_RegisterCallback(void *res, CSK_TRNG_SignalEvent_t cb_event){

    CHECK_RESOURCES(res);
	TRNG_RESOURCES * pTrng = (TRNG_RESOURCES *)res;

	pTrng->info->cb_event = cb_event;

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
