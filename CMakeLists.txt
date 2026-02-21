cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(aiui)

include(version.cmake)

add_definitions(-DCONFIG_EXT_RAM)

# Cloud provider selection
if(CONFIG_MY_CLOUD)
    add_definitions(-DMY_CLOUD)
    message(STATUS "Using MY_CLOUD (self-hosted server)")
elseif(CONFIG_LISTEN_CLOUD)
    add_definitions(-DLISTEN_CLOUD)
    message(STATUS "Using LISTEN_CLOUD (AIUI)")
else()
    # Default to MY_CLOUD if neither is specified
    add_definitions(-DMY_CLOUD)
    message(STATUS "Using MY_CLOUD (self-hosted server) - default")
endif()

# Add project root directory to include path for app-config-tls-generic.h and memap.h
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(modules)

listenai_add_executable(${PROJECT_NAME})

target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--wrap,gettimeofday
    -Wl,--wrap,time
    -Wl,--wrap,gmtime_r
)



