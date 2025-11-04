########################################
if(DEFINED CONFIG_SDK_MODULE_LIBMAD)
set(LIBRARY_NAME mad)
add_library(${LIBRARY_NAME} STATIC "")
target_compile_options(${LIBRARY_NAME} PRIVATE  -Wno-return-local-addr -Wno-unused -Wno-implicit -Wno-cast-function-type -Wno-implicit-fallthrough -Wpointer-arith -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wno-unused-parameter -Wno-sign-compare -Wno-old-style-declaration -Wno-error=maybe-uninitialized -Wno-error=char-subscripts -Wno-error=unused-label -Wno-error=unused-value -DCORE_DEBUG_LEVEL=0 -DNDEBUG -DSQLITE_DEBUG)
target_compile_options(${LIBRARY_NAME} PRIVATE  "-fno-profile-arcs" "-fno-test-coverage" "-fno-stack-protector" "-ffunction-sections"  "-fdata-sections" "-fstrict-volatile-bitfields"  "-nostdlib")
target_compile_definitions(${LIBRARY_NAME} PUBLIC -DSQLITE_HAS_CODEC -DCODEC_TYPE=CODEC_TYPE_AES256 )

set(SOURCES
    src/bit.c
    src/decoder.c
    src/fixed.c
    src/frame.c
    src/huffman.c
    src/layer12.c
    src/layer3.c
    src/stream.c
    src/synth.c
    src/timer.c
    src/version.c
)

target_include_directories(${LIBRARY_NAME} 
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(${LIBRARY_NAME} PRIVATE 
    ${SOURCES}
)

if(DEFINED LISTENAI_CMAKE_PATH)
listenai_link_libraries(${LIBRARY_NAME})
endif()

#
# Compile definitions
#

option(OPTIMIZE "SPEED" SPEED)
if(OPTIMIZE STREQUAL "SPEED")
  message(STATUS "Optimizing for speed over accuracy.")
  target_compile_definitions(${LIBRARY_NAME} PRIVATE OPT_SPEED)
else()
  message(STATUS "Optimizing for accuracy over speed.")
  target_compile_definitions(${LIBRARY_NAME} PRIVATE OPT_ACCURACY)
endif()

option(ASO "ARM" ON)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(STATUS "Using 64 bit fixed point math")
  option(FPM_64BIT "64 bit fixed point math" ON)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86" OR "i386")
  message(STATUS "Using x86 fixed point math")
  option (FPM_INTEL "x86 fixed point math" ON)
  if(ASO)
    target_compile_definitions(${LIBRARY_NAME} PRIVATE ASO_ZEROCHECK)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*(arm|ARM).*")
  message(STATUS "Using ARM fixed point math")
  option (FPM_ARM "ARM fixed point math" ON)
  if(ASO)
    enable_language(ASM)
    target_compile_definitions(${LIBRARY_NAME} PRIVATE ASO_INTERLEAVE1 ASO_IMDCT)
    target_sources(${LIBRARY_NAME} PRIVATE imdct_l_arm.S)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*(mips|MIPS).*")
  message(STATUS "Using MIPS fixed point math")
  option(FPM_MIPS "MIPS fixed point math" ON)
  if(ASO)
    target_compile_definitions(${LIBRARY_NAME} PRIVATE ASO_INTERLEAVE2 ASO_ZEROCHECK)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*(sparc|SPARC).*")
  message(STATUS "Using SPARC fixed point math")
  option(FPM_SPARC "SPARC fixed point math" ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".(ppc|PPC|powerpc).*")
  message(STATUS "Using PowerPC fixed point math")
  option(FPM_PPC "PowerPC fixed point math" ON)
else()
  message(WARNING "Target CPU architecture not detected. Fixed-point math will yield limited accuracy.")
  option(FPM_DEFAULT "Generic fixed-point math" ON)
endif()


#check_type_size(int SIZEOF_INT BUILTIN_TYPES_ONLY LANGUAGE C)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/mad.h.in ${CMAKE_CURRENT_BINARY_DIR}/mad.h @ONLY)

endif()
