
cmake_minimum_required(VERSION 3.20.0)

if (DEFINED WEST_PYTHON)
    find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
endif()

project(session_text)

if (Zephyr_FOUND)
    target_sources(app PRIVATE main.c)
    add_subdirectory(net_conn)
    target_link_libraries(app PRIVATE ls_chat)
    
    if (DEFINED DEVICE_ID_STRING)
        target_compile_definitions(app PRIVATE DEVICE_ID_STRING="${DEVICE_ID_STRING}")
    endif()
else()
    add_executable(${PROJECT_NAME} main.c)

    if(NOT DEFINED LSCHAT_BASE)
        set(LSCHAT_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../)
    endif()
    set(ENV{LSCHAT_BASE} ${LSCHAT_BASE})

    if(NOT DEFINED LISA_BASE)
        set(LISA_BASE ${LSCHAT_BASE}/lisa)
    endif()
    set(ENV{LISA_BASE} ${LISA_BASE})

    include(${LISA_BASE}/lisa.cmake)

    add_subdirectory(${LSCHAT_BASE} lschat)
    target_link_libraries(${PROJECT_NAME} PRIVATE ls_chat)

    if (DEFINED DEVICE_ID_STRING)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEVICE_ID_STRING="${DEVICE_ID_STRING}")
    endif()

endif()
