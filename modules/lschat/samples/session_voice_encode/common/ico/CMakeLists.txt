if(CONFIG_LISA_LINUX)
    set(LIB_PLATFORM linux_x86_x64)
elseif(CONFIG_LISA_ZEPHYR)
    set(LIB_PLATFORM zephyr)
else()
    message(FATAL_ERROR, "Unknown platform")
endif()

set(ICO_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/${LIB_PLATFORM}/libico.a)

add_library(ico STATIC IMPORTED GLOBAL)
set_target_properties(ico PROPERTIES IMPORTED_LOCATION ${ICO_LIB_PATH})
target_include_directories(ico INTERFACE ./inc)
