/*
 * pet_buzzer.h - piezo beep synthesis for the pet
 */
#ifndef PET_BUZZER_H
#define PET_BUZZER_H

#include <stdint.h>

/* Start the beep worker thread. Safe to call pet_buzzer_post() before init
 * (segments are dropped). */
int pet_buzzer_init(void);

/* Post one beep segment (frequency in Hz, duration in ms). Called from the
 * emulator thread when the ROM stops the piezo (duration is only known then).
 * Values are clamped; segments are dropped while TTS/alarm is active. */
void pet_buzzer_post(uint32_t freq_hz, uint32_t dur_ms);

#endif /* PET_BUZZER_H */
