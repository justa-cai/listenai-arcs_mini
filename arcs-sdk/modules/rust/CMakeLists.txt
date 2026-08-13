# ARCS SDK Rust Adapter — CMake integration.
#
# The `arcs` crate is rlib-only. Per-sample staticlib builds (orchestrated
# by rust_sample.cmake / listenai_maybe_enable_rust_target) bundle the arcs
# rlib code together with the sample's own code, so this
# directory does NOT produce a CMake-linkable artifact for arcs itself.
#
# Responsibilities:
#   1. Validate cargo is reachable (early failure with actionable message)
#   2. Provide `arcs_rust_check` target for early validation builds
#   3. Cache ARCS_RUST_DIR for sample-side helpers to reference
#   4. Provide `arcs-rust-glue` OBJECT library exposing C shims that bridge
#      Rust no_std code to SDK services (e.g. arcs_rust_log -> elog_output)

cmake_minimum_required(VERSION 3.20)

# ── Once-guard for multi-sample builds ─────────────────────────────────
if(TARGET arcs-rust-glue)
    return()
endif()

# ── Find cargo (with HINTS for keg-only rustup installs) ───────────────
find_program(CARGO_BIN cargo
    HINTS
        /usr/local/opt/rustup/bin       # macOS Intel Homebrew keg-only rustup
        /opt/homebrew/opt/rustup/bin    # macOS Apple Silicon Homebrew
        $ENV{HOME}/.cargo/bin           # rustup-init default
)
if(NOT CARGO_BIN)
    message(FATAL_ERROR
        "cargo not found.\n"
        "Install rustup via https://rustup.rs (or `brew install rustup` on macOS), then:\n"
        "  cd ${CMAKE_CURRENT_SOURCE_DIR} && rustup show\n"
        "(rustup auto-installs the toolchain locked in rust-toolchain.toml)")
endif()
message(STATUS "Found cargo: ${CARGO_BIN}")

# ── Export directory for downstream sample helpers ─────────────────────
set(ARCS_RUST_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CACHE INTERNAL "Root of the modules/rust adapter directory")

# ── Smoke-check target: validates the arcs crate compiles ──────────────
# Use `cargo check` (no codegen) — fast, catches missing deps / syntax /
# trait errors early without producing a heavy build artifact.
add_custom_target(arcs_rust_check
    COMMAND ${CARGO_BIN} check --release -p arcs
    WORKING_DIRECTORY ${ARCS_RUST_DIR}
    COMMENT "Validating arcs Rust crate compiles (cargo check)")

# ── C shim: forwards Rust log calls to easylogger ──────────────────────
# Built as an OBJECT library. rust_app.cmake reuses this target's source list
# and compile usage requirements when SDK link paths cannot consume OBJECT
# libraries directly.

add_library(arcs-rust-glue OBJECT
    ${ARCS_RUST_DIR}/arcs/glue/arcs_rust_log.c
    ${ARCS_RUST_DIR}/arcs/glue/arcs_rust_dev.c
    ${ARCS_RUST_DIR}/arcs/glue/arcs_rust_fs.c
    ${ARCS_RUST_DIR}/arcs/glue/arcs_rust_contract.c)

target_include_directories(arcs-rust-glue PUBLIC
    ${ARCS_RUST_DIR}/arcs/glue)

# easylogger lives in modules/easylogger; samples already get its includes
# via the standard SDK include propagation. Add it here explicitly so the
# OBJECT library compiles without depending on listenai_add_executable side
# effects.
# fs/audio headers back the self-guarded sections in arcs_rust_fs.c /
# arcs_rust_contract.c (empty TUs unless the matching CONFIG_* is set).
target_include_directories(arcs-rust-glue PRIVATE
    $ENV{ARCS_BASE}/modules/easylogger/easylogger/inc
    $ENV{ARCS_BASE}/system/log
    $ENV{ARCS_BASE}/drivers/lisa_device
    $ENV{ARCS_BASE}/drivers/lisa_audio
    $ENV{ARCS_BASE}/modules/fs/lsfs
    $ENV{ARCS_BASE}/modules/fs/lvfs
    $ENV{ARCS_BASE}/modules/fs/port/include
    $ENV{ARCS_BASE}/modules/fs/disk/include)

# rust_app.cmake contains the shared staticlib build/link helper; thin wrappers
# such as rust_sample.cmake provide SDK sample defaults.
