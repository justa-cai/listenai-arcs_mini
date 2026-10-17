# ARCS-MINI LLM Terminal

An offline-first LLM chat terminal running entirely on the [LISTENAI ARCS-MINI](../../../arcs-mini)
(RISC-V dual-core SoC, 240×240 LCD). A tiny GPT-Neo model executes on the AP core
while a macOS-style LVGL terminal on the CP core renders answers token-by-token —
no keyboard, no cloud LLM required.

![Terminal home](docs/images/llmterm-home.png)
![Q&A](docs/images/llmterm-qa.png)

*The board is answering on its own — the LLM runs on-device.*

## Features

- **On-device LLM** — TinyTalk (GPT-Neo, 256-dim / 8 layers, 5.8 MB weights
  XIP-mapped from flash) served by a resident AP-core task; ~442 ms/token,
  English prompts, streamed back token-by-token over inter-core mailbox IPC.
- **LVGL terminal desktop** — full-screen macOS Terminal-style UI (traffic
  lights, dim questions in green, bright answers), the only desktop of the
  firmware. Typewriter streaming, ↑/↓ history scroll.
- **Network keyboard (UDP)** — type on your PC, watch it echo live on the
  device screen. Discovery, per-key input, heartbeats — see
  `tools/llmkb/llm_keyboard.py`.
- **Push-to-talk ASR** — hold the function key (PB4), speak English, release:
  the AIUI cloud IAT result is auto-submitted as a question. 5 s cap,
  8 s hold = power off.
- **Whole-answer TTS** — after generation completes, the full answer is
  synthesized once by the cloud and played as a single segment. Cloud
  upsell/promo audio from the voice session is filtered out in terminal mode.
- **Offline resilience** — LLM Q&A keeps working with no WiFi; ASR/TTS show a
  hint and degrade gracefully.

![Conversation & push-to-talk](docs/images/llmterm-ptt.png)

## Architecture

```
 ┌── CP core (business, hart 1) ──────────────┐   ┌── AP core (algo, hart 0) ─┐
 │ term_ui   LVGL terminal (worq.ui)          │   │ llm_service               │
 │ tts_speak whole-answer cloud TTS           │   │  TinyTalk GPT-Neo         │
 │ voice_asr PB4 push-to-talk → AIUI IAT      │   │  token streaming          │
 │ net_kb    UDP keyboard :38001              │   └────────────┬──────────────┘
 │ llm_client  multi-subscriber IPC client ◄──┼── mailbox SRAM ┘ (flush/inval)
 └────────────────────────────────────────────┘
```

## Build

```bash
source ./arcs-sdk/env.sh                 # toolchain + env (must be sourced)

# CP firmware (the terminal app)
./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini

# AP firmware (minimal LLM service, ~6 MB with embedded weights)
./build.sh -S ./apps/remote-ap -B build-remote-ap -DBOARD=arcs_mini
```

Outputs: `build/arcs-mini.bin`, `build-remote-ap/remote-ap.bin`.

## Flash

ADB (recommended, CP app only):

```bash
bash adb_download.sh -S res/arcs-mini app
```

See `CLAUDE.md` for full-flash / serial (cskburn) flows.

## Usage

1. Power on — the terminal boots straight into `LLM@ARCS-MINI`.
2. **Type** — run the PC keyboard on the same LAN:
   ```bash
   python3 tools/llmkb/llm_keyboard.py    # auto-discovers the device
   ```
   Type + Enter; the question echoes in green and the answer streams in.
3. **Speak** — hold the function key, say an English sentence, release.
   The recognized text is submitted automatically.
4. **Listen** — the complete answer is spoken after generation.

Runtime toggles (adb shell or serial console):

```text
llmterm tts on|off     answer speaking (default on)
llmterm asr on|off     push-to-talk (default on)
llmterm asr inject <text>   simulate a recognition result
llmterm stat           ip / peer / llm / tts / asr status
```

### WiFi setup via adb shell

The terminal has no provisioning UI — connect WiFi manually over adb:

```bash
adb shell "wifi connect YourSSID YourPassword"   # connect & save (open AP: omit password)
adb shell "wifi list"                            # saved networks
adb shell "wifi scan"                            # scan nearby APs
```

The credential is persisted, so the device auto-reconnects after reboots.
Once WiFi is up, ASR/TTS and the network keyboard come alive
(`llmterm stat` → `cloud=1`).

## Notes & limits

- The on-device model is a TinyStories-trained TinyTalk: **English only**,
  short factual answers, sometimes confidently wrong. It is a footprint
  demo, not ChatGPT.
- ASR/TTS use the LISTENAI cloud (WiFi + LSC session required) and share the
  product's monthly interaction quota; LLM Q&A itself never leaves the device.
- Emulator: the whole stack runs under [Lisem](https://github.com/LISTENAI/Lisem)
  (QEMU) — screenshots above were captured that way, no hardware needed.

## More

- Original solution README: [README-firmware.MD](README-firmware.MD)
- 中文文档：[README.zh-CN.md](README.zh-CN.md)
