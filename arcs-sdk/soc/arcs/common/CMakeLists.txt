# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

listenai_library_named(arcs_soc_common)

listenai_library_sources(
    sys_reboot.c
)

listenai_include_directories(
    ${ARCS_SDK_BASE}/system/os/inc
)

add_subdirectory(startup)
