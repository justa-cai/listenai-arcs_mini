#include "lisa_log.h"

#include <stdio.h>
#include <stdlib.h>

#include <ft2build.h>
#include FT_FREETYPE_H

extern const unsigned char data_As_I_Lay_Dying_ttf[];
extern const unsigned long data_As_I_Lay_Dying_ttf_len;

// 打印位图数据
void print_bitmap(FT_Bitmap* bitmap) {
    for (int y = 0; y < bitmap->rows; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            unsigned char value = bitmap->buffer[y * bitmap->pitch + x];
            printf("%c", value > 128 ? '#' : ' ');
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    FT_Library library;
    FT_Face face;
    FT_Error error;

    // 初始化FreeType库
    error = FT_Init_FreeType(&library);
    if (error) {
        CLOGE("Error: Could not init FreeType library\n");
        return -1;
    }
    
    // 打印FreeType库版本
    FT_Int major, minor, patch;
    FT_Library_Version(library, &major, &minor, &patch);
    LOGD("FreeType version: %d.%d.%d\n", major, minor, patch);

    // 从内存加载字体
    error = FT_New_Memory_Face(library,
                              data_As_I_Lay_Dying_ttf,        // 字体文件数据
                              data_As_I_Lay_Dying_ttf_len,    // 数据长度
                              0,                 // face index
                              &face);
    if (error) {
        CLOGE("Error: Could not open font from memory(%d)\n", error);
        FT_Done_FreeType(library);
        return -1;
    }
    
    // 打印字体信息
    LOGD("Font family: %s\n", face->family_name ? face->family_name : "(unknown)");
    LOGD("Font style: %s\n", face->style_name ? face->style_name : "(unknown)");
    LOGD("Number of glyphs: %ld\n", face->num_glyphs);
    LOGD("Face flags: 0x%lx\n", face->face_flags);
    
    // 设置字符大小
    error = FT_Set_Char_Size(
        face,    /* handle to face object           */
        0,       /* char_width in 1/64th of points  */
        16*64,   /* char_height in 1/64th of points */
        300,     /* horizontal device resolution    */
        300);    /* vertical device resolution      */

    if (error) {
        CLOGE("Error: Could not set char size\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return -1;
    }

    // 循环加载字符 'A' 到 'D'
    for (char c = 'A'; c <= 'D'; c++) {
        // 加载字符
        error = FT_Load_Char(face, c, FT_LOAD_RENDER);
        if (error) {
            CLOGE("Error: Could not load and render glyph '%c'\n", c);
            continue;
        }

        // 打印字符信息
        LOGD("\nCharacter '%c' metrics:\n", c);
        LOGD("width: %ld\n", face->glyph->metrics.width / 64);
        LOGD("height: %ld\n", face->glyph->metrics.height / 64);
        LOGD("horiBearingX: %ld\n", face->glyph->metrics.horiBearingX / 64);
        LOGD("horiBearingY: %ld\n", face->glyph->metrics.horiBearingY / 64);
        LOGD("horiAdvance: %ld\n", face->glyph->metrics.horiAdvance / 64);

        // 打印位图
        LOGD("Bitmap for '%c':\n", c);
        print_bitmap(&face->glyph->bitmap);
        LOGD("\n"); // 添加空行分隔不同字符的输出
    }

    // 清理资源
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    LOGI("FreeType Simple Use Completed\n");

    return 0;
}
