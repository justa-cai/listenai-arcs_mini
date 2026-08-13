# WiFi PM Dual-Core HAL Sample

This sample validates ARCS HAL PM in dual-core mode before the `lisa_pm` dual-core validation stage.

## Core Roles

- AP core (`remote/`): IPC master, HAL PM peer, simple computation context check.
- CP core (main project): IPC slave, HAL PM primary, WiFi + LWIP + application + HAL PM.

The sample intentionally disables SDK IPC auto init and calls IPC init manually from `main()`, following the `soc/arcs/hal/src/arcs/ipc_demo3` initialization model.

## Memory Layout

Flash:

| Image | Flash base | Burn offset |
|------|------------|-------------|
| AP | `0x30000000` | `0x0` |
| CP | `0x30100000` | `0x100000` |

PSRAM:

| Core | Range | Size |
|------|-------|------|
| AP | `0x28000000` - `0x28400000` | 4M |
| CP | `0x28400000` - `0x28800000` | 4M |

## Configure WiFi

Edit `src/main.c` before building:

```c
#define TARGET_WIFI_SSID "listenai"
#define TARGET_WIFI_PWD  "listenai"
```

## Build

From the SDK root:

```bash
./build.sh -C -S samples/drivers/hal/wifi_pm_dual_core -DBOARD=arcs_evb
```

Expected artifacts:

```text
build/remote/ap.bin
build/arcs.bin
```

## Burn

Replace `<serial>` with the board serial device:

```bash
./tools/burn/cskburn -C arcs -s <serial> -b 3000000 \
    0x0      build/remote/ap.bin \
    0x100000 build/arcs.bin
```

## Expected Logs

AP success indicators:

```text
AP ipc master ready
boot cp from flash: 0x30100000
AP pm/vrtc init ok
AP ctx alive: counter=
```

CP success indicators:

```text
CP ipc slave ready
CP pm/vrtc init ok
DHCP Success
WiFi power save enabled
System HAL PM light sleep enabled
CP ctx alive: counter=
wk:
```

Failure indicators:

```text
AP CTX CHECK FAILED
CP CTX CHECK FAILED
assert
backtrace
exception
fault
WiFi/IP wait timeout
```

## Manual Board Validation

Board validation is manual. A successful run shows both AP and CP context counters continuing to increase after multiple sleep/wakeup cycles, CP DHCP success, CP WiFi PS enabled, HAL PM light sleep enabled, and HAL PM wake logs.
