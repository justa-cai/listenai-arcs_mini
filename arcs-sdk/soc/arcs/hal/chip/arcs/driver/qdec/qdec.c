/*
 * qdec.c
 *
 * Created on:
 *
 */
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "Driver_Common.h"
#include "Driver_QDEC.h"
#include "arcs_ap.h"
#include "qdec_reg.h"
#include "ClockManager.h"

#define CSK_QDEC_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)
#define CSK_QDEC_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

// driver version
static const
CSK_DRIVER_VERSION qdec_driver_version = { CSK_QDEC_API_VERSION, CSK_QDEC_DRV_VERSION };

#define DEBUG_LOG   1 // 0

#if DEBUG_LOG
#define LOGD(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG


//------------------------------------------------------------------------------------------


_FAST_DATA_VI static QDEC_HandleTypeDef qdec_dev = {
		(void*)QDEC_BASE,
		{0},
		NULL,
		0,
};


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/


/**
  * @brief  Handles QDEC interrupt request.
  * @retval None
  */
_FAST_FUNC_RO void QDEC_IRQ_Handler()
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev.Instance;
	uint32_t status = qdec_reg->REG_QDEC_ISR.all;

	/* Clear the corresponding interrupt flags */
	qdec_reg->REG_DEC_ICR.all = status;

 	/* All events sent to callback */
	if(status)
	{
		if(qdec_dev.cb_event)
			qdec_dev.cb_event(status, 0);
	}

}


/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Return QDEC instance.
  * @retval Instance of QDEC
  */
void* QDEC_Instance()
{
	return &qdec_dev;
}

/**
  * @brief  Initializes the QDEC according to the specified
  *         parameters in the QDEC_InitTypeDef and create the associated handle.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @param  callback: pointer to the callback function
  * @retval execution status
  */
