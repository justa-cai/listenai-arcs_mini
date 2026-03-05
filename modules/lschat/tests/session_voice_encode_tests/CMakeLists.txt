cmake_minimum_required(VERSION 3.20.0)

if(DEFINED WEST_PYTHON)
    find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
endif()

project(session_voice_encode_tests)

if(DEFINED WEST_PYTHON)
    target_sources(app PRIVATE main.c)
    add_subdirectory(common/net_conn ${CMAKE_BINARY_DIR}/net_conn)
    add_subdirectory(common/resource ${CMAKE_BINARY_DIR}/resource)
    add_subdirectory(common/speex-1.2.1 ${CMAKE_BINARY_DIR}/speex-1.2.1)
    add_subdirectory(common/ico ${CMAKE_BINARY_DIR}/ico)
    target_link_libraries(app PRIVATE ls_chat ico speex)
else()
    set(LSCHAT_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../)
    set(LISA_BASE ${LSCHAT_BASE}/lisa)
    set(ENV{LSCHAT_BASE} ${LSCHAT_BASE})
    set(ENV{LISA_BASE} ${LISA_BASE})

    include(${LISA_BASE}/lisa.cmake)

    add_executable(${PROJECT_NAME} main.c)
    add_subdirectory(${LSCHAT_BASE} lschat)
    add_subdirectory(common/speex-1.2.1 speex)
    add_subdirectory(common/ico ico)
    target_link_libraries(${PROJECT_NAME} PRIVATE ls_chat speex ico)

    set(gen_dir ${CMAKE_BINARY_DIR})

    generate_inc_file_for_target(${PROJECT_NAME} common/resource/weather.pcm ${gen_dir}/weather.pcm.inc)
    target_include_directories(${PROJECT_NAME} PRIVATE ${gen_dir})
endif()
