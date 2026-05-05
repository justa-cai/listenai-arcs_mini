#include "chip.h"
#include "spiflash.h"
#include "arcs_ap.h"
#include "boot_flash.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define __boot_flash_ramcode__ __attribute__((section(".boot_f.ramcode")))

/* 单次擦除粒度：boot 看门狗配置 int_time_15 @ 32kHz 约 1s，必须比这小不少。
 * 16KB 在典型 NOR flash 上约 4 个 4KB sector，耗时百毫秒量级，留足余量；
 * 更小粒度 session 进出占比会过高。 */
#define BOOT_FLASH_ERASE_CHUNK (16U * 1024U)

extern int boot_watchdog_feed(void);

static int boot_flash_register_set(uint32_t addr, uint32_t data_in, uint32_t data_mask, uint32_t *data_out);
static int boot_flash_register_get(uint32_t addr, uint32_t *data_out);

static bool boot_flash_auto_unlock = true;
static uint32_t boot_flash_session_depth;

#if CONFIG_DUAL_FLASH
static struct flash_status_register fl_status[2] = {0};
#else
static struct flash_status_register fl_status[1] = {0};
#endif

static FLASH_DEV flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF,        // divider is 1
    /* OTA erase/program runs under FreeRTOS in second stage; keep flash ops in a critical section. */
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 2000000,
    .addr_bytes = 3,
    .addr_auto = 0,
};

struct flash_status_register *boot_flash_status_register_get(uint8_t idx)
{
    if (idx >= ARRAY_SIZE(fl_status)) {
        return NULL;
    }

    return &fl_status[idx];
}

static void boot_flash_select_idx(uint8_t idx)
{
    if (idx) {
        outw(0x47600058, 0xffff0006);
    } else {
        outw(0x47600058, 0xffff0009);
    }
}

static void boot_flash_select_all(void)
{
    outw(0x47600058, 0xffff0000);
}

int boot_flash_reg_set(uint8_t idx, uint32_t addr, uint32_t data_in, uint32_t data_mask, uint32_t *data_out)
{
    int r;

    boot_flash_select_idx(idx);
    r = boot_flash_register_set(addr, data_in, data_mask, data_out);
    boot_flash_select_all();
    if (r) {
        printf("boot flash register set failed, idx:%d, addr:0x%08x, data_in:0x%08x, data_mask:0x%08x\n", idx, addr,
               data_in, data_mask);
    }

    return r;
}

int boot_flash_reg_get(uint8_t idx, uint32_t addr, uint32_t *data_out)
{
    int r;

    boot_flash_select_idx(idx);
    r = boot_flash_register_get(addr, data_out);
    boot_flash_select_all();
    if (r) {
        printf("boot flash register get failed, idx:%d, addr:0x%08x, data_out:0x%08x\n", idx, addr, *data_out);
    }

    return r;
}

static int boot_flash_register_set(uint32_t addr, uint32_t data_in, uint32_t data_mask, uint32_t *data_out)
{
    uint32_t data_ret = 0;
    uint32_t set_bits;
    uint32_t read_bits;
    int res = -1;

    // set the related status register bits
    res = flash_status_register_get(&flash_dev, addr, &data_ret);
    if (res) {
        return -1;
    }

    set_bits = data_in & data_mask;
    read_bits = data_ret & data_mask;
    if (set_bits == read_bits) {
        *data_out = data_ret;
        return 0;
    }

    data_ret &= (~data_mask);
    data_in &= data_mask;
    data_in |= data_ret;

    flash_write_protection_set(&flash_dev, false);
    res = flash_status_register_set(&flash_dev, addr, data_in, data_out);
    flash_write_protection_set(&flash_dev, true);

    if (res) {
        return -1;
    }

    return 0;
}

static int boot_flash_register_get(uint32_t addr, uint32_t *data_out)
{
    return flash_status_register_get(&flash_dev, addr, data_out);
}

#define FLASH0_START (0)
#define FLASH0_SIZE  (1024 * 1024 * 16)
#define FLASHDL_BASE (0x47700000)

int boot_flash_init(void)
{
    int r;
#if CONFIG_DUAL_FLASH
    __disable_irq();
    __RWMB();
    __FENCE_I();
    uint32_t dat = inw(FLASHDL_BASE);
    dat &= ~(1UL << 17);
    outw(FLASHDL_BASE, dat);  // disable delay line

#endif
    r = flash_init(&flash_dev, 0, 0);
    if (r != 0) {
        printf("boot flash init failed, r = %d\n", r);
        return r;
    }

#if CONFIG_DUAL_FLASH
    outw(0x47600054, ((FLASH0_SIZE >> 12) << 16) | FLASH0_START);
    outw(0x47600058, 0xffff0000);
    __FENCE_I();
    __enable_irq();
#endif

    int i;
    for (i = 0; i < ARRAY_SIZE(fl_status); i++) {
        boot_flash_reg_get(i, 1, &fl_status[i].register_1);
        boot_flash_reg_get(i, 2, &fl_status[i].register_2);
        boot_flash_reg_get(i, 3, &fl_status[i].register_3);
        printf("flash %d status register: 0x%04x, 0x%04x, 0x%04x\n", i, fl_status[i].register_1,
               fl_status[i].register_2, fl_status[i].register_3);
    }

    return 0;
}

