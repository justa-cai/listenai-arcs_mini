cmake_minimum_required(VERSION 3.10)
project(ebus_example C)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加调试符号
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CONFIG_EBUS_ENV_OS_POSIX ON CACHE BOOL "Use POSIX environment" FORCE)
set(CONFIG_EBUS_ENV_OS_FREERTOS OFF CACHE BOOL "Use FreeRTOS environment" FORCE)
add_definitions(-DCONFIG_EBUS_ENV_OS_POSIX=1)
add_definitions(-UCONFIG_EBUS_ENV_OS_FREERTOS)  # 取消FREERTOS定义

# 添加可执行文件
add_executable(ebus_example main.c)

# 设置包含目录
target_include_directories(ebus_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inlcude
)

# 链接ebus库
target_link_libraries(ebus_example PRIVATE
    ebus
    pthread
)

# 如果ebus库不在系统路径中，需要指定库路径
if(NOT DEFINED LISTENAI_SDK_PATH)
    # 添加ebus库的构建
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/ebus)
endif()

# 添加一个自定义目标，用于运行示例程序
add_custom_target(run_example
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ebus_example
    DEPENDS ebus_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "运行ebus示例程序"
)
