# Flash OTA Source Notes

Use this reference only when `source=flash`.

## Media staging difference

- Build `app_only_trigger` with its default flash trigger configuration
- Generate `ota.txz`
- Burn `ota.txz` to flash address `0x600000`
- Burn the trigger APP to `0x40000`

## Required truth checks

- `CONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH=y`
- `CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE=<generated ota.txz size>`
- Recovery boot still comes from `samples/subsys/uboot/recovery_basic`

## Required serial markers

The serial log must include all of these markers:

- `APP-only OTA trigger sample running`
- `APP-only OTA source: flash`
- `APP-only OTA request saved, rebooting`
- `ota: txz update start`
- `ota: src=0x30600000 size=0x...`
- `ota: txz update success`
- `ota: lifecycle updated, reboot`
- `APP-only OTA upgraded firmware running`

## Common failures

- Missing `ota: src=0x30600000 ...`: the OTA package was not burned to the flash OTA area
- Package size mismatch: the sample config no longer matches the static `res/` payload output
- Missing upgraded firmware marker after the final reboot: OTA write or reboot flow failed
