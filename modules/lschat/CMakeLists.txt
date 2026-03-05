if (DEFINED CONFIG_LS_CHAT)
    if (DEFINED CONFIG_LISA_LINUX)
        execute_process(
          COMMAND ${CMAKE_C_COMPILER} --version
          OUTPUT_VARIABLE C_COMPILER_VERSION
        )
        message(STATUS "C Compiler Version: ${C_COMPILER_VERSION}")
        add_subdirectory(${LISA_BASE} lisa)
        add_library(
            ls_chat
            STATIC
            src/lsc_core.c
            src/conns/lsc_conn.c
            src/sessions/session_objrec.c
            src/sessions/session_request.c
            src/sessions/session_text.c
            src/sessions/session_voice.c
            src/sessions/sessions_core.c
            src/stream_text/lsc_stream_text_thread.c
            src/stream_text/lsc_stream_text.c
            src/ultis/base64/lsc_base64.c
        )
        target_include_directories(ls_chat PUBLIC include/pri_include)
        target_include_directories(ls_chat PUBLIC src/ultis/base64)
        target_include_directories(ls_chat PUBLIC src/ultis)
        target_include_directories(ls_chat PUBLIC include)
        target_link_libraries(ls_chat PUBLIC lisa)
    elseif(DEFINED ENV{ARCS_BASE})
        listenai_library_named(ls_chat)
        listenai_library_sources(
            src/lsc_core.c
            src/conns/lsc_conn.c
            src/sessions/session_objrec.c
            src/sessions/session_request.c
            src/sessions/session_text.c
            src/sessions/session_voice.c
            src/sessions/sessions_core.c
            src/stream_text/lsc_stream_text_thread.c
            src/stream_text/lsc_stream_text.c
            src/ultis/base64/lsc_base64.c
        )
        listenai_include_directories(
          include/pri_include
          src/ultis/base64
          src/ultis
          include
        )
    else()
        zephyr_library_named(ls_chat)
        zephyr_include_directories(include include/pri_include)
        add_subdirectory(src)
        target_link_libraries(ls_chat PUBLIC lisa)
    endif()
endif()

find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND git describe --always --abbrev=16 --dirty --tag
    OUTPUT_VARIABLE                  CMAKE_LSCHAT_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE                   stderr
    RESULT_VARIABLE                  return_code
    WORKING_DIRECTORY                ${CMAKE_CURRENT_SOURCE_DIR}
  )
  if(return_code)
    message(STATUS "git describe failed: ${stderr}")
  elseif(NOT "${stderr}" STREQUAL "")
    message(STATUS "git describe warned: ${stderr}")
  endif()
endif()

set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version")

set(LSCHAT_VERSION "unknown")

if (EXISTS "${VERSION_FILE}")
    file(READ "${VERSION_FILE}" VERSION_CONTENTS)
    string(STRIP "${VERSION_CONTENTS}" LSCHAT_VERSION)
else()
    message(WARNING "version file not found: ${VERSION_FILE}")
endif()

message(STATUS "lschat commit: ${CMAKE_LSCHAT_GIT_COMMIT}")
message(STATUS "lschat version: ${LSCHAT_VERSION}")

add_compile_definitions(LSCHAT_GIT_COMMIT="${CMAKE_LSCHAT_GIT_COMMIT}")
add_compile_definitions(LSCHAT_VERSION="${LSCHAT_VERSION}")
