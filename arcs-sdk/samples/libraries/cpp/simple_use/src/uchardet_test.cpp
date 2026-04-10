#include <cstring>
#include <iostream>
#include "uchardet.h"
#include "uchardet_test.h"

static const char *uchar_str[] = {
    "简体中文简体中文简体中文简体中文简体中文简体中文简体中文简体中文简体中文简体中文简体中文", //gb18030
    "姹夊瓧婕㈠瓧绲变竴绶ㄧ⒓钀湅纰�", //zh_utf8
};

void uchardet_test(void){

    for(int i=0;i<sizeof(uchar_str)/sizeof(uchar_str[0]);i++){
        uchardet_t  handle = uchardet_new();
        uchardet_handle_data(handle, uchar_str[i], strlen(uchar_str[i]));
        uchardet_data_end(handle);
        std::cout << "Detected encoding: " << uchardet_get_charset(handle) << std::endl;
        uchardet_delete(handle);
    }
    return;
}
