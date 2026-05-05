---
name: uboot-ota-board-verify
description: Use when working in this repository and the user wants an automated real-board verification of the `recovery_basic + app_only_trigger` U-Boot OTA flow on `arcs_evb`, with build, burn, media staging, serial capture, and pass/fail evidence for Flash or TF source.
---

# U-Boot OTA Board Verify

This project-local skill automates the board-level OTA verification loop for the fixed
sample pair below:

- boot: `samples/subsys/uboot/recovery_basic`
- app: `samples/subsys/uboot_ota/app_only_trigger`
- board: `arcs_evb`

## When To Use

- The user asks whether the latest Flash OTA flow works on board
- The user asks whether the latest TF OTA flow works on board
- The user wants build + burn + ADB/serial evidence, not just a host build
- The user wants a repeatable board verification command for the OTA flow

## Fixed Defaults

- Serial port: `/dev/ttyACM0`
- Board: `arcs_evb`
- App slot burn address: `0x40000`
- Flash OTA package burn address: `0x600000`
- TF request path: `download/update.txz`
- Evidence root: `.cache/uboot_ota_board_verify`

## Workflow

1. Decide `source=flash|tf`
2. Read the matching source reference:
   - Flash: `references/flash.md`
   - TF: `references/tf.md`
3. Run the verifier script:

```bash
bash .project/skills/uboot-ota-board-verify/scripts/run_verify.sh --source tf
bash .project/skills/uboot-ota-board-verify/scripts/run_verify.sh --source flash
```

4. Read `summary.txt` and the referenced logs before claiming success or failure

## Allowed Overrides

The skill is intentionally narrow. Only override these when the user asks:

- `--serial <path>`
- `--board <board>`
- `--tf-path <request-path>`
- `--out-root <dir>`

Do not generalize this skill to arbitrary samples or arbitrary flash layouts.

## Rules

- This skill is for real-board verification; host-only build evidence is not enough
- TF mode must verify host / device sha256 for the pushed `ota.txz`
- Flash mode must verify the sample-configured package size matches the generated `ota.txz`
- Do not claim PASS unless the upgraded firmware marker appears after OTA and after a second reset
- If any prerequisite or marker fails, report FAIL with the exact log path

## Output Shape

Each run writes to:

- `.cache/uboot_ota_board_verify/<timestamp>-<source>/`
- `.cache/uboot_ota_board_verify/latest`

Expect at least:

- `summary.txt`
- `build_*.txt`
- `burn_*.txt`
- `serial_*.txt`
- `adb_*.txt` for TF mode

`summary.txt` is the source of truth for PASS / FAIL and points to the exact evidence files.
