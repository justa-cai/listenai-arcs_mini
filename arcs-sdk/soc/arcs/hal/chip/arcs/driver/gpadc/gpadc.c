/**
  ******************************************************************************
  * @file    gpadc_venus.c
  * @author  ListenAI Application Team
  * @brief   GPADC HAL module driver.
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
#include "gpadc.h"
#include "ClockManager.h"
#include "PowerManager.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define CSK_GPADC_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

#define CHECK_RESOURCES(res)  do{\
        if(res != &gpadc_resources){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)


/* Private function prototypes -----------------------------------------------*/
static void GPADC0_IRQ_Handler(void);

/* Private macro -------------------------------------------------------------*/


/* Private variables ---------------------------------------------------------*/
static const CSK_DRIVER_VERSION gpadc_driver_version = {
        CSK_GPADC_API_VERSION,
        CSK_GPADC_DRV_VERSION
};

GPADC_INFO info_gpadc = {
        0,
        {(void*)0},
		(void*)0,
		{0},
		(void*)0,
};

static GPADC_RESOURCES gpadc_resources = {
        IP_GPADC,
		IRQ_GPADC_VECTOR,
		GPADC0_IRQ_Handler,
        &info_gpadc,
};

/* Private functions ---------------------------------------------------------*/
static void
gpadc_irq_handler(GPADC_RESOURCES* hgpadc){

	uint32_t intflag, intflag2, channelflag, cmpflag;

	intflag = hgpadc->reg->REG_ADC_ISR0.all;
	intflag2 = hgpadc->reg->REG_ADC_ISR1.all;
	for(uint32_t cnt = 0; cnt < CSK_GPADC_CHANNEL_NUM; cnt++){
		channelflag = intflag & ((0x01<<cnt)<<CSK_ADC_ISR_FIFO_EMPTY_Pos);
		hgpadc->user_param = channelflag;
		if(channelflag){
			hgpadc->reg->REG_ADC_IRSR0.all = channelflag;
			hgpadc->info->cb_event[cnt](CSK_GPADC_FIFO_EMPTY, (void*)hgpadc->user_param);
		}

		channelflag = intflag2 & ((0x01<<cnt)<<CSK_ADC_ISR_FIFO_FULL_Pos);
		hgpadc->user_param = channelflag;
		if(channelflag){
			hgpadc->reg->REG_ADC_IRSR1.all = channelflag;
			hgpadc->info->cb_event[cnt](CSK_GPADC_FIFO_FULL, (void*)hgpadc->user_param);
		}

		channelflag = intflag2 & ((0x01<<cnt)<<CSK_ADC_ISR_FIFO_THD_Pos);
		hgpadc->user_param = channelflag;
		if(channelflag){
			hgpadc->reg->REG_ADC_IRSR1.all = channelflag;
			hgpadc->info->cb_event[cnt](CSK_GPADC_FIFO_THD, (void*)hgpadc->user_param);
		}
	}

	cmpflag = intflag & (0x01<<CSK_ADC_ISR_COMPLETE_Pos);
	if(cmpflag){
		hgpadc->reg->REG_ADC_IRSR0.all = cmpflag;
		while(hgpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR == 1){
			hgpadc->reg->REG_ADC_IRSR0.all = cmpflag;
		}

		hgpadc->user_param = hgpadc->reg->REG_ADC_CH_SEL.all & 0xFFFF;
		if(hgpadc->reg->REG_ADC_IRSR0.bit.EOC_ERR_IRSR == 1){
			hgpadc->reg->REG_ADC_IRSR0.bit.EOC_ERR_IRSR = 1;
			while(hgpadc->reg->REG_ADC_IRSR0.bit.EOC_ERR_IRSR == 1){
				hgpadc->reg->REG_ADC_IRSR0.bit.EOC_ERR_IRSR = 1;
			}
			hgpadc->info->cmp_event(CSK_GPADC_EOC_ERROR, (void*)hgpadc->user_param);
		}else{
			hgpadc->info->cmp_event(CSK_GPADC_COMPLETE, (void*)hgpadc->user_param);
		}

	}
}

