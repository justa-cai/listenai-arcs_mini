cmake_minimum_required(VERSION 3.16)
project(wifi_manager_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加调试选项
set(CMAKE_BUILD_TYPE Debug)
set(DEBUG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 为链接器添加选项保留调试信息  
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

# 配置cmake模块路径
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

find_package(unity QUIET)

# 如果找到Unity包，使用包提供的变量
if(unity_FOUND)
    message(STATUS "Found Unity package")
    set(UNITY_LIB unity::framework)
else()
    message(FATAL_ERROR "Not found Unity package")
endif()

set(PRJ_ROOT ../..)

# 创建测试可执行文件
add_executable(tests)
set(EXECUTABLE_NAME tests)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    UNIT_TEST
    CONFIG_WIFI_DEV_STATION=1
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    CONFIG_WIFI_MGR_NVS_PARTITION_OFFSET=0x100000
    CONFIG_WIFI_MGR_NVS_PARTITION_SIZE=0x100000
    CONFIG_WIFI_MGR_NVS_FLASH_SECTOR_SIZE=0x1000
    CONFIG_WIFI_MGR_AUTO_CONNECT_TIMEOUT_MS=15000
    CONFIG_WIFI_MGR_THREAD_STACK=1024
    CONFIG_WIFI_MGR_THREAD_PRIORITY=5
    CONFIG_WIFI_MGR_CONNECT_TIMEOUT=15000
    _GNU_SOURCE
    _POSIX_C_SOURCE=200809L
)

target_include_directories(tests PRIVATE
    ${PRJ_ROOT}/test/utils/fff
    ${PRJ_ROOT}/test/utils/unity_config
)

target_sources(tests PRIVATE
    ${PRJ_ROOT}/src/wifi_manager.c
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/src
    ${PRJ_ROOT}/include
    ${PRJ_ROOT}/include/wifi_manager
    ${PRJ_ROOT}/test/common/fake_lisa_log
)

target_sources(tests PRIVATE
    test_wifi_manager.c
)

add_subdirectory(${PRJ_ROOT}/test/common/fake_lisa_common fake_lisa_common)
add_subdirectory(${PRJ_ROOT}/test/common/fake_lisa_time fake_lisa_time)
add_subdirectory(${PRJ_ROOT}/test/common/fake_lisa_thread fake_lisa_thread)
add_subdirectory(${PRJ_ROOT}/test/common/fake_lisa_mutex fake_lisa_mutex)
add_subdirectory(${PRJ_ROOT}/test/common/fake_nvs fake_nvs)
add_subdirectory(${PRJ_ROOT}/test/common/mock_heap_caps mock_heap_caps)
add_subdirectory(${PRJ_ROOT}/test/common/freertos freertos)
add_subdirectory(${PRJ_ROOT}/test/common/mock_lisa_kv mock_lisa_kv)
add_subdirectory(${PRJ_ROOT}/test/common/mock_wifi mock_wifi)
add_subdirectory(${PRJ_ROOT}/test/common/mock_platform_dev mock_platform_dev)
add_subdirectory(${PRJ_ROOT}/test/common/mock_mem mock_mem)
add_subdirectory(${PRJ_ROOT}/test/common/mock_os mock_os)
add_subdirectory(${PRJ_ROOT}/test/common/mock_wifi_manager_storage mock_wifi_manager_storage)

target_link_libraries(tests PRIVATE 
    ${UNITY_LIB}
)

# 允许使用CTest运行测试
enable_testing()
add_test(NAME wifi_manager_tests COMMAND tests)
