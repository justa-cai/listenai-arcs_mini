cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(remote-ap C ASM)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
    src/llm_service.c
    src/neo.c
    src/story_arena.c
    src/story_llm.c
    src/story_mem.c
    res/model.S
)

# .incbin 的搜索路径是编译工作目录（build/），必须显式指向源码 res/
set_source_files_properties(res/model.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/res"
)

# 推理内核单独提级（全局 -Os 严重拖慢热点）
set_source_files_properties(src/neo.c PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
set_source_files_properties(src/story_llm.c PROPERTIES COMPILE_OPTIONS "-O2")

# neo.c 的 softmax/gelu 需要 expf/tanhf（数学库）
listenai_link_libraries(m)
