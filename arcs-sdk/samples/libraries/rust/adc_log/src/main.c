/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_adc_log"
#include <lisa_log.h>

extern int rust_main(void);

/*
 * Reads the internal VBAT (ch6) and TEMP (ch7) channels, which need no pinmux,
 * so this entry is a thin shim (no lisa_adc_pinmux override required).
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust ADC read-and-log demo ===");
    int ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }
    return 0;
}
