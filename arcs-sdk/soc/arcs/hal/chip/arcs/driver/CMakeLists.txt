cmake_minimum_required(VERSION 3.10)

listenai_compile_definitions(CFG_MEM_OPT=1)

file(GLOB_RECURSE ARCS_HAL_SRCS
    ${ARCS_HAL_LIB_PATH}/chip/arcs/driver/*.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/uart/
    ${CMAKE_CURRENT_SOURCE_DIR}/gpio/
)

list(APPEND ARCS_HAL_SRCS ${ARCS_HAL_SRCS})

if(CONFIG_ARCS_HAL_LSF)

    if(CONFIG_HAL_LSF_CLIENT)
        file(GLOB_RECURSE ARCS_HAL_SRCS_LSF
            ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_client/*.c
            ${ARCS_HAL_LIB_PATH}/chip/arcs/urpc_client/*.c
        )

        listenai_include_directories(
            ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_client
            ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_client/mtrace
            ${ARCS_HAL_LIB_PATH}/chip/arcs/include/lsf_client
            ${ARCS_HAL_LIB_PATH}/chip/arcs/include/lsf_client/fifo
            ${ARCS_HAL_LIB_PATH}/chip/arcs/include/lsf_client/ic_mutex
        )

        if(CONFIG_ARCS_HAL_IC_MUTEX)
            file(GLOB_RECURSE ARCS_HAL_SRCS_IC_MUTEX
                ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_client/ic_mutex/*.c
            )
            list(APPEND ARCS_HAL_SRCS_LSF ${ARCS_HAL_SRCS_IC_MUTEX})
        endif()

    elseif(CONFIG_HAL_LSF_SERVER)
        file(GLOB_RECURSE ARCS_HAL_SRCS_LSF
            ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_server/*.c
            ${ARCS_HAL_LIB_PATH}/chip/arcs/urpc_server/*.c
        )
        listenai_include_directories(
            ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_server
            ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_server/mtrace
            ${ARCS_HAL_LIB_PATH}/chip/arcs/include/lsf_server
            ${ARCS_HAL_LIB_PATH}/chip/arcs/include/lsf_server/fifo
            ${ARCS_HAL_LIB_PATH}/chip/arcs/include/lsf_server/ic_mutex
        )

        if(CONFIG_ARCS_HAL_IC_MUTEX)
            file(GLOB_RECURSE ARCS_HAL_SRCS_IC_MUTEX
                ${ARCS_HAL_LIB_PATH}/chip/arcs/lsf_server/ic_mutex/*.c
            )
            list(APPEND ARCS_HAL_SRCS_LSF ${ARCS_HAL_SRCS_IC_MUTEX})
        endif()
    endif()

    listenai_include_directories(
        ${ARCS_HAL_LIB_PATH}/chip/arcs/include/urpc
    )

    # list(FILTER ARCS_HAL_SRCS_LSF EXCLUDE REGEX ".*ic_mutex.*\\.c$")
    list(APPEND ARCS_HAL_SRCS ${ARCS_HAL_SRCS_LSF})

endif()

list(FILTER ARCS_HAL_SRCS EXCLUDE REGEX ".*usb_subsys/.*")
list(FILTER ARCS_HAL_SRCS EXCLUDE REGEX ".*ota/.*")

listenai_library_sources(${ARCS_HAL_SRCS})
listenai_include_directories(
    ${ARCS_HAL_LIB_PATH}/chip/arcs/driver/debug
)

