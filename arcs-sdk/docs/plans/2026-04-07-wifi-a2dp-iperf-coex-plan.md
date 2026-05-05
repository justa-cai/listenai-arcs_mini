# WiFi A2DP iperf Coex Sample Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a new manual coexistence sample for `arcs_evb` that combines WiFi `iperf_like` throughput testing with classic Bluetooth A2DP source streaming so users can independently trigger each side and observe coexistence impact.

**Architecture:** Create `samples/network/coex/wifi_a2dp_iperf_coex/` by using `samples/network/iperf_like/` as the primary skeleton, then split Bluetooth classic discovery, connection, and A2DP source audio into dedicated modules. The resulting sample initializes both subsystems at boot, but keeps WiFi connect, iperf traffic, BT inquiry, BT connect, and BT audio streaming fully manual.

**Tech Stack:** ARCS SDK sample layout, FreeRTOS tasks, Lisa WiFi, WiFi manager, lwIP, Lisa Bluetooth Classic, Lisa BT Audio Framework, Lisa Shell

**Design doc:** `docs/plans/2026-04-07-wifi-a2dp-iperf-coex-design.md`

**Strategy:** 先搭建 sample 骨架并确保可编译，再分别引入手动 WiFi、iperf 控制、经典蓝牙与 A2DP 推流，最后补 README 与实板验证说明。由于当前 sample 没有独立单元测试框架，每个任务的“测试”以 fail-first 编译或链接检查加样例整体验证构建为主。

---

## Key Files Reference

| File | Role |
|------|------|
| `samples/network/iperf_like/src/main.c` | Current WiFi/iperf app state loop |
| `samples/network/iperf_like/src/nettest_wifi.c` | WiFi init, DHCP callback, auto-connect logic |
| `samples/network/iperf_like/src/nettest_iperf3.c` | TCP throughput worker |
| `samples/network/iperf_like/src/nettest_shell.c` | `iperf` shell command model |
| `samples/network/iperf_like/prj.conf` | WiFi and iperf baseline config |
| `samples/bluetooth/classic/a2dp_source/src/main.c` | BT init, discovery, connect, and audio source commands |
| `samples/bluetooth/classic/a2dp_source/prj.conf` | Classic BT and BT audio framework config |
| `samples/network/coex/dual_core/wifi_ble_netcfg/src/main.c` | Existing coex sample naming/reference |

---

### Task 1: Scaffold the new sample directory and boot skeleton

**Files:**
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/CMakeLists.txt`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/Kconfig`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/prj.conf`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/sample.yaml`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/CMakeLists.txt`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/main.c`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_wifi.h`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt.h`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt_audio.h`

**Step 1: Write the failing test**

Create the sample skeleton and make `src/main.c` include and call the new subsystem entry points before any implementation exists:

```c
#include "coex_wifi.h"
#include "coex_bt.h"
#include "coex_bt_audio.h"

