cmake_minimum_required(VERSION 3.16)
project(sys_chip_id_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(UNITY_ROOT ${REPO_ROOT}/modules/unity/src)

add_executable(tests
    ${REPO_ROOT}/system/sys/sys_chip_id.c
    ${UNITY_ROOT}/unity.c
    test_sys_chip_id.c
)

target_include_directories(tests PRIVATE
    ${REPO_ROOT}/system/sys/inc
    ${UNITY_ROOT}
)

enable_testing()
add_test(NAME sys_chip_id_host_tests COMMAND tests)
