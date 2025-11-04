if (CONFIG_MODULE_FREERTOS)
	set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
	set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})
	file(GLOB_RECURSE LIB_SRCS "*.c")
	list(FILTER LIB_SRCS EXCLUDE REGEX "heap_[^3].c")

	listenai_library_named(freertos)
	listenai_library_sources(${LIB_SRCS})
	listenai_include_directories(
		${LIB_PATH}
		${LIB_PATH}/include
	)

	listenai_library_compile_options_ifdef(
		CONFIG_ARCS_HAL_WARNING_DISABLE
		PRIVATE
		-w
	)

	enable_language(ASM)
	listenai_library_sources(${LIB_PATH}/portable/portasm.S)
endif()
