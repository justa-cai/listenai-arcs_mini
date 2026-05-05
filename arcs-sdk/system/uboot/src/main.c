#include "xutils.h"
// #include <stdio.h>
#include <stdlib.h>
// #include <string.h>

#include "syslog.h"
#include "cache.h"
#include "PSRAMManager.h"
#include "ClockManager.h"
#include "PowerManager.h"
#include "Driver_WDT.h"
// #include "FreeRTOS.h"
// #include "task.h"
// #include "timers.h"
// #include "stdio.h"
#include "arcs_ap.h"

// #include "adb.h"
// #include "adb_device.h"
// #include "adb_shell.h"
// #include "adb_sync.h"
#include "boot_adb_runtime.h"
#include "boot_config.h"
#ifdef CONFIG_BOOT_ADB
#include "boot_recovery_adb_gate.h"
#endif
#ifdef CONFIG_BOOT_DISPLAY
#include "boot_display.h"
#endif
#include "boot_version.h"

#include "boot_flash.h"
#include "trng.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "multi_heap.h"
#include "esp_heap_caps.h"
#ifdef CONFIG_BOOT_OTA_PACKAGE
#include "boot_config.h"
#include "boot_ota.h"
#include "boot_ota_handoff.h"
#include "boot_ota_lifecycle.h"
#include "boot_ota_request.h"
#include "boot_ota_source.h"
#endif
#include "boot_power_guard.h"
#ifdef CONFIG_BOOT_POWER_GUARD
#include "boot_config.h"
#endif

extern int boot_watchdog_feed(void);
extern int lisa_device_init(void);

#ifdef CONFIG_BOOT_ADB_SHELL
static void boot_flush_exec_range(const void *addr, uint32_t size)
{
    uintptr_t start = (uintptr_t)addr;
    uintptr_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1U;
    uintptr_t aligned_start = start & ~line_mask;
    uintptr_t aligned_end = (start + size + line_mask) & ~line_mask;

    HAL_FlushDCache_by_Addr((uint32_t *)aligned_start, (uint32_t)(aligned_end - aligned_start));
    __asm__ volatile("fence.i");
}

int lisa_log_init(void);
#endif

#define BOOT_MAIN_TASK_STACK_WORDS 2048

/* stage1 内部所有 CMN reset 入口都应通过此 helper：先把 latch 切到 AON
 * IOMUX force-output，避免 CMN reset 期间 GPIO 外设复位让电池模式下
 * MOSFET 栅极掉电。public_api_contract.py 要求本文件直接出现
 * __HAL_PMU_WholeChip_RST_ENABLE() 字面量，故保留在此 helper 内部。 */
static void main_reboot_cmn(void)
{
    boot_power_guard_lock_latch_for_reset();
    __HAL_PMU_WholeChip_RST_ENABLE();
}

#if 0
static const uint32_t psram_size[] = {0, 32, 0, 64, 0, 128, 512, 256};

TimerHandle_t usb_conn_timer;
static bool usb_connected = false;

void tud_mount_cb(void)
{
    printf("USB device connected.\n");
    usb_connected = true;
}

void tud_umount_cb(void)
{
    printf("USB device disconnected.\n");
}

void vTimerCallback(TimerHandle_t xTimer)
{
    if (usb_connected) {
        return;
    }

    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    info->boot_wdt = 1;
    printf("USB connected timeout, reboot!\n");
    vTaskDelay(pdMS_TO_TICKS(200));

    __asm__ volatile("fence.i");
    /* 为防止看门狗无法复位, 冗余软件复位 */
    info->req = 1;
    main_reboot_cmn();
}