int main(int argc, char **argv)
{
    lisa_shell_init();
    coex_wifi_init();
    coex_bt_init();
    coex_bt_audio_init();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

Do not create the `.c` implementations yet.

**Step 2: Run test to verify it fails**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: FAIL with missing source files or undefined references for `coex_wifi_init`, `coex_bt_init`, and `coex_bt_audio_init`.

**Step 3: Write minimal implementation**

Add minimal stub `.c` files and baseline build files:

- `CMakeLists.txt` and `src/CMakeLists.txt` follow the multi-source pattern from `samples/network/iperf_like/`
- `Kconfig` uses the standard sample template
- `sample.yaml` starts as build-only
- `prj.conf` initially merges the baseline WiFi + BT config without feature-specific tuning yet
- `coex_wifi.c`, `coex_bt.c`, and `coex_bt_audio.c` export stub init functions returning success / logging startup

**Step 4: Run test to verify it passes**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: PASS build with a bootable empty coexistence skeleton.

**Step 5: Commit**

```bash
git add samples/network/coex/wifi_a2dp_iperf_coex
git commit -m "feat(samples): scaffold wifi a2dp iperf coexistence sample"
```

---

### Task 2: Add manual WiFi state management and WiFi shell commands

**Files:**
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/CMakeLists.txt`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_wifi.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_wifi.h`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_shell.c`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_shell.h`
- Copy/Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/fs/user_fs.c`
- Copy/Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/fs/user_fs.h`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/main.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/prj.conf`

**Step 1: Write the failing test**

Extend `coex_shell.c` to export these commands before implementing the backing functions:

```c
SHELL_EXPORT_CMD(..., wifi, cmd_wifi, wifi controls);
```

Inside `cmd_wifi`, call these not-yet-implemented APIs:

```c
coex_wifi_connect();
coex_wifi_disconnect();
coex_wifi_get_status(&status);
```

**Step 2: Run test to verify it fails**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: FAIL with undefined references for the new manual WiFi APIs.

**Step 3: Write minimal implementation**

Implement `coex_wifi.c` by adapting `samples/network/iperf_like/src/nettest_wifi.c`:

- keep MAC manager init, DHCP callback, and WiFi manager callback
- keep `lisa_wifi_init()` and `init_done` callback
- remove automatic AP save and automatic `wifi_mgr_auto_connect_start()` from init
- add explicit APIs:

```c
int coex_wifi_init(void);
int coex_wifi_connect(void);
int coex_wifi_disconnect(void);
bool coex_wifi_is_ready(void);
void coex_wifi_get_status(coex_wifi_status_t *status);
```

- use `CONFIG_IPERF_WIFI_SSID` and `CONFIG_IPERF_WIFI_PWD` for `wifi connect`
- ensure `main.c` initializes file system and KV only once before WiFi/BT modules

**Step 4: Run test to verify it passes**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: PASS build with `wifi connect|disconnect|status` commands compiled in.

**Step 5: Commit**

```bash
git add samples/network/coex/wifi_a2dp_iperf_coex
git commit -m "feat(samples): add manual wifi controls for coex sample"
```

---

### Task 3: Port the iperf app state machine without auto-connect behavior

**Files:**
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_result.c`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_result.h`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_iperf.c`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_iperf.h`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_shell.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/main.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/CMakeLists.txt`

**Step 1: Write the failing test**

Port the `iperf` shell command first, but wire it to not-yet-implemented coex iperf APIs:

```c
coex_iperf_set_enabled(true);
coex_iperf_set_mode(mode);
coex_iperf_get_status(&status);
```

Add the `iperf` command export before adding the implementation.

**Step 2: Run test to verify it fails**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: FAIL with undefined references for `coex_iperf_*` symbols.

**Step 3: Write minimal implementation**

Implement `coex_result.*` and `coex_iperf.*` by adapting these files:

- `samples/network/iperf_like/src/nettest_result.c`
- `samples/network/iperf_like/src/nettest_result.h`
- `samples/network/iperf_like/src/nettest_iperf3.c`
- `samples/network/iperf_like/src/nettest_iperf3.h`
- `samples/network/iperf_like/src/main.c`
- `samples/network/iperf_like/src/nettest_shell.c`

Required behavior:

- preserve `uplink|downlink|bidirectional`
- preserve per-round summary statistics
- preserve `iperf status`
- remove automatic `coex_wifi_connect()` or reconnect attempts from the loop
- when `iperf start` is called and WiFi is not ready, print a message telling the user to run `wifi connect`
- when WiFi disconnects mid-test, abort the current round and keep the loop paused until WiFi is ready again

**Step 4: Run test to verify it passes**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: PASS build with `iperf start|stop|status|mode` restored under the manual coexistence control model.

**Step 5: Commit**

```bash
git add samples/network/coex/wifi_a2dp_iperf_coex
git commit -m "feat(samples): add manual iperf workflow to coex sample"
```

---

### Task 4: Add classic Bluetooth discovery, connect, and A2DP source streaming

**Files:**
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt.h`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt_audio.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt_audio.h`
- Copy/Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/audio_pcm.h`
- Copy/Create: `samples/network/coex/wifi_a2dp_iperf_coex/src/audio_sbc.h`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_shell.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/CMakeLists.txt`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/prj.conf`

**Step 1: Write the failing test**

Export the BT shell commands in `coex_shell.c` before their implementations exist:

```c
bt_inquiry
bt_connect
bt_connect_index
bt_audio_start
bt_audio_stop
bt_audio_volume
```

Wire them to these missing APIs:

```c
coex_bt_inquiry();
coex_bt_connect_by_name(name);
coex_bt_connect_by_index(index);
coex_bt_audio_start();
coex_bt_audio_stop();
coex_bt_audio_set_volume(volume);
```

**Step 2: Run test to verify it fails**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: FAIL with undefined BT / BT audio symbols.

**Step 3: Write minimal implementation**

Implement by adapting `samples/bluetooth/classic/a2dp_source/src/main.c`:

- move BT init and discovery callback registration into `coex_bt_init()`
- keep device discovery logging and cache simple connection state for status output
- keep `lisa_bluetooth_connect_by_name()` and `lisa_bluetooth_connect_by_index()` behavior
- move the PCM audio task, `bt_vintf_init()`, `vintf_profile_open()`, `vintf_playback_write()`, and volume control into `coex_bt_audio.*`
- default to PCM encode mode as in `a2dp_source`
- ensure failures close the profile and clear task state

**Step 4: Run test to verify it passes**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: PASS build with full BT inquiry/connect/audio command set linked into the coexistence sample.

**Step 5: Commit**

```bash
git add samples/network/coex/wifi_a2dp_iperf_coex
git commit -m "feat(samples): add classic bt a2dp source to coex sample"
```

---

### Task 5: Add unified coexistence status, polish configs, and document validation workflow

**Files:**
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_shell.c`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_wifi.h`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_iperf.h`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt.h`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/src/coex_bt_audio.h`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/prj.conf`
- Create: `samples/network/coex/wifi_a2dp_iperf_coex/README.md`
- Modify: `samples/network/coex/wifi_a2dp_iperf_coex/sample.yaml`

**Step 1: Write the failing test**

Add the `coex status` command implementation first and make it call missing status accessors, for example:

```c
coex_bt_get_status(&bt_status);
coex_bt_audio_get_status(&audio_status);
coex_iperf_get_status(&iperf_status);
coex_wifi_get_status(&wifi_status);
```

If some status accessors do not exist yet, intentionally call them before implementing them.

**Step 2: Run test to verify it fails**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: FAIL with missing status accessor symbols or structs.

**Step 3: Write minimal implementation**

Implement the unified status output and finish the sample metadata:

- `coex status` prints WiFi readiness, iperf state/mode/last result, BT connection state, and audio streaming state
- tune `prj.conf` stack and heap values based on the merged WiFi + BT requirements
- `sample.yaml` remains `build_only: true`
- write `README.md` with these sections:
  - feature summary
  - required test environment
  - build / flash includes
  - command list
  - recommended coexistence experiments
  - expected observations

**Step 4: Run test to verify it passes**

Run:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

Expected: PASS build, and the README accurately describes the available commands and manual coexistence workflow.

**Step 5: Commit**

```bash
git add samples/network/coex/wifi_a2dp_iperf_coex
git commit -m "feat(samples): finalize wifi a2dp iperf coexistence sample"
```

---

### Task 6: Perform manual board-level coexistence verification on `arcs_evb`

**Files:**
- Modify if needed: `samples/network/coex/wifi_a2dp_iperf_coex/README.md`

**Step 1: Write the failing test**

Prepare the manual verification matrix in `README.md` before running it. Treat any missing command, crash, or unusable flow as a failing test case.

Verification matrix:

```text
1. wifi connect -> iperf start
2. iperf running -> bt_inquiry
3. iperf running -> bt_connect -> bt_audio_start
4. bt audio streaming -> iperf mode bidirectional -> iperf start
5. coex status at each phase
```

**Step 2: Run test to verify it fails**

Flash and run the sample. Any of the following counts as a failure:

- WiFi cannot connect or obtain DHCP
- `iperf` command set does not work
- BT inquiry or connect fails unexpectedly
- `bt_audio_start` cannot start streaming after a successful connection
- `coex status` output is incomplete or misleading

Suggested build and flash commands:

```bash
./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
./build.sh -F -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```

**Step 3: Write minimal implementation**

Fix only the concrete failure observed from board logs, then rerun the same manual scenario.

**Step 4: Run test to verify it passes**

Capture and save the evidence needed for the final response:

- board: `arcs_evb`
- host command: `iperf3 -s`
- key UART lines showing WiFi ready, iperf summary, BT discovery/connection, and audio start
- a short conclusion on whether WiFi throughput dropped, audio stuttered, or both remained stable

**Step 5: Commit**

```bash
git add samples/network/coex/wifi_a2dp_iperf_coex/README.md
# include source/config files only if verification required code fixes
# git add samples/network/coex/wifi_a2dp_iperf_coex/...
git commit -m "test(samples): verify wifi a2dp iperf coexistence sample on arcs_evb"
```
