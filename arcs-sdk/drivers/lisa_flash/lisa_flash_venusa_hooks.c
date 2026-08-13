/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_flash_venusa_hooks.c
 * @brief Strong VENUSA Flash hook implementations.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lisa_device.h"
#include "lisa_log.h"
#include "spiflash.h"

#include "ic_message.h"
#include "sys_init.h"
#include "venusa_ap.h"

#define VENUSA_FLASH_HOOK_IDLE          1U
#define VENUSA_FLASH_HOOK_WAITING       2U
#define VENUSA_FLASH_HOOK_HALTED        3U
#define VENUSA_FLASH_HOOK_CANCEL        4U
#define VENUSA_FLASH_HOOK_CANCELED      5U
#define VENUSA_FLASH_HOOK_MAILBOX_ALIGN 4U
#define VENUSA_SRAM_PHYS_BASE           0x20000000U
#define VENUSA_SRAM_PHYS_SIZE           0x00080000U

struct venusa_flash_hook_payload {
    uint32_t request_addr;
    uint32_t ack_addr;
};

/*
 * VenusA has no ITCM. Keep the flash hook mailbox in .data, which is copied to
 * SRAM/RAM at boot and can be polled while XIP flash is busy. VenusA startup
 * marks the physical SRAM window as non-cacheable, so both cores can exchange
 * this mailbox with normal load/store operations.
 */
static volatile uint32_t venusa_flash_request_mailbox __attribute__((section(".data"), used, aligned(4))) =
    VENUSA_FLASH_HOOK_IDLE;
static volatile uint32_t venusa_flash_ack_mailbox __attribute__((section(".data"), used, aligned(4))) =
    VENUSA_FLASH_HOOK_IDLE;
static uint8_t venusa_flash_msg_registered;

static inline void venusa_flash_hook_fence(void)
{
    __asm__ volatile("fence rw, rw" ::: "memory");
}

static inline __attribute__((always_inline)) uint32_t
venusa_flash_mailbox_read(volatile uint32_t *mailbox)
{
    venusa_flash_hook_fence();
    return *mailbox;
}

static inline __attribute__((always_inline)) void
venusa_flash_mailbox_write(volatile uint32_t *mailbox, uint32_t value)
{
    *mailbox = value;
    venusa_flash_hook_fence();
}

static int venusa_flash_mailbox_addr_valid(uint32_t mailbox_addr)
{
    if ((mailbox_addr & (VENUSA_FLASH_HOOK_MAILBOX_ALIGN - 1U)) != 0U) {
        return 0;
    }

    return mailbox_addr >= VENUSA_SRAM_PHYS_BASE &&
           mailbox_addr <= (VENUSA_SRAM_PHYS_BASE + VENUSA_SRAM_PHYS_SIZE - sizeof(uint32_t));
}

__attribute__((weak)) void lisa_flash_wait_before_disable_irq_hook(volatile uint32_t *mailbox)
{
    (void)mailbox;
}

__attribute__((weak)) void lisa_flash_wait_before_poll_hook(volatile uint32_t *mailbox)
{
    (void)mailbox;
}

__attribute__((weak)) void lisa_flash_wait_before_enable_irq_hook(volatile uint32_t *mailbox)
{
    (void)mailbox;
}

__attribute__((weak)) void lisa_flash_wait_after_enable_irq_hook(volatile uint32_t *mailbox)
{
    (void)mailbox;
}

static void venusa_flash_wait_mailbox_idle(volatile uint32_t *request, volatile uint32_t *ack)
    __attribute__((section(".fast.text"), noinline));

