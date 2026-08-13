# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

listenai_library_named(arcs_soc_common)

listenai_library_sources(
    sys_reboot.c
    sys_reset_reason.c
    sys_chip_id.c
)

add_subdirectory(startup)
