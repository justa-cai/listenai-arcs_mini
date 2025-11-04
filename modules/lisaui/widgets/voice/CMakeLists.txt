# SPDX-License-Identifier: Apache-2.0

listenai_library_named(lisa_ui_voice)

FILE(GLOB LISAUI_SOURCES "*.c")
listenai_library_sources(
    ${LISAUI_SOURCES}
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
