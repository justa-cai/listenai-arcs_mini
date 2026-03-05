cmake_minimum_required(VERSION 3.20.0)

if (DEFINED WEST_PYTHON)
    find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
endif()

project(lsc_test)

if (DEFINED WEST_PYTHON)
    target_sources(app PRIVATE main.c)
    add_subdirectory(net_conn)
    target_link_libraries(app PRIVATE ls_chat)
else()
    set(LSCHAT_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../)
    set(LISA_BASE ${LSCHAT_BASE}/lisa)
    set(ENV{LSCHAT_BASE} ${LSCHAT_BASE})
    set(ENV{LISA_BASE} ${LISA_BASE})

    include(${LISA_BASE}/lisa.cmake)

    add_executable(${PROJECT_NAME} main.c)
    add_subdirectory(${LSCHAT_BASE} lschat)
    target_link_libraries(${PROJECT_NAME} PRIVATE ls_chat)
endif()
