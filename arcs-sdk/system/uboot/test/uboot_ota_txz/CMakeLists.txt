cmake_minimum_required(VERSION 3.16)
project(uboot_ota_txz_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(PRJ_ROOT ../..)
set(FIXTURE_DIR ${CMAKE_CURRENT_BINARY_DIR}/fixture)
set(FIXTURE_PAYLOAD_SMALL ${FIXTURE_DIR}/payload_small.bin)
set(FIXTURE_PAYLOAD_SMALL_CP ${FIXTURE_DIR}/payload_small_cp.bin)
set(FIXTURE_TAR_SMALL ${FIXTURE_DIR}/ota_small.tar)
set(FIXTURE_TXZ_SMALL ${FIXTURE_DIR}/ota_small.txz)
set(FIXTURE_PAYLOAD_LARGE ${FIXTURE_DIR}/payload_large.bin)
set(FIXTURE_TAR_LARGE ${FIXTURE_DIR}/ota_large.tar)
set(FIXTURE_TXZ_LARGE ${FIXTURE_DIR}/ota_large.txz)
set(FIXTURE_TAR_LOWERCASE ${FIXTURE_DIR}/ota_lowercase.tar)
set(FIXTURE_TAR_JUNK_FIRST ${FIXTURE_DIR}/ota_junk_first.tar)
set(FIXTURE_TAR_MISSING_CONFIG ${FIXTURE_DIR}/ota_missing_config.tar)
set(FIXTURE_TAR_OVERSIZE ${FIXTURE_DIR}/ota_oversize.tar)
set(FIXTURE_TAR_BAD_MD5 ${FIXTURE_DIR}/ota_bad_md5.tar)
set(FIXTURE_TXZ_BAD_MD5 ${FIXTURE_DIR}/ota_bad_md5.txz)
set(FIXTURE_SMALL_SIZE 1500)
set(FIXTURE_SMALL_CP_SIZE 24)
set(FIXTURE_LARGE_SIZE 210880)
set(FIXTURE_OVERSIZE_SIZE 262145)
set(FIXTURE_AP_TARGET_ADDR 0x30026000)
set(FIXTURE_CP_TARGET_ADDR 0x30226000)

add_custom_command(
    OUTPUT
        ${FIXTURE_PAYLOAD_SMALL}
        ${FIXTURE_PAYLOAD_SMALL_CP}
        ${FIXTURE_TAR_SMALL}
        ${FIXTURE_TXZ_SMALL}
        ${FIXTURE_PAYLOAD_LARGE}
        ${FIXTURE_TAR_LARGE}
        ${FIXTURE_TXZ_LARGE}
        ${FIXTURE_TAR_LOWERCASE}
        ${FIXTURE_TAR_JUNK_FIRST}
        ${FIXTURE_TAR_MISSING_CONFIG}
        ${FIXTURE_TAR_OVERSIZE}
        ${FIXTURE_TAR_BAD_MD5}
        ${FIXTURE_TXZ_BAD_MD5}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${FIXTURE_DIR}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
            --payload ${FIXTURE_PAYLOAD_SMALL}
            --second-payload ${FIXTURE_PAYLOAD_SMALL_CP}
            --tar ${FIXTURE_TAR_SMALL}
            --txz ${FIXTURE_TXZ_SMALL}
            --size ${FIXTURE_SMALL_SIZE}
            --second-size ${FIXTURE_SMALL_CP_SIZE}
            --image-name image/AP.bin
            --second-image-name image/CP.bin
            --manifest
            --target-addr ${FIXTURE_AP_TARGET_ADDR}
            --second-target-addr ${FIXTURE_CP_TARGET_ADDR}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
            --payload ${FIXTURE_PAYLOAD_LARGE}
            --tar ${FIXTURE_TAR_LARGE}
            --txz ${FIXTURE_TXZ_LARGE}
            --size ${FIXTURE_LARGE_SIZE}
            --image-name image/AP.bin
            --manifest
            --target-addr ${FIXTURE_AP_TARGET_ADDR}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
            --payload ${FIXTURE_PAYLOAD_SMALL}
            --tar ${FIXTURE_TAR_LOWERCASE}
            --size ${FIXTURE_SMALL_SIZE}
            --image-name image/ap.bin
            --manifest
            --target-addr ${FIXTURE_AP_TARGET_ADDR}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
            --payload ${FIXTURE_PAYLOAD_SMALL}
            --tar ${FIXTURE_TAR_JUNK_FIRST}
            --size ${FIXTURE_SMALL_SIZE}
            --image-name image/AP.bin
            --manifest
            --target-addr ${FIXTURE_AP_TARGET_ADDR}
            --extra-file-name junk.bin
            --extra-file-size 17
            --extra-file-before-image
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
            --payload ${FIXTURE_PAYLOAD_SMALL}
            --tar ${FIXTURE_TAR_MISSING_CONFIG}
            --size ${FIXTURE_SMALL_SIZE}
            --image-name image/AP.bin
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
            --payload ${FIXTURE_DIR}/payload_oversize.bin
            --tar ${FIXTURE_TAR_OVERSIZE}
            --size ${FIXTURE_OVERSIZE_SIZE}
            --image-name image/AP.bin
            --manifest
            --target-addr ${FIXTURE_AP_TARGET_ADDR}
            --declared-size ${FIXTURE_SMALL_SIZE}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
            --payload ${FIXTURE_PAYLOAD_SMALL}
            --second-payload ${FIXTURE_PAYLOAD_SMALL_CP}
            --tar ${FIXTURE_TAR_BAD_MD5}
            --txz ${FIXTURE_TXZ_BAD_MD5}
            --size ${FIXTURE_SMALL_SIZE}
            --second-size ${FIXTURE_SMALL_CP_SIZE}
            --image-name image/AP.bin
            --second-image-name image/CP.bin
            --manifest
            --target-addr ${FIXTURE_AP_TARGET_ADDR}
            --second-target-addr ${FIXTURE_CP_TARGET_ADDR}
            --bad-second-md5
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_fixture.py
    COMMENT "Generating OTA TXZ fixture"
)

add_custom_target(uboot_ota_txz_fixture ALL
    DEPENDS
        ${FIXTURE_PAYLOAD_SMALL}
        ${FIXTURE_PAYLOAD_SMALL_CP}
        ${FIXTURE_TAR_SMALL}
        ${FIXTURE_TXZ_SMALL}
        ${FIXTURE_PAYLOAD_LARGE}
        ${FIXTURE_TAR_LARGE}
        ${FIXTURE_TXZ_LARGE}
        ${FIXTURE_TAR_LOWERCASE}
        ${FIXTURE_TAR_JUNK_FIRST}
        ${FIXTURE_TAR_MISSING_CONFIG}
        ${FIXTURE_TAR_OVERSIZE}
        ${FIXTURE_TAR_BAD_MD5}
        ${FIXTURE_TXZ_BAD_MD5}
)

add_executable(tests
    ${PRJ_ROOT}/src/boot_md5.c
    ${PRJ_ROOT}/src/xzdec/xz_crc32.c
    ${PRJ_ROOT}/src/xzdec/xz_dec_stream.c
    ${PRJ_ROOT}/src/xzdec/xz_dec_lzma2.c
    ${PRJ_ROOT}/src/xzdec/xz_dec_bcj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../modules/cjson/cJSON.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../soc/arcs/hal/chip/arcs/driver/debug/tinyprintf.c
    ${PRJ_ROOT}/src/boot_ota_manifest.c
    test_boot_ota_txz.c
)

add_dependencies(tests uboot_ota_txz_fixture)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/include
    ${PRJ_ROOT}/src
    ${PRJ_ROOT}/src/xzdec/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../modules/cjson
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../soc/arcs/hal/chip/arcs/driver/debug
)

