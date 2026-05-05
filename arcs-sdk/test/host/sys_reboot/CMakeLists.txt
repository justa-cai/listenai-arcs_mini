cmake_minimum_required(VERSION 3.16)
project(sys_reboot_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

add_executable(tests
    ${REPO_ROOT}/system/os/reboot.c
    ${REPO_ROOT}/soc/arcs/common/sys_reboot.c
    test_sys_reboot.c
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes
    ${REPO_ROOT}/system/os/inc
)

enable_testing()
add_test(NAME sys_reboot_host_tests COMMAND tests)
