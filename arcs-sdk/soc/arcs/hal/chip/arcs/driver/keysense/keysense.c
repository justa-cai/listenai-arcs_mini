/**
  ******************************************************************************
  * @file    Driver_KEYSENSE.h
  * @author  ListenAI Application Team
  * @brief   Header file of KEYSENSE HAL module.
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
#include "Driver_KEYSENSE.h"
#include "arcs_ap.h"

/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @defgroup KEYSENSE KEYSENSE
  * @brief KEYSENSE HAL module driver
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
// KEYSENSE flags
#define KEYSENSE_FLAG_INITIALIZED          (1U << 0)
#define KEYSENSE_FLAG_POWERED              (1U << 1)
#define KEYSENSE_FLAG_CONFIGURED           (1U << 2)

typedef struct _KEYSENSE_INFO
{
	uint32_t flags;         		   	// KEYSENSE driver flags
	CSK_KEYSENSE_SignalEvent_t cb_event[CSK_KEYSENSE_MODE_NUM]; 	// Event callback
	uint32_t clk_source[2];
	void (*origin_IRQ_handler)(void);
} KEYSENSE_INFO;


// KEYSENSE Resource Configuration
typedef const struct
{
    KEYSENSE_RegDef* reg;                	  // KEYSENSE register interface
    uint32_t irq_num;
    void (*irq_handler)(void);
    KEYSENSE_INFO* info;               	  // Run-Time control information
    uint32_t user_param;
}KEYSENSE_RESOURCES;


/* Private define ------------------------------------------------------------*/
#define CSK_GPADC_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

#define CHECK_RESOURCES(res)  do{\
        if((res != &keysense0_resources) && (res != &keysense1_resources)){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)


/* Private function prototypes -----------------------------------------------*/
static void KEYSENSE0_IRQ_Handler(void);
static void KEYSENSE1_IRQ_Handler(void);


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static const CSK_DRIVER_VERSION keysense_driver_version = {
        CSK_KEYSENSE_API_VERSION,
        CSK_GPADC_DRV_VERSION
};

KEYSENSE_INFO info_keysense0 = {
        0,
        {(void*)0},
		{0},
		(void*)0,
};


KEYSENSE_INFO info_keysense1 = {
        0,
        {(void*)0},
        {0},
        (void*)0,
};

static const KEYSENSE_RESOURCES keysense0_resources = {
		IP_KEYSENSE0,
		IRQ_AON_KS0_VECTOR,
		KEYSENSE0_IRQ_Handler,
        &info_keysense0,
};
static const KEYSENSE_RESOURCES keysense1_resources = {
		IP_KEYSENSE1,
		IRQ_AON_KS1_VECTOR,
		KEYSENSE1_IRQ_Handler,
        &info_keysense1,
};

/* Private functions ---------------------------------------------------------*/
static void
kensense_irq_handler(KEYSENSE_RESOURCES* hkeysense){

	uint32_t intflag, modeflag;

	intflag = hkeysense->reg->REG_KS_ISR.all;

	for(uint32_t cnt = 0; cnt < CSK_KEYSENSE_MODE_NUM; cnt++){
		modeflag = intflag & (0x01<<cnt);
		if(modeflag){
			hkeysense->reg->REG_KS_ICR.all = modeflag;
			hkeysense->info->cb_event[cnt]((void*)hkeysense->user_param);
		}
	}
}


static void
KEYSENSE0_IRQ_Handler(void)
{
	kensense_irq_handler(KEYSENSE0());
}

static void
KEYSENSE1_IRQ_Handler(void)
{
    kensense_irq_handler(KEYSENSE1());
}

/** @defgroup KEYSENSE_Exported_Functions KEYSENSE Exported Functions
  * @{
  */

/** @defgroup KEYSENSE_Exported_Functions_Group1 Initialization and de-initialization functions
  *  @brief    Initialization and de-initialization functions
  *
@verbatim
 ===============================================================================
              ##### Initialization and de-initialization functions #####
 ===============================================================================
    [..]
		KEYSENSE initialize functions

@endverbatim
  * @{
  */