target_compile_definitions(tests PRIVATE
    CONFIG_MEM_FLASH_BASE=0x30000000UL
    FIXTURE_SMALL_SIZE=${FIXTURE_SMALL_SIZE}
    FIXTURE_SMALL_CP_SIZE=${FIXTURE_SMALL_CP_SIZE}
    UBOOT_OTA_TXZ_TEST_SMALL_PAYLOAD_PATH="${FIXTURE_PAYLOAD_SMALL}"
    UBOOT_OTA_TXZ_TEST_SMALL_CP_PAYLOAD_PATH="${FIXTURE_PAYLOAD_SMALL_CP}"
    UBOOT_OTA_TXZ_TEST_SMALL_TAR_PATH="${FIXTURE_TAR_SMALL}"
    UBOOT_OTA_TXZ_TEST_SMALL_TXZ_PATH="${FIXTURE_TXZ_SMALL}"
    UBOOT_OTA_TXZ_TEST_LARGE_PAYLOAD_PATH="${FIXTURE_PAYLOAD_LARGE}"
    UBOOT_OTA_TXZ_TEST_LARGE_TAR_PATH="${FIXTURE_TAR_LARGE}"
    UBOOT_OTA_TXZ_TEST_LARGE_TXZ_PATH="${FIXTURE_TXZ_LARGE}"
    UBOOT_OTA_TXZ_TEST_LOWERCASE_TAR_PATH="${FIXTURE_TAR_LOWERCASE}"
    UBOOT_OTA_TXZ_TEST_JUNK_FIRST_TAR_PATH="${FIXTURE_TAR_JUNK_FIRST}"
    UBOOT_OTA_TXZ_TEST_MISSING_CONFIG_TAR_PATH="${FIXTURE_TAR_MISSING_CONFIG}"
    UBOOT_OTA_TXZ_TEST_OVERSIZE_TAR_PATH="${FIXTURE_TAR_OVERSIZE}"
    UBOOT_OTA_TXZ_TEST_BAD_MD5_TAR_PATH="${FIXTURE_TAR_BAD_MD5}"
    UBOOT_OTA_TXZ_TEST_BAD_MD5_TXZ_PATH="${FIXTURE_TXZ_BAD_MD5}"
)

target_compile_options(tests PRIVATE
    -include stddef.h
)

target_link_libraries(tests PRIVATE m)

enable_testing()
add_test(NAME uboot_ota_txz_tests COMMAND tests)