void usb_task(void *arg)
{
    printk("usb task\n");

    usb_connected = false;

    /* 10秒无连接, 重启 */
    usb_conn_timer = xTimerCreate("usb-conn", pdMS_TO_TICKS(10 * 1000), pdFALSE, (void *)0, vTimerCallback);
    if (usb_conn_timer == NULL) {
        printk("usb task: create timer failed\n");
        return;
    }
    xTimerStart(usb_conn_timer, 0);

    // enable usb clock
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;   // Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; // 16bit mode

    adb_init();

#if CONFIG_ADB_SHELL
    adb_shell_init();
#endif

#if CONFIG_ADB_SYNC
    adb_sync_init();
#endif

    tud_disconnect(); // soft-disconnect from host
    // Initialize TinyUSB
    tusb_init();
    tud_connect(); // soft-connect to host

    while (1) {
        extern int boot_watchdog_feed(void);
        boot_watchdog_feed();
        tud_task();
    }
}
#endif

static void heap_summary(void)
{
    void heap_summary_info(void);
    heap_summary_info();
}
static void task_summary(void)
{
    uint32_t tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *const items = exram_malloc(sizeof(uint32_t), tasks * sizeof(TaskStatus_t));
    if (items) {
        uint32_t total = 0;
        tasks = uxTaskGetSystemState(items, tasks, &total);
        if (total > 0) {
            TRACE("%20s %9s %9s %9s %18s %12s %9s"
                , "[TASK]", "[STA.]", "[PRIO.]", "[STK/B]", "[TOTAL]", "[CALL.]", "[PCT.%]");
            for (TaskStatus_t *stat = items; stat < &items[tasks]; stat++) {
                TRACE("%9s@%p %9s %9d %9d %9d@%9p %12d %8d%%", stat->pcTaskName, stat->xHandle
                    , ((const char *[]){"RUN", "RDY", "BLK", "SUS", "DEL" })[stat->eCurrentState]
                    , (int)stat->uxCurrentPriority, (int)stat->usStackHighWaterMark * sizeof(StackType_t)
                    , (stat->pxEndOfStack - stat->pxStackBase)* sizeof(StackType_t), stat->pxStackBase
                    , (int)stat->ulRunTimeCounter, (int)(100.0f * stat->ulRunTimeCounter / total));
            }
            TRACE("");
        }
        exram_free(items);
    }
}

static void heap_travel_cb(void *start, void *end, multi_heap_info_t *info)
{
    printf("%-12p\t%-12p\t%-12d\t%-12d\t%-12d\n", start, end, info->total_free_bytes, info->total_allocated_bytes,
           info->minimum_free_bytes);
}

static void heap_info(void)
{
    void heap_caps_travel(void (*callback)(void *, void *, multi_heap_info_t *));
    printf("**********************heap info**********************\n");
    printf("%-12s\t%-12s\t%-12s\t%-12s\t%-12s\n", "start", "end", "free", "used", "free(min)");
    heap_caps_travel(heap_travel_cb);
    printf("\n");
}

#ifdef CONFIG_BOOT_OTA_PACKAGE
static int boot_ota_update_handler(const uboot_ota_request_t *req,
                                   uboot_ota_failure_info_t *failure,
                                   void *ctx)
{
    boot_ota_source_t source;
    int ret;

    (void)ctx;

    if (boot_ota_source_from_request(req, &source, failure) != 0) {
        printk("ota: request source convert failed\n");
        return -1;
    }

    ret = boot_ota_txz_update_from_source(NULL, &source, failure);
    if (ret == BOOT_OTA_SOURCE_ERR_UNAVAILABLE) {
        printk("ota: source unavailable, discard request\n");
        return BOOT_OTA_LIFECYCLE_FAILED_DISCARD_REQUEST;
    }

    return ret;
}