static void
GPADC0_IRQ_Handler(void)
{
	gpadc_irq_handler(GPADC());
}

void* GPADC(void){
    return (void*)&gpadc_resources;
}


/**
 * @brief Retrieves the version of the GPADC driver.
 *
 * This function returns the current version of the GPADC driver being used.
 *
 * @return The version of the GPADC driver.
 */
CSK_DRIVER_VERSION
HAL_GPADC_GetVersion(void){
	// Return the global variable that holds the GPADC driver version
    return gpadc_driver_version;
}

/**
 * @brief Initializes the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function initializes the GPADC by enabling its clock, resetting it, setting up
 * necessary registers, and configuring interrupts. It also registers an interrupt service
 * routine (ISR) for handling GPADC interrupts.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 *
 * @return Returns CSK_DRIVER_OK if initialization is successful.
 */
int32_t HAL_GPADC_Initialize(void *res) {
    // Check if the resources are valid
    CHECK_RESOURCES(res);

    // Cast the input resource pointer to GPADC_RESOURCES type
    GPADC_RESOURCES *pGpadc = (GPADC_RESOURCES *)res;

    // Enable the GPADC clock
    __HAL_CRM_GPADC_CLK_ENABLE();

    // Reset the GPADC
    __HAL_PMU_GPADC_RST_ENABLE();

    // Set the powered flag in the GPADC info structure
    pGpadc->info->flags |= GPADC_FLAG_POWERED;

    // Clear hard trigger and ADC enable bits in REG_ADC_CTRL1 register
    pGpadc->reg->REG_ADC_CTRL1.bit.HARD_TRIG = 1;
    pGpadc->reg->REG_ADC_CTRL1.bit.ADC_EN = 1;

    // Clear all interrupt status registers
    pGpadc->reg->REG_ADC_IRSR0.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IRSR1.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IRSR2.all = 0xFFFFFFFF;

    // Mask all interrupts by setting all bits in the interrupt mask registers
    pGpadc->reg->REG_ADC_IMR0.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IMR1.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IMR2.all = 0xFFFFFFFF;

    // Register the ISR for handling GPADC interrupts
    register_ISR(pGpadc->irq_num, pGpadc->irq_handler, NULL);

    // Enable the GPADC interrupt
    enable_IRQ(pGpadc->irq_num);

    // The ARCS_D GPADC will be enabled automatically by a soft trigger

    // Return success status code
    return CSK_DRIVER_OK;
}

/**
 * @brief Uninitializes the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function uninitializes the GPADC by disabling its clock, resetting it, clearing
 * necessary registers, and configuring interrupts. It also unregisters the interrupt service
 * routine (ISR) for handling GPADC interrupts.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 *
 * @return Returns CSK_DRIVER_OK if uninitialization is successful.
 */
int32_t HAL_GPADC_Uninitialize(void *res) {
    // Check if the resources are valid
    CHECK_RESOURCES(res);

    // Cast the input resource pointer to GPADC_RESOURCES type
    GPADC_RESOURCES *pGpadc = (GPADC_RESOURCES *)res;

    // Reset the GPADC
    __HAL_PMU_GPADC_RST_ENABLE();

    // Mask all interrupts by setting all bits in the interrupt mask registers
    pGpadc->reg->REG_ADC_IMR0.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IMR1.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IMR2.all = 0xFFFFFFFF;

    // Clear all interrupt status registers
    pGpadc->reg->REG_ADC_IRSR0.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IRSR1.all = 0xFFFFFFFF;
    pGpadc->reg->REG_ADC_IRSR2.all = 0xFFFFFFFF;

    // Disable the GPADC interrupt
    disable_IRQ(pGpadc->irq_num);

    // Unregister the ISR for handling GPADC interrupts
    register_ISR(pGpadc->irq_num, NULL, NULL);

    // Clear hard trigger and ADC enable bits in REG_ADC_CTRL1 register
    pGpadc->reg->REG_ADC_CTRL1.bit.HARD_TRIG = 1;
    pGpadc->reg->REG_ADC_CTRL1.bit.ADC_EN = 1;

    // Return success status code
    return CSK_DRIVER_OK;
}

