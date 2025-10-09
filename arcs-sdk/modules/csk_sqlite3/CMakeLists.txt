if(CONFIG_CSK_SQLITE)

if(CONFIG_CSK_SQLITE_SDK_CSK6)
zephyr_library_named(csk_sqlite3)
zephyr_include_directories(
                          include
                          private_include
                          )

zephyr_library_sources(
    sqlite3.c
    shox96_0_2.c
    csk6_sqlite3.c
    )
elseif(CONFIG_CSK_SQLITE_SDK_ARCS)

set(LIBRARY_NAME csk_sqlite3)
add_library(${LIBRARY_NAME} STATIC "")

target_include_directories(${LIBRARY_NAME} PUBLIC
    include
)

target_include_directories(${LIBRARY_NAME} PRIVATE
    private_include
)
set(SOURCES
    sqlite3.c
    shox96_0_2.c
    arcs_sqlite3.c
)

target_sources(${LIBRARY_NAME} PRIVATE 
    ${SOURCES}
)

target_compile_options(${LIBRARY_NAME} PRIVATE -w -fno-fast-math)
target_compile_definitions(${LIBRARY_NAME} PUBLIC -DSQLITE_HAS_CODEC=1)
target_link_libraries(${LIBRARY_NAME} PUBLIC lsfs)

if(DEFINED LISTENAI_CMAKE_PATH)
listenai_link_libraries(${LIBRARY_NAME})
endif()

endif()
endif()
