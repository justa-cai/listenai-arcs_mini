/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_panic_reboot"
#include <lisa_log.h>

extern int rust_main(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust panic-reboot demo ===");
    /* rust_main panics; with the panic-reboot feature the panic handler resets
     * the chip, so this call does not return (the board reboots). */
    int ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }
    return 0;
}