static void venusa_flash_wait_mailbox_idle(volatile uint32_t *request, volatile uint32_t *ack)
{
#if CONFIG_LISA_FLASH_VENUSA_DISABLE_INTERRUPTS
    uint8_t gint_enabled = GINT_enabled();

    lisa_flash_wait_before_disable_irq_hook(request);
    disable_GINT();
    venusa_flash_hook_fence();
#else
    lisa_flash_wait_before_disable_irq_hook(request);
    venusa_flash_hook_fence();
#endif

    uint32_t value = venusa_flash_mailbox_read(request);
    uint8_t peer_halted = 0U;

    if (value == VENUSA_FLASH_HOOK_WAITING) {
        lisa_flash_wait_before_poll_hook(request);
        venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_HALTED);
        peer_halted = 1U;
    } else if (value == VENUSA_FLASH_HOOK_CANCEL) {
        venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_CANCELED);
    }

    while (peer_halted) {
        value = venusa_flash_mailbox_read(request);
        if (value == VENUSA_FLASH_HOOK_IDLE) {
            break;
        }
        if (value == VENUSA_FLASH_HOOK_CANCEL) {
            venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_CANCELED);
            break;
        }
        venusa_flash_hook_fence();
        __asm__ volatile("nop" ::: "memory");
    }

    venusa_flash_hook_fence();
#if CONFIG_LISA_FLASH_VENUSA_DISABLE_INTERRUPTS
    if (gint_enabled) {
        lisa_flash_wait_before_enable_irq_hook(request);
        enable_GINT();
        lisa_flash_wait_after_enable_irq_hook(request);
    }
#else
    lisa_flash_wait_before_enable_irq_hook(request);
    lisa_flash_wait_after_enable_irq_hook(request);
#endif
}

static int32_t venusa_flash_msg_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t *msg)
{
    (void)handle;

    if (!msg || !msg->msg || msg->len != sizeof(struct venusa_flash_hook_payload)) {
        return IC_MESSAGE_ERR_PARAM_INVALID;
    }

    struct venusa_flash_hook_payload payload;

    memcpy(&payload, msg->msg, sizeof(payload));
    if (!venusa_flash_mailbox_addr_valid(payload.request_addr) ||
        !venusa_flash_mailbox_addr_valid(payload.ack_addr)) {
        return IC_MESSAGE_ERR_PARAM_INVALID;
    }

    volatile uint32_t *request = (volatile uint32_t *)(uintptr_t)payload.request_addr;
    volatile uint32_t *ack = (volatile uint32_t *)(uintptr_t)payload.ack_addr;

    venusa_flash_wait_mailbox_idle(request, ack);

    return IC_MESSAGE_ERR_NONE;
}

int lisa_flash_hook_init(void)
{
    if (venusa_flash_msg_registered) {
        return LISA_DEVICE_OK;
    }

    int ret = ic_message_register_by_id(IC_MESSAGE_ID_FLASH_MSG, venusa_flash_msg_cb, NULL);
    if (ret == IC_MESSAGE_ERR_NONE || ret == IC_MESSAGE_ERR_NOT_NULL) {
        LOGI("lisa flash hook init ok");
        venusa_flash_msg_registered = 1U;
        return LISA_DEVICE_OK;
    }

    LOGE("lisa flash hook init failed, r: %d", ret);

    return ret == IC_MESSAGE_ERR_NOT_INITED ? LISA_DEVICE_ERR_NOT_READY : LISA_DEVICE_ERR_IO;
}

/*
 * The flash hook depends on ic_message auto-init, and Flash HAL init may use
 * the hook to stop the peer core. Register it after PRE_DEVICES late init and
 * before LISA device initialization runs at PRE_KERNEL/POST_KERNEL.
 */
SYS_INIT(lisa_flash_hook_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, SYS_INIT_SUB_PRIORITY_LAST);

static int venusa_flash_cancel_wait(volatile uint32_t *request, volatile uint32_t *ack)
{
    uint32_t timeout = CONFIG_LISA_FLASH_VENUSA_TIMEOUT;

    venusa_flash_mailbox_write(request, VENUSA_FLASH_HOOK_CANCEL);
    do {
        if (venusa_flash_mailbox_read(ack) == VENUSA_FLASH_HOOK_CANCELED) {
            venusa_flash_mailbox_write(request, VENUSA_FLASH_HOOK_IDLE);
            venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_IDLE);
            return LISA_DEVICE_ERR_TIMEOUT;
        }
        if (timeout-- == 0U) {
            venusa_flash_mailbox_write(request, VENUSA_FLASH_HOOK_IDLE);
            venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_IDLE);
            return LISA_DEVICE_ERR_TIMEOUT;
        }
        venusa_flash_hook_fence();
        __asm__ volatile("nop" ::: "memory");
    } while (1);
}

