#include "ClockManager.h"
#include "gpio.h"

#define CSK_GPIO_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)

// driver version
static const CSK_DRIVER_VERSION gpio_driver_version = {
        CSK_GPIO_API_VERSION,
        CSK_GPIO_DRV_VERSION
};

#define CHECK_RESOURCES(res)  do{\
        if((res != &gpioa_resources) && (res != &gpiob_resources)){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

/*
 * GPIOA RESOURCES
 * */

static void GPIOA_IRQ_Handler(void);

static _GPIO_ gpioa_info[ARCS_MAX_GPIOA] = {0};

static _GPIO_INFO info_a = {
        NULL,
        NULL,
        gpioa_info,
};

static const GPIO_RESOURCES gpioa_resources = {
        IP_GPIOA,
		IRQ_GPIOA_VECTOR,
        GPIOA_IRQ_Handler,
        ARCS_MAX_GPIOA,
        &info_a,
};

/*
 * GPIOB RESOURCES
 * */
static void GPIOB_IRQ_Handler(void);

static _GPIO_ gpiob_info[ARCS_MAX_GPIOB] = {0};

static _GPIO_INFO info_b = {
        NULL,
        NULL,
        gpiob_info,
};

static const GPIO_RESOURCES gpiob_resources = {
        IP_GPIOB,
        IRQ_GPIOB_VECTOR,
        GPIOB_IRQ_Handler,
        ARCS_MAX_GPIOB,
        &info_b,
};

// **************************************************

static void _FAST_FUNC_SRAM
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

static void
gpio_set_pull_mode(GPIO_RESOURCES* gpio, uint32_t pull_mode, uint32_t pin_mask){
    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        if(pin_mask & (0x1 << i)){
            gpio->info->gpio_info[i].mode = pull_mode;
        }
    }
}

static void
gpio_set_dir(GPIO_RESOURCES* gpio, uint32_t dir, uint32_t pin_mask){
    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        if(pin_mask & (0x1 << i)){
            gpio->info->gpio_info[i].dir = dir;
        }
    }
}

void* GPIOA(void){
    return (void*)&gpioa_resources;
}

void* GPIOB(void){
    return (void*)&gpiob_resources;
}

CSK_DRIVER_VERSION
GPIO_GetVersion(void){
    return gpio_driver_version;
}

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

int32_t
GPIO_PinRead(void* res, uint32_t pin_mask){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    return (gpio->reg->REG_DATAIN.all & pin_mask) ? 1 : 0;
}

int32_t
GPIO_SetDir(void* res, uint32_t pin_mask, uint32_t dir){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    dir ? (gpio->reg->REG_CHANNELDIR.all |= pin_mask) : (gpio->reg->REG_CHANNELDIR.all &= (~pin_mask));

    gpio_set_dir(gpio, dir, pin_mask);

    return CSK_DRIVER_OK;
}

int32_t
GPIO_Status(void* res, _GPIO_** status, uint32_t* size){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    *status = gpio->info->gpio_info;
    *size = gpio->max_num;

    return CSK_DRIVER_OK;
}

int32_t 
GPIO_SetCallback(void* res, uint32_t pin_mask, CSK_GPIO_SignalEvent_t cb_event, void* usr){
    CHECK_RESOURCES(res);
    GPIO_RESOURCES* gpio = (GPIO_RESOURCES*)res;

    // Find the index of the set bit using CTZ (Count Trailing Zeros)
    uint32_t pin_idx = __builtin_ctz(pin_mask);
    
    // Verify it's a single bit and within range
    if ((pin_mask & (pin_mask - 1)) != 0 || pin_idx >= gpio->max_num) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    
    gpio->info->gpio_info[pin_idx].cb = cb_event;
    gpio->info->gpio_info[pin_idx].usr = usr;
    
    return CSK_DRIVER_OK;
}

static void _FAST_FUNC_SRAM
GPIO_IRQ_Handler(GPIO_RESOURCES* gpio){
    uint32_t iir;
    iir = gpio->reg->REG_INTRSTATUS.all;

    if(gpio->info->cb_event){
        // clear gpio interrupt status
        gpio->reg->REG_INTRSTATUS.all = iir;
        gpio->info->cb_event(iir, gpio->info->workspace);
    }

    uint32_t i = 0;
    for(i = 0; i < gpio->max_num; i++){
        if(iir & (0x1 << i)){
            if(gpio->info->gpio_info[i].cb){
                gpio->reg->REG_INTRSTATUS.all = (0x1 << i);
                gpio->info->gpio_info[i].cb(iir, gpio->info->gpio_info[i].usr);
            }
        }
    }
}

static void
GPIOA_IRQ_Handler(void){
    GPIO_IRQ_Handler(&gpioa_resources);
}

static void
GPIOB_IRQ_Handler(void){
    GPIO_IRQ_Handler(&gpiob_resources);
}
