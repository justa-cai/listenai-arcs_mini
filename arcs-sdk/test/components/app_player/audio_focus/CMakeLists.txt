cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 添加测试源文件
target_sources(${PROJECT_NAME} PRIVATE
    test_app_player.c           # 主测试运行器
    test_common.c               # 公共辅助函数和焦点配置
    test_focus_preempt.c        # 焦点抢占测试用例 (13个测试)
    test_focus_behavior.c       # 焦点行为策略测试 (4个测试)
    test_focus_callback.c       # 焦点回调机制测试 (6个测试)
    test_focus_concurrency.c    # 焦点并发测试 (6个测试)
)

add_subdirectory(common)

target_link_libraries(${PROJECT_NAME} PRIVATE unity)
