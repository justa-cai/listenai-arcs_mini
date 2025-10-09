/**
  ******************************************************************************
  * @file    Driver_PMU.h
  * @author  ListenAI Application Team
  * @brief   Header file of PMU HAL module.
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


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CSK_DRIVER_PMU_H
#define __CSK_DRIVER_PMU_H


#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"
#include "PowerManager.h"

/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @addtogroup PMU
  * @{
  */


/* Exported types ------------------------------------------------------------*/
/** @defgroup PMU_Exported_Types PMU Exported Types
  * @{
  */

/**
  * @brief  add description
  */
typedef void (*CSK_PMU_SignalEvent_t) (void* workspace);

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/

/** @defgroup PMU_Exported_Constants PMU Exported Constants
  * @{
  */
#define CSK_PMU_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)
#define CSK_PMU_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)


/*---------------------Control mode for application---------------------------------*/
/** @defgroup PMU_HOLD_mode_entry PMU SLEEP mode entry
  * @{
  */
#define PMU_HOLDENTRY_WFI              ((uint8_t)0x01)
#define PMU_HOLDENTRY_WFE              ((uint8_t)0x02)
/**
  * @}
  */

/** @defgroup PMU_LIGHTSLEEP_mode_entry PMU STOP mode entry
  * @{
  */
#define PMU_LIGHTSLEEPENTRY_WFI               ((uint8_t)0x01)
#define PMU_LIGHTSLEEPENTRY_WFE               ((uint8_t)0x02)
/**
  * @}
  */
#define PMU_SLEEPMODE_MODE1                 (1)
#define PMU_SLEEPMODE_MODE2                 (2)
#define PMU_SLEEPMODE_MODE3                 (3)

//wakeup cause
#define WAKUP_CAUSE_RTC                     (1<<(30-17))
#define WAKUP_CAUSE_BT_TIMER                (1<<(29-17))
#define WAKUP_CAUSE_GPIOB7                  (1<<(28-17))
#define WAKUP_CAUSE_GPIOB6 					(1<<(27-17))
#define WAKUP_CAUSE_GPIOB5 					(1<<(26-17))
#define WAKUP_CAUSE_GPIOB4 					(1<<(25-17))
#define WAKUP_CAUSE_GPIOB3 					(1<<(24-17))
#define WAKUP_CAUSE_GPIOB2 					(1<<(23-17))
#define WAKUP_CAUSE_GPIOB1 					(1<<(22-17))
#define WAKUP_CAUSE_GPIOB0 					(1<<(21-17))
#define WAKUP_CAUSE_TIMER 					(1<<(20-17))
#define WAKUP_CAUSE_KEY1 					(1<<(19-17))
#define WAKUP_CAUSE_KEY0 					(1<<(18-17))
#define WAKUP_CAUSE_IWDT 					(1<<(17-17))

//reset cause
#define RESET_CAUSE_AP_RESET                (1<<16)
#define RESET_CAUSE_WDT_RESET               (1<<17)
#define RESET_CAUSE_LOCKUP_RESET            (1<<18)



//wakeup source select
#define CSK_PMU_WAKE_SELECT_Pos             0
#define CSK_PMU_WAKE_SELECT_Msk				(0x7F << CSK_PMU_WAKE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_GPIOB_6			(0x40 << CSK_PMU_WAKE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_GPIOB_5			(0x20 << CSK_PMU_WAKE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_GPIOB_4			(0x10 << CSK_PMU_WAKE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_GPIOB_3			(0x08 << CSK_PMU_WAKE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_GPIOB_2			(0x04 << CSK_PMU_WAKE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_GPIOB_1			(0x02 << CSK_PMU_WAKE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_GPIOB_0			(0x01 << CSK_PMU_WAKE_SELECT_Pos)

#define CSK_PMU_WAKE_MODULE_SELECT_Pos      7
#define CSK_PMU_WAKE_MODULE_SELECT_Msk      (0x3F << CSK_PMU_WAKE_MODULE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_RTC				(0x20 << CSK_PMU_WAKE_MODULE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_TIMER			(0x10 << CSK_PMU_WAKE_MODULE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_KEY1			(0x08 << CSK_PMU_WAKE_MODULE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_KEY0			(0x04 << CSK_PMU_WAKE_MODULE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_IWDT			(0x02 << CSK_PMU_WAKE_MODULE_SELECT_Pos)
#define CSK_PMU_WAKE_SELECT_BT_TIMER		(0x01 << CSK_PMU_WAKE_MODULE_SELECT_Pos)


