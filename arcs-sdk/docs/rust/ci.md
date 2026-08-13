# Rust CI Jobs

Four GitLab jobs (defined in `.gitlab/ci/rust.yml`) gate the Rust no_std
stack. The first three run on regular MRs; the on-board job runs only
on scheduled nightly pipelines, on the manual button, or on MRs labeled
`runs-on-board`.

## `rust:lint`

Runs `cargo fmt --check` and
`cargo clippy --release -p arcs --target riscv32imac-unknown-none-elf -- -D warnings`.

**Triggers on changes to:** `modules/rust/**`, `samples/libraries/rust/**`,
`test/rust/**`.

**On failure:**

- `rustfmt` errors: `cd modules/rust && cargo fmt` locally, commit the
  result.
- `clippy` errors: read the diagnostic, fix the code. If a warning is a
  genuine false positive, add `#[allow(...)]` with a comment that
  explains why; do not blanket-allow at crate level.

## `rust:build`

Runs the SDK's standard `build.sh` for the three Rust workloads:

```sh
bash build.sh -C -S samples/libraries/rust/helloworld -DBOARD=arcs_evb
bash build.sh -C -S samples/libraries/rust/blinky     -DBOARD=arcs_evb
bash build.sh -C -S test/rust/binding_test            -DBOARD=arcs_evb
```

The job exports `ARCS_BASE=$CI_PROJECT_DIR` defensively in case the
runner image does not source `env.sh`.

**Triggers on changes to:** `modules/rust/**`, `samples/libraries/rust/**`,
`test/rust/**`.

**On failure:** read the build log. Common causes:

- Missing C symbol from a `sys/*.rs` binding — the C function name or
  signature changed; update the hand-written `extern "C"` and refresh
  the "Last synced against C header at commit" line in the file header.
- CMake helper not picking up the sample — confirm `LISTENAI_RUST_APP`
  is set *before* `include(... /rust_sample.cmake)` and the subsequent
  `listenai_maybe_enable_rust_target()`.
- Missing Kconfig gate — the sample's `prj.conf` is missing
  `CONFIG_LISA_GPIO_DEVICE=y` (or the matching UART gates) for the
  drivers it uses.

## `rust:bindgen-drift`

Generates reference FFI from C headers via `bindgen` and diffs against
the hand-written `arcs/src/sys/*.rs`. Marked `allow_failure: true`
during MVP rollout while we tune the normalizer
(`tools/rust/bindgen-normalize.py`).

**Triggers on changes to:** `modules/rust/arcs/src/sys/**`,
`modules/rust/arcs/glue/**`, `system/log/**`, `system/os/inc/**`,
`drivers/lisa_gpio/**`, `drivers/lisa_uart/**`, `drivers/lisa_device/**`.

**On failure:**

1. Read the diff. Distinguish substantive ABI differences from
   normalizer noise (bindgen tends to inline anonymous types, reorder
   attributes, etc.).
2. For real ABI changes: edit the affected `sys/<module>.rs` by hand
   and update the "Last synced against C header at commit" comment in
   the file header.
3. For normalizer noise: improve `tools/rust/bindgen-normalize.py` and
   re-run.
4. Reproduce locally with `./tools/rust/bindgen-drift.sh`.

## `rust:onboard-binding-test`

Flashes `test/rust/binding_test` on a real `arcs_evb` and greps for
`=== Rust Binding Test PASSED (8/8) ===`.

**Not a blocking gate** on regular MRs (board capacity). Runs on:

- Scheduled nightly pipelines (`$CI_PIPELINE_SOURCE == "schedule"`,
  `on_success`)
- MRs labeled `runs-on-board` (manual button)
- Plain manual invocation (`allow_failure: true`)

The job currently shells out to `tools/board-bench/flash-and-capture.sh`
and requires runners tagged `[board-bench, arcs-evb]`. Until those
runners and the script land, the manual invocation will fail at the
capture step — the CMake → cargo → ELF → `.bin` build step still runs
and provides smoke-check value. Local verification uses the cskburn +
pyserial recipe documented in `modules/rust/README.md`.

**On failure:** examine the `board-bench-log.txt` artifact. Common
causes:

- A new binding has wrong ABI (struct layout, function signature) —
  often reproduces only with real hardware in the loop.
- The test references a device name not registered on the board (e.g.
  the `prj.conf` is missing a `CONFIG_LISA_*` gate).
- Cross-coupled reset failed — flash succeeded but the board didn't
  reboot; try power-cycling and re-running.

## Adding a new CI rule

If you add a new `arcs/src/sys/*.rs` module or a new C header
dependency:

1. Update the `rules.changes:` glob in the relevant job(s) above.
2. Add the new header path to `tools/rust/bindgen-drift.sh`'s
   `check ...` line so drift coverage extends to the new binding.
3. If the binding ships its own C shim, append the shim source to the
   `arcs-rust-glue` OBJECT library in `modules/rust/CMakeLists.txt`.
