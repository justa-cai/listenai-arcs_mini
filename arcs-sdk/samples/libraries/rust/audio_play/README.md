# Rust Audio Playback (`audio_play`)

Plays two short generated tones — **1 kHz** then **500 Hz**, 0.8 s each with a
0.2 s gap — on the `lisa_audio` DAC (`audio0`), entirely from Rust.

It exercises the R6 `arcs::Audio` API:

```rust
let audio = arcs::Audio::open(c"audio0")?;
audio.configure_play(&PlayConfig { sample_rate: RATE_16K, channels: CH_LEFT,
                                   sample_bits: BIT_16, .. })?;
audio.play_start()?;
audio.write_all(&pcm)?;   // blocks while the driver's DMA pool is full
audio.flush()?;           // wait for the last buffer to drain
audio.play_stop()?;
```

Tones are synthesized from **integer** sine lookup tables (no FPU / no
floating-point). The PCM working buffer is allocated in **PSRAM** through the
R5 dual allocator (`RawBox::new_in(Zone::Psram, ..)`), and the audio driver's
own DMA buffer pool is allocated from the PSRAM heap as well.

## Format

- 16 kHz sample rate, 16-bit, mono (`CH_LEFT`)
- 12 × 256-sample DMA buffer pool
- −12 dB digital gain (matches the C/Zig reference samples)

## Build & flash (arcs_evb)

```sh
export ARCS_BASE=$PWD
bash build.sh -C -S samples/libraries/rust/audio_play -DBOARD=arcs_evb
# flash build/rust_audio_play.bin with cskburn, then watch the serial console
```

Expected serial output (EasyLogger):

```
=== Rust audio playback demo ===
audio_play: audio0 opened
audio_play: configured 16kHz/16bit/mono
audio_play: pcm buffer @ 0x28xxxxxx (PSRAM)
audio_play: play started
audio_play: 1kHz tone done
audio_play: 500Hz tone done
audio_play: flushed
audio_play:done
```

Connect a headphone / line-in to the board's line-out (`LIN_OUTP/N`) to hear
the two beeps. To drive the on-board speaker amplifier instead, enable the PA
in `prj.conf`:

```
CONFIG_LISA_AUDIO_PLAY_PA_ENABLE=y
CONFIG_LISA_AUDIO_PLAY_PA_PIN=27
```
