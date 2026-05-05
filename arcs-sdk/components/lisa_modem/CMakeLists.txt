if (CONFIG_LISA_MODEM)
listenai_library_named(lisa_modem)

set(MODEM_DRIVER_SOURCES)
set(MODEM_DRIVER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/drivers)

if (CONFIG_LISA_MODEM_DRIVER_ML307)
    file(GLOB ML307_DRIVER_SOURCES drivers/ml307/*.c)
    list(APPEND MODEM_DRIVER_SOURCES ${ML307_DRIVER_SOURCES})
    list(APPEND MODEM_DRIVER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/drivers/ml307)
endif()

if (CONFIG_LISA_MODEM_DRIVER_EC801E)
    file(GLOB EC801E_DRIVER_SOURCES drivers/ec801e/*.c)
    list(APPEND MODEM_DRIVER_SOURCES ${EC801E_DRIVER_SOURCES})
    list(APPEND MODEM_DRIVER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/drivers/ec801e)
endif()

listenai_library_sources(
    at_mem.c
    at_client.c
    transport/at_transport_uart.c
    lisa_modem_module.c
    core/modem_probe_utils.c
    drivers/common/modem_dispatcher.c
    drivers/common/modem_endpoint_runtime.c
    drivers/common/modem_runtime_common.c
    ${MODEM_DRIVER_SOURCES}
)

if (CONFIG_CHERRYUSB_HOST_GSM)
    listenai_library_sources(
        transport/at_transport_usbh_serial.c
    )
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${MODEM_DRIVER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/transport
)
endif()
