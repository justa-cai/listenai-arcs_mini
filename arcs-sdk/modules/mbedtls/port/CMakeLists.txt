set(MBEDTLS_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

listenai_library_sources(
    ${MBEDTLS_PORT_DIR}/ls_crypto.c
    ${MBEDTLS_PORT_DIR}/csk_common.c
)

if(CONFIG_LWIP)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/net_sockets.c
    )
endif()

if (CONFIG_WIFI)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/wifi_lib_api/mbedtls_crypto.c
    )
endif()

listenai_include_directories(${LISTENAI_CURRENT_LIBRARY}
    PRIVATE ${MBEDTLS_PORT_DIR}
)

# target_include_directories(${LISTENAI_CURRENT_LIBRARY}
#     PUBLIC ${MBEDTLS_PORT_DIR}/platform
# )
listenai_include_directories(
    ${MBEDTLS_PORT_DIR}/platform
)

if(NOT DEFINED CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/mem/ls_mem.c
    )

    # target_include_directories(${LISTENAI_CURRENT_LIBRARY}
    #     PUBLIC ${MBEDTLS_PORT_DIR}/mem
    # )
    listenai_include_directories(
        ${MBEDTLS_PORT_DIR}/mem
    )
endif()

if(CONFIG_MBEDTLS_HAVE_TIME_DATE)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/time/ls_timer.c
    )

    target_include_directories(${LISTENAI_CURRENT_LIBRARY}
        PUBLIC ${MBEDTLS_PORT_DIR}/time
    )
endif()

# if(CONFIG_MBEDTLS_HARDWARE_ENTROPY)
listenai_library_sources(
    ${MBEDTLS_PORT_DIR}/csk_trng.c
)
# endif()

if(CONFIG_MBEDTLS_HARDWARE_AES)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/aes/csk_aes.c
        ${MBEDTLS_PORT_DIR}/aes/csk_aes_xts.c
    )

    # target_include_directories(${LISTENAI_CURRENT_LIBRARY}
    #     PUBLIC ${MBEDTLS_PORT_DIR}/aes
    # )
    listenai_include_directories(${MBEDTLS_PORT_DIR}/aes)
endif()

if(CONFIG_MBEDTLS_HARDWARE_SHA1)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/sha/csk_sha1.c
    )

    target_include_directories(${LISTENAI_CURRENT_LIBRARY}
        PUBLIC ${MBEDTLS_PORT_DIR}/sha
    )
endif()

if(CONFIG_MBEDTLS_HARDWARE_SHA256)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/sha/csk_sha256.c
    )

    target_include_directories(${LISTENAI_CURRENT_LIBRARY}
        PUBLIC ${MBEDTLS_PORT_DIR}/sha
    )
endif()

if(CONFIG_MBEDTLS_HARDWARE_SHA512)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/sha/csk_sha512.c
    )

    target_include_directories(${LISTENAI_CURRENT_LIBRARY}
        PUBLIC ${MBEDTLS_PORT_DIR}/sha
    )
endif()

if(CONFIG_MBEDTLS_HARDWARE_AES_CCM)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/mac/ccm/csk_ccm.c
    )

    # target_include_directories(${LISTENAI_CURRENT_LIBRARY}
    #     PUBLIC ${MBEDTLS_PORT_DIR}/mac/ccm
    # )
    listenai_include_directories(${MBEDTLS_PORT_DIR}/mac/ccm)
endif()

if (CONFIG_MBEDTLS_HARDWARE_AES_GCM)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/mac/gcm/csk_gcm.c
    )

    # target_include_directories(${LISTENAI_CURRENT_LIBRARY}
    #     PUBLIC ${MBEDTLS_PORT_DIR}/mac/gcm
    # )
    listenai_include_directories(${MBEDTLS_PORT_DIR}/mac/gcm)
endif()

if(CONFIG_MBEDTLS_HARDWARE_AES_CMAC)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/mac/cmac/csk_cmac.c
    )

    # target_include_directories(${LISTENAI_CURRENT_LIBRARY}
    #     PUBLIC ${MBEDTLS_PORT_DIR}/mac/cmac
    # )
    listenai_include_directories(${MBEDTLS_PORT_DIR}/mac/cmac)
endif()

if(CONFIG_MBEDTLS_HARDWARE_SHA_HMAC)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/mac/hmac/csk_hmac.c
    )

    target_link_options(${LISTENAI_CURRENT_LIBRARY} BEFORE PUBLIC "-Wl,--wrap=mbedtls_md_hmac")
endif()

if(CONFIG_MBEDTLS_HARDWARE_MPI_MODULAR_EXPONENTIATION)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/bignum/bignum_alt.c
    )

    target_link_options(${LISTENAI_CURRENT_LIBRARY} BEFORE PUBLIC "-Wl,--wrap=mbedtls_mpi_exp_mod")
endif()

if(CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY)
    listenai_library_sources(
        ${MBEDTLS_PORT_DIR}/bignum/bignum_alt.c
        ${MBEDTLS_PORT_DIR}/ecc/ecc_alt.c
        ${MBEDTLS_PORT_DIR}/ecc/ecdsa_alt.c
    )

    target_link_options(${LISTENAI_CURRENT_LIBRARY} BEFORE PUBLIC "-Wl,--wrap=mbedtls_mpi_exp_mod")
    target_link_options(${LISTENAI_CURRENT_LIBRARY} BEFORE PUBLIC "-Wl,--wrap=mbedtls_ecp_mul_restartable")
endif()
