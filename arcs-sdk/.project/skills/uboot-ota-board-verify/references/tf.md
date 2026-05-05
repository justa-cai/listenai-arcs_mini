# TF OTA Source Notes

Use this reference only when `source=tf`.

## Media staging difference

- Build `app_only_trigger` with TF trigger enabled
- The request path may be:
  - `download/update.txz`
  - `/download/update.txz`
  - `/SD:/download/update.txz`
- Push the generated OTA package to the normalized device path under `/SD:/...`

Default push path:

- request path: `download/update.txz`
- normalized device path: `/SD:/download/update.txz`

## Required truth checks

- `CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF=y`
- `CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF_PATH="<request path>"`
- Recovery boot enumerates ADB before the APP trigger is flashed
- Host and device md5 for `ota.txz` must match exactly (verified by adb pull + host md5sum comparison, since boot shell has no file-level hash command)

## Required serial markers

The serial log must include all of these markers:

- `APP-only OTA trigger sample running`
- `APP-only OTA source: tf path=<request path>`
- `APP-only OTA request saved, rebooting`
- `ota: txz update start`
- `ota: src=/SD:/... size=0x...`
- `ota: txz update success`
- `ota: lifecycle updated, reboot`
- `APP-only OTA upgraded firmware running`

## Common failures

- No ADB device after recovery boot: boot did not enter recovery ADB, or USB enumeration failed
- `adb push` / `sha256` failure: TF card is missing, mount failed, or the path is wrong
- Missing `/SD:/...` boot marker: boot did not normalize or open the TF request path
- Missing upgraded firmware marker after the final reboot: OTA apply or reboot flow failed