/**
 * @brief Controls the GPADC settings based on the provided control flags.
 *
 * This function configures the GPADC channel selection and DMA channel enablement
 * based on the input control flags. It checks if the GPADC is powered before making
 * any changes.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] control Control flags for configuring GPADC settings.
 *
 * @return Returns the status code of the operation. If successful, returns CSK_DRIVER_OK.
 */
uint32_t HAL_GPADC_Control(void* res, uint32_t control) {
    // Check if the resources are valid
    CHECK_RESOURCES(res);

    // Cast the input resource pointer to GPADC_RESOURCES type
    GPADC_RESOURCES *pGpadc = (GPADC_RESOURCES *)res;
    uint32_t tmp;

    // Check if the GPADC is powered
    if (!(pGpadc->info->flags & GPADC_FLAG_POWERED)) {
        // GPADC not powered
        return CSK_DRIVER_ERROR;
    }

    // Channel select: extract the channel selection bits from the control flags
    tmp = (control & CSK_GPADC_CHANNEL_SEL_Msk) >> CSK_GPADC_CHANNEL_SEL_Pos;
    pGpadc->reg->REG_ADC_CH_SEL.bit.ADC_CH_SEL = tmp;

    // DMA channel enable: extract the DMA enable bit from the control flags
    tmp = (control & CSK_GPADC_DMA_ENABLE_Msk) >> CSK_GPADC_DMA_ENABLE_Pos;
    pGpadc->reg->REG_ADC_CH_SEL.bit.DMA_CH_EN = tmp;

    // Return success status code
    return CSK_DRIVER_OK;
}


/**
 * @brief Starts the GPADC (General Purpose Analog-to-Digital Converter) for a software-triggered ADC conversion.
 *
 * This function initiates an ADC conversion in single conversion mode (W1P) using a software trigger.
 * The conversion is triggered by setting the SOFT_TRIG bit.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 *
 * @return Returns the status code of the operation. If successful, returns CSK_DRIVER_OK.
 */
uint32_t HAL_GPADC_Start(void* res)
{
    // Check if the resources are valid
    CHECK_RESOURCES(res);

    // Cast the input resource pointer to GPADC_RESOURCES type
    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res;

    // Start software ADC conversion, W1P (Single Conversion Mode)
    pGpadc->reg->REG_ADC_CTRL0.bit.SOFT_TRIG = 1;

    // Return the success status code
    return CSK_DRIVER_OK;
}

/**
 * @brief Stops the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function stops the GPADC operation by disabling the ADC and clearing any pending interrupt status.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 *
 * @return Returns CSK_DRIVER_OK if the stop operation is successful.
 */
uint32_t HAL_GPADC_Stop(void* res)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // If not complete, clear status manually
    if(pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR == 1){
        pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR = 1; // Clear the ADC complete interrupt status
    }
    pGpadc->reg->REG_ADC_CTRL1.bit.ADC_EN = 0; // Disable the ADC

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Polls for GPADC conversion completion.
 *
 * This function waits for the GPADC conversion to complete by polling the relevant status registers.
 * If DMA channel is not enabled, it will wait until the ADC_COMPLETE_IRSR flag is set.
 * It also handles trigger conditions if GPT or KS triggers are enabled.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] Timeout Timeout value (not used in this implementation).
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_PollForConversion(void* res, uint32_t Timeout)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // If DMA channel is not enabled, wait for ADC conversion to complete
    if(pGpadc->reg->REG_ADC_CH_SEL.bit.DMA_CH_EN == 0){
        while(pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR == 0); // Wait until ADC_COMPLETE_IRSR flag is set
        pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR = 1; // Clear the ADC_COMPLETE_IRSR flag
    }
    // If GPT or KS triggers are enabled, set the HARD_TRIG bit
    if((pGpadc->reg->REG_ADC_CTRL0.bit.GPT_TRIG_EN == 1) || (pGpadc->reg->REG_ADC_CTRL0.bit.KS_TRIG_EN == 1)){
        pGpadc->reg->REG_ADC_CTRL1.bit.HARD_TRIG = 1; // Set the HARD_TRIG bit to start conversion
    }
    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Starts the General Purpose Analog-to-Digital Converter (GPADC) with interrupt.
 *
 * This function starts the GPADC operation by enabling the interrupt mask for the completion event,
 * clearing any pending interrupt status, and then triggering a software conversion.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 *
 * @return Returns CSK_DRIVER_OK if the start operation is successful.
 */
