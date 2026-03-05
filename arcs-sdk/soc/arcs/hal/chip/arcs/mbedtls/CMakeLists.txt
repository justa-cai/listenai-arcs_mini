cmake_minimum_required(VERSION 3.13)

if(CONFIG_ARCS_HAL_MBEDTLS)
    # 设置mbedtls版本目录
    message(STATUS "ARCS HAL mbedtls ver ${CONFIG_ARCS_HAL_MBEDTLS_VER}")
    set(MBEDTLSDIR ${CONFIG_ARCS_HAL_MBEDTLS_VER})

    # 核心源文件
    set(CSRCS_CORE
        ${MBEDTLSDIR}/library/platform_util.c
        ${MBEDTLSDIR}/library/constant_time.c
        ${MBEDTLSDIR}/library/bignum.c
        ${MBEDTLSDIR}/library/ecp.c
        ${MBEDTLSDIR}/library/ecp_curves.c
        ${MBEDTLSDIR}/library/ecdsa.c
        ${MBEDTLSDIR}/library/ecdh.c
        ${MBEDTLSDIR}/library/pk.c
        ${MBEDTLSDIR}/library/pk_wrap.c
        ${MBEDTLSDIR}/library/pkparse.c
        ${MBEDTLSDIR}/library/md.c
        ${MBEDTLSDIR}/library/asn1parse.c
        ${MBEDTLSDIR}/library/asn1write.c
        ${MBEDTLSDIR}/library/oid.c
    )

    # mbedtls 3.x版本需要额外的bignum_core.c
    if(CONFIG_ARCS_HAL_MBEDTLS_MAJOR_VER EQUAL 3)
        list(APPEND CSRCS_CORE ${MBEDTLSDIR}/library/bignum_core.c)
    endif()

    # 添加移植层文件
    listenai_library_sources(
        port/ls_crypto.c
        port/mbedtls_crypto.c
    )
    listenai_include_directories(
        port/
        ${MBEDTLSDIR}/
    )

    # 创建库
    listenai_library_named(arcs_hal_mbedtls)
    listenai_library_sources(${CSRCS_CORE})

    # 设置包含目录
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/${MBEDTLSDIR}/include
        ${CMAKE_HOME_DIRECTORY}/include/rtos
        ${CMAKE_HOME_DIRECTORY}/chip/${CHIP}/rtos/rtos_al
        ${CMAKE_HOME_DIRECTORY}/include/CMSIS
        ${CMAKE_HOME_DIRECTORY}/include/CMSIS/core
        ${CMAKE_HOME_DIRECTORY}/modules/rtos/portable/${BUILDTOOL}/non_secure
        ${CMAKE_CURRENT_SOURCE_DIR}/port
    )

    # 添加mbedtls内部库文件的私有包含路径（用于访问library/common.h等内部头文件）
    target_include_directories(${LISTENAI_CURRENT_LIBRARY}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${MBEDTLSDIR}/library
    )

    # 编译定义
    if(CONFIG_ARCS_HAL_MBEDTLS_CFG_HSU)
        listenai_compile_definitions(CFG_HSU)
    endif()

    if(DEFINED CONFIG_ARCS_HAL_MBEDTLS_MAJOR_VER)
        listenai_compile_definitions(MBEDTLS_MAJOR_VER=${CONFIG_ARCS_HAL_MBEDTLS_MAJOR_VER})
    endif()

    message(STATUS "ARCS HAL mbedtls major ver ${CONFIG_ARCS_HAL_MBEDTLS_MAJOR_VER}")
endif() # CONFIG_ARCS_HAL_MBEDTLS
