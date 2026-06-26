# SPDX-License-Identifier: Apache-2.0
# ARCS SDK version header generation script
# Called by the main CMakeLists.txt through execute_process.

# Try to get the git commit hash.
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS ${ARCS_SDK_BASE}/.git)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --abbrev=12 --always
        WORKING_DIRECTORY ${ARCS_SDK_BASE}
        OUTPUT_VARIABLE BUILD_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

# Fall back when git metadata is unavailable.
if(NOT BUILD_VERSION)
    set(BUILD_VERSION "unknown")
endif()

# Generate the header from the template.
configure_file(${ARCS_SDK_BASE}/sdk_version.h.in ${OUT_FILE} @ONLY)

message(STATUS "Generating version header: ${OUT_FILE}")
message(STATUS "  BUILD_VERSION: ${BUILD_VERSION}")
