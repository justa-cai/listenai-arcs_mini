cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 添加测试源文件
target_sources(${PROJECT_NAME} PRIVATE
    test_app_player.c           # 主测试运行器
    test_common.c               # 公共辅助函数
    test_player_lifecycle.c     # 生命周期测试
    test_player_callback.c      # 回调测试
    test_player_state.c         # 状态测试
    test_player_play.c          # 播放功能测试
    test_player_control.c       # 播放控制测试
    test_player_seek.c          # 进度控制测试
    test_player_volume.c        # 音量控制测试
    test_player_stream.c        # 流式接口测试
)

add_subdirectory(common)

target_link_libraries(${PROJECT_NAME} PRIVATE unity)