uint32_t HAL_GPADC_Start_IT(void* res)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Mask complete interrupt
    pGpadc->reg->REG_ADC_IMR0.bit.ADC_COMPLETE_IMR = 1;

    // If not complete, clear status manually
    if(pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR == 1){
        pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR = 1; // Clear the ADC complete interrupt status
    }

    // Unmask complete interrupt
    pGpadc->reg->REG_ADC_IMR0.bit.ADC_COMPLETE_IMR = 0;

    // Software ADC conversion
    pGpadc->reg->REG_ADC_CTRL0.bit.SOFT_TRIG = 1; // Trigger a software ADC conversion

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Stops the General Purpose Analog-to-Digital Converter (GPADC) with interrupt.
 *
 * This function stops the GPADC operation by masking the completion interrupt,
 * clearing any pending interrupt status if necessary, and disabling the ADC.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 *
 * @return Returns CSK_DRIVER_OK if the stop operation is successful.
 */
uint32_t HAL_GPADC_Stop_IT(void* res)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Mask complete interrupt
    pGpadc->reg->REG_ADC_IMR0.bit.ADC_COMPLETE_IMR = 1;

    // If not complete, clear status manually
    if(pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR == 1){
        pGpadc->reg->REG_ADC_IRSR0.bit.ADC_COMPLETE_IRSR = 1; // Clear the ADC complete interrupt status
    }
    pGpadc->reg->REG_ADC_CTRL1.bit.ADC_EN = 0; // Disable the ADC

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Enables or disables the GPT trigger for the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the GPT trigger enable bit in the GPADC control register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] enable Flag indicating whether to enable (1) or disable (0) the GPT trigger.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_SetGptTrigger_enable(void* res, uint8_t enable)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    pGpadc->reg->REG_ADC_CTRL0.bit.GPT_TRIG_EN = enable; // Set the GPT trigger enable bit

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Enables or disables the Keysense trigger for the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the Keysense trigger enable bit in the GPADC control register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] enable Flag indicating whether to enable (1) or disable (0) the Keysense trigger.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_SetKeysenseTrigger_enable(void* res, uint8_t enable)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    pGpadc->reg->REG_ADC_CTRL0.bit.KS_TRIG_EN = enable; // Set the Keysense trigger enable bit

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Sets the trigger number for the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the trigger number in the GPADC control register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] trignum The trigger number to be set.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_SetTriggerNum(void* res, uint32_t trignum)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    pGpadc->reg->REG_ADC_CTRL0.bit.ADC_TRIG_NUM = trignum; // Set the trigger number in the GPADC control register

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Sets the sample wait time for the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the sample wait time in the GPADC control register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] waitTime The sample wait time to be set.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_SetSampleWaitTime(void* res, uint32_t waitTime)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Set the sample wait time in the GPADC control register
    pGpadc->reg->REG_ADC_CTRL0.bit.SAMPLE_TIME = waitTime;

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Sets the setup wait time for the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the setup wait time in the GPADC control register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] waitTime The setup wait time to be set.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_SetSetupWaitTime(void* res, uint8_t waitTime)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Set the setup wait time in the GPADC control register
    pGpadc->reg->REG_ADC_CTRL0.bit.ADC_SETUP_WAIT = waitTime;

    return CSK_DRIVER_OK; // Return success status code
}


