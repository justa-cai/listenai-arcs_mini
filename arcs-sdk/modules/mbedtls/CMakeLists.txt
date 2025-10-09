if (DEFINED CONFIG_SDK_MODULE_MBEDTLS)

    set(MBEDTLS_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls)
    set(MBEDTLS_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/configs)

    listenai_library_named(mbedtls)

    if (CONFIG_MBEDTLS_HSU)
        listenai_compile_definitions(CFG_HSU=1)
    endif()

    list(APPEND mbedtls_src_crypto
        ${MBEDTLS_LIBRARY_DIR}/library/aes.c
        ${MBEDTLS_LIBRARY_DIR}/library/aesni.c
        ${MBEDTLS_LIBRARY_DIR}/library/arc4.c
        ${MBEDTLS_LIBRARY_DIR}/library/aria.c
        ${MBEDTLS_LIBRARY_DIR}/library/asn1parse.c
        ${MBEDTLS_LIBRARY_DIR}/library/asn1write.c
        ${MBEDTLS_LIBRARY_DIR}/library/base64.c
        ${MBEDTLS_LIBRARY_DIR}/library/bignum.c
        ${MBEDTLS_LIBRARY_DIR}/library/blowfish.c
        ${MBEDTLS_LIBRARY_DIR}/library/camellia.c
        ${MBEDTLS_LIBRARY_DIR}/library/ccm.c
        ${MBEDTLS_LIBRARY_DIR}/library/chacha20.c
        ${MBEDTLS_LIBRARY_DIR}/library/chachapoly.c
        ${MBEDTLS_LIBRARY_DIR}/library/cipher.c
        ${MBEDTLS_LIBRARY_DIR}/library/cipher_wrap.c
        ${MBEDTLS_LIBRARY_DIR}/library/constant_time.c
        ${MBEDTLS_LIBRARY_DIR}/library/cmac.c
        ${MBEDTLS_LIBRARY_DIR}/library/ctr_drbg.c
        ${MBEDTLS_LIBRARY_DIR}/library/des.c
        ${MBEDTLS_LIBRARY_DIR}/library/dhm.c
        ${MBEDTLS_LIBRARY_DIR}/library/ecdh.c
        ${MBEDTLS_LIBRARY_DIR}/library/ecdsa.c
        ${MBEDTLS_LIBRARY_DIR}/library/ecjpake.c
        ${MBEDTLS_LIBRARY_DIR}/library/ecp.c
        ${MBEDTLS_LIBRARY_DIR}/library/ecp_curves.c
        ${MBEDTLS_LIBRARY_DIR}/library/entropy.c
        ${MBEDTLS_LIBRARY_DIR}/library/entropy_poll.c
        ${MBEDTLS_LIBRARY_DIR}/library/error.c
        ${MBEDTLS_LIBRARY_DIR}/library/gcm.c
        ${MBEDTLS_LIBRARY_DIR}/library/havege.c
        ${MBEDTLS_LIBRARY_DIR}/library/hkdf.c
        ${MBEDTLS_LIBRARY_DIR}/library/hmac_drbg.c
        ${MBEDTLS_LIBRARY_DIR}/library/md.c
        ${MBEDTLS_LIBRARY_DIR}/library/md2.c
        ${MBEDTLS_LIBRARY_DIR}/library/md4.c
        ${MBEDTLS_LIBRARY_DIR}/library/md5.c
        ${MBEDTLS_LIBRARY_DIR}/library/memory_buffer_alloc.c
        ${MBEDTLS_LIBRARY_DIR}/library/mps_reader.c
        ${MBEDTLS_LIBRARY_DIR}/library/mps_trace.c
        ${MBEDTLS_LIBRARY_DIR}/library/nist_kw.c
        ${MBEDTLS_LIBRARY_DIR}/library/oid.c
        ${MBEDTLS_LIBRARY_DIR}/library/padlock.c
        ${MBEDTLS_LIBRARY_DIR}/library/pem.c
        ${MBEDTLS_LIBRARY_DIR}/library/pk.c
        ${MBEDTLS_LIBRARY_DIR}/library/pk_wrap.c
        ${MBEDTLS_LIBRARY_DIR}/library/pkcs12.c
        ${MBEDTLS_LIBRARY_DIR}/library/pkcs5.c
        ${MBEDTLS_LIBRARY_DIR}/library/pkparse.c
        ${MBEDTLS_LIBRARY_DIR}/library/pkwrite.c
        ${MBEDTLS_LIBRARY_DIR}/library/platform.c
        ${MBEDTLS_LIBRARY_DIR}/library/platform_util.c
        ${MBEDTLS_LIBRARY_DIR}/library/poly1305.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_aead.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_cipher.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_client.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_driver_wrappers.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_ecp.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_hash.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_mac.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_rsa.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_se.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_slot_management.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_crypto_storage.c
        ${MBEDTLS_LIBRARY_DIR}/library/psa_its_file.c
        ${MBEDTLS_LIBRARY_DIR}/library/ripemd160.c
        ${MBEDTLS_LIBRARY_DIR}/library/rsa.c
        ${MBEDTLS_LIBRARY_DIR}/library/rsa_internal.c
        ${MBEDTLS_LIBRARY_DIR}/library/sha1.c
        ${MBEDTLS_LIBRARY_DIR}/library/sha256.c
        ${MBEDTLS_LIBRARY_DIR}/library/sha512.c
        ${MBEDTLS_LIBRARY_DIR}/library/threading.c
        ${MBEDTLS_LIBRARY_DIR}/library/timing.c
        ${MBEDTLS_LIBRARY_DIR}/library/version.c
        ${MBEDTLS_LIBRARY_DIR}/library/version_features.c
        ${MBEDTLS_LIBRARY_DIR}/library/xtea.c
    )

    list(APPEND mbedtls_src_crypto
        ${MBEDTLS_LIBRARY_DIR}/3rdparty/everest/library/everest.c
        ${MBEDTLS_LIBRARY_DIR}/3rdparty/everest/library/x25519.c
        ${MBEDTLS_LIBRARY_DIR}/3rdparty/everest/library/Hacl_Curve25519_joined.c
    )

    list(APPEND mbedtls_src_x509
        ${MBEDTLS_LIBRARY_DIR}/library/certs.c
        ${MBEDTLS_LIBRARY_DIR}/library/pkcs11.c
        ${MBEDTLS_LIBRARY_DIR}/library/x509.c
        ${MBEDTLS_LIBRARY_DIR}/library/x509_create.c
        ${MBEDTLS_LIBRARY_DIR}/library/x509_crl.c
        ${MBEDTLS_LIBRARY_DIR}/library/x509_crt.c
        ${MBEDTLS_LIBRARY_DIR}/library/x509_csr.c
        ${MBEDTLS_LIBRARY_DIR}/library/x509write_crt.c
    )

    list(APPEND mbedtls_src_tls
        ${MBEDTLS_LIBRARY_DIR}/library/debug.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_cache.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_ciphersuites.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_cli.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_cookie.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_msg.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_srv.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_ticket.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_tls.c
        ${MBEDTLS_LIBRARY_DIR}/library/ssl_tls13_keys.c
    )

    listenai_library_sources(
        ${mbedtls_src_crypto}
        ${mbedtls_src_x509}
        ${mbedtls_src_tls}       
    )

    listenai_include_directories(
        ${MBEDTLS_LIBRARY_DIR}/include
        ${MBEDTLS_LIBRARY_DIR}/3rdparty/everest/include
    )

    target_include_directories(${LISTENAI_CURRENT_LIBRARY} 
        PRIVATE ${MBEDTLS_LIBRARY_DIR}/library
        PRIVATE ${MBEDTLS_LIBRARY_DIR}/3rdparty/everest/include/everest
        PRIVATE ${MBEDTLS_LIBRARY_DIR}/3rdparty/everest/include/everest/kremlib
    )

    listenai_compile_definitions(
        MBEDTLS_CONFIG_FILE="${CONFIG_MBEDTLS_CFG_FILE}"
    )

    listenai_include_directories(${MBEDTLS_CONFIG_DIR})

    add_subdirectory(port)

endif()