void boot_flash_lock_clear(void)
{
    int i;
    uint32_t data_out;

    for (i = 0; i < ARRAY_SIZE(fl_status); i++) {
        boot_flash_reg_set(i, 1, 0, 0x7c, &data_out);
        boot_flash_reg_set(i, 2, 0, 0x40, &data_out);
    }
}

void boot_flash_lock_boot(void)
{
    uint32_t data_out;

    boot_flash_reg_set(0, 1, 0, 0x24, &data_out);
    boot_flash_reg_set(0, 2, 0, 0x40, &data_out);
}

void boot_flash_lock_resume(void)
{
    int i;
    uint32_t data_out;

    for (i = 0; i < ARRAY_SIZE(fl_status); i++) {
        boot_flash_reg_set(i, 1, fl_status[i].register_1, 0x7c, &data_out);
        boot_flash_reg_set(i, 2, fl_status[i].register_2, 0x40, &data_out);
    }
}

void boot_flash_session_begin(void)
{
    if (boot_flash_session_depth == 0) {
        boot_flash_lock_clear();
        flash_write_protection_set(&flash_dev, false);
    }

    boot_flash_session_depth++;
}

void boot_flash_session_end(void)
{
    if (boot_flash_session_depth == 0) {
        return;
    }

    boot_flash_session_depth--;
    if (boot_flash_session_depth == 0) {
        flash_write_protection_set(&flash_dev, true);
        boot_flash_lock_resume();
    }
}

bool boot_flash_auto_unlock_mode_set(bool m)
{
    boot_flash_auto_unlock = m;

    return 0;
}

bool boot_flash_auto_unlock_mode_get(void)
{
    return boot_flash_auto_unlock;
}

__boot_flash_ramcode__ void boot_flash_read(uint8_t *src, uint8_t *dst, uint32_t size)
{
    if (src == NULL || dst == NULL || size == 0) {
        return;
    }

    memcpy(dst, src, size);
}

int boot_flash_id_get(uint32_t *id)
{
    int r = spirom_cmd_send(&flash_dev, SPIROM_CMD_RDID, 0x0, 0, NULL, (unsigned int *)id);

    if (r) {
        return -1;
    }

    *id &= SPIROM_ID_MASK;

    return 0;
}

int boot_flash_size_get(uint32_t *size)
{
    uint32_t id = 0;
    int r = boot_flash_id_get(&id);

    if (r) {
        return -1;
    }

    *size = 2 << (((id >> 16) & 0xFF) - 1);
#if CONFIG_DUAL_FLASH
    *size *= 2;
#endif

    return 0;
}

__boot_flash_ramcode__ static void boot_flash_commit(uint8_t *addr, uint32_t size)
{
    HAL_InvalidateDCache_by_Addr((uint32_t *)addr, size);
    __RWMB();
}

__boot_flash_ramcode__ int boot_flash_write(uint8_t *addr, uint8_t *data, uint32_t size)
{
    int r;
    bool own_session = (boot_flash_session_depth == 0);

    boot_watchdog_feed();

    if (own_session) {
        boot_flash_session_begin();
    }
    r = flash_write(&flash_dev, (off_t)(addr - 0x30000000), data, size);
    if (own_session) {
        boot_flash_session_end();
    }

    if (r == 0) {
        boot_flash_commit(addr, size);
    }

    return r;
}

__boot_flash_ramcode__ int boot_flash_erase(uint8_t *addr, uint32_t size)
{
    int r = 0;
    bool own_session = (boot_flash_session_depth == 0);

    /* 大块擦除（几 MB 级别 app 分区）需要分段喂狗：以 sector 粒度循环，
     * 每轮重进 flash session，途中可以安全调用 boot_watchdog_feed（不在
     * flash session 内，指令取自 flash 没问题）。 */
    uint32_t erased = 0;
    while (erased < size) {
        uint32_t chunk = size - erased;
        if (chunk > BOOT_FLASH_ERASE_CHUNK) {
            chunk = BOOT_FLASH_ERASE_CHUNK;
        }

        boot_watchdog_feed();

        if (own_session) {
            boot_flash_session_begin();
        }
        r = flash_erase(&flash_dev, (off_t)(addr + erased - 0x30000000), chunk);
        if (own_session) {
            boot_flash_session_end();
        }
        if (r != 0) {
            break;
        }
        erased += chunk;
    }

    if (r == 0) {
        boot_flash_commit(addr, size);
    }

    return r;
}
