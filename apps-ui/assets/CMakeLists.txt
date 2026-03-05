add_subdirectory(fonts)

set(ASSETS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/embed)
set(ASSETS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated/assets)
set(ASSETS_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/assets.py)

# Get list of input asset files
file(GLOB_RECURSE ASSET_FILES
    ${ASSETS_SOURCE_DIR}/*.png
    ${ASSETS_SOURCE_DIR}/*.jpg
    ${ASSETS_SOURCE_DIR}/*.jpeg
)

# Generate list of expected output C files based on input files
# Match Python script naming: path/to/file-name.png -> path_to_file_name_png.c
set(GENERATED_ASSET_SOURCES)
foreach(ASSET_FILE ${ASSET_FILES})
    # Get relative path from ASSETS_SOURCE_DIR
    file(RELATIVE_PATH REL_PATH ${ASSETS_SOURCE_DIR} ${ASSET_FILE})
    # Convert path to C filename: match Python's sanitize_var_name function
    # Replace /, \, ., - with _
    string(REPLACE "/" "_" C_FILENAME "${REL_PATH}")
    string(REPLACE "\\" "_" C_FILENAME "${C_FILENAME}")
    string(REPLACE "." "_" C_FILENAME "${C_FILENAME}")
    string(REPLACE "-" "_" C_FILENAME "${C_FILENAME}")
    set(C_FILENAME "${C_FILENAME}.c")
    # Add to list with absolute path
    list(APPEND GENERATED_ASSET_SOURCES "${ASSETS_OUTPUT_DIR}/${C_FILENAME}")
endforeach()

list(LENGTH GENERATED_ASSET_SOURCES ASSET_COUNT)
message(STATUS "Will generate ${ASSET_COUNT} asset C files")

# Run generation during CMake configure to ensure files exist
execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS_OUTPUT_DIR}
    COMMAND python3 ${ASSETS_SCRIPT} ${ASSETS_SOURCE_DIR} ${ASSETS_OUTPUT_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE ASSETS_RESULT
)

if(NOT ASSETS_RESULT EQUAL 0)
    message(WARNING "Failed to generate assets during configuration")
endif()

# Use a stamp file to track generation for incremental builds
set(ASSETS_STAMP_FILE ${ASSETS_OUTPUT_DIR}/.assets_stamp)

# Custom command for incremental builds
add_custom_command(
    OUTPUT ${ASSETS_STAMP_FILE}
    COMMAND python3 ${ASSETS_SCRIPT} ${ASSETS_SOURCE_DIR} ${ASSETS_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSETS_STAMP_FILE}
    DEPENDS ${ASSETS_SCRIPT} ${ASSET_FILES}
    COMMENT "Regenerating asset C files from ${ASSETS_SOURCE_DIR}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

# Create a custom target
add_custom_target(generate_assets_target ALL
    DEPENDS ${ASSETS_STAMP_FILE}
)

# Mark generated files as GENERATED
set_source_files_properties(
    ${GENERATED_ASSET_SOURCES}
    PROPERTIES GENERATED TRUE
)

# Make library depend on generation target
add_dependencies(${LISA_UI_LIB_NAME} generate_assets_target)

# Add assets source files
target_sources(${LISA_UI_LIB_NAME} PRIVATE
    ble_qr.c
    ${GENERATED_ASSET_SOURCES}
)
target_include_directories(${LISA_UI_LIB_NAME} PUBLIC 
    ${ASSETS_OUTPUT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
