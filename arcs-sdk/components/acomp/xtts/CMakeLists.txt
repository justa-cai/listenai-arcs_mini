
if (CONFIG_ACOMP_XTTS)

listenai_library_sources(
    acomp_xtts.c
    acomp_xtts_res.c
)

listenai_include_directories(
    ./
)
endif()
