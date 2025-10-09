if(DEFINED CONFIG_SDK_MODULE_COREHTTP)

listenai_library_named(coreHTTP)

include_directories(../../lisa_porting/include/lisa_os)

add_definitions(-DHTTP_DO_NOT_USE_CUSTOM_CONFIG)
add_definitions(-Dhttp_message_needs_eof=ls_http_message_needs_eof)
add_definitions(-Dhttp_should_keep_alive=ls_http_should_keep_alive)
add_definitions(-Dhttp_parser_execute=ls_http_parser_execute)
add_definitions(-Dhttp_method_str=ls_http_method_str)
add_definitions(-Dhttp_parser_init=ls_http_parser_init)
add_definitions(-Dhttp_errno_name=ls_http_errno_name)
add_definitions(-Dhttp_errno_description=ls_http_errno_description)
add_definitions(-Dhttp_parser_parse_url=ls_http_parser_parse_url)
add_definitions(-Dhttp_parser_pause=ls_http_parser_pause)
add_definitions(-Dhttp_body_is_final=ls_http_body_is_final)
add_definitions(-Dhttp_parser_set_max_header_size=ls_http_parser_set_max_header_size)
add_definitions(-Dhttp_parser_url_init=ls_http_parser_url_init)
add_definitions(-Dhttp_errno_description=ls_http_errno_description)
add_definitions(-Dhttp_errno_name=ls_http_errno_name)
add_definitions(-Dhttp_status_str=ls_http_status_str)
add_definitions(-Dhttp_parser_settings_init=ls_http_parser_settings_init)
add_definitions(-Dhttp_parser_version=ls_http_parser_version)

listenai_library_sources(
    source/core_http_client.c
    source/dependency/3rdparty/http_parser/http_parser.c
)

listenai_include_directories(
    source/include
    source/interface
    source/dependency/3rdparty/http_parser
)
endif()
