/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_audio_play"
#include <lisa_log.h>

extern int rust_main(void);

/*
 * Plays two short tones (1 kHz then 500 Hz) on the "audio0" DAC. No pinmux is
 * needed here: the audio driver claims its pads from Kconfig, so this is a
 * thin shim into the Rust entry point.
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust audio playback demo ===");
    int ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }
    return 0;
}
