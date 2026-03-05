cmake_minimum_required(VERSION 3.16)
project(wifi_manager_arcs_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Debug)
set(DEBUG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

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

add_executable(tests)
set(EXECUTABLE_NAME tests)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    UNIT_TEST
    CONFIG_WIFI_DEV_STATION=1
    CONFIG_WIFI_MGR_THREAD_STACK=1024
    CONFIG_WIFI_MGR_THREAD_PRIORITY=5
    CONFIG_WIFI_MGR_QUEUE_COUNT=4
    CONFIG_WIFI_MGR_AUTO_CONNECT_TIMEOUT_MS=15000
    CONFIG_WIFI_MGR_CONNECT_TIMEOUT=15000
    _GNU_SOURCE
    _POSIX_C_SOURCE=200809L
)

target_include_directories(tests PRIVATE
    ${PRJ_ROOT}/test/utils/fff
    ${PRJ_ROOT}/test/utils/unity_config
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/src
    ${PRJ_ROOT}/include
    ${PRJ_ROOT}/include/wifi_manager
    ${PRJ_ROOT}/port/wifi_ops/arcs
    ${PRJ_ROOT}/test/common/fake_lisa_log
    ${PRJ_ROOT}/test/common/mock_wifi_api
    ${PRJ_ROOT}/test/common/mock_ls_event
    ${PRJ_ROOT}/test/common/mock_cache
    ${PRJ_ROOT}/test/common/mock_heap_caps
    ${PRJ_ROOT}/test/common/fake_nvs
    ${PRJ_ROOT}/test/common/freertos
)

target_sources(tests PRIVATE
    test_wifi_manager_arcs.c
    ${PRJ_ROOT}/src/wifi_manager.c
    ${PRJ_ROOT}/port/wifi_ops/arcs/wifi_arcs.c
    ${PRJ_ROOT}/test/common/mock_wifi_api/mock_wifi_api.c
    ${PRJ_ROOT}/test/common/mock_ls_event/mock_ls_event.c
    ${PRJ_ROOT}/test/common/mock_cache/mock_cache.c
)

add_subdirectory(${PRJ_ROOT}/test/common/mock_mem mock_mem)
add_subdirectory(${PRJ_ROOT}/test/common/mock_os mock_os)
add_subdirectory(${PRJ_ROOT}/test/common/mock_wifi_manager_storage mock_wifi_manager_storage)
add_subdirectory(${PRJ_ROOT}/test/common/mock_heap_caps mock_heap_caps)
add_subdirectory(${PRJ_ROOT}/test/common/fake_lisa_time fake_lisa_time)

target_link_libraries(tests PRIVATE 
    ${UNITY_LIB}
)

enable_testing()
add_test(NAME wifi_manager_arcs_tests COMMAND tests)
