/*
 * Project:      GPIO Driver definitions
 */

#ifndef __CSK_DRIVER_GPIO_H
#define __CSK_DRIVER_GPIO_H

#include "Driver_Common.h"

// API version
#define CSK_GPIO_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)

// GPIO direction
#define CSK_GPIO_DIR_INPUT              (0x0)
#define CSK_GPIO_DIR_OUTPUT             (0x1)

// GPIO control mode
#define CSK_GPIO_INTR_Pos               0
#define CSK_GPIO_INTR_Mask              (0x3UL << CSK_GPIO_INTR_Pos)
#define CSK_GPIO_INTR_ENABLE            (2UL << CSK_GPIO_INTR_Pos)
#define CSK_GPIO_INTR_DISABLE           (1UL << CSK_GPIO_INTR_Pos)

#define CSK_GPIO_SET_INTR_Pos           2
#define CSK_GPIO_SET_INTR_Mask          (0x7UL << CSK_GPIO_SET_INTR_Pos)
#define CSK_GPIO_SET_INTR_LOW_LEVEL     (1UL << CSK_GPIO_SET_INTR_Pos)
#define CSK_GPIO_SET_INTR_HIGH_LEVEL    (2UL << CSK_GPIO_SET_INTR_Pos)
#define CSK_GPIO_SET_INTR_NEGATIVE_EDGE (3UL << CSK_GPIO_SET_INTR_Pos)
#define CSK_GPIO_SET_INTR_POSITIVE_EDGE (4UL << CSK_GPIO_SET_INTR_Pos)
#define CSK_GPIO_SET_INTR_DUAL_EDGE     (5UL << CSK_GPIO_SET_INTR_Pos)

//GPIO pull up/down mode
#define CSK_GPIO_MODE_Pos               5
#define CSK_GPIO_MODE_Mask              (0x3UL << CSK_GPIO_MODE_Pos)
#define CSK_GPIO_MODE_PULL_UP           (2UL << CSK_GPIO_MODE_Pos)
#define CSK_GPIO_MODE_PULL_DOWN         (3UL << CSK_GPIO_MODE_Pos)
#define CSK_GPIO_MODE_PULL_NONE         (1UL << CSK_GPIO_MODE_Pos)

// de-bounce clock
#define CSK_GPIO_DEBOUNCE_CLK_Pos       7
#define CSK_GPIO_DEBOUNCE_CLK_Mask      (0x3UL << CSK_GPIO_DEBOUNCE_CLK_Pos)
#define CSK_GPIO_DEBOUNCE_CLK_EXT       (1UL << CSK_GPIO_DEBOUNCE_CLK_Pos)
#define CSK_GPIO_DEBOUNCE_CLK_PCLK      (2UL << CSK_GPIO_DEBOUNCE_CLK_Pos)

// de-bounce enable switch
#define CSK_GPIO_DEBOUNCE_Pos           9
#define CSK_GPIO_DEBOUNCE_Mask          (0x3UL << CSK_GPIO_DEBOUNCE_Pos)
#define CSK_GPIO_DEBOUNCE_ENABLE        (2UL << CSK_GPIO_DEBOUNCE_Pos)
#define CSK_GPIO_DEBOUNCE_DISABLE       (1UL << CSK_GPIO_DEBOUNCE_Pos)

/******************control macro*************************/
#define CSK_GPIO_CONTROL_Pos            11
#define CSK_GPIO_CONTROL_Mask           (0x1UL << CSK_GPIO_CONTROL_Pos)
#define CSK_GPIO_DEBOUNCE_SCALE         (0x1UL << CSK_GPIO_CONTROL_Pos)