#define CSK_PMU_WAKE_GPIOB_POLARITY_Pos     13
#define CSK_PMU_WAKE_GPIOB_POLARITY_Msk		(0x7F << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
//wake up when GPIOB is high
#define CSK_PMU_WAKE_GPIOB_0_POLARITY_HIGH	(0x0 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
//wake up when GPIOB is low
#define CSK_PMU_WAKE_GPIOB_0_POLARITY_LOW	(0x1 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_1_POLARITY_HIGH	(0x0 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_1_POLARITY_LOW	(0x2 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_2_POLARITY_HIGH	(0x0 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_2_POLARITY_LOW	(0x4 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_3_POLARITY_HIGH	(0x0 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_3_POLARITY_LOW	(0x8 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_4_POLARITY_HIGH	(0x0 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_4_POLARITY_LOW	(0x10 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_5_POLARITY_HIGH	(0x0 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)
#define CSK_PMU_WAKE_GPIOB_5_POLARITY_LOW	(0x20 << CSK_PMU_WAKE_GPIOB_POLARITY_Pos)

/*----- PMU interrupt mode : -----*/

/**
  * @}
  */


/* Exported macro ------------------------------------------------------------*/
/** @defgroup PMU_Exported_Macro PMU Exported Macro
  * @{
  */



/**
  * @}
  */


/* Exported functions --------------------------------------------------------*/
/** @addtogroup PMU_Exported_Functions PMU Exported Functions
  * @{
  */

/** @addtogroup PMU_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
void* PMU(void);

/**
  * @brief add description.
  * @return None
  */
int32_t HAL_PMU_Initialize(void *res);

/**
  * @brief add description.
  * @return None
  */
int32_t HAL_PMU_Uninitialize(void *res);

/**
  * @brief add description.
  * @return None
  */
int32_t HAL_PMU_PowerControl(void *res, CSK_POWER_STATE state);

/**
  * @brief add description.
  * @return None
  */
int32_t HAL_PMU_Control(void* res, uint32_t control);
/**
  * @}
  */


/** @addtogroup PMU_Exported_Functions_Group2 Peripheral Control functions
  * @{
  */
CSK_DRIVER_VERSION HAL_PMU_GetVersion(void);

/**
  * @brief add description.
  * @return None
  */
int32_t HAL_PMU_InterruptEnable(void* res);

/**
  * @brief add description.
  * @return None
  */
int32_t HAL_PMU_InterruptDisable(void* res);


/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  * refer to
  * RESET_CAUSE_AP_RESET                (1<<16)
  * RESET_CAUSE_WDT_RESET               (1<<17)
  * RESET_CAUSE_LOCKUP_RESET            (1<<18)
  */
int32_t HAL_PMU_GetSysResetCause(void* res);

/**
  * @brief add description.
  * @note add description
  * @retval int32_t
  * refer to
#define WAKUP_CAUSE_RTC                     (1<<30)
#define WAKUP_CAUSE_BT_TIMER                (1<<29)
#define WAKUP_CAUSE_GPIOB7                  (1<<28)
#define WAKUP_CAUSE_GPIOB6                  (1<<27)
#define WAKUP_CAUSE_GPIOB5                  (1<<26)
#define WAKUP_CAUSE_GPIOB4                  (1<<25)
#define WAKUP_CAUSE_GPIOB3                  (1<<24)
#define WAKUP_CAUSE_GPIOB2                  (1<<23)
#define WAKUP_CAUSE_GPIOB1                  (1<<22)
#define WAKUP_CAUSE_GPIOB0                  (1<<21)
#define WAKUP_CAUSE_TIMER                   (1<<20)
#define WAKUP_CAUSE_KEY1                    (1<<19)
#define WAKUP_CAUSE_KEY0                    (1<<18)
#define WAKUP_CAUSE_IWDT                    (1<<17)
  */
int32_t HAL_PMU_GetWakeCause(void* res);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_ClearWakeCause(void);


/**
  * @brief add description.
  * @return None
  */
int32_t HAL_PMU_RegisterCallback(void *res, CSK_PMU_SignalEvent_t cb_event);