static void boot_ota_try_handle_update(void)
{
    int result;

    if (boot_ota_request_get_mode() != BOOT_MODE_UPDATE) {
        if (boot_ota_request_has_pending()) {
            /* flash 里还挂着一份上轮 OTA 的请求记录，说明 lifecycle 没跑
             * 完；RSVD4 里的 mode 被硬复位 / POR 清掉了，把它写回 UPDATE
             * 让下面继续跑。这样"erase 到一半被硬复位"的设备能续上。*/
            printk("ota: resume pending request after mode reset\n");
            boot_ota_request_set_mode(BOOT_MODE_UPDATE);
        } else {
            if (boot_ota_handoff_has_pending_update()) {
                printk("ota: clear stale pending handoff and reboot\n");
                boot_ota_handoff_clear_pending_update();
                /* CMN reset：AON SW reset 在 stage1 外设全开上下文会触发
                 * stage0 第一次 flash 读 fault 复位循环，见 power-management
                 * §3.2。CMN reset 保留 boot_info；CMN reset 在本芯片上会顺
                 * 手置 POR_STATUS，guard 仅靠 POR 会把全 0 的 boot_info 当
                 * 冷启动 + USB → charging_wait，借 resume_normal_boot 给
                 * guard 一个明确的 warm 信号。*/
                struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
                info->resume_normal_boot = 1;
                main_reboot_cmn();
            }
            return;
        }
    }

#ifdef CONFIG_BOOT_DISPLAY
    boot_display_init();
    boot_display_show_ota();
#endif

    result = boot_ota_lifecycle_run(boot_ota_update_handler, NULL);
    if (result == BOOT_OTA_LIFECYCLE_IDLE) {
        return;
    }

    if (result == BOOT_OTA_LIFECYCLE_UPDATED) {
        boot_ota_handoff_clear_pending_update();
        /* 见 stale handoff 路径：清掉 ota_pending 之后 boot_info 全 0，
         * CMN reset 顺手置 POR 会让 guard 误判冷启动。置 resume_normal_boot
         * 给 guard 明确的 warm 信号。*/
        struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
        info->resume_normal_boot = 1;
        printk("ota: lifecycle updated, reboot\n");
        main_reboot_cmn();
        return;
    }

    if (result == BOOT_OTA_LIFECYCLE_DISCARDED) {
        boot_ota_handoff_clear_pending_update();
        printk("ota: lifecycle discarded, continue normal boot\n");
        return;
    }

    printk("ota: lifecycle failed, retry after reboot\n");
    /* 失败路径要进 recovery 让用户介入。先清掉 ota_pending 防 gate 走
     * OTA 重试无限循环（gate 现在看到 req=1 + ota_pending=1 会直接路
     * 由到 OTA handler）。recovery_software_enter 单独留 req=1，gate
     * 兜底成 HARD_REQ 进 recovery UI。CMN reset 同上。*/
    boot_ota_handoff_clear_pending_update();
    boot_recovery_software_enter();
    main_reboot_cmn();
}
#endif

void main_task(void *p)
{
#ifdef CONFIG_BOOT_ADB_SHELL
    extern void boot_handshake_detect_init(void);
    boot_handshake_detect_init();
#endif

    boot_watchdog_feed();

    /* 双flash需要使用此引脚 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, CSK_IOMUX_FUNC_ALTER1);
    GPIO_Initialize(GPIOA(), NULL, NULL);

#ifdef CONFIG_BOOT_POWER_GUARD
    {
        struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
        if (info->charging_wait) {
            info->charging_wait = 0;

#ifdef CONFIG_BOOT_DISPLAY
            boot_display_init();
            boot_display_show_charging();
#endif
            /* 关机态插 USB：显示 3 秒充电提示后走 shutdown_req 进假关机自旋。
             * 期间持续轮询 POWER_KEY——用户在此时长按 3 秒就置 resume_normal_boot
             * 直接起系统，避免被迫先等这 3 秒过完、再经假关机态再长按 3 秒。
             * guard 入口已 latch(0)，期间拔 USB 则 VCC 掉 → 真关机。100ms
             * 粒度喂 boot watchdog，防它先超时把我们复位掉。*/
            const int tick_ms = 100;
            const int charging_total_ms = 3000;
            const int boot_hold_ms = 3000;
            int elapsed_ms = 0;
            int pb_held_ms = 0;
            bool confirm_boot = false;
            while (elapsed_ms < charging_total_ms || pb_held_ms > 0) {
                boot_watchdog_feed();
                vTaskDelay(pdMS_TO_TICKS(tick_ms));
                elapsed_ms += tick_ms;
                if (boot_power_guard_key_pressed()) {
                    pb_held_ms += tick_ms;
                    if (pb_held_ms >= boot_hold_ms) {
                        confirm_boot = true;
                        break;
                    }
                } else {
                    pb_held_ms = 0;
                }
            }
            if (confirm_boot) {
                info->resume_normal_boot = 1;
            } else {
                info->shutdown_req = 1;
            }
            main_reboot_cmn();
            while (1) {
                __asm__ volatile("wfi");
            }
        }
    }
