cmake_minimum_required(VERSION 3.16)
project(sdmmc_init_test C)

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

set(SDMMC_INIT_TEST_SOURCES
    ../../port/arcs/sdmmc_init.c
    test_sdmmc_init.c
)

function(add_sdmmc_init_test_target target)
    add_executable(${target} ${SDMMC_INIT_TEST_SOURCES})

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${target} PRIVATE ${UNITY_LIB})
endfunction()

add_sdmmc_init_test_target(tests)
target_compile_definitions(tests PRIVATE
    CONFIG_BOOT_ADB=1
    CONFIG_LISA_SDMMC_SCAN_TIMEOUT_MS=3000
    TEST_EXPECTED_SCAN_TIMEOUT_MS=3000
)

add_sdmmc_init_test_target(tests_boot_default)
target_compile_definitions(tests_boot_default PRIVATE
    CONFIG_BOOT_ADB=1
)

add_sdmmc_init_test_target(tests_app)

enable_testing()
add_test(NAME sdmmc_init_tests COMMAND tests)
add_test(NAME sdmmc_init_boot_default_tests COMMAND tests_boot_default)
add_test(NAME sdmmc_init_app_tests COMMAND tests_app)
