cmake_minimum_required(VERSION 3.16)
project(mac_manager_test C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

find_package(unity QUIET)

if(unity_FOUND)
    message(STATUS "Found Unity package")
    set(UNITY_LIB unity::framework)
else()
    message(FATAL_ERROR "Not found Unity package")
endif()

set(PRJ_ROOT ../..)

macro(listenai_library_named _name)
    add_library(${_name} STATIC "")
    set(LISTENAI_CURRENT_LIBRARY ${_name})
endmacro()

macro(listenai_library_sources source)
    target_sources(${LISTENAI_CURRENT_LIBRARY} PRIVATE ${source} ${ARGN})
endmacro()

macro(listenai_link_libraries lib)
endmacro()

macro(listenai_include_directories source)
    target_include_directories(${LISTENAI_CURRENT_LIBRARY} PUBLIC ${source} ${ARGN})
endmacro()


add_executable(tests)

target_include_directories(tests PRIVATE
    ${PRJ_ROOT}/test/utils/fff
    ${PRJ_ROOT}/test/utils/unity_config
)

set(CONFIG_MAC_MANAGER 1 CACHE INTERNAL "")
set(CONFIG_MAC_MANAGER_MAC_BY_CHIP_ID 1 CACHE INTERNAL "")

add_subdirectory(${PRJ_ROOT} mac_manager)

target_sources(mac_manager PRIVATE
    ${PRJ_ROOT}/test/common/sysheap/mock_sysheap.c
    ${PRJ_ROOT}/test/common/mock_driver_efuse/mock_Driver_EFUSE.c
    ${PRJ_ROOT}/test/common/freertos/mock_task.c
)

target_include_directories(mac_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/test/common/sysheap
    ${PRJ_ROOT}/test/common/mock_driver_efuse
    ${PRJ_ROOT}/test/common/fake_lisa_log
    ${PRJ_ROOT}/test/common/freertos
    ${PRJ_ROOT}/test/utils/fff
)

target_link_libraries(tests mac_manager)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(tests PRIVATE
    test_mac_manager.c
)

target_include_directories(tests PRIVATE
    ${PRJ_ROOT}/test/common/sysheap
    ${PRJ_ROOT}/test/common/mock_driver_efuse
    ${PRJ_ROOT}/test/common
    ${PRJ_ROOT}/test/utils/fff
)


target_link_libraries(tests ${UNITY_LIB})

# 允许使用CTest运行测试
enable_testing()
add_test(NAME mac_manager_tests COMMAND tests)
