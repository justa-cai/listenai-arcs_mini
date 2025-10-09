#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "Driver_GPIO.h"
#include "systick.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "lisa_display.h"
#include "display_brightness.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_COMM_CMD_SLEEP_IN            0x10
#define DISPLAY_COMM_CMD_SLEEP_OUT           0x11
#define DISPLAY_COMM_CMD_INV_OFF             0x20
#define DISPLAY_COMM_CMD_INV_ON              0x21
#define DISPLAY_COMM_CMD_GAMSET              0x26
#define DISPLAY_COMM_CMD_DISP_OFF            0x28
#define DISPLAY_COMM_CMD_DISP_ON             0x29
#define DISPLAY_COMM_CMD_INTER_REG_ENABLE1   0xFE
#define DISPLAY_COMM_CMD_INTER_REG_ENABLE2   0xEF
#define DISPLAY_COMM_CMD_CASET               0x2a
#define DISPLAY_COMM_CMD_RASET               0x2b
#define DISPLAY_COMM_CMD_RAMWR               0x2c

#define DISPLAY_COMN_OPCODE_WRITE_CMD        (0x02ULL)
#define DISPLAY_COMM_OPCODE_WRITE_IMG        (0x32ULL)

#define delay_ms SysTick_Delay_Ms

typedef void (display_common_te_event_cb)(uint32_t event, void *workspace);
typedef int (disp_comm_trans_prepare)(int cmd);

struct disp_init_cmd {
    int         cmd;
    const void  *data;
    size_t      data_bytes;
    uint32_t    delay;
};

#define DISP_INIT_ITEM(_cmd, _delay, ...)                                                                                   \
    {                                                                                                                  \
        .cmd = _cmd,                                                                                                   \
        .data = (uint8_t[]){__VA_ARGS__},                                                                              \
        .data_bytes = sizeof((uint8_t[]){__VA_ARGS__}),                                                                \
        .delay = _delay,                                                                                               \
    }

struct disp_mem_area_input {
    uint16_t panel_w;
    uint16_t panel_h;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t x_offset;
    uint16_t y_offset;
    enum display_orientation orient;
};

struct disp_mem_area_output {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

typedef uint8_t disp_mem_coord[4];

struct display_obj {
    bool initialized;
    enum display_orientation orientation;
    SemaphoreHandle_t mutex;
#if CONFIG_LISA_DISPLAY_TE_SYNC
    SemaphoreHandle_t te_sem;
#endif
#if CONFIG_LISA_DISPLAY_BUSY_SYNC
    SemaphoreHandle_t busy_sem;
#endif
};

void disp_comm_rst_init(pin_info_t *rst_pin);

void disp_comm_rst_set(void);

void disp_comm_rst_clr(void);

#if CONFIG_LISA_DISPLAY_TE_SYNC
void disp_comm_te_init(SemaphoreHandle_t sem_handle, pin_info_t *te_pin);

int disp_comm_te_wait(SemaphoreHandle_t sem_handle, uint32_t timeout_ms);
#endif

#if CONFIG_LISA_DISPLAY_BUSY_SYNC
void disp_comm_busy_init(SemaphoreHandle_t sem_handle, pin_info_t *busy_pin);

int disp_comm_busy_wait(SemaphoreHandle_t sem_handle, uint32_t timeout_ms);
#endif

void disp_comm_set_mem_area(struct disp_mem_area_input *input, disp_mem_coord x, disp_mem_coord y);

#ifdef __cplusplus
}
#endif
