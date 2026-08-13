#![no_std]

extern crate alloc;

use arcs::audio::{PlayConfig, BIT_16, CH_LEFT, RATE_16K};
use arcs::heap::{RawBox, Zone};

const SAMPLE_RATE: u32 = 16_000;
const CHUNK: usize = 256;

// 1 kHz sine lookup table: 16 points = 16 kHz / 1 kHz, amplitude ~16000.
// Integer table avoids any floating-point in no_std (the SoC has no FPU here).
const SINE_1K: [i16; 16] = [
    0, 6180, 11585, 15137, 16000, 15137, 11585, 6180, 0, -6180, -11585, -15137, -16000, -15137,
    -11585, -6180,
];

// 500 Hz sine lookup table: 32 points = 16 kHz / 500 Hz.
const SINE_500: [i16; 32] = [
    0, 3121, 6180, 9102, 11585, 13623, 15137, 15956, 16000, 15956, 15137, 13623, 11585, 9102, 6180,
    3121, 0, -3121, -6180, -9102, -11585, -13623, -15137, -15956, -16000, -15956, -15137, -13623,
    -11585, -9102, -6180, -3121,
];

/// Fill `buf` from `lut` (cycling) and stream it until `total` samples are sent.
fn play_tone(audio: &arcs::Audio, buf: &mut [i16], lut: &[i16], total: u32) -> arcs::Result<()> {
    let cap = buf.len() as u32;
    let n = lut.len() as u32;
    let mut written = 0u32;
    while written < total {
        let chunk = (total - written).min(cap) as usize;
        for (i, s) in buf[..chunk].iter_mut().enumerate() {
            *s = lut[((written + i as u32) % n) as usize];
        }
        audio.write_all(&buf[..chunk])?;
        written += chunk as u32;
    }
    Ok(())
}

/// Stream `total` samples of silence (used as inter-tone gaps).
fn play_silence(audio: &arcs::Audio, buf: &mut [i16], total: u32) -> arcs::Result<()> {
    let cap = buf.len() as u32;
    let mut written = 0u32;
    while written < total {
        let chunk = (total - written).min(cap) as usize;
        buf[..chunk].iter_mut().for_each(|s| *s = 0);
        audio.write_all(&buf[..chunk])?;
        written += chunk as u32;
    }
    Ok(())
}

#[arcs::main]
fn main() -> arcs::Result<()> {
    let audio = arcs::Audio::open(c"audio0")?;
    arcs::log::info!("audio_play: audio0 opened");

    let cfg = PlayConfig {
        sample_rate: RATE_16K,
        channels: CH_LEFT,
        sample_bits: BIT_16,
        analog_gain: 0,
        digital_gain: -12,
        buffer_count: 12,
        buffer_samples: CHUNK as u16,
    };
    audio.configure_play(&cfg)?;
    arcs::log::info!("audio_play: configured 16kHz/16bit/mono");

    // PCM working buffer lives in PSRAM via the R5 dual allocator. The address
    // logged below should start at 0x28.. (PSRAM cached alias).
    let mut pcm: RawBox<[i16; CHUNK]> = RawBox::new_in(Zone::Psram, [0i16; CHUNK])?;
    arcs::log::info!("audio_play: pcm buffer @ {:p} (PSRAM)", pcm[..].as_ptr());

    audio.play_start()?;
    arcs::log::info!("audio_play: play started");

    // 1 kHz tone (0.8 s) -> short gap -> 500 Hz tone (0.8 s) -> short gap.
    play_tone(&audio, &mut pcm[..], &SINE_1K, SAMPLE_RATE * 8 / 10)?;
    arcs::log::info!("audio_play: 1kHz tone done");
    play_silence(&audio, &mut pcm[..], SAMPLE_RATE * 2 / 10)?;
    play_tone(&audio, &mut pcm[..], &SINE_500, SAMPLE_RATE * 8 / 10)?;
    arcs::log::info!("audio_play: 500Hz tone done");
    play_silence(&audio, &mut pcm[..], SAMPLE_RATE * 2 / 10)?;

    audio.flush()?;
    arcs::log::info!("audio_play: flushed");
    audio.play_stop()?;
    arcs::log::info!("audio_play:done");
    Ok(())
}
