if (DEFINED CONFIG_SDK_MODULE_OPUS_DECODER)
listenai_library_sources(
  opus_encoder.c
  opus_wrapper.c
)

listenai_link_libraries(
  ${CMAKE_CURRENT_SOURCE_DIR}/libopus.a
)

listenai_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)
endif()
