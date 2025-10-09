if(DEFINED CONFIG_FREERTOS_CPP11)
  set(TARGET_NAME psram_freertos-cpp11)
  listenai_library_named(${TARGET_NAME})

  set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )

  listenai_compile_definitions(_GLIBCXX_HAS_GTHREADS)
  
  FILE(GLOB_RECURSE sources "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/cpp11_gcc/*.cpp")
  FILE(GLOB_RECURSE source_stdcpp_v13 "${CMAKE_CURRENT_SOURCE_DIR}/libstdc++_gcc/v13/*.cc")

  listenai_library_sources(${sources} ${source_stdcpp_v13})

  if (DEFINED CONFIG_FREERTOS_CPP11_FORCE_USE_EXRAM)
    listenai_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/sys_common/exram_memory.cpp)
  endif()

  listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/cpp11_gcc
  )
endif()