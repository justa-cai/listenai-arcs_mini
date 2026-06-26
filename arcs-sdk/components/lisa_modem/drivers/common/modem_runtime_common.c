/**
 * @file modem_runtime_common.c
 * @brief Shared helpers for modem endpoint runtimes
 */

#include "drivers/common/modem_runtime_common.h"
#include "FreeRTOS.h"
#include "task.h"

int modem_runtime_recv_ring(struct ring_buf *ring_buf, char *buffer, size_t length,
                            uint32_t timeout_ms, SemaphoreHandle_t data_sem,
                            modem_runtime_wait_hook_t wait_hook, void *user_data)
{
    TickType_t start_tick;
    TickType_t timeout_ticks;
    uint32_t buf_available;
    uint32_t to_read;
    uint32_t bytes_read;

    // timeout_ms = 100;
    if (!ring_buf || !buffer || length == 0) {
        return -1;
    }

    if (ring_buf_size_get(ring_buf) == 0U) {
        if (timeout_ms == 0U) {
            return -1;
        }

        start_tick = xTaskGetTickCount();
        timeout_ticks = pdMS_TO_TICKS(timeout_ms);

        while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
            if (ring_buf_size_get(ring_buf) > 0U) {
                break;
            }

            if (data_sem) {
                TickType_t remaining = timeout_ticks - (xTaskGetTickCount() - start_tick);
                if (remaining == 0 || remaining > timeout_ticks) {
                    break;
                }
                xSemaphoreTake(data_sem, remaining);
            } else if (wait_hook) {
                wait_hook(user_data);
                vTaskDelay(pdMS_TO_TICKS(3));
            } else {
                vTaskDelay(pdMS_TO_TICKS(3));
            }
        }
    }

    buf_available = ring_buf_size_get(ring_buf);
    if (buf_available == 0) {
        return -1;
    }

    to_read = (length > buf_available) ? buf_available : (uint32_t)length;
    bytes_read = ring_buf_get(ring_buf, (uint8_t *)buffer, to_read);
    return bytes_read > 0 ? (int)bytes_read : 0;
}
