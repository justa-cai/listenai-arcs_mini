# LISA Shell
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_SHELL)

listenai_library_named(lisa_shell)

listenai_library_sources(
    lisa_shell.c
)
listenai_include_directories(.)

endif() # CONFIG_LISA_SHELL
