/*
 * ss_audio.h - microphone PCM tap for SoundSense streaming
 *
 * Registers a second lisa_audio observer (the wakeup engine owns the first
 * slot) and copies the LEFT channel (the microphone; right is the AEC
 * reference) into a PSRAM drop-oldest ring buffer. The sender task in
 * ss_core drains it; if nobody drains (disconnected) the buffer simply
 * wraps and old audio is discarded - bounded memory, no back-pressure on
 * the audio dispatch thread.
 */
#ifndef SS_AUDIO_H
#define SS_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

/* mono 16 kHz s16 PCM tap */
int ss_audio_start(void);
void ss_audio_stop(void);
bool ss_audio_is_running(void);

/* Read up to len bytes of mono PCM into buf (oldest first).
 * Returns bytes actually read (0 when empty). Never blocks for more than
 * timeout_ms. When the ring overflows the oldest data is dropped first. */
uint32_t ss_audio_read(uint8_t *buf, uint32_t len, uint32_t timeout_ms);

#endif /* SS_AUDIO_H */
