listenai_library_named(app-miniapp)

# Keep runtime state and Lua/libm small data out of the scarce CP SRAM.
# Ordinary code and read-only data continue to execute/read from Flash.
listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/miniapp_psram.ld
    SLOT COMPONENTS
    SORT_KEY "50-app-miniapp"
)
listenai_add_linker_scatter(SCATLOAD .miniapp.psram.data)
listenai_add_linker_scatter(SCATZERO .miniapp.psram.bss)

set(MINIAPP_LUA_DIR ${PROJECT_TOP_DIR}/arcs-sdk/samples/subsys/lua/lua_binding_lvgl/components/lua)

listenai_library_sources(
    miniapp_storage.c
    miniapp_runtime.c
    miniapp_ui.c
)

listenai_library_sources_ifdef(CONFIG_MINIAPP_ADB_DEBUG miniapp_adb.c)

add_library(miniapp_lua STATIC
    ${MINIAPP_LUA_DIR}/lapi.c
    ${MINIAPP_LUA_DIR}/lauxlib.c
    ${MINIAPP_LUA_DIR}/lbaselib.c
    ${MINIAPP_LUA_DIR}/lcode.c
    ${MINIAPP_LUA_DIR}/lctype.c
    ${MINIAPP_LUA_DIR}/ldebug.c
    ${MINIAPP_LUA_DIR}/ldo.c
    ${MINIAPP_LUA_DIR}/ldump.c
    ${MINIAPP_LUA_DIR}/lfunc.c
    ${MINIAPP_LUA_DIR}/lgc.c
    ${MINIAPP_LUA_DIR}/llex.c
    ${MINIAPP_LUA_DIR}/lmathlib.c
    ${MINIAPP_LUA_DIR}/lmem.c
    ${MINIAPP_LUA_DIR}/lobject.c
    ${MINIAPP_LUA_DIR}/lopcodes.c
    ${MINIAPP_LUA_DIR}/lparser.c
    ${MINIAPP_LUA_DIR}/lstate.c
    ${MINIAPP_LUA_DIR}/lstring.c
    ${MINIAPP_LUA_DIR}/lstrlib.c
    ${MINIAPP_LUA_DIR}/ltable.c
    ${MINIAPP_LUA_DIR}/ltablib.c
    ${MINIAPP_LUA_DIR}/ltm.c
    ${MINIAPP_LUA_DIR}/lundump.c
    ${MINIAPP_LUA_DIR}/lutf8lib.c
    ${MINIAPP_LUA_DIR}/lvm.c
    ${MINIAPP_LUA_DIR}/lzio.c
)
target_include_directories(miniapp_lua PUBLIC ${MINIAPP_LUA_DIR})
target_compile_options(miniapp_lua PRIVATE -O2 -std=c99 -fno-stack-protector -fno-common)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_TOP_DIR}/apps-ui
    ${PROJECT_TOP_DIR}/apps-ui/apps/llm
    ${PROJECT_TOP_DIR}/apps-ui/assets/fonts
    ${PROJECT_TOP_DIR}/apps-ui/lv-i18n
    ${PROJECT_TOP_DIR}/apps/arcs-mini/services
)

listenai_link_libraries(miniapp_lua)
listenai_link_libraries(m)
