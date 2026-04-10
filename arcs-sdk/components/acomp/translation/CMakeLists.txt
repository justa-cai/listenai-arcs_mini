
if (CONFIG_ACOMP_TRANSLATION)

listenai_library_sources(
    acomp_translation.c
    acomp_translation_res.c
)

listenai_include_directories(
    ./
)
endif()
