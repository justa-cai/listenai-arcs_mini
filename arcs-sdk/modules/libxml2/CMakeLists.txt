if (DEFINED CONFIG_SDK_MODULE_LIBXML)
    set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})

    #set(COMPONENT_SRCDIRS ./libxml2-2.9.14 )
    set(LIB_SRCS
        ./libxml2-2.9.14/buf.c
        ./libxml2-2.9.14/catalog.c
        ./libxml2-2.9.14/chvalid.c
        ./libxml2-2.9.14/debugXML.c
        ./libxml2-2.9.14/dict.c
        ./libxml2-2.9.14/encoding.c
        ./libxml2-2.9.14/entities.c
        ./libxml2-2.9.14/error.c
        ./libxml2-2.9.14/globals.c
        ./libxml2-2.9.14/hash.c
        ./libxml2-2.9.14/HTMLparser.c
        ./libxml2-2.9.14/HTMLtree.c
        ./libxml2-2.9.14/list.c
        ./libxml2-2.9.14/parser.c
        ./libxml2-2.9.14/parserInternals.c
        ./libxml2-2.9.14/SAX.c
        ./libxml2-2.9.14/SAX2.c
        ./libxml2-2.9.14/threads.c
        ./libxml2-2.9.14/tree.c
        ./libxml2-2.9.14/uri.c
        ./libxml2-2.9.14/valid.c
        ./libxml2-2.9.14/xmlIO.c
        ./libxml2-2.9.14/xmlmemory.c
        ./libxml2-2.9.14/xmlregexp.c
        ./libxml2-2.9.14/xmlstring.c
        ./libxml2-2.9.14/xmlunicode.c
        ./libxml2-2.9.14/xpath.c
        ./libxml2-2.9.14/xmlsave.c
    )
    listenai_library_named(xml)
    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}
        ${LIB_PATH}/libxml2-2.9.14
        ${LIB_PATH}/libxml2-2.9.14/include
    )
endif()
