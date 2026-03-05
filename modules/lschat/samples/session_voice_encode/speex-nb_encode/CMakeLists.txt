cmake_minimum_required(VERSION 3.20.0)

if(DEFINED WEST_PYTHON)
    find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
endif()

project(speex-nb_encode)

if(DEFINED WEST_PYTHON)
    target_sources(app PRIVATE main.c)
    add_subdirectory(../common/net_conn ${CMAKE_BINARY_DIR}/net_conn)
    add_subdirectory(../common/resource ${CMAKE_BINARY_DIR}/resource)
    add_subdirectory(../common/speex-1.2.1 ${CMAKE_BINARY_DIR}/speex-1.2.1)
    target_link_libraries(app PRIVATE ls_chat speex)

    if(DEFINED DEVICE_ID_STRING)
        target_compile_definitions(app PRIVATE DEVICE_ID_STRING="${DEVICE_ID_STRING}")
    endif()

else()
    add_executable(${PROJECT_NAME} main.c)

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
    add_subdirectory(../common/speex-1.2.1 speex)
    target_link_libraries(${PROJECT_NAME} PRIVATE ls_chat speex)

    set(gen_dir ${CMAKE_BINARY_DIR})

    generate_inc_file_for_target(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/../common/resource/weather.pcm ${gen_dir}/weather.pcm.inc)
    target_include_directories(${PROJECT_NAME} PRIVATE ${gen_dir})

    if(DEFINED DEVICE_ID_STRING)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEVICE_ID_STRING="${DEVICE_ID_STRING}")
    endif()
endif()
