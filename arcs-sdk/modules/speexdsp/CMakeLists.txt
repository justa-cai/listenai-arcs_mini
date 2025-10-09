if (DEFINED CONFIG_SDK_MODULE_SPEEXDSP)
cmake_minimum_required(VERSION 3.13)

listenai_library_named(SpeexDSP VERSION 1.2.0)

target_compile_definitions(SpeexDSP PRIVATE -DARCS_PLATFORM)
target_compile_definitions(SpeexDSP PRIVATE -DFIXED_POINT -DEXPORT=)

#add_definitions(-DARM5E_ASM)
#add_definitions(-DUSE_NEON)
target_compile_options(SpeexDSP PRIVATE -g -O3 -w -funwind-tables -fcommon -fpic)
# option(BUILD_SHARED_LIBS "Build dynamic library" ON)
# option(BUILD_STATIC_LIBS "Build dynamic library" ON)

target_compile_definitions(SpeexDSP PRIVATE -Dmalloc=lisa_mem_alloc)
target_compile_definitions(SpeexDSP PRIVATE -Dcalloc=lisa_mem_calloc)
target_compile_definitions(SpeexDSP PRIVATE -Drealloc=lisa_mem_realloc)
target_compile_definitions(SpeexDSP PRIVATE -Dfree=lisa_mem_free)
target_compile_definitions(SpeexDSP PRIVATE -Dsram_malloc=lisa_mem_sram_alloc)
target_compile_definitions(SpeexDSP PRIVATE -Dsram_free=lisa_mem_sram_free)
target_compile_definitions(SpeexDSP PRIVATE -DFIXED_POINT)
target_compile_definitions(SpeexDSP PRIVATE -DEXPORT=)

file(GLOB_RECURSE LIB_SRC "./speex/*.c")

listenai_library_sources(${LIB_SRC})
listenai_include_directories(
  ./
  ./speex
)

set_target_properties(SpeexDSP PROPERTIES
  VERSION ${CMAKE_PROJECT_VERSION}
  SOVERSION ${CMAKE_PROJECT_VERSION}
)

endif()
