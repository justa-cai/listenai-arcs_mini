#
# SPDX-License-Identifier: BSD-3-Clause
#

if (CONFIG_EXCLIB)
    listenai_library_named(exclib)
    add_subdirectory(posix)
endif()