/**
 * @brief Sets the Vref selection for the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the Vref selection in the GPADC configuration register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] vrefsel The Vref selection value to be set.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_SetVrefSel(void* res, uint8_t vrefsel)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Set the Vref select setting in the GPADC configuration register
    pGpadc->reg->REG_ADC_CONFIG.bit.VREF_SEL = vrefsel;

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Enables or disables the input buffer for the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the input buffer enable setting in the GPADC configuration register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] enable The enable value to be set (1 to enable, 0 to disable).
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful.
 */
uint32_t HAL_GPADC_SetVinBuf_Enable(void* res, uint8_t enable)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Set the input buffer enable setting in the GPADC configuration register
    pGpadc->reg->REG_ADC_CTRL1.bit.VIN_BUF_CFG = enable;

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Sets the FIFO threshold for a specific channel of the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the FIFO threshold for a specified channel in the GPADC. The channel number must be less than 8,
 * and the threshold value is shifted left by the channel number multiplied by 2 bits to set the appropriate bits
 * in the FIFO threshold register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] channelnum The channel number for which to set the FIFO threshold (must be less than 8).
 * @param[in] threshold The FIFO threshold value to be set.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful, or CSK_DRIVER_ERROR_PARAMETER if the channel number is invalid.
 */
uint32_t HAL_GPADC_SetFifoThd0(void* res, uint8_t channelnum, uint8_t threshold)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    if(channelnum < 8){
        // Set the FIFO threshold for the specified channel
        pGpadc->reg->REG_ADC_FIFO_THD0.all |= threshold << (channelnum<<2);
    }
    else{
        // Return an error if the channel number is invalid
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Sets the FIFO threshold for a specific channel of the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function sets the FIFO threshold for a specified channel in the GPADC. The channel number must be less than 8,
 * and the threshold value is shifted left by the channel number multiplied by 2 bits to set the appropriate bits
 * in the FIFO threshold register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] channelnum The channel number for which to set the FIFO threshold (must be less than 8).
 * @param[in] threshold The FIFO threshold value to be set.
 *
 * @return Returns CSK_DRIVER_OK if the operation is successful, or CSK_DRIVER_ERROR_PARAMETER if the channel number is invalid.
 */
uint32_t HAL_GPADC_SetFifoThd1(void* res, uint8_t channelnum, uint8_t threshold)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    if(channelnum < 8){
        // Set the FIFO threshold for the specified channel
        pGpadc->reg->REG_ADC_FIFO_THD1.all |= threshold << (channelnum<<2);
    }
    else{
        // Return an error if the channel number is invalid
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    return CSK_DRIVER_OK; // Return success status code
}

/**
  * @brief Enables the FIFO interrupt for a specific channel of the General Purpose Analog-to-Digital Converter (GPADC).
  *
  * This function enables the specified type of FIFO interrupt for a given channel. The types of interrupts that can be enabled are:
  * - FIFO empty
  * - FIFO full
  * - FIFO threshold
  *
  * @param[in] res Pointer to the GPADC resource structure.
  * @param[in] channelnum The channel number for which to enable the FIFO interrupt.
  * @param[in] type The type of FIFO interrupt to enable.
  *
  * @retval uint32_t Returns CSK_DRIVER_OK if the operation is successful, or CSK_DRIVER_ERROR_PARAMETER if an invalid interrupt type is specified.
  */
uint32_t HAL_GPADC_EnableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

	GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    switch (type) {
    case CSK_GPADC_FIFO_EMPTY:
    	// Clear the bit corresponding to the FIFO empty interrupt for the specified channel
    	pGpadc->reg->REG_ADC_IMR0.all &= ~(0x01<<channelnum<<16);
        break;
    case CSK_GPADC_FIFO_FULL:
    	// Clear the bit corresponding to the FIFO full interrupt for the specified channel
    	pGpadc->reg->REG_ADC_IMR1.all &= ~(0x01<<channelnum<<0);
        break;
    case CSK_GPADC_FIFO_THD:
    	// Clear the bit corresponding to the FIFO threshold interrupt for the specified channel
    	pGpadc->reg->REG_ADC_IMR1.all &= ~(0x01<<channelnum<<16);
    	break;
    default:
    	// Return an error if an invalid interrupt type is specified
    	return CSK_DRIVER_ERROR_PARAMETER;
    }

	return CSK_DRIVER_OK; // Return success status code
}


