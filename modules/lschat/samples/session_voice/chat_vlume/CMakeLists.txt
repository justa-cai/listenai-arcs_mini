cmake_minimum_required(VERSION 3.20.0)

if (DEFINED WEST_PYTHON)
    find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
endif()

project(chat_volume)

if (DEFINED WEST_PYTHON)
    target_sources(app PRIVATE main.c chat_volume.c)
    add_subdirectory(net_conn)
    add_subdirectory(resource)
    target_link_libraries(app PRIVATE ls_chat)

    if (DEFINED DEVICE_ID_STRING)
        target_compile_definitions(app PRIVATE DEVICE_ID_STRING="${DEVICE_ID_STRING}")
    endif()

else()
    add_executable(${PROJECT_NAME} main.c chat_volume.c)

    if(NOT DEFINED LSCHAT_BASE)
        set(LSCHAT_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../../)
    endif()
    set(ENV{LSCHAT_BASE} ${LSCHAT_BASE})

    if(NOT DEFINED LISA_BASE)
        set(LISA_BASE ${LSCHAT_BASE}/lisa)
    endif()
    set(ENV{LISA_BASE} ${LISA_BASE})

    include(${LISA_BASE}/lisa.cmake)

    add_subdirectory(${LSCHAT_BASE} lschat)
    target_link_libraries(${PROJECT_NAME} PRIVATE ls_chat)

    set(gen_dir ${CMAKE_BINARY_DIR})

    generate_inc_file_for_target(${PROJECT_NAME} resource/set_vol_20.pcm ${gen_dir}/set_vol_20.pcm.inc)
    target_include_directories(${PROJECT_NAME} PRIVATE ${gen_dir})

    if (DEFINED DEVICE_ID_STRING)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEVICE_ID_STRING="${DEVICE_ID_STRING}")
    endif()

endif()
