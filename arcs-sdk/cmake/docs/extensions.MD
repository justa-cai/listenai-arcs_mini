# ListenAI Cmake扩展说明文档
### listenai_library_named
简介: 定义一个名称为`_name`的静态库

参数:

+ _name 指定该静态库的名称

### listenai_library_sources
简介: 为当前的目标添加源文件

提示: 必须在使用`listenai_library_named`之后使用

参数:

+ source 源文件

+ ${ARGN} 可不參數



使用示例:

```

listenai_library_sources(test.c test1.c test2.c test3.c)

```

### listenai_library_sources_ifdef
简介: 当配置有效时,为当前的目标添加源文件

提示: 必须在使用`listenai_library_named`之后使用

参数:

+ feature_toggle 配置选项

+ ${ARGN} 可不參數



使用示例:

当`CONFIG_TEST`有效时,将添加源文件

```

listenai_library_sources_ifdef(CONFIG_TEST test.c test1.c test2.c test3.c)

```



### listenai_library_compile_options
简介: 为当前的目标提添加编译选项

提示: 必须在使用`listenai_library_named`之后使用

参数:

+ scope 作用范围, 可选择:PRIVATE, PUBLIC

+ option 编译选项

+ ${ARGN} 可不參數

### listenai_library_compile_options_ifdef
简介: 当配置项有效时, 为当前的目标提添加编译选项

提示: 必须在使用`listenai_library_named`之后使用

参数:

+ feature_toggle 配置项

+ scope 作用范围, 可选择:PRIVATE, PUBLIC

+ option 编译选项

+ ${ARGN} 可不參數

### listenai_append_cmake_library
简介: 将指定的目标添加到`LISTENAI_LIBS`属性中

参数:

+ library 待添加的目标

### listenai_add_subdirectory_ifdef
简介: 当配置有效时, 添加子目录

参数:

+ feature_toggle 配置项

+ dir 子目录

### listenai_target_sources_ifdef
简介: 当配置有效时, 为指定的目标添加源文件

参数:

+ feature_toggle 配置项

+ target 指定的目标

+ scope 作用范围,PRIVATE PUBLIC INTERFACE

+ item 源文件

+ ${ARGN} 可不參數

### listenai_target_compile_definitions_ifdef
简介: 当配置有效时, 为指定的目标添添加宏定义

参数:

+ feature_toggle 配置项

+ target 指定的目标

+ scope 作用范围 PRIVATE, PUBLIC, INTERFACE

+ item 宏定义

+ ${ARGV} 可变参数

### listenai_target_include_directories_ifdef
简介: 当配置有效时, 为指定的目标添加头文件路径

参数:

+ feature_toggle 配置项

+ target 指定的目标

+ scope 作用范围 PRIVATE, PUBLIC, INTERFACE

+ item 待被链接的目标

+ ${ARGV} 可变参数

### listenai_target_link_libraries_ifdef
简介: 当配置有效时, 为指定的目标连接其他目标

参数:

+ feature_toggle 配置项

+ target 指定的目标

+ item 待被链接的目标

+ ${ARGV} 可变参数

### listenai_add_compile_option_ifdef
简介: 当配置有效时, 添加全局的编译选项

参数:

+ feature_toggle 配置项

+ option 编译选项

+ ${ARGV} 可变参数

### listenai_target_compile_option_ifdef
简介: 当配置有效时, 为指定的目标添加编译选项

参数:

+ feature_toggle 配置项

+ target 指定的目标

+ scope 作用范围

+ option 编译选项

+ ${ARGV} 可变参数

### listenai_compile_options
简介: 为listenai_interface添加编译选项

参数:

+ ${ARGV} 可变参数

### listenai_compile_definitions
简介: 为listenai_interface添加宏定义

参数:

+ ${ARGV} 可变参数

### listenai_compile_definitions_ifdef
简介: 当配置有效时, 为listenai_interface添加宏定义

参数:

+ feature_toggle 配置项

+ ${ARGV} 可变参数

### listenai_link_libraries
简介: 将目标链接到listenai_interface中

参数:

+ item 被连接的目标

### listenai_include_directories
简介: 将头文件加入listenai_interface中

参数:

+ target_name 目标

### listenai_generate_bin
简介: 为目标生成bin文件

参数:

+ target_name 目标

### listenai_generate_hex
简介: 为目标生成hex文件

参数:

+ target_name 目标

### listenai_generate_lst
简介: 为目标生成lst文件

参数:

+ target_name 目标

### listenai_link_all_modules
简介: 将所有通过listenai cmake扩展定义的目标连接到指定的目标中

参数:

+ target_name 将要链接到的目标

### listenai_find_and_link_modules
简介: 递归查找LISTENAI_MODULES_DIR_LIST列表中的目录下的CMakeLists.txt,

将目录下的CMakeLists.txt加入到构建中,

并将利用listenai cmake扩展定义的目标链接到指定的目标中

提示: 在某一层目录中找到CMakeLists.txt后,将不再递归查找



参数:

+ target 将要链接到的目标

### listenai_add_executable
添加可执行文件

此宏会自动为可执行文件添加bin,hex,lst文件输出

此宏会自动扫描LISTENAI_MODULES_DIR_LIST 列表中的目录, 并添加模块到构建系统中

### listenai_executable_set_link_file
为可执行文件设置链接脚本

### listenai_generate_boot_header
为可执行文件添加listenai boot header

