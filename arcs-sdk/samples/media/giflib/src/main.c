#include "gif_lib.h"
#include "lisa_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 小型测试GIF文件的二进制数据
// 这是一个1x1像素的简单GIF（黑色像素）
static const unsigned char sample_gif[] = {
    // GIF头部 - GIF89a
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61,
    // 逻辑屏幕宽度和高度 (1x1)
    0x01, 0x00, 0x01, 0x00,
    // 全局颜色表
    0x80, 0x01, 0x00,
    // 颜色表 (黑白两色)
    0x00, 0x00, 0x00,  // 黑色
    0xFF, 0xFF, 0xFF,  // 白色
    // 图像块引导 - ','
    0x2C,
    // 图像左上角位置 (0,0)
    0x00, 0x00, 0x00, 0x00,
    // 图像宽度和高度 (1x1)
    0x01, 0x00, 0x01, 0x00,
    // 图像使用局部颜色表标志
    0x00,
    // LZW最小码长度
    0x02,
    // 块大小(1) + 数据 + 块结束
    0x01, 0x01, 0x00,
    // GIF终止符
    0x3B
};

// 自定义内存读取函数，用于从内存中读取GIF数据
typedef struct {
    const unsigned char *data;
    size_t size;
    size_t pos;
} MemoryBuffer;

// GifLib自定义I/O函数，从内存读取
int memory_read_func(GifFileType *gif, GifByteType *buf, int len) {
    MemoryBuffer *buffer = (MemoryBuffer *)gif->UserData;
    
    if (buffer->pos >= buffer->size) {
        return 0;
    }
    
    int remaining = buffer->size - buffer->pos;
    int read_size = (remaining < len) ? remaining : len;
    
    memcpy(buf, buffer->data + buffer->pos, read_size);
    buffer->pos += read_size;
    
    return read_size;
}

// 显示GIF文件信息
void print_gif_info(GifFileType *gif) {
    LOGI("GIF信息");
    LOGI("  宽度: %d", gif->SWidth);
    LOGI("  高度: %d", gif->SHeight);
    LOGI("  颜色分辨率: %d", gif->SColorResolution);
    LOGI("  背景色索引: %d", gif->SBackGroundColor);
    LOGI("  图像总数: %d", gif->ImageCount);
    
    // 打印全局调色板信息（如果存在）
    if (gif->SColorMap) {
        LOGI("  全局调色板");
        LOGI("    颜色数: %d", gif->SColorMap->ColorCount);
        LOGI("    BitsPerPixel: %d", gif->SColorMap->BitsPerPixel);
        
        // 显示前几个颜色（为了演示）
        int colors_to_show = (gif->SColorMap->ColorCount > 3) ? 3 : gif->SColorMap->ColorCount;
        for (int i = 0; i < colors_to_show; i++) {
            GifColorType color = gif->SColorMap->Colors[i];
            LOGI("    颜色#%d: RGB(%d, %d, %d)", 
                   i, color.Red, color.Green, color.Blue);
        }
    } else {
        LOGI("  无全局调色板");
    }
}

// 显示GIF帧信息
void print_frame_info(SavedImage *image) {
    LOGI("  帧信息");
    LOGI("    左上角: (%d, %d)", image->ImageDesc.Left, image->ImageDesc.Top);
    LOGI("    宽度: %d", image->ImageDesc.Width);
    LOGI("    高度: %d", image->ImageDesc.Height);
    LOGI("    交错: %s", image->ImageDesc.Interlace ? "是" : "否");
    
    // 打印局部调色板信息（如果存在）
    if (image->ImageDesc.ColorMap) {
        LOGI("    局部调色板");
        LOGI("      颜色数: %d", image->ImageDesc.ColorMap->ColorCount);
    } else {
        LOGI("    无局部调色板");
    }
    
    // 打印扩展块信息
    for (int i = 0; i < image->ExtensionBlockCount; i++) {
        ExtensionBlock *ext = &image->ExtensionBlocks[i];
        LOGI("    扩展块#%d: 功能码=%d, 大小=%d", 
               i, ext->Function, ext->ByteCount);
        
        // 如果是图形控制扩展块，解析其内容
        if (ext->Function == GRAPHICS_EXT_FUNC_CODE && ext->ByteCount >= 4) {
            int disposal = (ext->Bytes[0] >> 2) & 0x7;
            int userInput = (ext->Bytes[0] >> 1) & 0x1;
            int transparency = ext->Bytes[0] & 0x1;
            int delay = ext->Bytes[1] | (ext->Bytes[2] << 8); // 延迟时间（1/100秒）
            int transparentColor = ext->Bytes[3];
            
            LOGI("      图形控制: 处理方式=%d, 用户输入=%d", disposal, userInput);
            if (transparency) {
                LOGI("      透明度: 使用透明色, 透明色索引=%d", transparentColor);
            } else {
                LOGI("      透明度: 无透明色");
            }
            LOGI("      延迟时间: %d/100秒", delay);
        }
    }
}

int main(void) {
    int error = 0;
    GifFileType *gif = NULL;
    MemoryBuffer buffer = {
        .data = sample_gif,
        .size = sizeof(sample_gif),
        .pos = 0
    };
    
    LOGI("正在打开GIF数据");
    gif = DGifOpen(&buffer, memory_read_func, &error);
    if (!gif) {
        LOGI("无法打开GIF: %s", GifErrorString(error));
        return 1;
    }
    
    // 读取GIF内容
    LOGI("正在解析GIF数据");
    if (DGifSlurp(gif) != GIF_OK) {
        LOGI("无法读取GIF内容: %s", GifErrorString(gif->Error));
        DGifCloseFile(gif, &error);
        return 1;
    }
    
    // 打印GIF信息
    print_gif_info(gif);
    
    // 处理所有图像帧
    LOGI("GIF包含 %d 个图像", gif->ImageCount);
    for (int i = 0; i < gif->ImageCount; i++) {
        SavedImage *image = &gif->SavedImages[i];
        LOGI("图像 #%d", i);
        print_frame_info(image);
        
        // 这里可以添加更多处理代码，例如解码和显示图像数据
    }
    
    // 关闭GIF文件
    if (DGifCloseFile(gif, &error) != GIF_OK) {
        LOGI("关闭GIF文件时出错: %s", GifErrorString(error));
        return 1;
    }
    
    LOGI("GifLib sample completed successfully");
    return 0;
}
