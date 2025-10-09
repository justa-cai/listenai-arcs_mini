#include <stddef.h>
#include <stdint.h>

#include "Driver_GPT_PWM.h"
#include "IOMuxManager.h"

#include "display_common.h"
#include "lisa_display.h"
#include "lisa_log.h"

void *disp_rst_dev = NULL;
uint8_t disp_rst_pin = 0;

void disp_comm_rst_init(pin_info_t *rst_pin)
{
    void *rst_dev = rst_pin->pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    IOMuxManager_PinConfigure(rst_pin->pad, rst_pin->pin, rst_pin->func); // GPIO LCD_RESET

    GPIO_Initialize(rst_dev, NULL, NULL);
    GPIO_SetDir(rst_dev, (1UL << rst_pin->pin), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(rst_dev, (1UL << rst_pin->pin), 1);
}

void disp_comm_rst_set(void)
{
    GPIO_PinWrite(disp_rst_dev, (1UL << disp_rst_pin), 1);
}

void disp_comm_rst_clr(void)
{
    GPIO_PinWrite(disp_rst_dev, (1UL << disp_rst_pin), 0);
}

#if CONFIG_LISA_DISPLAY_TE_SYNC
static uint8_t disp_te_pin = 0;
static void GPIO_TE_EventCallback(uint32_t event, void *workspace)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)workspace;

    if (event & (1UL << disp_te_pin)) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void disp_comm_te_init(SemaphoreHandle_t sem_handle, pin_info_t *te_pin)
{
    void *te_dev = te_pin->pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    disp_te_pin = te_pin->pin;
    IOMuxManager_PinConfigure(te_pin->pad, te_pin->pin, te_pin->func);

    GPIO_Initialize(te_dev, NULL, NULL);
    GPIO_SetDir(te_dev, (1UL << te_pin->pin), CSK_GPIO_DIR_INPUT);
    GPIO_Control(te_dev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_POSITIVE_EDGE |
                                    CSK_GPIO_INTR_ENABLE , (1UL << te_pin->pin));
    GPIO_SetCallback(te_dev, (1UL << te_pin->pin), GPIO_TE_EventCallback, sem_handle);
}

int disp_comm_te_wait(SemaphoreHandle_t sem_handle, uint32_t timeout_ms)
{
    if (!sem_handle) {
        LOGE("[%s] sem_handle is NULL\n", __func__);
        return -1;
    }
    xQueueReset(sem_handle);
    if (xSemaphoreTake(sem_handle, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return -1;
    }

    return 0;
}
#endif

#if CONFIG_LISA_DISPLAY_BUSY_SYNC
static uint8_t disp_busy_pin = 0;

static void GPIO_BUSY_EventCallback(uint32_t event, void *workspace)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)workspace;

    if (event & (1UL << disp_busy_pin)) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void disp_comm_busy_init(SemaphoreHandle_t sem_handle, pin_info_t *busy_pin)
{
    void *busy_dev = busy_pin->pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
    disp_busy_pin = busy_pin->pin;
    IOMuxManager_PinConfigure(busy_pin->pad, busy_pin->pin, busy_pin->func);

    GPIO_Initialize(busy_dev, NULL, NULL);
    GPIO_SetDir(busy_dev, (1UL << busy_pin->pin), CSK_GPIO_DIR_INPUT);
    GPIO_Control(busy_dev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_POSITIVE_EDGE |
                                    CSK_GPIO_INTR_ENABLE , (1UL << busy_pin->pin));
    GPIO_SetCallback(busy_dev, (1UL << busy_pin->pin), GPIO_BUSY_EventCallback, sem_handle);
}

int disp_comm_busy_wait(SemaphoreHandle_t sem_handle, uint32_t timeout_ms)
{
    if (!sem_handle) {
        LOGE("[%s] sem_handle is NULL\n", __func__);
        return -1;
    }
    xQueueReset(sem_handle);
    if (xSemaphoreTake(sem_handle, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return -1;
    }

    return 0;
}
#endif

void disp_comm_set_mem_area(struct disp_mem_area_input *input, disp_mem_coord x, disp_mem_coord y)
{
    uint16_t new_x, new_y, new_w, new_h;

    if (input->orient == DISPLAY_ORIENTATION_ROTATED_90) {
        new_x = input->panel_w - (input->y + input->h);
        new_y = input->x;
        new_w = input->h;
        new_h = input->w;
    } else if (input->orient == DISPLAY_ORIENTATION_ROTATED_270) {
        new_x = input->y;
        new_y = input->panel_h - input->x - input->w;
        new_w = input->h;
        new_h = input->w;
    } else {
        new_x = input->x;
        new_y = input->y;
        new_w = input->w;
        new_h = input->h;
    }

    new_x += input->x_offset;
    new_y += input->y_offset;

    x[0] = (new_x >> 8);
    x[1] = (new_x & 0xff);
    x[2] = ((new_x + new_w - 1) >> 8);
    x[3] = ((new_x + new_w - 1) & 0xff);

    y[0] = (new_y >> 8);
    y[1] = (new_y & 0xff);
    y[2] = ((new_y + new_h - 1) >> 8);
    y[3] = ((new_y + new_h - 1) & 0xff);
}