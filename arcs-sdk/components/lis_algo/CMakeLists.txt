if (DEFINED CONFIG_LIS_ALGO)
    listenai_library_named(lis_algo)
    listenai_library_sources(
        lis_ocr.c
        lis_trans.c
        lis_tts.c
        lis_algo.c
        algo/img_stream/img_stream.c
        #algo
        algo/algo_lsf_client.c
        #algo: xtts
        algo/xttsservice/xtts_app.c
        algo/xttsservice/xtts_server.c
        algo/xttsservice/xtts_service.c
        algo/xttsservice/xtts_stream.c
    )
    listenai_include_directories(
        ./
        ./algo
        ./algo/xttsservice
        ./algo/img_stream
    )
if (DEFINED CONFIG_LIS_ALGO_WORDSEG)
    listenai_library_sources(
        #algo: wordseg
        algo/wordseg/source/friso_array.c
        algo/wordseg/source/friso_ctype.c
        algo/wordseg/source/friso_GBK.c
        algo/wordseg/source/friso_hash.c
        algo/wordseg/source/friso_lexicon.c
        algo/wordseg/source/friso_link.c
        algo/wordseg/source/friso_memory.c
        algo/wordseg/source/friso_string.c
        algo/wordseg/source/friso_UTF8.c
        algo/wordseg/source/friso.c
        algo/wordseg/source/ipa2iflyphone.c
        algo/wordseg/source/wordseg_engine.c
        algo/wordseg/source/wordseg_res.c
        algo/wordseg/source/wordseg_wordcorrection.c
        algo/wordseg/source/wordseg_wrapper.c
        algo/wordseg/source/wordseg.c
    )
    listenai_include_directories(
        ./algo/wordseg/include/
        ./algo/wordseg/source
    )
endif()

endif()
