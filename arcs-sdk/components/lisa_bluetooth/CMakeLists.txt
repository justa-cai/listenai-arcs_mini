# LISA Bluetooth
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if (CONFIG_LISA_BLUETOOTH)
    listenai_library_named(lisa_bluetooth)
    
    listenai_include_directories(
        ./
        ./aud_if
        ${ARCS_SDK_BASE}/soc/arcs/hal/chip/arcs/wcnd/include/bt_inc
        ${ARCS_SDK_BASE}/soc/arcs/hal/chip/arcs/btos/task/api
        ${ARCS_SDK_BASE}/soc/arcs/hal/chip/arcs/btos
        ${ARCS_SDK_BASE}/soc/arcs/hal/modules/profiles/dis/diss/api
        ${ARCS_SDK_BASE}/soc/arcs/hal/modules/profiles/bas/bass/api
    )

    listenai_library_sources(
        bt_app_if.c
        bt_ble_if.c
        bt_stack_if.c
        bt_classic_if.c
        lisa_bluetooth.c
    )

    if (CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO)
        listenai_library_sources(
            bt_music_if.c
            aud_if/aud_if.c
            aud_if/aud_pro_if.c
        )
    endif()
    if (CONFIG_LISA_BLUETOOTH_CLASSIC_HFP)
        listenai_library_sources(
            bt_call_if.c
        )
    endif()
    add_subdirectory(lib)
endif()

