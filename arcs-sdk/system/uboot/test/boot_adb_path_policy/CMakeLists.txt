cmake_minimum_required(VERSION 3.16)
project(boot_adb_path_policy_test C)

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
    test_boot_adb_path_policy.c
)

target_include_directories(tests PRIVATE
    ${PRJ_ROOT}/src
)

target_link_libraries(tests PRIVATE ${UNITY_LIB})

enable_testing()
add_test(NAME boot_adb_path_policy_tests COMMAND tests)
