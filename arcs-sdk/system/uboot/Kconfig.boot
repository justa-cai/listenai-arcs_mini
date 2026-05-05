#
# Boot standalone Kconfig defaults that must stay local to the boot image build.
#

config BOOT_STANDALONE
    bool
    default y

config LINK_OPTION_LISTENAI_LIBRARY_GROUP
    bool
    default y

config ARCS_HAL_EXT_RAM_SECTION
    string
    default ".ramcode"

config MEM_PSRAM_BASE
    hex
    default 0x28000000

config MEM_PSRAM_SIZE
    hex
    default 0x00800000

config MAIN_TASK_STACK_SIZE
    int
    default 2048

config MAIN_TASK_PRIORITY
    int
    default 1

choice BOOT_APP_CORE
    prompt "Application target core"
    default BOOT_APP_CORE_CP
    help
      Select which core the application runs on.

config BOOT_APP_CORE_AUTO
    bool "Auto-detect from image header"
    help
      Read target core from the target_core field written by mkhdr -f.
      When the flag is absent (legacy image), falls back to the
      BOOT_APP_CORE_AUTO_FALLBACK choice below.

config BOOT_APP_CORE_CP
    bool "CP core (force)"
    help
      Always start the application on the CP core via software reset.
      Ignores image header flags.

config BOOT_APP_CORE_AP
    bool "AP core (force, direct call)"
    help
      Always call the application entry point directly on the AP core.
      Ignores image header flags.

endchoice

choice BOOT_APP_CORE_AUTO_FALLBACK
    prompt "Auto-detect fallback core (legacy image)"
    default BOOT_APP_CORE_AUTO_FALLBACK_CP
    depends on BOOT_APP_CORE_AUTO
    help
      When auto-detect is enabled but the app image does not contain
      a valid target core flag, fall back to this core selection.

config BOOT_APP_CORE_AUTO_FALLBACK_CP
    bool "CP core"

config BOOT_APP_CORE_AUTO_FALLBACK_AP
    bool "AP core"

endchoice