/**
  * @brief PMU wake up source select
  * @param[in]  CSK_PMU_WAKE_SELECT_GPIOB_6
  *        CSK_PMU_WAKE_SELECT_GPIOB_5
  *        CSK_PMU_WAKE_SELECT_GPIOB_4
  *        CSK_PMU_WAKE_SELECT_GPIOB_3
  *        CSK_PMU_WAKE_SELECT_GPIOB_2
  *        CSK_PMU_WAKE_SELECT_GPIOB_1
  *        CSK_PMU_WAKE_SELECT_GPIOB_0
  *        CSK_PMU_WAKE_SELECT_RTC
  *        CSK_PMU_WAKE_SELECT_TIMER
  *        CSK_PMU_WAKE_SELECT_KEY1
  *        CSK_PMU_WAKE_SELECT_KEY0
  *        CSK_PMU_WAKE_SELECT_IWDT
  *        CSK_PMU_WAKE_SELECT_BT_TIMER
  * @return None
  */
void HAL_PMU_WakeUp_Source_Select(void *res, uint32_t source);


/**
  * @brief PMU wake up source deselect
  * @param[in]  CSK_PMU_WAKE_SELECT_GPIOB_6
  *        CSK_PMU_WAKE_SELECT_GPIOB_5
  *        CSK_PMU_WAKE_SELECT_GPIOB_4
  *        CSK_PMU_WAKE_SELECT_GPIOB_3
  *        CSK_PMU_WAKE_SELECT_GPIOB_2
  *        CSK_PMU_WAKE_SELECT_GPIOB_1
  *        CSK_PMU_WAKE_SELECT_GPIOB_0
  *        CSK_PMU_WAKE_SELECT_RTC
  *        CSK_PMU_WAKE_SELECT_TIMER
  *        CSK_PMU_WAKE_SELECT_KEY1
  *        CSK_PMU_WAKE_SELECT_KEY0
  *        CSK_PMU_WAKE_SELECT_IWDT
  *        CSK_PMU_WAKE_SELECT_BT_TIMER
  * @return None
  */
void HAL_PMU_WakeUp_Source_DeSelect(void *res, uint32_t source);


/**
  * @brief PMU wake up polarity select
  * @param[in]  CSK_PMU_WAKE_GPIOB_0_POLARITY_HIGH
  *        CSK_PMU_WAKE_GPIOB_0_POLARITY_LOW
  *        CSK_PMU_WAKE_GPIOB_1_POLARITY_HIGH
  *        CSK_PMU_WAKE_GPIOB_1_POLARITY_LOW
  *        CSK_PMU_WAKE_GPIOB_2_POLARITY_HIGH
  *        CSK_PMU_WAKE_GPIOB_2_POLARITY_LOW
  *        CSK_PMU_WAKE_GPIOB_3_POLARITY_HIGH
  *        CSK_PMU_WAKE_GPIOB_3_POLARITY_LOW
  *        CSK_PMU_WAKE_GPIOB_4_POLARITY_HIGH
  *        CSK_PMU_WAKE_GPIOB_4_POLARITY_LOW
  *        CSK_PMU_WAKE_GPIOB_5_POLARITY_HIGH
  *        CSK_PMU_WAKE_GPIOB_5_POLARITY_LOW
  * @return None
  */
void HAL_PMU_WakeUp_Polarity_Select(void *res, uint32_t source);


/**
  * @brief PMU wake up polarity reset
  * @return None
  */
void HAL_PMU_WakeUp_Polarity_Reset(void *res);


/* WakeUp pins configuration */
/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_EnableWakeUpPin(void *res,uint32_t WakeUpPinx);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_DisableWakeUpPin(void *res,uint32_t WakeUpPinx);


/* Low Power modes entry */
/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_EnterHoldMode(void* res, uint8_t SLEEPEntry);

/* Pmu enter sleep preconfig */
/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_PreConfigSleepTrigger(pmu_sleep_trigger_t sleepTrigger);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_EnterDeepSleepMode(pmu_sleepmode_t SleepMode, uint8_t SLEEPEntry);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_EnableSleepOnExit(void);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_DisableSleepOnExit(void);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_EnableSEVOnPend(void);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_DisableSEVOnPend(void);


/**
  * @brief add description.
  * @return None
  */
void HAL_PMU_EnterDeepSleepMode_WithCoreReg(void* res);


/**
  * @brief JPG module reset signal generated.
  * @return None
  */
void HAL_PMU_Jpg_Reset_Enable(void);


