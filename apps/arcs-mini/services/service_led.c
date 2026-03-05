#include "shell.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"

#define TAG "led"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "lisa_log.h"
#include "lisa_gpio.h"
#include "lisa_thread.h"

#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "board.h"

#define GPIO_DEVICE "gpiob"

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
static lisa_device_t *gpio_dev;

static int led_hw_init(void)
{
    gpio_dev = lisa_device_get(GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        LOGE("Error: %s device not ready", GPIO_DEVICE);
        return -1;
    }

    int ret = lisa_gpio_configure(gpio_dev, LED_PIN, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_HIGH);
    if (ret != 0) {
        LOGE("Error: GPIO configuration failed (code: %d)", ret);
        return -1;
    }

    return 0;
}

static void led_hw_on(void)
{
    lisa_gpio_write_pin(gpio_dev, LED_PIN, LISA_GPIO_LOW);
}

static void led_hw_off(void)
{
    lisa_gpio_write_pin(gpio_dev, LED_PIN, LISA_GPIO_HIGH);
}

static void led_task(void *param)
{
    (void)param;
    led_msg_t msg;
    uint32_t on_ms = 0;
    uint32_t off_ms = 0;
    int blinking = 0;
    int led_state = 0;
    TickType_t last_wake_time = 0;
    TickType_t next_toggle_time = 0;

    led_hw_init();
    last_wake_time = xTaskGetTickCount();

    for (;;) {
        TickType_t wait_time = portMAX_DELAY;

        if (blinking) {
            TickType_t current_time = xTaskGetTickCount();
            if (current_time >= next_toggle_time) {
                if (led_state) {
                    led_hw_off();
                    led_state = 0;
                    next_toggle_time = current_time + pdMS_TO_TICKS(off_ms);
                } else {
                    led_hw_on();
                    led_state = 1;
                    next_toggle_time = current_time + pdMS_TO_TICKS(on_ms);
                }
            }
            wait_time = next_toggle_time > current_time ? (next_toggle_time - current_time) : 0;
        }

        if (xQueueReceive(s_led_queue, &msg, wait_time) == pdPASS) {
            switch (msg.cmd) {
            case LED_CMD_ON:
                blinking = 0;
                led_state = 1;
                led_hw_on();
                break;
            case LED_CMD_OFF:
                blinking = 0;
                led_state = 0;
                led_hw_off();
                break;
            case LED_CMD_BLINK:
                if (msg.on_ms == 0 && msg.off_ms == 0) {
                    blinking = 0;
                    led_state = 0;
                    led_hw_off();
                } else {
                    on_ms = msg.on_ms ? msg.on_ms : 100;
                    off_ms = msg.off_ms ? msg.off_ms : 100;
                    blinking = 1;
                    led_state = 0;
                    next_toggle_time = xTaskGetTickCount();
                }
                break;
            case LED_CMD_STOP:
            default:
                blinking = 0;
                led_state = 0;
                led_hw_off();
                break;
            }
        }
    }
}

void service_led_init(void)
{
    if (s_led_queue != NULL) {
        return;
    }
    s_led_queue = xQueueCreate(4, sizeof(led_msg_t));
    if (s_led_queue == NULL) {
        LISA_LOGE(TAG, "create queue failed");
        return;
    }

    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"led_task",
        .stack_size = 2048,
        .priority = LISA_OS_PRIORITY_LOW,
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
    led_msg_t msg = {.cmd = cmd, .on_ms = on_ms, .off_ms = off_ms};
    BaseType_t ret = xQueueSend(s_led_queue, &msg, pdMS_TO_TICKS(100));
    if (ret != pdPASS) {
        LISA_LOGW(TAG, "LED queue send failed");
    }
}

void service_led_on(void)
{
    led_send_cmd(LED_CMD_ON, 0, 0);
}

void service_led_off(void)
{
    led_send_cmd(LED_CMD_OFF, 0, 0);
}

void service_led_blink(uint32_t on_ms, uint32_t off_ms)
{
    led_send_cmd(LED_CMD_BLINK, on_ms, off_ms);
}

void service_led_stop(void)
{
    led_send_cmd(LED_CMD_STOP, 0, 0);
}
