cmake_minimum_required(VERSION 3.16)
project(adb_sync_stage_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PRJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../components/tinyusb-appclass/adb)
set(UNITY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/unity/src)

add_executable(tests
    ${PRJ_ROOT}/adb_sync_stage.c
    ${UNITY_ROOT}/unity.c
    test_adb_sync_stage.c
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}
    ${UNITY_ROOT}
)

target_compile_definitions(tests PRIVATE
    CONFIG_LOG=0
)

enable_testing()
add_test(NAME adb_sync_stage_host_tests COMMAND tests)
