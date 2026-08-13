# VENUSA chip specific configuration

# Include local configuration if exists
if(EXISTS ${CMAKE_SOURCE_DIR}/cmake_config.local)
    include(${CMAKE_SOURCE_DIR}/cmake_config.local)
endif()

# If $CHIP not set, set CHIP to "venusa"
if(NOT DEFINED CHIP)
    set(CHIP "venusa")
    message(STATUS "CHIP not defined, defaulting to: ${CHIP}")
endif()

# If $TGT not set, get target name from directory
if(NOT DEFINED TGT)
    get_filename_component(TGT ${CMAKE_SOURCE_DIR} NAME)
    message(STATUS "Using target name: ${TGT}")
endif()

# Toolchain configuration
if(NOT DEFINED NUCLEI_TOOLCHAIN_PATH)
    if(DEFINED ENV{NUCLEI_TOOLCHAIN_PATH})
        set(NUCLEI_TOOLCHAIN_PATH $ENV{NUCLEI_TOOLCHAIN_PATH} CACHE PATH "Nuclei toolchain path from environment")
    else()
        message(FATAL_ERROR "NUCLEI_TOOLCHAIN_PATH is not defined. Please set it to the Nuclei toolchain installation path or define the environment variable NUCLEI_TOOLCHAIN_PATH.")
    endif()
endif()

# Set toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Set compiler paths
string(REPLACE "\\" "/" NUCLEI_TOOLCHAIN_PATH_NORMALIZED ${NUCLEI_TOOLCHAIN_PATH})

if(WIN32)
    set(TOOLCHAIN_EXT ".exe")
else()
    set(TOOLCHAIN_EXT "")
endif()

set(CMAKE_C_COMPILER "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-gcc${TOOLCHAIN_EXT}")
set(CMAKE_CXX_COMPILER "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-g++${TOOLCHAIN_EXT}")
set(CMAKE_ASM_COMPILER "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-gcc${TOOLCHAIN_EXT}")
set(CMAKE_OBJCOPY "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-objcopy${TOOLCHAIN_EXT}")
set(CMAKE_SIZE "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-size${TOOLCHAIN_EXT}")
set(CMAKE_NM "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-nm${TOOLCHAIN_EXT}")
set(CMAKE_OBJDUMP "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-objdump${TOOLCHAIN_EXT}")
set(CMAKE_READELF "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-readelf${TOOLCHAIN_EXT}")
set(CMAKE_AR "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-ar${TOOLCHAIN_EXT}")
set(CMAKE_RANLIB "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-ranlib${TOOLCHAIN_EXT}")
set(CMAKE_STRIP "${NUCLEI_TOOLCHAIN_PATH_NORMALIZED}/bin/riscv64-unknown-elf-strip${TOOLCHAIN_EXT}")


# Neutralize Windows GNU defaults (applied if first run was without toolchain)
set(CMAKE_EXE_LINKER_FLAGS_INIT "")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_IMPORT_LIBRARY_SUFFIX "")
set(CMAKE_PLATFORM_HAS_SEPARATE_IMPORT_LIBRARY OFF)
set(CMAKE_EXECUTABLE_SUFFIX "")

# Provide clean link rules (no <TARGET_IMPLIB> / version flags)
set(CMAKE_C_LINK_EXECUTABLE
  "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")

# Avoid host libs
set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")

# Remove system libraries
set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_CXX_STANDARD_LIBRARIES "")

# Prevent CMake from testing compiler (which would fail without target board)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)

# Tell CMake this is cross-compiling to prevent adding host system libraries
set(CMAKE_CROSSCOMPILING TRUE)


# Platform definitions (can be extended)
set(PLATFORM_DEF "")

# Add global include directories
include_directories(
    ./
    ${TOPDIR}/chip/${CHIP}/bsp
    ${TOPDIR}/chip/${CHIP}/include
    ${TOPDIR}/chip/${CHIP}/include/NMSIS/Core/Include
    ${TOPDIR}/chip/${CHIP}/include/NMSIS/DSP/Include
    ${TOPDIR}/chip/${CHIP}/include/register
    ${TOPDIR}/include/bsp
)

# Set target name from TGT variable
set(TGT_NAME ${TGT})

# Set output directories
set(LIBOUT ${TOPDIR}/lib/${CHIP}-${TGT_NAME})
set(TGTOUT ${TOPDIR}/out/${CHIP})

# Create output directories
file(MAKE_DIRECTORY ${LIBOUT})
file(MAKE_DIRECTORY ${TGTOUT})


# Add global compiler flags and definitions
add_compile_definitions(
    IC_BOARD=${IC_BOARD}
    CHIP=${CHIP}
    TGT=${TGT}
)



# Core flags from platform.mk
set(COREFLAGS
    -march=rv32imafc_zba_zbb_zbc_zbs_xxldsp
    -mabi=ilp32f
    -mcmodel=medlow
    -mtune=nuclei-300-series
    --specs=nosys.specs
)

