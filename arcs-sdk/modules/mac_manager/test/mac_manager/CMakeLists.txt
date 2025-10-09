cmake_minimum_required(VERSION 3.16)
project(mac_manager_test C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

target_include_directories(tests PRIVATE
    ${PRJ_ROOT}/test/utils/fff
    ${PRJ_ROOT}/test/utils/unity_config
)

target_sources(tests PRIVATE
    ${PRJ_ROOT}/src/mac_manager.c
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRJ_ROOT}/src
    ${PRJ_ROOT}/test/common/fake_lisa_log
)

target_sources(tests PRIVATE
    test_mac_manager.c
    mock_mem_ops.c
    mock_content_ops.c
)


target_link_libraries(tests ${UNITY_LIB})

# 允许使用CTest运行测试
enable_testing()
add_test(NAME mac_manager_tests COMMAND tests)
