file(GLOB_RECURSE EVS_SOURCES
    auth/lisa_evs_auth.c
    utils/*.c
    lisa_evs.c
    lisa_evs_sync.c
    focus/lisa_evs_audiomgr.c
    rec/lisa_evs_rec.c
    sockets/*.c
    proc/*.c
    xz/*.c
)

listenai_library_named(psram_lisa_evs)

# sources
listenai_library_sources(
    ${EVS_SOURCES}
)

# includes
listenai_include_directories(
    ./
    include
    sockets
)

target_compile_definitions(psram_lisa_evs PRIVATE EXPORT= FIXED_POINT USE_SMALLFT)
