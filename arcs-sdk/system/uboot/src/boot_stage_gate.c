#include "boot_stage_gate.h"

static bool boot_stage_gate_has_persistent_recovery(const struct boot_config *cfg)
{
    return boot_config_has_recovery_request(cfg);
}

static void boot_stage_gate_note_persistent_recovery(uint32_t *boot_info_raw)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    if (boot_info_raw == NULL) {
        return;
    }

    bits.raw = *boot_info_raw;
    if (bits.info.recover_reason == RECOVER_REASON_NONE) {
        bits.info.recover_reason = RECOVER_REASON_HARD_REQ;
        *boot_info_raw = bits.raw;
    }
}

static void boot_stage_gate_note_transient_recovery(uint32_t *boot_info_raw)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    if (boot_info_raw == NULL) {
        return;
    }

    bits.raw = *boot_info_raw;
    if (bits.info.recover_reason != RECOVER_REASON_NONE) {
        return;
    }

    if (bits.info.boot_wdt != 0u) {
        bits.info.recover_reason = RECOVER_REASON_BOOT_WDT_TIMEOUT;
        *boot_info_raw = bits.raw;
        return;
    }

    if (bits.info.handshake_timeout != 0u) {
        bits.info.recover_reason = RECOVER_REASON_AP_WDT_TIMEOUT;
        *boot_info_raw = bits.raw;
        return;
    }

    /* OTA 路由：app 通过 uboot_ota_start 同时置 req=1 + ota_pending=1，
     * 走 CMN reset 后 boot_info 保留，gate 在这里命中 req 分支。但这是
     * 升级意图不是 recovery，不要兜底 HARD_REQ 污染 recover_reason，否
     * 则 stage1 会按 recovery 渲染 UI 并跳过 OTA handler。 */
    if (bits.info.ota_pending != 0u) {
        return;
    }

    /* req=1 但没填 recover_reason：兜底按 HARD_REQ 处理，让 main_task
     * 起 recovery UI + ADB。不然会落到既不显示也不起 ADB 的 while(1)
     * 假死态（老 app 漏写 recover_reason / adb_reboot.c 走 do_recovery
     * 时就是这种情况）。*/
    bits.info.recover_reason = RECOVER_REASON_HARD_REQ;
    *boot_info_raw = bits.raw;
}

bool boot_stage_gate_should_enter_second_stage(uint32_t *boot_info_raw, const struct boot_config *cfg)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    if (boot_info_raw != NULL) {
        bits.raw = *boot_info_raw;

        if (bits.info.req != 0u) {
            boot_stage_gate_note_transient_recovery(boot_info_raw);
            bits.raw = *boot_info_raw;
            bits.info.req = 0u;
            *boot_info_raw = bits.raw;
            return true;
        }

        if (bits.info.ota_pending != 0u) {
            return true;
        }

        if (bits.info.charging_wait != 0u) {
            return true;
        }
    }

    if (!boot_stage_gate_has_persistent_recovery(cfg)) {
        return false;
    }

    boot_stage_gate_note_persistent_recovery(boot_info_raw);
    return true;
}