/**
  * @brief Disables the FIFO interrupt for a specific channel of the General Purpose Analog-to-Digital Converter (GPADC).
  *
  * This function disables the specified type of FIFO interrupt for a given channel. The types of interrupts that can be disabled are:
  * - FIFO empty
  * - FIFO full
  * - FIFO threshold
  *
  * @param[in] res Pointer to the GPADC resource structure.
  * @param[in] channelnum The channel number for which to disable the FIFO interrupt.
  * @param[in] type The type of FIFO interrupt to disable.
  *
  * @retval uint32_t Returns CSK_DRIVER_OK if the operation is successful, or CSK_DRIVER_ERROR_PARAMETER if an invalid interrupt type is specified.
  */
uint32_t HAL_GPADC_DisableFifoInterrupt(void* res, uint8_t channelnum, GPADC_FIFOINT_TYPE type)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

	GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    switch (type) {
    case CSK_GPADC_FIFO_EMPTY:
    	// Set the bit corresponding to the FIFO empty interrupt for the specified channel
    	pGpadc->reg->REG_ADC_IMR0.all |= (0x01<<channelnum<<16);
        break;
    case CSK_GPADC_FIFO_FULL:
    	// Set the bit corresponding to the FIFO full interrupt for the specified channel
    	pGpadc->reg->REG_ADC_IMR1.all |= (0x01<<channelnum<<0);
        break;
    case CSK_GPADC_FIFO_THD:
    	// Set the bit corresponding to the FIFO threshold interrupt for the specified channel
    	pGpadc->reg->REG_ADC_IMR1.all |= (0x01<<channelnum<<16);
    	break;
    default:
    	// Return an error if an invalid interrupt type is specified
    	return CSK_DRIVER_ERROR_PARAMETER;
    }

	return CSK_DRIVER_OK; // Return success status code
}


/**
  * @brief Enables the comparison interrupt for the General Purpose Analog-to-Digital Converter (GPADC).
  *
  * This function enables the comparison interrupt by setting the appropriate bit in the interrupt mask register.
  *
  * @param[in] res Pointer to the GPADC resource structure.
  *
  * @retval uint32_t Returns CSK_DRIVER_OK if the operation is successful.
  */
uint32_t HAL_GPADC_EnableCmpInterrupt(void* res)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Clear the bit corresponding to the comparison complete interrupt
    pGpadc->reg->REG_ADC_IMR0.bit.ADC_COMPLETE_IMR = 0;

    return CSK_DRIVER_OK; // Return success status code
}


/**
  * @brief Disables the comparison interrupt for the General Purpose Analog-to-Digital Converter (GPADC).
  *
  * This function disables the comparison interrupt by setting the appropriate bit in the interrupt mask register.
  *
  * @param[in] res Pointer to the GPADC resource structure.
  *
  * @retval uint32_t Returns CSK_DRIVER_OK if the operation is successful.
  */
uint32_t HAL_GPADC_DisableCmpInterrupt(void* res)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    // Set the bit corresponding to the comparison complete interrupt
    pGpadc->reg->REG_ADC_IMR0.bit.ADC_COMPLETE_IMR = 1;

    return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Gets the value from a specified channel of the General Purpose Analog-to-Digital Converter (GPADC).
 *
 * This function reads the ADC value from the specified channel by accessing the appropriate register.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] channelnum The number of the channel to read from.
 *
 * @retval uint16_t Returns the ADC value of the specified channel. If the channel number is invalid, returns 0.
 */
