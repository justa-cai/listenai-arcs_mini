# LISA Bluetooth
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if (CONFIG_LISA_BLUETOOTH)
    listenai_library_named(lisa_bluetooth)

    set(HAL_CHIP_DIR ${ARCS_SDK_BASE}/soc/arcs/hal/chip/arcs)

    # Include directories shared by both AP and CP cores
    listenai_include_directories(
        .
    )

    if (CONFIG_LISA_BLUETOOTH_BUILD_IPC_CLIENT)
        # IPC caller: forward Bluetooth API calls to the peer image.
        listenai_library_sources(
            lisa_ble_api_ipc.c
        )

        if (CONFIG_LISA_BLUETOOTH_CLASSIC)
            listenai_library_sources(
                lisa_bt_classic_api_ipc.c
            )
        endif()
    endif()

    if (CONFIG_LISA_BLUETOOTH_BUILD_LOCAL_STACK)
        # Local stack or IPC receiver: run the Bluetooth stack in this image.
        listenai_include_directories(
            ./aud_if
        )

        listenai_library_sources(
            bt_app_user.c
            bt_ble_user.c
            bt_stack_cfg.c
            lisa_bluetooth.c
            lisa_ble_api_local.c
        )

        if ((CONFIG_LISA_BLUETOOTH_BUILD_NETCFG_BLE_SERVER OR CONFIG_BLE_PROFILE_NETCFG_BLES) AND NOT CONFIG_LISA_BLUETOOTH_CUSTOM_NETCFG)
            listenai_library_sources(
                app_net_cfg.c
            )
        endif()

        if (CONFIG_LISA_BLUETOOTH_CLASSIC)
            listenai_library_sources(
                bt_classic_user.c
                lisa_bt_classic_api_local.c
            )
        endif()

        if (CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO)
            listenai_library_sources(
                aud_if/aud_if.c
                aud_if/aud_pro_if.c
            )
        endif()
    endif()
endif()
