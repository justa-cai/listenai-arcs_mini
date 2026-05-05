set(UBOOT_RECOVERY_API_PATH OFF)
if(DEFINED CONFIG_BOOT_RECOVERY_API OR DEFINED CONFIG_BOOT)
    set(UBOOT_RECOVERY_API_PATH ON)
    listenai_library_sources(
        uboot_recovery_api.c
        ../src/boot_flash.c
        ../src/boot_config.c
    )
endif()

set(UBOOT_OTA_API_PATH OFF)
if(DEFINED CONFIG_BOOT_OTA_API OR DEFINED CONFIG_BOOT_OTA_PACKAGE)
    set(UBOOT_OTA_API_PATH ON)
    listenai_library_sources(
        uboot_ota_api.c
        ../src/boot_ota_lifecycle.c
        ../src/boot_ota_request.c
    )
endif()

set(UBOOT_WDT_API_PATH OFF)
if(DEFINED CONFIG_BOOT_WDT_API OR DEFINED CONFIG_BOOT)
    set(UBOOT_WDT_API_PATH ON)
    listenai_library_sources(
        uboot_wdt_api.c
    )
endif()

set(UBOOT_POWER_API_PATH OFF)
if(DEFINED CONFIG_BOOT_POWER_API OR DEFINED CONFIG_BOOT)
    set(UBOOT_POWER_API_PATH ON)
    listenai_library_sources(
        uboot_power_api.c
    )
endif()

set(UBOOT_FEATURES_API_PATH OFF)
if(DEFINED CONFIG_BOOT_FEATURES_API OR DEFINED CONFIG_BOOT)
    set(UBOOT_FEATURES_API_PATH ON)
    listenai_library_sources(
        uboot_features_api.c
    )
endif()

if(UBOOT_OTA_API_PATH AND DEFINED CONFIG_BOOT_CONTROL_STORE_FLASH)
    listenai_library_sources(
        boot_control_store_lisa_flash.c
    )
endif()

if(UBOOT_OTA_API_PATH AND DEFINED CONFIG_BOOT_CONTROL_STORE_NONE)
    listenai_library_sources(
        ../src/boot_control_store_none.c
    )
endif()

if(UBOOT_OTA_API_PATH AND DEFINED CONFIG_BOOT_CONTROL_STORE_TEST)
    listenai_library_sources(
        ../src/boot_control_store_test.c
    )
endif()

if(UBOOT_RECOVERY_API_PATH OR UBOOT_OTA_API_PATH)
    listenai_library_sources(
        ../src/boot_ota_handoff.c
    )
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_include_directories(${LISTENAI_CURRENT_LIBRARY} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
