#include "shell.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"

/* LED service implementation */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "led.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"

#define TAG "led"

typedef enum {
    LED_CMD_ON = 0,
    LED_CMD_OFF,
    LED_CMD_BLINK,
    LED_CMD_STOP,
} led_cmd_e;

typedef struct {
    led_cmd_e cmd;
    uint32_t on_ms;
    uint32_t off_ms;
} led_msg_t;

static lisa_thread_t *s_led_task = NULL;
static QueueHandle_t s_led_queue = NULL;

/**
 * @brief Control LED
 * @param state true: LED on, false: LED off
 */
void led_hw_control(bool state)
{
    uint32_t pin_mask = 0x01 << 1;
    uint32_t pin_value = !state;  // LED is active low
    
    GPIO_PinWrite(GPIOB(), pin_mask, pin_value);
}

/* Concrete hardware hooks using RGB helpers */
void led_hw_init(void)
{
    /* Initialize GPIOB module */
    GPIO_Initialize(GPIOB(), NULL, NULL);
    
    /* Configure IO Mux for LED pins */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, CSK_IOMUX_FUNC_DEFAULT);  /* Green LED - Pin1 */

    /* Configure Green LED - GPIOB Pin1 */
    uint32_t green_pin_mask = 0x01 << 1;
    GPIO_SetDir(GPIOB(), green_pin_mask, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB(), green_pin_mask, 1); /* LED off (high level) */
}

void led_hw_on(void)
{
    led_hw_control(true);
}

void led_hw_off(void)
{
    led_hw_control(false);
}

static void led_task(void *param)
{
    (void)param;
    led_msg_t msg;
    uint32_t on_ms = 0;
    uint32_t off_ms = 0;
    int blinking = 0;


    for (;;) {
        if (!blinking) {
            if (xQueueReceive(s_led_queue, &msg, portMAX_DELAY) != pdPASS) {
                continue;
            }
        } else {
            /* While blinking, poll queue with timeout to allow preemption */
            if (xQueueReceive(s_led_queue, &msg, 0) == pdPASS) {
                /* Got a new command: fall through to handle */
            } else {
                /* Perform one blink cycle */
                led_hw_on();
                vTaskDelay(pdMS_TO_TICKS(on_ms));
                led_hw_off();
                vTaskDelay(pdMS_TO_TICKS(off_ms));
                continue;
            }
        }

        switch (msg.cmd) {
            case LED_CMD_ON:
                blinking = 0;
                led_hw_on();
                break;
            case LED_CMD_OFF:
                blinking = 0;
                led_hw_off();
                break;
            case LED_CMD_BLINK:
                if (msg.on_ms == 0 && msg.off_ms == 0) {
                    /* Treat as off */
                    blinking = 0;
                    led_hw_off();
                } else {
                    on_ms = msg.on_ms ? msg.on_ms : 100;
                    off_ms = msg.off_ms ? msg.off_ms : 100;
                    blinking = 1;
                    /* Don't start the first cycle here, let the main loop handle it */
                }
                break;
            case LED_CMD_STOP:
            default:
                blinking = 0;
                led_hw_off();
                break;
        }
    }
}

void app_led_init(void)
{
    if (s_led_queue != NULL) {
        return;
    }
    led_hw_init();
    s_led_queue = xQueueCreate(4, sizeof(led_msg_t));
    if (s_led_queue == NULL) {
        LISA_LOGE(TAG, "create queue failed");
        return;
    }
    
    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"led_task",
        .stack_size = 2048,
        .priority = LISA_OS_PRIORITY_LOW
    };
    
    s_led_task = lisa_thread_create(&attr, led_task, NULL);
    if (s_led_task == NULL) {
        LISA_LOGE(TAG, "create led task failed");
        vQueueDelete(s_led_queue);
        s_led_queue = NULL;
        return;
    }
    LISA_LOGI(TAG, "LED service initialized");
}

static inline void led_send_cmd(led_cmd_e cmd, uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) {
        LISA_LOGW(TAG, "LED queue not ready");
        return;
    }
    led_msg_t msg = { .cmd = cmd, .on_ms = on_ms, .off_ms = off_ms };
    BaseType_t ret = xQueueSend(s_led_queue, &msg, pdMS_TO_TICKS(100));
    if (ret != pdPASS) {
        LISA_LOGW(TAG, "LED queue send failed");
    }
}

void app_led_on(void)
{
    led_send_cmd(LED_CMD_ON, 0, 0);
}

void app_led_off(void)
{
    led_send_cmd(LED_CMD_OFF, 0, 0);
}

void app_led_blink(uint32_t on_ms, uint32_t off_ms)
{
    led_send_cmd(LED_CMD_BLINK, on_ms, off_ms);
}

void app_led_stop(void)
{
    led_send_cmd(LED_CMD_STOP, 0, 0);
}