# Add core flags to all compile operations
add_compile_options(${COREFLAGS})

# Assembly specific flags
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp")

# Linker flags
string(REPLACE ";" " " COREFLAGS_STRING "${COREFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COREFLAGS_STRING} -nostartfiles -Wl,--gc-sections -Wl,--print-memory-usage")


# Custom target for cleaning
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TOPDIR}/lib/${CHIP}-${TGT_NAME}
    COMMAND ${CMAKE_COMMAND} -E remove ${TOPDIR}/out/${CHIP}/${TGT_NAME}
    COMMAND ${CMAKE_COMMAND} -E remove ${TOPDIR}/out/${CHIP}/${TGT_NAME}.*
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMENT "Cleaning all build artifacts (excluding current build directory structure)"
)

# Function to create a module library
function(create_module_library MODULE_NAME MODULE_SOURCES)
    # Get parent scope variables
    if(NOT DEFINED LIBOUT)
        set(LIBOUT ${TOPDIR}/lib/${CHIP})
    endif()
    
    add_library(${MODULE_NAME} STATIC ${MODULE_SOURCES})
    
    # Set library output directory
    set_target_properties(${MODULE_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${LIBOUT}
        OUTPUT_NAME ${MODULE_NAME}
    )
    
    # Add any module-specific configurations here
endfunction()

# Function to create a demo target
function(create_target_executable TGT_NAME TGT_SOURCES TGT_LIBS)
    # Get parent scope variables
    #if(NOT DEFINED TGTOUT)
    #    set(TGTOUT ${TOPDIR}/out/${CHIP})
    #endif()
    
    # Create executable
    add_executable(${TGT_NAME} ${TGT_SOURCES})
    
    # Link libraries (using keyword signature)
    target_link_libraries(
        ${TGT_NAME} PRIVATE
        -Wl,--start-group
        ${TGT_LIBS}
        c
        gcc
        -Wl,--end-group
    )

    
    # Set output directory
    set_target_properties(${TGT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TGTOUT}
        OUTPUT_NAME ${TGT_NAME}
    )

    # Custom commands for additional output formats
    add_custom_command(TARGET ${TGT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary ${TGTOUT}/${TGT_NAME} ${TGTOUT}/${TGT_NAME}.bin
        COMMAND ${CMAKE_SIZE} ${TGTOUT}/${TGT_NAME}
        COMMAND ${CMAKE_NM} -n -l -C ${TGTOUT}/${TGT_NAME} > ${TGTOUT}/${TGT_NAME}.symbol
        COMMAND ${CMAKE_OBJDUMP} -d ${TGTOUT}/${TGT_NAME} > ${TGTOUT}/${TGT_NAME}.dis
        COMMAND ${CMAKE_READELF} -a ${TGTOUT}/${TGT_NAME} > ${TGTOUT}/${TGT_NAME}.readelf
        COMMENT "Creating binary, symbol, disassembly, readelf files and showing size for ${TGT_NAME}"
    )
endfunction()


# Common project setup function for venusa projects
function(setup_project)

    # Set optimization level
    if(BUILD_NANO EQUAL 1)
        set(OPTIMIZATION_FLAGS "-Os")
    else()
        set(OPTIMIZATION_FLAGS "-O2")
    endif()
    
    # Add global compiler flags
    add_compile_options(
        ${OPTIMIZATION_FLAGS}
        -Wall -Wno-format -Wno-unused -Wno-comment
        -MMD
        -ffunction-sections
        -fdata-sections
    )

    # Hart ID configuration
    if(CONFIG_HARTID EQUAL 1)
        add_compile_definitions(BOOT_HARTID=1)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -DBOOT_HARTID=1")
    else()
        add_compile_definitions(BOOT_HARTID=0)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -DBOOT_HARTID=0")
    endif()

    # SMP configuration
    if(CONFIG_USE_SMP)
        add_compile_definitions(configNUMBER_OF_CORES=2)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-defsym=__SMP_CPU_CNT=2")
    endif()

    # Finalize linker script setup (allows demos to override LDSCRIPT before calling this)
    # Set default linker script if not provided
    if(NOT DEFINED LDSCRIPT OR LDSCRIPT STREQUAL "")
        set(LDSCRIPT ${TOPDIR}/chip/${CHIP}/arch/ram.ld)
        message(STATUS "Using default linker script: ${LDSCRIPT}")
    else()
        message(STATUS "Using user provided linker script: ${LDSCRIPT}")
    endif()

    # Check if linker script exists
    if(NOT EXISTS ${LDSCRIPT})
        message(FATAL_ERROR "Linker script not found: ${LDSCRIPT}")
    endif()
    # Add linker script to linker flags
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${LDSCRIPT}" PARENT_SCOPE)

endfunction()
