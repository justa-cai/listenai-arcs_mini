cmake_minimum_required(VERSION 3.13)
set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../modules/tinyusb
    $ENV{ARCS_BASE}/../modules/easylogger
    $ENV{ARCS_BASE}/../modules/letter-shell
    $ENV{ARCS_BASE}/../modules/fs
    $ENV{ARCS_BASE}/../components
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})


project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
    src/fs.c
    src/usb_descriptors.c
    src/sdmmc_init.c
)

listenai_include_directories(
    src
)

listenai_link_libraries(lsfs)
listenai_link_libraries(disk)
listenai_link_libraries(elmfat)
