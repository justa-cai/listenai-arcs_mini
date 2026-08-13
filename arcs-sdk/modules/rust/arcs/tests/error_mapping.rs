//! Host-side unit tests for the error code mapping. These run on the host
//! (e.g. cargo test --target $(rustc -vV | sed -n 's/host: //p')) and don't
//! exercise any FFI — pure logic verification.
#![cfg(not(target_os = "none"))]

use arcs::Error;

// Re-export of the crate-private helper for testing.
// We can't call from_c directly (pub(crate)) — re-test via the public surface
// once HAL is wired in Phase B. For now, verify the enum variants exist.

#[test]
fn error_variants_compile() {
    let _ = Error::NotFound;
    let _ = Error::InvalidArg;
    let _ = Error::Io(-99);
}