/**
  * @brief Get keysense resource structure.
  *
  * This function returns a pointer to the keysense resource structure.
  *
  * @note This function is typically used to retrieve the necessary resources
  *       for keysense operations.
  *
  * @retval void* Pointer to keysense resource structure.
  */
void* KEYSENSE0(void){
    return (void*)&keysense0_resources;
}

void* KEYSENSE1(void){
    return (void*)&keysense1_resources;
}
/**
  * @brief Get the keysense driver version.
  *
  * This function returns the version of the keysense driver.
  *
  * @note The returned version can be used to verify compatibility or
  *       diagnose issues with the keysense driver.
  *
  * @retval CSK_DRIVER_VERSION Version of the keysense driver.
  */
CSK_DRIVER_VERSION
KEYSENSE_GetVersion(void){
    return keysense_driver_version;
}

/**
  * @brief Initialize the keysense hardware.
  *
  * This function initializes the keysense hardware with the provided resources.
  * It sets up the internal registers, enables the keysense IRQ and configures
  * the necessary hardware parameters.
  *
  * @param res Pointer to keysense resources.
  *
  * @note Ensure that the resources provided are valid and compatible with the
  *       keysense hardware.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful, error code otherwise.
  */
int32_t
HAL_KEYSENSE_Initialize(void *res){

    CHECK_RESOURCES(res);

	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;

	pKeysense->info->flags |= KEYSENSE_FLAG_POWERED;

	pKeysense->reg->REG_KS_IMR.all = 0xF;
	pKeysense->reg->REG_KS_ICR.all = 0xF;

	//Todo:Keysense clock enable
	//Todo:Add keysense reset
    register_ISR(pKeysense->irq_num, pKeysense->irq_handler, NULL);

    enable_IRQ(pKeysense->irq_num);

    return CSK_DRIVER_OK;
}

/**
  * @brief Uninitialize the keysense hardware.
  *
  * This function uninitializes the keysense hardware, disables the IRQ and
  * clears any hardware configurations.
  *
  * @param res Pointer to keysense resources.
  *
  * @note This function should be called to safely turn off or reset the
  *       keysense hardware.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful, error code otherwise.
  */
int32_t
HAL_KEYSENSE_Uninitialize(void *res){
    CHECK_RESOURCES(res);

	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;

	//Todo: add keysense reset

    disable_IRQ(pKeysense->irq_num);

    register_ISR(pKeysense->irq_num, NULL, NULL);

    return CSK_DRIVER_OK;
}


/**
  * @brief Control keysense hardware settings.
  *
  * This function configures the keysense hardware based on the specified control parameters.
  * It sets the ADC trigger threshold and the wakeup count threshold.
  *
  * @param res Pointer to keysense resources.
  * @param control Control flags combined using bitwise OR. These flags determine the
  *        settings to be applied to the keysense hardware.
  *        - CSK_KEYSENSE_ADC_TRIGGER_THD_Msk: Mask for ADC trigger threshold.
  *        - CSK_KEYSENSE_ADC_TRIGGER_THD_Pos: Position for ADC trigger threshold.
  *        - CSK_KEYSENSE_WAKEUP_CNT_THD_Msk: Mask for wakeup count threshold.
  *        - CSK_KEYSENSE_WAKEUP_CNT_THD_Pos: Position for wakeup count threshold.
  *
  * @note The keysense hardware must be powered on (KEYSENSE_FLAG_POWERED flag set) before
  *       calling this function.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful, error code otherwise.
  */
int32_t
HAL_KEYSENSE_Control(void* res, uint32_t control){
    CHECK_RESOURCES(res);

	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;
	uint32_t tmp;

    if ((pKeysense->info->flags & KEYSENSE_FLAG_POWERED) == 0U) {
        // UART not powered
        return CSK_DRIVER_ERROR;
    }

    tmp = (control & CSK_KEYSENSE_ADC_TRIGGER_THD_Msk) >> CSK_KEYSENSE_ADC_TRIGGER_THD_Pos;
    pKeysense->reg->REG_KS_THD.bit.KS_THD_ADC_TRIG = tmp;

    tmp = (control & CSK_KEYSENSE_WAKEUP_CNT_THD_Msk) >> CSK_KEYSENSE_WAKEUP_CNT_THD_Pos;
    pKeysense->reg->REG_KS_THD.bit.KS_THD_WAKEUP = tmp;

    return CSK_DRIVER_OK;
}

