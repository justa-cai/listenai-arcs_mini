/**
 * @file    gpio.c
 * @author  ListenAI Application Team
 * @brief   GPIO Hardware Abstraction Layer (HAL) Driver Implementation
 *          This file implements the driver interface for General Purpose I/O (GPIO) peripherals,
 *          providing functionality including initialization, pin control, interrupt management,
 *          and status monitoring.
 *
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
#include "ClockManager.h"
#include "gpio.h"

/**
 * @brief GPIO Driver Version Number
 * @details Follows CSK driver versioning scheme (major.minor).
 */
#define CSK_GPIO_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

/**
 * @brief Driver Version Information Structure
 * @details Populated with API and driver version numbers.
 */
// driver version
static const CSK_DRIVER_VERSION gpio_driver_version = {
        CSK_GPIO_API_VERSION,
        CSK_GPIO_DRV_VERSION
};

/**
 * @brief Resource Validation Macro
 * @details Checks if provided resource pointer matches valid GPIO instances (GPIOA/GPIOB).
 * @param res Pointer to GPIO resources structure to validate
 */
#define CHECK_RESOURCES(res)  do{\
        if((res != &gpioa_resources) && (res != &gpiob_resources)){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

/*
 * GPIOA RESOURCES
 * */

/**
 * @brief GPIOA Interrupt Handler Function
 * @details Processes GPIOA interrupt events and dispatches to registered callback.
 */
static void GPIOA_IRQ_Handler(void);

/**
 * @brief GPIOA Pin State Array
 * @details Stores individual pin configurations for GPIOA instance.
 */
static _GPIO_ gpioa_info[VENUSA_MAX_GPIOA] = {0};

/**
 * @brief GPIOA Information Container
 * @details Holds callback function, workspace pointer, and per-pin state array.
 */
static _GPIO_INFO info_a = {
        NULL,
        NULL,
        gpioa_info,
};

/**
 * @brief GPIOA Resource Configuration
 * @details Contains base address, IRQ vector, handler function, max pin count, and info container.
 */
static const GPIO_RESOURCES gpioa_resources = {
		IP_GPIO0,
		IRQ_GPIO0_VECTOR,
        GPIOA_IRQ_Handler,
        VENUSA_MAX_GPIOA,
        &info_a,
};

/*
 * GPIOB RESOURCES
 * */

/**
 * @brief GPIOB Interrupt Handler Function
 * @details Processes GPIOB interrupt events and dispatches to registered callback.
 */
static void GPIOB_IRQ_Handler(void);

/**
 * @brief GPIOB Pin State Array
 * @details Stores individual pin configurations for GPIOB instance.
 */
static _GPIO_ gpiob_info[VENUSA_MAX_GPIOB] = {0};

/**
 * @brief GPIOB Information Container
 * @details Holds callback function, workspace pointer, and per-pin state array.
 */
static _GPIO_INFO info_b = {
        NULL,
        NULL,
        gpiob_info,
};

/**
 * @brief GPIOB Resource Configuration
 * @details Contains base address, IRQ vector, handler function, max pin count, and info container.
 */
static const GPIO_RESOURCES gpiob_resources = {
		IP_GPIO1,
        IRQ_GPIO1_VECTOR,
        GPIOB_IRQ_Handler,
        VENUSA_MAX_GPIOB,
        &info_b,
};

// **************************************************

/**
 * @fn gpio_set_intr_mode
 * @brief Set Interrupt Mode for Specified Pins
 * @param[in] gpio Pointer to GPIO resources structure
 * @param[in] intr_mode Desired interrupt mode (low/high level, edge detection)
 * @param[in] pin_mask Bitmask identifying target pins
 * @details Programs interrupt detection logic for specified pins across multiple register banks.
 *          Updates both hardware registers and internal state tracking.
 */
static void
gpio_set_intr_mode(GPIO_RESOURCES* gpio, uint32_t intr_mode, uint32_t pin_mask){
    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        if(pin_mask & (0x1 << i)){

            if((i >= 0) && (i < 8)){
                gpio->reg->REG_INTRMODE0.all &= ~(7 << (i * 4));
                gpio->reg->REG_INTRMODE0.all |= (intr_mode << (i * 4));
            }

            else if((i >= 8) && (i < 16)){
                gpio->reg->REG_INTRMODE1.all &= ~(7 << ((i-8) * 4));
                gpio->reg->REG_INTRMODE1.all |= (intr_mode << ((i-8) * 4));
            }

            else if((i >= 16) && (i < 24)){
                gpio->reg->REG_INTRMODE2.all &= ~(7 << ((i-16) * 4));
                gpio->reg->REG_INTRMODE2.all |= (intr_mode << ((i-16) * 4));
            }

            else if((i >= 24) && (i < 32)){
                gpio->reg->REG_INTRMODE3.all &= ~(7 << ((i-24) * 4));
                gpio->reg->REG_INTRMODE3.all |= (intr_mode << ((i-24) * 4));
            }

            ((_GPIO_*)gpio->info->gpio_info + i)->int_mode = intr_mode;
        }
    }
}

