#include <stdio.h>
#include <stdint.h>
#include "Driver_EFUSE.h"

//打印efuse中从地址0至0x1中的内容
void efuse_dump_all(void)
{
    printf("Start dumping eFuse contents:\n");
    for (uint8_t addr = 0; addr < 0x1; addr++) {
        uint32_t value = 0;
        if (efuse_read_word(addr, &value) == 0) {
            // 输出地址及所存值
            printf("eFuse[0x%02X] = 0x%08X\n", addr, value);
         }
    }
}

int main(int argc, char **argv)
{
    printf("Hello, world! efuse\n");
    //读取uuid
    uint64_t uuid = efuse_read_uuid();
    // 打印64位UUID，16进制格式
    uint32_t uuid_high = (uint32_t)(uuid >> 32);
    uint32_t uuid_low  = (uint32_t)(uuid & 0xFFFFFFFF);
    printf("Device UUID: 0x%08X%08X\n", uuid_high, uuid_low);
    efuse_dump_all();
    printf("eFuse dump complete.\n");
    return 0;
}
