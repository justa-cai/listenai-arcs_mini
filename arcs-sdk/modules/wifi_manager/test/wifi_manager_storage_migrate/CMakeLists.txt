cmake_minimum_required(VERSION 3.16)
project(wifi_manager_storage_migrate_test C)

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
    ${PRJ_ROOT}/test/common/fake_lisa_log
)

target_sources(tests PRIVATE
    test_wifi_manager_storage_migrate.c
    mock_storage_deps.c
    ${PRJ_ROOT}/src/wifi_manager_storage_migrate.c
)

add_subdirectory(${PRJ_ROOT}/test/common/mock_lisa_kv mock_lisa_kv)

target_link_libraries(tests PRIVATE
    ${UNITY_LIB}
)

enable_testing()
add_test(NAME wifi_manager_storage_migrate_tests COMMAND tests)
