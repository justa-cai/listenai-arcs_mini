file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version VERSION_CONTENTS)

string(REGEX MATCH "major: *([0-9]+)" MAJOR_MATCH ${VERSION_CONTENTS})
set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
string(REGEX MATCH "minor: *([0-9]+)" MINOR_MATCH ${VERSION_CONTENTS})
set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_1})
string(REGEX MATCH "build: *([0-9]+)" BUILD_MATCH ${VERSION_CONTENTS})
set(PROJECT_VERSION_BUILD ${CMAKE_MATCH_1})
set(PROJECT_VERSION_STR "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_BUILD}")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
            COMMAND git describe --always --dirty --tag --long --abbrev=8
            OUTPUT_VARIABLE                  PROJECT_VERSION_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            ERROR_VARIABLE                   stderr
            RESULT_VARIABLE                  return_code
            WORKING_DIRECTORY                ${CMAKE_CURRENT_LIST_DIR}
        )
        if(return_code)
            message(STATUS "git describe failed: ${stderr}")
        elseif(NOT "${stderr}" STREQUAL "")
            message(STATUS "git describe warned: ${stderr}")
        endif()
endif()

if (NOT DEFINED PROJECT_VERSION_COMMIT)
    set(PROJECT_VERSION_COMMIT "unknown")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_BINARY_DIR}/generated/include/project_version.h
    @ONLY
)

include_directories(${CMAKE_BINARY_DIR}/generated/include)