/**
 * @fn gpio_set_pull_mode
 * @brief Set Pull Resistor Mode for Specified Pins
 * @param[in] gpio Pointer to GPIO resources structure
 * @param[in] pull_mode Desired pull mode (none/up/down)
 * @param[in] pin_mask Bitmask identifying target pins
 * @details Updates pull resistor configuration in internal state tracking system.
 */
static void
gpio_set_pull_mode(GPIO_RESOURCES* gpio, uint32_t pull_mode, uint32_t pin_mask){
    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        if(pin_mask & (0x1 << i)){
            gpio->info->gpio_info[i].mode = pull_mode;
        }
    }
}

/**
 * @fn gpio_set_dir
 * @brief Set Pin Direction for Specified Pins
 * @param[in] gpio Pointer to GPIO resources structure
 * @param[in] dir Pin direction (input/output)
 * @param[in] pin_mask Bitmask identifying target pins
 * @details Updates direction setting in internal state tracking system.
 */
static void
gpio_set_dir(GPIO_RESOURCES* gpio, uint32_t dir, uint32_t pin_mask){
    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        if(pin_mask & (0x1 << i)){
            gpio->info->gpio_info[i].dir = dir;
        }
    }
}

/**
 * @fn GPIOA
 * @brief Get Address of GPIOA Resources
 * @return Pointer to GPIOA resources structure
 */
void* GPIOA(void){
    return (void*)&gpioa_resources;
}

/**
 * @fn GPIOB
 * @brief Get Address of GPIOB Resources
 * @return Pointer to GPIOB resources structure
 */
void* GPIOB(void){
    return (void*)&gpiob_resources;
}

/**
 * @fn GPIO_GetVersion
 * @brief Get Driver Version Information
 * @return DriverVersion structure containing version numbers
 */
CSK_DRIVER_VERSION
GPIO_GetVersion(void){
    return gpio_driver_version;
}

/**
 * @fn GPIO_Initialize
 * @brief Initialize GPIO Peripheral
 * @param[in] res Pointer to GPIO resources structure
 * @param[in] cb_event Event callback function
 * @param[in] workspace User context data pointer
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Performs clock enable, registers IRQ handler, and initializes all pins to default state.
 *          Sets initial callback and workspace references.
 */