#endif

#ifdef CONFIG_BOOT_DISPLAY
    {
        struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
        if (info->recover_reason != RECOVER_REASON_NONE) {
            boot_display_init();
            boot_display_show_recovery();
        }
    }
#endif

    boot_flash_init();
    /* set recover flag */
    boot_config_init();

#ifdef CONFIG_BOOT_OTA_PACKAGE
    boot_ota_try_handle_update();
#endif

#ifdef CONFIG_BOOT_ADB
    uint32_t boot_info_raw = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

    if (boot_recovery_adb_should_start(boot_info_raw)) {
        (void)boot_adb_runtime_start(boot_info_raw);
    } else {
        boot_adb_runtime_log_skip(boot_info_raw);
    }
#endif

#if CONFIG_DISK_DRIVER && !(CONFIG_BOOT_ADB && CONFIG_ADB_SYNC)
    /* Sync-enabled recovery ADB owns /SD: preparation before USB connect. */
    extern int disk_init(const void *dev);
    disk_init(NULL);
#endif

    uint32_t last_time = pdTICKS_TO_MS(xTaskGetTickCount());
    while (1) {
        extern int boot_watchdog_feed(void);
        boot_watchdog_feed();
        vTaskDelay(500);
#ifdef SHOW_HEAP_INFO
        uint32_t curr_time = pdTICKS_TO_MS(xTaskGetTickCount());
        if ((curr_time - last_time) >= 3000) {
            heap_info();
            last_time = curr_time;
        }
#endif
    }

    vTaskDelete(NULL);
}

static char id_str[16 + 1] = {0};
const char *device_id_str_get(void)
{
    uint8_t fake_id[6] = {0};

    if (strlen(id_str) != 0) {
        return id_str;
    }
    uint8_t *id_1 = (uint8_t *)0x48600208;
    uint8_t *id_2 = (uint8_t *)0x4860020c;
    int i;

    uint32_t *id1 = (uint32_t *)id_1;
    uint32_t *id2 = (uint32_t *)id_2;

    if (*id1 == 0 && *id2 == 0) {
        uint32_t olen = 0;
        trng_get(fake_id, 6, &olen);
        sprintf(id_str, "fake");
        char *s = id_str + 4;
        for (i = 0; i < 6; i++) {
            sprintf(s + i * 2, "%02x", fake_id[i]);
        }
    } else {
        for (i = 0; i < 8; i++) {
            sprintf(id_str + i * 2, "%02x", id_1[i]);
        }
        for (i = 0; i < 8; i++) {
            sprintf(id_str + i * 2 + 8, "%02x", id_2[i]);
        }
    }

    id_str[16] = '\0';

    return id_str;
}

void boot_mode_reason_print(void)
{
    const char *reason_str[RECOVER_REASON_MAX] = {
        "none", "soft req", "hard req", "app invalid", "ap wdt timeout", "boot wdt timeout",
    };

    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    const char *reason = info->recover_reason >= RECOVER_REASON_MAX ? "unknown" : reason_str[info->recover_reason];

    printk("boot reason: %s, handshake timeout: %d\n", reason, info->handshake_timeout);
}

static void mem_dump(uint8_t *addr, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i += 16) {
        boot_watchdog_feed();
        printk("%08x: ", addr + i);
        uint32_t j;
        for (j = 0; j < 16; j++) {
            printk("%02x ", addr[i + j]);
        }
        printk("\n");
    }
}