/**
  * @}
  */


/** @defgroup KEYSENSE_Exported_Functions_Group2 control functions
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
  * @brief Enable specific interrupts for keysense.
  *
  * This function enables the interrupts for keysense based on the specified interrupt mode.
  *
  * @param res Pointer to keysense resources.
  * @param intmode Interrupt mode type defined in KEYSENSE_INTERRUPT_MODE_TYPE.
  *
  * @note Ensure that the keysense resources are valid before calling this function.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful.
  */
int32_t
HAL_KEYSENSE_InterruptEnable(void* res, KEYSENSE_INTERRUPT_MODE_TYPE intmode){

    CHECK_RESOURCES(res);
	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;

	pKeysense->reg->REG_KS_IMR.all &= ~(intmode&0x0F);

    return CSK_DRIVER_OK;
}

/**
  * @brief Disable specific interrupts for keysense.
  *
  * This function disables the interrupts for keysense based on the specified interrupt mode.
  *
  * @param res Pointer to keysense resources.
  * @param intmode Interrupt mode type defined in KEYSENSE_INTERRUPT_MODE_TYPE.
  *
  * @note Ensure that the keysense resources are valid before calling this function.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful.
  */
int32_t
HAL_KEYSENSE_InterruptDisable(void* res, KEYSENSE_INTERRUPT_MODE_TYPE intmode){
    CHECK_RESOURCES(res);
	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;

	pKeysense->reg->REG_KS_IMR.all |= (intmode&0x0F);

    return CSK_DRIVER_OK;
}

/**
  * @brief Enable the keysense hardware.
  *
  * This function enables the keysense hardware by setting the corresponding configuration bit.
  *
  * @param res Pointer to keysense resources.
  *
  * @note Ensure that the keysense resources are valid before calling this function.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful.
  */
int32_t
HAL_KEYSENSE_Enable(void* res){

    CHECK_RESOURCES(res);
	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;

	pKeysense->reg->REG_KS_CFG.bit.KS_EN = 1;

    return CSK_DRIVER_OK;
}

/**
  * @brief Disable the keysense hardware.
  *
  * This function disables the keysense hardware by resetting the corresponding configuration bit.
  *
  * @param res Pointer to keysense resources.
  *
  * @note Ensure that the keysense resources are valid before calling this function.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful.
  */
int32_t
HAL_KEYSENSE_Disable(void* res){

    CHECK_RESOURCES(res);
	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;

	pKeysense->reg->REG_KS_CFG.bit.KS_EN = 0;

    return CSK_DRIVER_OK;
}

/**
  * @brief Register a callback function for keysense events.
  *
  * This function registers a user-defined callback function that will be called
  * when keysense events occur. The callback is associated with a specific keysense mode.
  *
  * @param res Pointer to keysense resources.
  * @param mode The keysense mode for which the callback is registered, defined in KEYSENSE_MODE_TYPE.
  * @param cb_event The callback function of type CSK_KEYSENSE_SignalEvent_t to be called on keysense events.
  *
  * @note Ensure that the keysense resources are valid and that the mode is supported by the hardware.
  *
  * @retval int32_t Returns CSK_DRIVER_OK if successful, error code otherwise.
  */
int32_t
HAL_KEYSENSE_RegisterCallback(void *res, KEYSENSE_MODE_TYPE mode, CSK_KEYSENSE_SignalEvent_t cb_event){

    CHECK_RESOURCES(res);
	KEYSENSE_RESOURCES * pKeysense = (KEYSENSE_RESOURCES *)res;

	pKeysense->info->cb_event[mode] = cb_event;

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





