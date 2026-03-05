cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 添加测试源文件
target_sources(${PROJECT_NAME} PRIVATE 
    test_lisa_device.c          # 主程序/测试运行器
    test_common.c               # 公共测试辅助函数
    test_device_register.c      # 设备注册测试
    test_device_get.c           # 设备获取测试
    test_device_state.c         # 设备状态测试
    test_device_stats.c         # 统计信息测试
    test_device_foreach.c       # 设备遍历测试
    test_device_priority.c      # 优先级测试
    test_device_query.c         # 查询接口测试
    test_device_debug.c         # 调试接口测试
)

target_link_libraries(${PROJECT_NAME} PRIVATE unity)