// Driver_GPIO only: GPIO pin ID and signal event ID
#define CSK_GPIO_PIN0             (1UL << 0)
#define CSK_GPIO_PIN1             (1UL << 1)
#define CSK_GPIO_PIN2             (1UL << 2)
#define CSK_GPIO_PIN3             (1UL << 3)
#define CSK_GPIO_PIN4             (1UL << 4)
#define CSK_GPIO_PIN5             (1UL << 5)
#define CSK_GPIO_PIN6             (1UL << 6)
#define CSK_GPIO_PIN7             (1UL << 7)
#define CSK_GPIO_PIN8             (1UL << 8)
#define CSK_GPIO_PIN9             (1UL << 9)
#define CSK_GPIO_PIN10            (1UL << 10)
#define CSK_GPIO_PIN11            (1UL << 11)
#define CSK_GPIO_PIN12            (1UL << 12)
#define CSK_GPIO_PIN13            (1UL << 13)
#define CSK_GPIO_PIN14            (1UL << 14)
#define CSK_GPIO_PIN15            (1UL << 15)
#define CSK_GPIO_PIN16            (1UL << 16)
#define CSK_GPIO_PIN17            (1UL << 17)
#define CSK_GPIO_PIN18            (1UL << 18)
#define CSK_GPIO_PIN19            (1UL << 19)
#define CSK_GPIO_PIN20            (1UL << 20)
#define CSK_GPIO_PIN21            (1UL << 21)
#define CSK_GPIO_PIN22            (1UL << 22)
#define CSK_GPIO_PIN23            (1UL << 23)
#define CSK_GPIO_PIN24            (1UL << 24)
#define CSK_GPIO_PIN25            (1UL << 25)
#define CSK_GPIO_PIN26            (1UL << 26)
#define CSK_GPIO_PIN27            (1UL << 27)
#define CSK_GPIO_PIN28            (1UL << 28)
#define CSK_GPIO_PIN29            (1UL << 29)
#define CSK_GPIO_PIN30            (1UL << 30)
#define CSK_GPIO_PIN31            (1UL << 31)

// callback function
typedef void (*CSK_GPIO_SignalEvent_t) (uint32_t event, void* workspace);

typedef enum {
    csk_gpio_dir_input = 0x0,
    csk_gpio_dir_output,
} _DIR_;

typedef enum {
    csk_gpio_mode_none_pull = 0x0,
    csk_gpio_mode_pull_up,
    csk_gpio_mode_pull_down,
} _MODE_;


typedef enum {
    _csk_gpio_int_mode_none_ = 0x0,
    csk_gpio_int_mode_high_level = 0x2,
    csk_gpio_int_mode_low_level = 0x3,
    csk_gpio_int_mode_negative_level = 0x5,
    csk_gpio_int_mode_positive_level = 0x6,
    csk_gpio_int_mode_dual_level = 0x7,
} _INT_MODE_;

typedef struct {
    _DIR_ dir;
    _MODE_ mode;
    _INT_MODE_ int_mode;
    CSK_GPIO_SignalEvent_t cb; 
    void* usr;
} _GPIO_;

CSK_DRIVER_VERSION GPIO_GetVersion(void);

int32_t GPIO_Initialize(void *res, CSK_GPIO_SignalEvent_t cb_event, void* workspace);

int32_t GPIO_Uninitialize(void *res);

int32_t GPIO_PowerControl(void* res, CSK_POWER_STATE state);

int32_t GPIO_Control(void* res, uint32_t control, uint32_t arg);

int32_t GPIO_PinWrite(void* res, uint32_t pin_mask, uint32_t val);

int32_t GPIO_PinRead(void* res, uint32_t pin_mask);

int32_t GPIO_SetDir(void* res, uint32_t pin_mask, uint32_t dir);

int32_t GPIO_Status(void* res, _GPIO_** status, uint32_t* size);

int32_t GPIO_SetCallback(void* res, uint32_t pin_mask, CSK_GPIO_SignalEvent_t cb_event, void* usr);

void* GPIOA(void);

void* GPIOB(void);

#endif /* __DRIVER_GPIO_H */
