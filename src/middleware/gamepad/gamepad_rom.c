/*
 * Gamepad ROM 动态加载: WS 推送的 ROM 暂存与生命周期管理。
 * 设计说明见 gamepad_rom.h 头注释; 协议见 doc/gamepad-protocol.md §4.6。
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "gamepad_rom.h"
#include "lisa_log.h"
#include "sysheap.h"

#define TAG "gamepad"

/* --- 传输态: 仅 WS 线程触碰, 无需加锁 --- */
static uint8_t *s_rx_buf = NULL;      /* 接收缓冲 (传输中) */
static uint32_t s_rx_size = 0;        /* rom_begin 声明的总大小 */
static uint32_t s_rx_recv = 0;        /* 已收字节数 */
static uint32_t s_rx_crc = 0;         /* rom_begin 声明的 crc32 (0=跳过校验) */

/* --- staging 状态: WS 线程写 (swap), LVGL 线程读 (acquire/retire), 临界区保护 --- */
static uint8_t *s_staged = NULL;      /* 当前生效的 staged ROM */
static uint32_t s_staged_size = 0;
static uint32_t s_gen = 0;            /* staging 代数, 每次 rom_end +1; 0=无 */
static uint32_t s_loaded_gen = 0;     /* presenter 当前加载的代数 (acquire 置, retire 清) */
static uint8_t *s_zombie = NULL;      /* 被替换但核心仍可能在读的旧缓冲 */
static uint32_t s_zombie_gen = 0;

/* 标准 CRC-32 (IEEE 802.3, zlib 兼容); 位循环实现, 256KB 约 2M 次迭代, 耗时几十 ms */
static uint32_t rom_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)(-(int32_t)(crc & 1U)));
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

/* --- WS 线程: 传输控制 --- */
int gamepad_rom_begin(uint32_t size, uint32_t crc32)
{
    if (size < 16U || size > GAMEPAD_ROM_MAX_BYTES) {
        LISA_LOGE(TAG, "rom_begin: bad size %u", (unsigned)size);
        return -1;
    }
    gamepad_rom_cancel();

    s_rx_buf = (uint8_t *)exram_malloc(4, size);
    if (!s_rx_buf) {
        LISA_LOGE(TAG, "rom_begin: alloc %u failed", (unsigned)size);
        return -1;
    }
    s_rx_size = size;
    s_rx_recv = 0;
    s_rx_crc = crc32;
    LISA_LOGI(TAG, "rom_begin: %u bytes (crc32=0x%08X)", (unsigned)size, (unsigned)crc32);
    return 0;
}

int gamepad_rom_data(const uint8_t *data, uint32_t len)
{
    if (!s_rx_buf) {
        return -1;
    }
    if (len > s_rx_size - s_rx_recv) {
        LISA_LOGE(TAG, "rom_data: overflow %u+%u > %u",
                  (unsigned)s_rx_recv, (unsigned)len, (unsigned)s_rx_size);
        gamepad_rom_cancel();
        return -1;
    }
    memcpy(s_rx_buf + s_rx_recv, data, len);
    s_rx_recv += len;
    return 0;
}

int gamepad_rom_end(const char **err_msg)
{
    /* 静态缓冲: 仅 WS 线程使用, 无并发 */
    static char s_err[64];

    if (err_msg) {
        *err_msg = "";
    }
    if (!s_rx_buf) {
        if (err_msg) {
            *err_msg = "no transfer in progress";
        }
        return -1;
    }
    if (s_rx_recv != s_rx_size) {
        snprintf(s_err, sizeof(s_err), "size mismatch: %u/%u",
                 (unsigned)s_rx_recv, (unsigned)s_rx_size);
        LISA_LOGE(TAG, "rom_end: %s", s_err);
        if (err_msg) {
            *err_msg = s_err;
        }
        gamepad_rom_cancel();
        return -1;
    }
    if (memcmp(s_rx_buf, "NES\x1a", 4) != 0) {
        if (err_msg) {
            *err_msg = "not an iNES/NES2.0 image";
        }
        gamepad_rom_cancel();
        return -1;
    }
    if (s_rx_crc != 0U && rom_crc32(s_rx_buf, s_rx_size) != s_rx_crc) {
        uint32_t got = rom_crc32(s_rx_buf, s_rx_size);
        snprintf(s_err, sizeof(s_err), "crc32 mismatch: %08X!=%08X",
                 (unsigned)got, (unsigned)s_rx_crc);
        if (err_msg) {
            *err_msg = s_err;
        }
        gamepad_rom_cancel();
        return -1;
    }

    /* 换入 staging: 旧缓冲若仍被核心引用 (loaded) 则成为 zombie, 等 retire 释放 */
    uint8_t *to_free = NULL;
    uint8_t *new_rom = s_rx_buf;
    uint32_t new_size = s_rx_size;

    taskENTER_CRITICAL();
    uint8_t *old = s_staged;
    uint32_t old_gen = s_gen;
    s_staged = new_rom;
    s_staged_size = new_size;
    s_gen = old_gen + 1U;
    if (old) {
        if (s_loaded_gen == old_gen) {
            if (s_zombie) {
                /* 理论不可达: zombie 在场说明 loaded 停在更早的代, 与上面条件矛盾 */
                LISA_LOGW(TAG, "rom_end: zombie slot busy, old buf leaked");
            }
            s_zombie = old;
            s_zombie_gen = old_gen;
        } else {
            to_free = old;
        }
    }
    taskEXIT_CRITICAL();

    /* 临界区外释放 */
    if (to_free) {
        exram_free(to_free);
    }

    /* 传输态复位 (缓冲所有权已移交 staging, 不能 cancel) */
    s_rx_buf = NULL;
    s_rx_size = 0;
    s_rx_recv = 0;
    s_rx_crc = 0;

    LISA_LOGI(TAG, "rom_end: staged gen=%u size=%u (zombie=%u)",
              (unsigned)s_gen, (unsigned)new_size, s_zombie ? 1U : 0U);
    return 0;
}

void gamepad_rom_cancel(void)
{
    if (s_rx_buf) {
        exram_free(s_rx_buf);
        s_rx_buf = NULL;
    }
    s_rx_size = 0;
    s_rx_recv = 0;
    s_rx_crc = 0;
}

/* --- LVGL 线程: 消费 --- */
uint32_t gamepad_rom_acquire(const uint8_t **buf, uint32_t *size)
{
    taskENTER_CRITICAL();
    uint32_t gen = s_gen;
    if (buf) {
        *buf = s_staged;
    }
    if (size) {
        *size = s_staged_size;
    }
    if (s_staged) {
        s_loaded_gen = s_gen;
    }
    taskEXIT_CRITICAL();
    return gen;
}

uint32_t gamepad_rom_staged_gen(void)
{
    taskENTER_CRITICAL();
    uint32_t gen = s_gen;
    taskEXIT_CRITICAL();
    return gen;
}

void gamepad_rom_retire(uint32_t gen)
{
    uint8_t *to_free = NULL;

    taskENTER_CRITICAL();
    if (s_zombie && s_zombie_gen <= gen) {
        to_free = s_zombie;
        s_zombie = NULL;
    }
    if (s_loaded_gen != 0U && s_loaded_gen <= gen) {
        s_loaded_gen = 0;
    }
    taskEXIT_CRITICAL();

    if (to_free) {
        exram_free(to_free);
        LISA_LOGI(TAG, "rom_retire: zombie gen<=%u freed", (unsigned)gen);
    }
}