uint16_t HAL_GPADC_GetValue(void* res, uint32_t channelnum)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

    GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

    uint16_t adc_value = 0; // Initialize ADC value to 0

    switch(channelnum){ // Select the appropriate register based on the channel number
        case CSK_GPADC_CHANNEL_SEL_VBAT:
            adc_value = pGpadc->reg->REG_ADC_RDR0.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_KEYSENSE0:
            adc_value = pGpadc->reg->REG_ADC_RDR1.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_KEYSENSE1:
            adc_value = pGpadc->reg->REG_ADC_RDR2.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_TEMP:
            adc_value = pGpadc->reg->REG_ADC_RDR3.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_0:
            adc_value = pGpadc->reg->REG_ADC_RDR4.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_1:
            adc_value = pGpadc->reg->REG_ADC_RDR5.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_2:
            adc_value = pGpadc->reg->REG_ADC_RDR6.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_3:
            adc_value = pGpadc->reg->REG_ADC_RDR7.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_4:
            adc_value = pGpadc->reg->REG_ADC_RDR8.all;
            break;
        case CSK_GPADC_CHANNEL_SEL_5:
            adc_value = pGpadc->reg->REG_ADC_RDR9.all;
            break;
        default:
            break; // If the channel number is invalid, do nothing
    }

    return adc_value; // Return the ADC value of the specified channel
}

/**
 * @brief Registers a callback function for a specific GPADC channel.
 *
 * This function assigns a user-defined callback function to be called when a specified event occurs on a given GPADC channel.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] channel The GPADC channel for which the callback is being registered.
 * @param[in] cb_event The callback function to be called when the specified event occurs.
 *
 * @return int32_t Returns CSK_DRIVER_OK if the operation is successful.
 */
int32_t HAL_GPADC_RegisterChannelCallback(void *res, GPADC_CHANNEL_TYPE channel, CSK_GPADC_SignalEvent_t cb_event)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

	GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

	pGpadc->info->cb_event[channel] = cb_event; // Assign the callback function to the specified channel

	return CSK_DRIVER_OK; // Return success status code
}

/**
 * @brief Registers a callback function for the GPADC complete event.
 *
 * This function assigns a user-defined callback function to be called when the GPADC complete event occurs.
 *
 * @param[in] res Pointer to the GPADC resource structure.
 * @param[in] cb_event The callback function to be called when the GPADC complete event occurs.
 *
 * @return int32_t Returns CSK_DRIVER_OK if the operation is successful.
 */
int32_t HAL_GPADC_RegisterCompleteCallback(void *res, CSK_GPADC_SignalEvent_t cb_event)
{
    CHECK_RESOURCES(res); // Check if the resources are valid

	GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res; // Cast the input resource pointer to GPADC_RESOURCES type

	pGpadc->info->cmp_event = cb_event; // Assign the callback function to the complete event

	return CSK_DRIVER_OK; // Return success status code
}

/**
  * @brief Configure the GPADC for CVD (Capacitive Voltage Divider) operation.
  *
  * This function configures various parameters required for CVD operation in the GPADC module.
  * It includes settings for guard ring, double sampling, and second charge inversion.
  *
  * @param res Pointer to GPADC resources.
  * @param gr_state State of the guard ring (enable/disable).
  * @param db_sample_state State of double sampling (enable/disable).
  * @param second_charge_invert State of second charge inversion (enable/disable).
  *
  * @retval uint32_t Returns CSK_DRIVER_OK if successful.
  */
uint32_t HAL_GPADC_CVD_Config(void* res, uint32_t gr_state, uint32_t db_sample_state, uint32_t second_charge_invert)
{
    CHECK_RESOURCES(res);

	GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res;
	//pGpadc->reg->REG_CVD_CTRL.all = 0;

	//guard ring
	pGpadc->reg->REG_CVD_CTRL.bit.GUARD_EN = gr_state;

	//dobule sample
	pGpadc->reg->REG_CVD_CTRL.bit.DOUBLE_SAMP_EN = db_sample_state;

	//adipen
	pGpadc->reg->REG_CVD_CTRL.bit.ADIPEN = second_charge_invert;

	return CSK_DRIVER_OK;
}

/**
  * @brief Control the GPADC for specific CVD settings.
  *
  * This function sets various timing parameters for the CVD operation in the GPADC module.
  * It includes settings for precharge A/B timers and acquisition time.
  *
  * @param res Pointer to GPADC resources.
  * @param control Bitmask of control parameters for CVD configuration.
  *
  * @note Ensure that the GPADC module is powered on before calling this function.
  *
  * @retval uint32_t Returns CSK_DRIVER_OK if successful, CSK_DRIVER_ERROR if not powered.
  */
