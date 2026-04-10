listenai_library_named(app)
listenai_library_sources(
    main.c
    fs/user_fs.c
)

listenai_include_directories(
    ./
    fs/
)

# 链接数学库以支持 sinf 等数学函数
target_link_libraries(app PRIVATE m)