int32_t QDEC_Init(QDEC_HandleTypeDef *qdec_dev, void *callback)
{
	if(qdec_dev == NULL)
	{
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	/* Check parameters */
    if((qdec_dev->Init.ModeX != QDEC_MODE_X1) && \
            (qdec_dev->Init.ModeX != QDEC_MODE_X2) && \
            (qdec_dev->Init.ModeX != QDEC_MODE_X4)){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if((qdec_dev->Init.ModeY != QDEC_MODE_X1) && \
            (qdec_dev->Init.ModeY != QDEC_MODE_X2) && \
            (qdec_dev->Init.ModeY != QDEC_MODE_X4)){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if((qdec_dev->Init.ModeZ != QDEC_MODE_X1) && \
            (qdec_dev->Init.ModeZ != QDEC_MODE_X2) && \
            (qdec_dev->Init.ModeZ != QDEC_MODE_X4)){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

	/* Enable QDEC clock */
	__HAL_CRM_QDEC_CLK_ENABLE();

	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;

    /* Configure the mode */
	qdec_reg->REG_QDEC_CTRL0.bit.X_MODE = qdec_dev->Init.ModeX;
	qdec_reg->REG_QDEC_CTRL0.bit.Y_MODE = qdec_dev->Init.ModeY;
	qdec_reg->REG_QDEC_CTRL0.bit.Z_MODE = qdec_dev->Init.ModeZ;

    /* Configure the swapping */
	if(qdec_dev->Init.SwapX)
		qdec_reg->REG_QDEC_CTRL0.bit.X_SWAP_EN = 1;
	else
		qdec_reg->REG_QDEC_CTRL0.bit.X_SWAP_EN = 0;

	if(qdec_dev->Init.SwapY)
		qdec_reg->REG_QDEC_CTRL0.bit.Y_SWAP_EN = 1;
	else
		qdec_reg->REG_QDEC_CTRL0.bit.Y_SWAP_EN = 0;

	if(qdec_dev->Init.SwapZ)
		qdec_reg->REG_QDEC_CTRL0.bit.Z_SWAP_EN = 1;
	else
		qdec_reg->REG_QDEC_CTRL0.bit.Z_SWAP_EN = 0;

    /* Configure the clock divider */
	if(qdec_dev->Init.ClkDivIn != QDEC_CLK_DIV_DIS)
	{
		qdec_reg->REG_QDEC_CTRL0.bit.CLK_DIV = qdec_dev->Init.ClkDivIn;
		qdec_reg->REG_QDEC_CTRL0.bit.CLK_DIV_LD_EN = 1;
	}

	if(qdec_dev->Init.ClkDivDeb != QDEC_CLK_DIV_DIS)
	{
		qdec_reg->REG_QDEC_CTRL0.bit.DEBOUNCE_CLK_DIV = qdec_dev->Init.ClkDivDeb;
		qdec_reg->REG_QDEC_CTRL0.bit.DEBOUNCE_CLK_DIV_LD_EN = 1;
		qdec_reg->REG_QDEC_CTRL0.bit.DEBOUNCE_EN = 1;
	}

	/* Set up callback */
	qdec_dev->cb_event = (CSK_QDEC_SignalEvent_t)callback;

	/* Update error code */
	qdec_dev->ErrorCode = QDEC_ERR_NONE;

	/* Init the low level hardware and interrupt */
	qdec_reg->REG_QDEC_IMR.all = qdec_dev->Init.IntSel;
	register_ISR(IRQ_QDEC_VECTOR, (ISR)QDEC_IRQ_Handler, NULL);
	enable_IRQ(IRQ_QDEC_VECTOR);

	return CSK_DRIVER_OK;
}

/**
  * @brief  De-Initializes the QDEC.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure.
  * @retval execution status
  */
int32_t QDEC_DeInit(QDEC_HandleTypeDef *qdec_dev)
{
	if(qdec_dev == NULL)
	{
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	disable_IRQ(IRQ_QDEC_VECTOR);

	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
	qdec_reg->REG_QDEC_IMR.all = 0;

	qdec_dev->ErrorCode = QDEC_ERR_NONE;

	/* Disable QDEC clock */
	__HAL_CRM_QDEC_CLK_DISABLE();

	return CSK_DRIVER_OK;
}


/**
  * @brief  Enable QDEC capture
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @param  axes: specify the corresponding axes
  * @retval None
  */
void QDEC_Start(QDEC_HandleTypeDef *qdec_dev, uint32_t axes)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
	/* Enable QDEC */
	if(QDEC_AXES_X & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.X_EN = 1;
	if(QDEC_AXES_Y & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.Y_EN = 1;
	if(QDEC_AXES_Z & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.Z_EN = 1;

    return;
}

/**
  * @brief  Disable QDEC capture
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @param  axes: specify the corresponding axes
  * @retval None
  */
void QDEC_Stop(QDEC_HandleTypeDef *qdec_dev, uint32_t axes)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
	/* Disable QDEC */
	if(QDEC_AXES_X & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.X_EN = 0;
	if(QDEC_AXES_Y & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.Y_EN = 0;
	if(QDEC_AXES_Z & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.Z_EN = 0;
}

/**
 * @brief  Return the QDEC error code
 *
 */
uint32_t QDEC_GetError(QDEC_HandleTypeDef *qdec_dev)
{
	return qdec_dev->ErrorCode;
}

/**
  * @brief  Read out the X counter.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @retval the counter value
  */
int16_t QDEC_Read_X(QDEC_HandleTypeDef *qdec_dev)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
    return (int16_t)qdec_reg->REG_QDEC_X_POS_CNT.all;
}

/**
  * @brief  Read out the Y counter.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @retval the counter value
  */
int16_t QDEC_Read_Y(QDEC_HandleTypeDef *qdec_dev)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
    return (int16_t)qdec_reg->REG_QDEC_Y_POS_CNT.all;
}

/**
  * @brief  Read out the Z counter.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @retval the counter value
  */
int16_t QDEC_Read_Z(QDEC_HandleTypeDef *qdec_dev)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
    return (int16_t)qdec_reg->REG_QDEC_Z_POS_CNT.all;
}

/**
  * @brief  Set the X counter event.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @retval None
  */
void QDEC_SetEvtThrd_X(QDEC_HandleTypeDef *qdec_dev, uint8_t threshold)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
    qdec_reg->REG_QDEC_CTRL1.bit.X_EVENT_TH = threshold;
}

/**
  * @brief  Set the Y counter event.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @retval None
  */
void QDEC_SetEvtThrd_Y(QDEC_HandleTypeDef *qdec_dev, uint8_t threshold)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
    qdec_reg->REG_QDEC_CTRL1.bit.Y_EVENT_TH = threshold;
}

/**
  * @brief  Set the Z counter event.
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @retval None
  */
void QDEC_SetEvtThrd_Z(QDEC_HandleTypeDef *qdec_dev, uint8_t threshold)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
    qdec_reg->REG_QDEC_CTRL1.bit.Z_EVENT_TH = threshold;
}

/**
  * @brief  Clear the counters of corresponding axes
  * @param  qdec_dev: pointer to a QDEC_HandleTypeDef structure that contains
  *                the configuration information.
  * @param  axes: specify the corresponding axes
  * @retval None
  */
void QDEC_ClearCounter(QDEC_HandleTypeDef *qdec_dev, uint32_t axes)
{
	QDEC_RegDef *qdec_reg = (QDEC_RegDef *)qdec_dev->Instance;
	if(QDEC_AXES_X & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.X_CLR_CNT = 1;
	if(QDEC_AXES_Y & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.Y_CLR_CNT = 1;
	if(QDEC_AXES_Z & axes)
		qdec_reg->REG_QDEC_CTRL0.bit.Z_CLR_CNT = 1;
}