static int venusa_flash_send_mailbox_addr(volatile uint32_t *request, volatile uint32_t *ack,
                                          size_t offset)
{
    struct venusa_flash_hook_payload payload = {
        .request_addr = (uint32_t)(uintptr_t)request,
        .ack_addr = (uint32_t)(uintptr_t)ack,
    };
    uint32_t request_state = venusa_flash_mailbox_read(request);

    if (request_state == 0U) {
        venusa_flash_mailbox_write(request, VENUSA_FLASH_HOOK_IDLE);
        request_state = VENUSA_FLASH_HOOK_IDLE;
    }

    if (request_state != VENUSA_FLASH_HOOK_IDLE) {
        return LISA_DEVICE_ERR_BUSY;
    }

    int ret = lisa_flash_hook_init();
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    (void)offset;

    /* The request mailbox is CP-owned; the ack mailbox is AP-owned. */
    venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_IDLE);
    venusa_flash_mailbox_write(request, VENUSA_FLASH_HOOK_WAITING);

    ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_FLASH_MSG, IC_MESSAGE_MSG_TYPE_EVT,
                                    &payload, sizeof(payload));
    if (ret != IC_MESSAGE_ERR_NONE) {
        venusa_flash_mailbox_write(request, VENUSA_FLASH_HOOK_IDLE);
        venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_IDLE);
        return LISA_DEVICE_ERR_IO;
    }

    uint32_t timeout = CONFIG_LISA_FLASH_VENUSA_TIMEOUT;
    while (venusa_flash_mailbox_read(ack) != VENUSA_FLASH_HOOK_HALTED) {
        if (timeout-- == 0U) {
            return venusa_flash_cancel_wait(request, ack);
        }
        venusa_flash_hook_fence();
        __asm__ volatile("nop" ::: "memory");
    }

    return LISA_DEVICE_OK;
}

static int venusa_flash_release_mailbox(volatile uint32_t *request, volatile uint32_t *ack)
{
    venusa_flash_mailbox_write(request, VENUSA_FLASH_HOOK_IDLE);
    venusa_flash_mailbox_write(ack, VENUSA_FLASH_HOOK_IDLE);

    return LISA_DEVICE_OK;
}

int lisa_flash_read_hook(FLASH_DEV *hal_dev, size_t offset, size_t len)
{
    (void)hal_dev;
    (void)len;

    return venusa_flash_send_mailbox_addr(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox, offset);
}

int lisa_flash_init_hook(FLASH_DEV *hal_dev)
{
    (void)hal_dev;

    return venusa_flash_send_mailbox_addr(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox, 0U);
}

int lisa_flash_init_done_hook(FLASH_DEV *hal_dev, int result)
{
    (void)hal_dev;
    (void)result;

    return venusa_flash_release_mailbox(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox);
}

int lisa_flash_read_done_hook(FLASH_DEV *hal_dev, size_t offset, size_t len, int result)
{
    (void)hal_dev;
    (void)offset;
    (void)len;
    (void)result;

    return venusa_flash_release_mailbox(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox);
}

int lisa_flash_write_hook(FLASH_DEV *hal_dev, size_t offset, size_t len)
{
    (void)hal_dev;
    (void)len;

    return venusa_flash_send_mailbox_addr(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox, offset);
}

int lisa_flash_write_done_hook(FLASH_DEV *hal_dev, size_t offset, size_t len, int result)
{
    (void)hal_dev;
    (void)offset;
    (void)len;
    (void)result;

    return venusa_flash_release_mailbox(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox);
}

int lisa_flash_erase_hook(FLASH_DEV *hal_dev, size_t offset, size_t size)
{
    (void)hal_dev;
    (void)size;

    return venusa_flash_send_mailbox_addr(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox, offset);
}

int lisa_flash_erase_done_hook(FLASH_DEV *hal_dev, size_t offset, size_t size, int result)
{
    (void)hal_dev;
    (void)offset;
    (void)size;
    (void)result;

    return venusa_flash_release_mailbox(&venusa_flash_request_mailbox, &venusa_flash_ack_mailbox);
}
