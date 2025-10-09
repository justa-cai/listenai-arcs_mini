#
# SPDX-License-Identifier: BSD-3-Clause
#

listenai_library_sources(
  regcomp.c
  regfree.c
  regexec.c
  regerror.c
)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
