# Driver for VENUSA

file(GLOB_RECURSE DRIVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

# Filter out USB subsystems from base sources
list(FILTER DRIVER_SOURCES EXCLUDE REGEX ".*/usb_subsys/.*")
list(FILTER DRIVER_SOURCES EXCLUDE REGEX ".*/usb_subsys_cherry/.*")

# Handle USB Subsystem selection
if(CONFIG_USB_SUBSYS STREQUAL "TINY")
    message(STATUS "USB Subsystem: TinyUSB")
    
    # Add TinyUSB sources
    file(GLOB_RECURSE TINYUSB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/*.S
    )
    list(APPEND DRIVER_SOURCES ${TINYUSB_SOURCES})

    # Add TinyUSB include directories
    set(USB_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/common
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/class
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/class/cdc
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/class/hid
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/class/vendor
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/class/video
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/controller
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/device
    )

    # Handle tusb_config.h
    if(DEFINED CFG_TUSB_CONFIG_H_PATHNAME)
        message(STATUS "PRIVATE tusb_config.h @ ${CFG_TUSB_CONFIG_H_PATHNAME}")
        add_compile_definitions(CFG_TUSB_CONFIG_FILE="${CFG_TUSB_CONFIG_H_PATHNAME}")
    else()
        message(STATUS "PUBLIC tusb_config.h @ ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys/tusb_config.h")
    endif()

elseif(CONFIG_USB_SUBSYS STREQUAL "CHERRY")
    message(STATUS "USB Subsystem: CherryUSB")

    # Add CherryUSB sources
    file(GLOB_RECURSE CHERRYUSB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/*.S
    )
    list(APPEND DRIVER_SOURCES ${CHERRYUSB_SOURCES})

    #filter out lisa driver and use musb driver instead
    list(FILTER DRIVER_SOURCES EXCLUDE REGEX ".*/usb_subsys_cherry/port/lisa/.*")
    # Filter host or device controllers depending on the target name
    if(TGT MATCHES "usbh_.*")
        list(FILTER DRIVER_SOURCES EXCLUDE REGEX ".*/usb_subsys_cherry/port/musb/usb_dc_musb\\.c")
    else()
        list(FILTER DRIVER_SOURCES EXCLUDE REGEX ".*/usb_subsys_cherry/port/musb/usb_hc_musb\\.c")
    endif()

    # Add CherryUSB include directories
    set(USB_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/core
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/common
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/class
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/class/hub
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/class/cdc
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/class/msc
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/class/video
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_subsys_cherry/port
    )

else()
    message(STATUS "USB Subsystem: NONE")
    set(USB_INCLUDE_DIRS "")
endif()

create_module_library(drv "${DRIVER_SOURCES}")

target_include_directories(drv PUBLIC
    ${USB_INCLUDE_DIRS}
)
