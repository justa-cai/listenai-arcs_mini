cmake_minimum_required(VERSION 3.16)
project(cherryusb_adb_device_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

set(PRJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../components/cherryusb-appclass/adb)
set(UNITY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/unity/src)

add_executable(tests
    ${UNITY_ROOT}/unity.c
    test_adb_device.c
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}
    ${UNITY_ROOT}
)

target_compile_definitions(tests PRIVATE
    _POSIX_C_SOURCE=200809L
    CONFIG_LOG=0
    CONFIG_BOOT_ADB=1
    CONFIG_USB_HS=1
    CONFIG_USB_ALIGN_SIZE=32
    CONFIG_ADB_MAX_PAYLOAD_SIZE=65536
    CONFIG_ADB_TASK_PRIORITY=1
)

target_link_libraries(tests PRIVATE Threads::Threads)

enable_testing()
add_test(NAME cherryusb_adb_device_host_tests COMMAND tests)
