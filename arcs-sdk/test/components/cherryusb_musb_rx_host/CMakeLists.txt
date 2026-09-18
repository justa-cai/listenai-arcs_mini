cmake_minimum_required(VERSION 3.16)
project(cherryusb_musb_rx_host_test C)
set(CMAKE_C_STANDARD 11)
set(CHERRYUSB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/chryusb)
set(UNITY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/unity/src)
enable_testing()
foreach(target tests tests_with_sof)
    add_executable(${target} test_musb_rx.c ${UNITY_ROOT}/unity.c)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${UNITY_ROOT}
        ${CHERRYUSB_ROOT}/core ${CHERRYUSB_ROOT}/common
        ${CHERRYUSB_ROOT}/port/musb)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
target_compile_definitions(tests_with_sof PRIVATE CONFIG_USBDEV_SOF_ENABLE=1)
