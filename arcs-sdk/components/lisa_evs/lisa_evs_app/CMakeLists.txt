listenai_library_named(psram_lisa_evs_app)

# sources
listenai_library_sources(
    lisa_evs_app.c
    evs_auth_mgr/evs_auth_mgr.c
    playmp3.c
)

# includes
listenai_include_directories(
    ./
    ./evs_auth_mgr
)

