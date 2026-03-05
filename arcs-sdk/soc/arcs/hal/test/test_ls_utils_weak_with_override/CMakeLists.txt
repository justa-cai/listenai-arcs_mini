cmake_minimum_required(VERSION 3.16)
project(test_ls_utils_weak_with_override C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 配置cmake模块路径
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake")

find_package(unity QUIET)

if(unity_FOUND)
    set(UNITY_LIB unity::framework)
else()
    message(FATAL_ERROR "Not found Unity package")
endif()

set(HAL_ROOT ../..)

# 创建测试可执行文件
add_executable(tests)

# Unity 测试框架配置
target_include_directories(tests PRIVATE
    ${HAL_ROOT}/test/utils/unity_config
    ${HAL_ROOT}/test/utils/fff
)

# 添加被测源文件 (包含 weak 符号)
target_sources(tests PRIVATE
    ${HAL_ROOT}/chip/arcs/utils/ls_utils.c
)

# 添加测试源文件 (包含覆盖实现)
target_sources(tests PRIVATE
    test_with_override.c
    mock_gpadc.c
)

# 头文件路径
target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HAL_ROOT}/chip/arcs/include
)

# 添加预处理器定义
target_compile_definitions(tests PRIVATE
    CONFIG_PM=0
)

# 链接 Unity 测试框架
target_link_libraries(tests ${UNITY_LIB})

# 允许使用CTest运行测试
enable_testing()
add_test(NAME test_ls_utils_weak_with_override COMMAND tests)
