# Rust Async / Embassy (`async_tasks`)

Runs two **concurrent async tasks** on a single [`embassy`](https://embassy.dev)
executor (R9), each driven by its own `embassy_time::Timer`:

```rust
#[embassy_executor::task]
async fn task_a() { loop { /* log */ Timer::after(Duration::from_millis(250)).await; } }

#[embassy_executor::task]
async fn task_b() { loop { /* log */ Timer::after(Duration::from_millis(600)).await; } }

#[arcs::main]
fn main() -> arcs::Result<()> {
    arcs_embassy::run(|spawner| {       // never returns; this task becomes the executor
        spawner.spawn(task_a()).ok();
        spawner.spawn(task_b()).ok();
    })
}
```

## How it works (FreeRTOS-cooperative embassy)

The integration lives in the `arcs-embassy` crate:

- **Executor**: `embassy_executor::raw::Executor` runs on the calling FreeRTOS
  task. When idle it **blocks on a FreeRTOS task notification** (not `wfi`), so
  other RTOS tasks (logging, idle) keep running. The `__pender` wakes that task.
- **Time driver**: an `embassy-time` `Driver` backed by the SoC's 64-bit 1 MHz
  monotonic SysTimer (`now()` in µs, `tick-hz-1_000_000`). Elapsed timers are
  dispatched from the executor loop, which then sleeps until the next deadline.
- **critical-section**: implemented with the FreeRTOS interrupt mask
  (`ulPortRaiseBASEPRI` / `vPortSetBASEPRI`).

A small C shim (`modules/rust/arcs-embassy/glue/arcs_embassy_glue.c`) exposes
the FreeRTOS macros/inlines as linkable symbols; the sample's `CMakeLists.txt`
compiles it.

Pinned to the last edition-2021 embassy releases that build on Rust 1.83.0:
`embassy-executor 0.9.1`, `embassy-time 0.4.0`, `embassy-time-driver 0.2.0`.

## Build & flash (arcs_evb)

```sh
export ARCS_BASE=$PWD
bash build.sh -C -S samples/libraries/rust/async_tasks -DBOARD=arcs_evb
# flash build/rust_async_tasks.bin with cskburn, then watch the serial console
```

Expected serial output — `A` ticks ~every 250 ms, `B` ~every 600 ms,
interleaving (≈2-3 `A`s per `B`), proving concurrent independent timers:

```
=== Rust async/embassy demo ===
async_tasks: starting embassy executor
async: A #0
async: B #0
async: A #1
async: A #2
async: B #1
async: A #3
async: A #4
async: B #2
async: A #5
async: B #3
async_tasks:ok concurrent timers on one executor
...
```

`arcs_embassy::Delay` also implements `embedded_hal_async::delay::DelayNs` for
use with async community drivers.
