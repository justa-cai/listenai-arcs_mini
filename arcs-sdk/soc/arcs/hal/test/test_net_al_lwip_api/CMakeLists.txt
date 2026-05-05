cmake_minimum_required(VERSION 3.16)
project(test_net_al_lwip_api C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake")

find_package(unity QUIET)

if(unity_FOUND)
    set(UNITY_LIB unity::framework)
else()
    message(FATAL_ERROR "Not found Unity package")
endif()

set(HAL_ROOT ../..)

# Mock libraries
add_subdirectory(${HAL_ROOT}/test/common/mock/lwip ${CMAKE_CURRENT_BINARY_DIR}/mock_lwip)
add_subdirectory(${HAL_ROOT}/test/common/mock/netif ${CMAKE_CURRENT_BINARY_DIR}/mock_netif)
add_subdirectory(${HAL_ROOT}/test/common/mock/rtos ${CMAKE_CURRENT_BINARY_DIR}/mock_rtos)

add_executable(tests)

target_include_directories(tests PRIVATE
    ${HAL_ROOT}/test/utils/unity_config
    ${HAL_ROOT}/test/utils/fff
    ${HAL_ROOT}/test/common/mock
    ${HAL_ROOT}/chip/arcs/include
    ${HAL_ROOT}/chip/arcs/include/net
    ${HAL_ROOT}/chip/arcs/include/wifi
    ${HAL_ROOT}/chip/arcs/include/utils
)

target_sources(tests PRIVATE
    ${HAL_ROOT}/chip/arcs/lwip/net_al/net_al.c
    test_net_al_lwip_api.c
)

set_source_files_properties(${HAL_ROOT}/chip/arcs/lwip/net_al/net_al.c PROPERTIES
    COMPILE_OPTIONS "-Wno-pointer-to-int-cast;-Wno-int-to-pointer-cast"
)

target_compile_definitions(tests PRIVATE
    CONFIG_PM=0
)

target_link_libraries(tests
    ${UNITY_LIB}
    mock_lwip
    mock_netif
    mock_rtos
)

enable_testing()
add_test(NAME test_net_al_lwip_api COMMAND tests)
