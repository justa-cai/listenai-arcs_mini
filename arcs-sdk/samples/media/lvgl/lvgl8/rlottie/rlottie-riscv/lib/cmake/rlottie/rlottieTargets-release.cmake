#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "rlottie::rlottie" for configuration "Release"
set_property(TARGET rlottie::rlottie APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(rlottie::rlottie PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "/home/tanha/workspace/ls26/voiceassistant/arcs-sdk/samples/media/lvgl/lvgl8/rlottie/rlottie-riscv/lib/librlottie.a"
  )

list(APPEND _cmake_import_check_targets rlottie::rlottie )
list(APPEND _cmake_import_check_files_for_rlottie::rlottie "/home/tanha/workspace/ls26/voiceassistant/arcs-sdk/samples/media/lvgl/lvgl8/rlottie/rlottie-riscv/lib/librlottie.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