/**
  * @brief TRNG module reset signal generated.
  * @return None
  */
void HAL_PMU_Trng_Reset_Enable(void);


/**
  * @brief QDEC module reset signal generated.
  * @return None
  */
void HAL_PMU_Qdec_Reset_Enable(void);


/**
  * @brief BT module reset signal generated.
  * @return None
  */
void HAL_PMU_Bt_Reset_Enable(void);


/**
  * @brief WIFI module reset signal generated.
  * @return None
  */
void HAL_PMU_Wf_Reset_Enable(void);

/**
  * @brief SDIOD module reset signal generated.
  * @return None
  */
void HAL_PMU_Sdiod_Reset_Enable(void);


/**
  * @brief SDIOH module reset signal generated.
  * @return None
  */
void HAL_PMU_Sdioh_Reset_Enable(void);


/**
  * @brief VIC module reset signal generated.
  * @return None
  */
void HAL_PMU_Vic_Reset_Enable(void);


/**
  * @brief GPADC module reset signal generated.
  * @return None
  */
void HAL_PMU_Gpadc_Reset_Enable(void);


/**
  * @brief CALENDAR module reset signal generated.
  * @return None
  */
void HAL_PMU_Calendar_Reset_Enable(void);


/**
  * @brief AP_DMA module reset signal generated.
  * @return None
  */
void HAL_PMU_Ap_Dma_Reset_Enable(void);


/**
  * @brief FLASH_CTRL module reset signal generated.
  * @return None
  */
void HAL_PMU_Flash_Ctrl_Reset_Enable(void);


/**
  * @brief PSRAM_CTRL module reset signal generated.
  * @return None
  */
void HAL_PMU_Psram_Ctrl_Reset_Enable(void);


/**
  * @brief CODEC module reset signal generated.
  * @return None
  */
void HAL_PMU_Codec_Reset_Enable(void);


/**
  * @brief APC module reset signal generated.
  * @return None
  */
void HAL_PMU_Apc_Reset_Enable(void);


/**
  * @brief GPIOA module reset signal generated.
  * @return None
  */
void HAL_PMU_Gpioa_Reset_Enable(void);


/**
  * @brief GPIOB module reset signal generated.
  * @return None
  */
void HAL_PMU_Gpiob_Reset_Enable(void);


/**
  * @brief CRYPTO module reset signal generated.
  * @return None
  */
void HAL_PMU_Crypto_Reset_Enable(void);


/**
  * @brief GPT module reset signal generated.
  * @return None
  */
void HAL_PMU_Gpt_Reset_Enable(void);


/**
  * @brief USBC module reset signal generated.
  * @return None
  */
void HAL_PMU_Usbc_Reset_Enable(void);


/**
  * @brief IR module reset signal generated.
  * @return None
  */
void HAL_PMU_Ir_Reset_Enable(void);


/**
  * @brief I2C1 module reset signal generated.
  * @return None
  */
void HAL_PMU_I2c1_Reset_Enable(void);


/**
  * @brief I2C0 module reset signal generated.
  * @return None
  */
void HAL_PMU_I2c0_Reset_Enable(void);


/**
  * @brief SPI2 module reset signal generated.
  * @return None
  */
void HAL_PMU_Spi2_Reset_Enable(void);


/**
  * @brief SPI1 module reset signal generated.
  * @return None
  */
void HAL_PMU_Spi1_Reset_Enable(void);


/**
  * @brief SPI0 module reset signal generated.
  * @return None
  */
void HAL_PMU_Spi0_Reset_Enable(void);


/**
  * @brief UART2 module reset signal generated.
  * @return None
  */
void HAL_PMU_Uart2_Reset_Enable(void);


/**
  * @brief UART1 module reset signal generated.
  * @return None
  */
void HAL_PMU_Uart1_Reset_Enable(void);


/**
  * @brief UART0 module reset signal generated.
  * @return None
  */
void HAL_PMU_Uart0_Reset_Enable(void);



/**
  * @brief  core soft reset signal generated.
  * @return None
  */
void HAL_PMU_Core_Software_Reset_Enable(void);


/**
  * @brief whole chip soft reset signal generated.
  * @return None
  */
void HAL_PMU_Chip_Software_Reset_Enable(void);


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

#ifdef __cplusplus
}
#endif

#endif /* __CSK_DRIVER_PMU_H */
