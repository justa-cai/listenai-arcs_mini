listenai_library_named(app-llm)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

listenai_library_sources(
    story_arena.c
    story_mem.c
    neo.c
    story_llm.c
    llm_app.c
)
