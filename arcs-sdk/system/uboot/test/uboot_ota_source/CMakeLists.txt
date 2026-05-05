cmake_minimum_required(VERSION 3.16)
project(uboot_ota_source_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(DEFINED ENV{UNITY_CMAKE_PREFIX})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{UNITY_CMAKE_PREFIX}")
endif()
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake")

find_package(unity QUIET)

if(unity_FOUND)
    set(UNITY_LIB unity::framework)
else()
    message(FATAL_ERROR "Not found Unity package")
endif()

set(PRJ_ROOT ../..)

add_executable(tests
    ${PRJ_ROOT}/src/boot_ota_source.c
    ${PRJ_ROOT}/src/boot_ota_request.c
    ${PRJ_ROOT}/src/boot_control_store_test.c
    test_boot_ota_source.c
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/include
    ${PRJ_ROOT}/src
)

target_compile_definitions(tests PRIVATE
    BOOT_OTA_SOURCE_UNIT_TEST=1
    CONFIG_BOOT_FLASH_SIZE=0x14000
)

target_link_libraries(tests PRIVATE ${UNITY_LIB})

enable_testing()
add_test(NAME uboot_ota_source_tests COMMAND tests)