static void psram_test(void)
{
    uint8_t test[256];
    uint32_t olen = 0;
    int rt = trng_get(test, 256, &olen);
    if (rt != 0 || olen != 256) {
        printk("trng get failed, %d, %d\n", rt, olen);
        while (1) {
        }
    }

    uint8_t *addr = (uint8_t *)0x28000000;
    uint32_t size = 16 * 1024 * 1024;

    uint32_t i;
    for (i = 0; i < size; i += 256) {
        memcpy(addr + i, test, 256);
        boot_watchdog_feed();
        HAL_FlushDCache_by_Addr((uint32_t *)(addr + i), 256);
        __asm__ volatile("fence.i");
        HAL_InvalidateDCache_by_Addr((uint32_t *)(addr + i), 256);
        __asm__ volatile("fence.i");
        for (uint32_t j = 0; j < 256; j++) {
            if (addr[i + j] != test[j]) {
                printk("psram test failed, addr: 0x%08x\n", addr + i + j);
                mem_dump(addr + i, 256);
                boot_watchdog_feed();
                printk("test data: \n");
                mem_dump(test, 256);
                while (1) {
                }
            }
        }
    }

    printk("psram test passed\n");
}

void main(void)
{
#ifdef CONFIG_BOOT_LOG
    syslog_init(CONFIG_SYSLOG_UART_PORT, CONFIG_SYSLOG_UART_BAUDRATE);
#endif
    extern int boot_watchdog_init(void);
    boot_watchdog_init();
#ifdef CONFIG_BOOT_LOG
    printk("\n\n------------------------------------------------\n");
    printk("boot running...\n");
    printk("boot version: %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
    printk("boot commit: %s\n", VERSION_COMMIT);
    printk("chip cfg: 0x%04x\n", (*((uint32_t *)0x48600204) >> 15));
    printk("serial num: %s\n", device_id_str_get());
    boot_mode_reason_print();
#endif
    boot_watchdog_feed();

    extern int sysheap_init(void);
    sysheap_init();

#ifdef CONFIG_BOOT_DISPLAY
    /* 驱动被 relocate 到 PSRAM，先把代码/数据从 flash 拷过去；DCache 非一致，
     * 需要 flush 后 fence.i，下一次取指才能看到新代码。*/
    extern void scatload_psram(void);
    extern void HAL_FlushDCache_by_Addr(uint32_t *addr, uint32_t dsize);
    scatload_psram();
    HAL_FlushDCache_by_Addr((uint32_t *)CONFIG_MEM_PSRAM_BASE, CONFIG_MEM_PSRAM_SIZE);
    __asm__ volatile("fence.i");
#endif

#ifdef CONFIG_BOOT_ADB_SHELL
    boot_flush_exec_range((const void *)lisa_log_init, 256U);
    lisa_log_init();
#endif

#ifdef CONFIG_LISA_DEVICE
    lisa_device_init();
#endif

    // extern void trace_init(void);
    // trace_init();

    BaseType_t r = xTaskCreate(main_task, "main", BOOT_MAIN_TASK_STACK_WORDS, NULL, OS_PRIO_DEF, NULL);
    if (r != pdPASS) {
        printk("xTaskCreate failed\n");
        while (1) {
        }
    }
    vTaskStartScheduler();

    while (1) {
    }
}

#include "sys/stat.h"

__attribute__((weak, used)) int _kill(int pid, int sig)
{
    return -1;
}

__attribute__((weak, used)) int _stat(const char *file, struct stat *st)
{
    return -1;
}

__attribute__((weak, used)) int _lseek(int file, int ptr, int dir)
{
    return -1;
}

__attribute__((weak, used)) int _open(const char *name, int flags, int mode)
{
    return -1;
}

__attribute__((weak, used)) int _close(int file)
{
    return -1;
}

__attribute__((weak, used)) int _read(int file, char *ptr, int len)
{
    return -1;
}

__attribute__((weak, used)) _ssize_t _write(int file, char *ptr, size_t len)
{
    return -1;
}

/* Reentrant versions that call the base implementations */
__attribute__((weak, used)) int _fstat_r(struct _reent *r, int file, struct stat *st)
{
    return -1;
}

__attribute__((weak, used)) int _getpid_r(struct _reent *r)
{
    return -1;
}
__attribute__((weak, used)) int _getpid(void)
{
    return 1;
}
__attribute__((weak, used)) int _isatty(int fd)
{
    return 1;
}