uint32_t HAL_GPADC_CVD_Control(void* res, uint32_t control)
{
	GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res;
	uint32_t tmp;

    if ((pGpadc->info->flags & GPADC_FLAG_POWERED) == 0U) {
        // UART not powered
        return CSK_DRIVER_ERROR;
    }

    //CVD prechargeA timer
    tmp = (control & CSK_GPADC_PRECHARGEA_CONTROL_Msk) >> CSK_GPADC_PRECHARGEA_CONTROL_Pos;
    pGpadc->reg->REG_CVD_CTRL.bit.PRECHGA_TIME = tmp;

    //CVD prechargeB timer
    tmp = (control & CSK_GPADC_PRECHARGEB_CONTROL_Msk) >> CSK_GPADC_PRECHARGEB_CONTROL_Pos;
    pGpadc->reg->REG_CVD_CTRL.bit.PRECHGB_TIME = tmp;

    //CVD Acquicition timer
    tmp = (control & CSK_GPADC_ACQUICITION_CONTROL_Msk) >> CSK_GPADC_ACQUICITION_CONTROL_Pos;
    pGpadc->reg->REG_CVD_CTRL.bit.ACQ_TIME = tmp;

	return CSK_DRIVER_OK;
}

/**
  * @brief Retrieve the current value from a specified GPADC channel in CVD mode.
  *
  * This function reads and returns the current value from the specified channel
  * of the GPADC module when operating in CVD mode.
  *
  * @param res Pointer to GPADC resources.
  * @param channelnum The GPADC channel number to read from in CVD mode.
  *
  * @retval uint32_t The current value from the specified GPADC channel in CVD mode.
  */
uint32_t HAL_GPADC_CVD_GetValue(void* res, uint32_t channelnum)
{
    CHECK_RESOURCES(res);

	GPADC_RESOURCES * pGpadc = (GPADC_RESOURCES *)res;

	uint32_t adc_value = 0;

	switch(channelnum){
		case CSK_GPADC_CHANNEL_SEL_VBAT:
			adc_value = pGpadc->reg->REG_ADC_RDR0.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_KEYSENSE0:
			adc_value = pGpadc->reg->REG_ADC_RDR1.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_KEYSENSE1:
			adc_value = pGpadc->reg->REG_ADC_RDR2.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_TEMP:
			adc_value = pGpadc->reg->REG_ADC_RDR3.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_0:
			adc_value = pGpadc->reg->REG_ADC_RDR4.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_1:
			adc_value = pGpadc->reg->REG_ADC_RDR5.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_2:
			adc_value = pGpadc->reg->REG_ADC_RDR6.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_3:
			adc_value = pGpadc->reg->REG_ADC_RDR7.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_4:
			adc_value = pGpadc->reg->REG_ADC_RDR8.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_5:
			adc_value = pGpadc->reg->REG_ADC_RDR9.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_CVD0:
			adc_value = pGpadc->reg->REG_ADC_RDR10.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_CVD1:
			adc_value = pGpadc->reg->REG_ADC_RDR11.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_CVD2:
			adc_value = pGpadc->reg->REG_ADC_RDR12.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_CVD3:
			adc_value = pGpadc->reg->REG_ADC_RDR13.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_CVD4:
			adc_value = pGpadc->reg->REG_ADC_RDR14.all;
			break;
		case CSK_GPADC_CHANNEL_SEL_CVD5:
			adc_value = pGpadc->reg->REG_ADC_RDR15.all;
			break;
		default:
			break;
	}

	return adc_value;
}

/**
 * @brief Generates a pseudo-random number.
 *
 * This function is intended to generate a pseudo-random number. Currently, it always returns 0.
 *
 * @return uint32_t A pseudo-random number (currently always 0).
 */
uint32_t ls_rand(void)
{
    // Return a fixed value of 0 as the pseudo-random number.
    return (0);
}

