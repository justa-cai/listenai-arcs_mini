if (CONFIG_SDK_MODULE_LIBJPEG_TURBO)
    set(JPEG_TURBO_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    
    # Set CMAKE_SYSTEM_PROCESSOR if not set (needed for cross-compilation)
    if(NOT CMAKE_SYSTEM_PROCESSOR)
        set(CMAKE_SYSTEM_PROCESSOR "riscv")
    endif()
    
    # Configure libjpeg-turbo options for embedded system
    set(ENABLE_SHARED OFF CACHE BOOL "Build shared libraries" FORCE)
    set(ENABLE_STATIC ON CACHE BOOL "Build static libraries" FORCE)
    set(WITH_SIMD OFF CACHE BOOL "Include SIMD extensions" FORCE)
    set(WITH_TURBOJPEG ON CACHE BOOL "Include TurboJPEG API library" FORCE)
    set(WITH_JAVA OFF CACHE BOOL "Build Java wrapper" FORCE)
    set(WITH_TOOLS OFF CACHE BOOL "Build command-line tools" FORCE)
    set(WITH_TESTS OFF CACHE BOOL "Build regression tests" FORCE)
    
    # Enable arithmetic coding support
    set(WITH_ARITH_ENC ON CACHE BOOL "Include arithmetic encoding support" FORCE)
    set(WITH_ARITH_DEC ON CACHE BOOL "Include arithmetic decoding support" FORCE)
    
    # Use JPEG library version 8 API
    set(WITH_JPEG8 ON CACHE BOOL "Emulate libjpeg v8 API/ABI" FORCE)
    
    # Force inline must be enabled for correct Huffman decoding
    set(FORCE_INLINE ON CACHE BOOL "Force function inlining" FORCE)
    
    # Embedded platform specific configurations (MUST be set BEFORE include)
    # These override CMake's auto-detection which fails in cross-compilation
    
    # 1. INLINE: Force always_inline for critical functions (Huffman decode)
    set(INLINE "inline __attribute__((always_inline))" CACHE STRING "Inline method" FORCE)
    set(INLINE_WORKS TRUE CACHE BOOL "Inline works" FORCE)
    
    # 2. THREAD_LOCAL: Not supported on bare-metal RTOS
    #    Set HAVE_THREAD_LOCAL=1 to suppress warning (even though THREAD_LOCAL is empty)
    set(THREAD_LOCAL "" CACHE STRING "Thread-local storage" FORCE)
    set(HAVE_THREAD_LOCAL 1 CACHE BOOL "Thread-local storage available" FORCE)
    
    # 3. HIDDEN: Use GCC visibility attribute
    set(HIDDEN "__attribute__((visibility(\"hidden\")))" CACHE STRING "Hidden visibility" FORCE)
    
    # 4. RIGHT_SHIFT_IS_UNSIGNED: Assume normal arithmetic right shift
    #    RISC-V GCC uses arithmetic shift (sign-extension) for signed types
    set(RIGHT_SHIFT_IS_UNSIGNED 0 CACHE BOOL "Right shift behavior" FORCE)
    
    # 5. HAVE_BUILTIN_CTZL: GCC supports __builtin_ctzl for bit operations
    set(HAVE_BUILTIN_CTZL 1 CACHE BOOL "Has __builtin_ctzl" FORCE)
    
    # 6. SIZE_T: RISC-V 32-bit platform
    set(SIZE_T 4 CACHE STRING "sizeof(size_t)" FORCE)
    set(SIZEOF_SIZE_T 4 CACHE STRING "sizeof(size_t)" FORCE)
    
    # # 7. BUILD: Build date identifier
    # set(BUILD "20250118" CACHE STRING "Build date" FORCE)
    
    # Include the original CMakeLists.txt
    include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt.org)
    
    # Fix SIZEOF_SIZE_T for RISC-V 32-bit (CMake detection returns 8 incorrectly)
    # This must be done AFTER include() as check_type_size() overrides our setting
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/jconfigint.h JCONFIGINT_CONTENT)
    string(REPLACE "#define SIZEOF_SIZE_T  8" "#define SIZEOF_SIZE_T  4" JCONFIGINT_CONTENT "${JCONFIGINT_CONTENT}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/jconfigint.h "${JCONFIGINT_CONTENT}")

    # Export include directories
    listenai_include_directories(${JPEG_TURBO_DIR}/src/)
    # Export jconfig.h path (generated config headers)
    listenai_include_directories(${CMAKE_CURRENT_BINARY_DIR})
    
    # Export only turbojpeg-static library
    # turbojpeg-static already contains all base JPEG functionality
    # Linking both turbojpeg-static and jpeg-static causes duplicate symbol errors
    listenai_link_libraries(turbojpeg-static)
endif()
