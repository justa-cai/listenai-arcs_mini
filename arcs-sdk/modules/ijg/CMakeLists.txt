if (CONFIG_SDK_MODULE_IJG)

    listenai_library_named(ijg)

    set(IJG_DIR ${CMAKE_CURRENT_SOURCE_DIR})

    list(APPEND ijg_common_src
        ${IJG_DIR}/jaricom.c
        ${IJG_DIR}/jcomapi.c
        ${IJG_DIR}/jutils.c
        ${IJG_DIR}/jerror.c

        ${IJG_DIR}/jmemmgr.c
        # ${IJG_DIR}/jmemansi.c
        ${IJG_DIR}/jmemnobs.c
    )

    list(APPEND ijg_compression_src
        ${IJG_DIR}/jcapimin.c
        ${IJG_DIR}/jcapistd.c
        ${IJG_DIR}/jcarith.c
        ${IJG_DIR}/jctrans.c
        ${IJG_DIR}/jcparam.c

        ${IJG_DIR}/jdatadst.c
        ${IJG_DIR}/jcinit.c
        ${IJG_DIR}/jcmaster.c
        ${IJG_DIR}/jcmarker.c
        ${IJG_DIR}/jcmainct.c
        ${IJG_DIR}/jcprepct.c

        ${IJG_DIR}/jccoefct.c
        ${IJG_DIR}/jccolor.c
        ${IJG_DIR}/jcsample.c
        ${IJG_DIR}/jchuff.c
        ${IJG_DIR}/jcdctmgr.c
        ${IJG_DIR}/jfdctfst.c

        ${IJG_DIR}/jfdctflt.c
        ${IJG_DIR}/jfdctint.c
    )

    list(APPEND ijg_dempression_src
        ${IJG_DIR}/jdapimin.c
        ${IJG_DIR}/jdapistd.c
        ${IJG_DIR}/jdarith.c
        ${IJG_DIR}/jdtrans.c
        ${IJG_DIR}/jdatasrc.c

        ${IJG_DIR}/jdmaster.c
        ${IJG_DIR}/jdinput.c
        ${IJG_DIR}/jdmarker.c
        ${IJG_DIR}/jdhuff.c
        ${IJG_DIR}/jdmainct.c
        
        ${IJG_DIR}/jdcoefct.c
        ${IJG_DIR}/jdpostct.c
        ${IJG_DIR}/jddctmgr.c
        ${IJG_DIR}/jidctfst.c
        ${IJG_DIR}/jidctflt.c
        
        ${IJG_DIR}/jidctint.c
        ${IJG_DIR}/jdsample.c
        ${IJG_DIR}/jdcolor.c
        ${IJG_DIR}/jquant1.c
        ${IJG_DIR}/jquant2.c
        ${IJG_DIR}/jdmerge.c
    )

    list(APPEND ijg_all_src 
        ${ijg_common_src}
        ${ijg_compression_src}
        ${ijg_dempression_src}
    )

    listenai_library_sources(${ijg_all_src})
    listenai_include_directories(${IJG_DIR})

endif()