int32_t
GPIO_Initialize(void *res, CSK_GPIO_SignalEvent_t cb_event, void* workspace){
    CHECK_RESOURCES(res);

    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    if (gpio == &gpioa_resources) {
    	__HAL_CRM_GPIO0_CLK_ENABLE();
    }
    else if (gpio == &gpiob_resources) {
    	__HAL_CRM_GPIO1_CLK_ENABLE();
    }
    else {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    gpio->info->cb_event = cb_event;
    gpio->info->workspace = workspace;

    register_ISR(gpio->irq_num, gpio->irq_handler, NULL);
    enable_IRQ(gpio->irq_num);

    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        ((_GPIO_*)gpio->info->gpio_info + i)->dir = csk_gpio_dir_input;
        ((_GPIO_*)gpio->info->gpio_info + i)->mode = csk_gpio_mode_none_pull;
        ((_GPIO_*)gpio->info->gpio_info + i)->int_mode = _csk_gpio_int_mode_none_;
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn GPIO_Uninitialize
 * @brief Deinitialize GPIO Peripheral
 * @param[in] res Pointer to GPIO resources structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Disables clock, unregisters IRQ handler, and resets all pins to default state.
 *          Clears callback and workspace references.
 */
int32_t
GPIO_Uninitialize(void *res){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    if (gpio == &gpioa_resources) {
    	__HAL_CRM_GPIO0_CLK_DISABLE();
    } else if (gpio == &gpiob_resources) {
    	__HAL_CRM_GPIO1_CLK_DISABLE();
    }
    else {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    disable_IRQ(gpio->irq_num);

    gpio->info->cb_event = NULL;
    gpio->info->workspace = NULL;

    register_ISR(gpio->irq_num, NULL, NULL);

    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        ((_GPIO_*)gpio->info->gpio_info + i)->dir = csk_gpio_dir_input;
        ((_GPIO_*)gpio->info->gpio_info + i)->mode = csk_gpio_mode_none_pull;
        ((_GPIO_*)gpio->info->gpio_info + i)->int_mode = _csk_gpio_int_mode_none_;
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn GPIO_Control
 * @brief Configure GPIO Control Settings
 * @param[in] res Pointer to GPIO resources structure
 * @param[in] control Control flags (debounce scale, interrupt control, etc.)
 * @param[in] arg Argument value for selected control operation
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Processes multiple control operations including debounce scaling, interrupt enable/disable,
 *          interrupt mode configuration, pull mode setup, clock selection, and debounce enable/disable.
 */
int32_t
GPIO_Control(void* res, uint32_t control, uint32_t arg){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    switch (control & CSK_GPIO_CONTROL_Mask){
    case CSK_GPIO_DEBOUNCE_SCALE:
        gpio->reg->REG_DEBOUNCECTRL.bit.DBPRESCALE = (arg & 0xff);
        return CSK_DRIVER_OK;
    }

    switch (control & CSK_GPIO_INTR_Mask){
    case CSK_GPIO_INTR_ENABLE:
        gpio->reg->REG_INTREN.all |= arg;
        break;
    case CSK_GPIO_INTR_DISABLE:
        gpio->reg->REG_INTREN.all &= (~arg);
        break;
    }

    // configure interrupt mode
    switch (control & CSK_GPIO_SET_INTR_Mask){
    case CSK_GPIO_SET_INTR_LOW_LEVEL:
        gpio_set_intr_mode(gpio, csk_gpio_int_mode_low_level, arg);
        break;

    case CSK_GPIO_SET_INTR_HIGH_LEVEL:
        gpio_set_intr_mode(gpio, csk_gpio_int_mode_high_level, arg);
        break;

    case CSK_GPIO_SET_INTR_NEGATIVE_EDGE:
        gpio_set_intr_mode(gpio, csk_gpio_int_mode_negative_level, arg);
        break;

    case CSK_GPIO_SET_INTR_POSITIVE_EDGE:
        gpio_set_intr_mode(gpio, csk_gpio_int_mode_positive_level, arg);
        break;

    case CSK_GPIO_SET_INTR_DUAL_EDGE:
        gpio_set_intr_mode(gpio, csk_gpio_int_mode_dual_level, arg);
        break;
    }

    // configure pull mode
    switch (control & CSK_GPIO_MODE_Mask){
    case CSK_GPIO_MODE_PULL_UP:
        gpio->reg->REG_PULLEN.all |= arg;
        gpio->reg->REG_PULLTYPE.all &= (~arg);
        gpio_set_pull_mode(gpio, csk_gpio_mode_pull_up, arg);
        break;
    case CSK_GPIO_MODE_PULL_DOWN:
        gpio->reg->REG_PULLEN.all |= arg;
        gpio->reg->REG_PULLTYPE.all |= arg;
        gpio_set_pull_mode(gpio, csk_gpio_mode_pull_down, arg);
        break;
    case CSK_GPIO_MODE_PULL_NONE:
        gpio->reg->REG_PULLEN.all &= (~arg);
        gpio_set_pull_mode(gpio, csk_gpio_mode_none_pull, arg);
        break;
    }

    switch (control & CSK_GPIO_DEBOUNCE_CLK_Mask){
    case CSK_GPIO_DEBOUNCE_CLK_EXT:
        gpio->reg->REG_DEBOUNCECTRL.bit.DBCLKSEL = 0x0;
        break;
    case CSK_GPIO_DEBOUNCE_CLK_PCLK:
        gpio->reg->REG_DEBOUNCECTRL.bit.DBCLKSEL = 0x1;
        break;
    }

    switch (control & CSK_GPIO_DEBOUNCE_Mask){
    case CSK_GPIO_DEBOUNCE_ENABLE:
        gpio->reg->REG_DEBOUNCEEN.all |= arg;
        break;
    case CSK_GPIO_DEBOUNCE_DISABLE:
        gpio->reg->REG_DEBOUNCEEN.all &= (~arg);
        break;
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn GPIO_PinWrite
 * @brief Write Output Value to Specified Pins
 * @param[in] res Pointer to GPIO resources structure
 * @param[in] pin_mask Bitmask identifying target pins
 * @param[in] val Value to write (non-zero=set, zero=clear)
 * @return CSK_DRIVER_OK on success
 * @details Directly manipulates output set/clear registers based on pin mask and value.
 */
int32_t
GPIO_PinWrite(void* res, uint32_t pin_mask, uint32_t val)
{
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    if(val)
    	gpio->reg->REG_DOUTSET.all = pin_mask;
    else
    	gpio->reg->REG_DOUTCLEAR.all = pin_mask;

    return CSK_DRIVER_OK;
}

/**
 * @fn GPIO_PinRead
 * @brief Read Input Value from Specified Pins
 * @param[in] res Pointer to GPIO resources structure
 * @param[in] pin_mask Bitmask identifying target pins
 * @return Non-zero if any specified pin is high, zero otherwise
 * @details Reads input data register and masks with pin mask to determine logical OR result.
 */
int32_t
GPIO_PinRead(void* res, uint32_t pin_mask){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    return (gpio->reg->REG_DATAIN.all & pin_mask) ? 1 : 0;
}

/**
 * @fn GPIO_SetDir
 * @brief Set Direction Mode for Specified Pins
 * @param[in] res Pointer to GPIO resources structure
 * @param[in] pin_mask Bitmask identifying target pins
 * @param[in] dir Direction mode (input/output)
 * @return CSK_DRIVER_OK on success
 * @details Updates channel direction register and internal state tracking.
 */
int32_t
GPIO_SetDir(void* res, uint32_t pin_mask, uint32_t dir){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    dir ? (gpio->reg->REG_CHANNELDIR.all |= pin_mask) : (gpio->reg->REG_CHANNELDIR.all &= (~pin_mask));

    gpio_set_dir(gpio, dir, pin_mask);

    return CSK_DRIVER_OK;
}

/**
 * @fn GPIO_Status
 * @brief Get Current Status of All Pins
 * @param[in] res Pointer to GPIO resources structure
 * @param[out] status Double pointer to store pin state array
 * @param[out] size Pointer to store number of pins
 * @return CSK_DRIVER_OK on success
 * @details Copies current pin state array and size information to caller-provided buffers.
 */
int32_t
GPIO_Status(void* res, _GPIO_** status, uint32_t* size){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    *status = gpio->info->gpio_info;
    *size = gpio->max_num;

    return CSK_DRIVER_OK;
}

/**
 * @fn GPIO_IRQ_Handler
 * @brief Common GPIO Interrupt Handler
 * @param[in] gpio Pointer to GPIO resources structure
 * @details Clears interrupt status flags and invokes registered event callback.
 */
static void
GPIO_IRQ_Handler(GPIO_RESOURCES* gpio){
    uint32_t iir;
    iir = gpio->reg->REG_INTRSTATUS.all;
    // clear gpio interrupt status
    gpio->reg->REG_INTRSTATUS.all = iir;

    gpio->info->cb_event(iir, gpio->info->workspace);
}

/**
 * @fn GPIOA_IRQ_Handler
 * @brief GPIOA Interrupt Handler Wrapper
 * @details Calls common interrupt handler with GPIOA resources.
 */
static void
GPIOA_IRQ_Handler(void){
    GPIO_IRQ_Handler(&gpioa_resources);
}

/**
 * @fn GPIOB_IRQ_Handler
 * @brief GPIOB Interrupt Handler Wrapper
 * @details Calls common interrupt handler with GPIOB resources.
 */
static void
GPIOB_IRQ_Handler(void){
    GPIO_IRQ_Handler(&gpiob_resources);
}
