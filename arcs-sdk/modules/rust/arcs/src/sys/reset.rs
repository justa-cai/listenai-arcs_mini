//! FFI binding for the ARCS software-reset entry (soc/arcs `arcs_ap.h`).
//!
//! `sys_platform_sw_full_reset` is a plain `void(void)` that triggers a full
//! software reset (writes `IP_AON_CTRL->REG_AON_SW_RESET`). It does not return.
//! Trivial signature — not routed through bindgen-drift.

extern "C" {
    /// Full software reset of the chip. Does not return.
    pub fn sys_platform_sw_full_reset();
}
