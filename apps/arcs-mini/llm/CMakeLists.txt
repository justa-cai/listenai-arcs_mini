listenai_library_named(app-llm)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

listenai_library_sources(
    story_arena.c
    story_mem.c
    neo.c
    story_llm.c
    llm_app.c
)

# 推理内核是纯计算热点（dot_q4q8 每 token 千万级 MAC），全局 -Os 严重拖慢；
    # 单独提级（正确性不受影响，仅代码体积增大，flash 充裕）
set_source_files_properties(neo.c PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
set_source_files_properties(story_llm.c PROPERTIES COMPILE_OPTIONS "-O2")
