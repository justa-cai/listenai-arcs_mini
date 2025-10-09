if (DEFINED CONFIG_SDK_MODULE_AAC_DECODER)
listenai_link_libraries(
  ${CMAKE_CURRENT_SOURCE_DIR}/libaacdec.a
)

listenai_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
endif()
