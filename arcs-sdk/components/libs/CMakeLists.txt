# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SCANPEN_DICT)

listenai_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/scanpen_dict/include")
listenai_link_libraries("${CMAKE_CURRENT_SOURCE_DIR}/scanpen_dict/libscanpen_dict.a")

endif()