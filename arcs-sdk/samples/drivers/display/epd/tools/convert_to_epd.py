#!/usr/bin/env python3
"""
PNG to EPD Data Converter
将PNG图像转换为墨水屏支持的1bpp单色数据

支持的抖动算法：
- Floyd-Steinberg (默认)
- Ordered (Bayer) Dithering
- Atkinson Dithering
- Sierra Dithering

用法:
python png_to_epd.py input.png [--algorithm floyd|ordered|atkinson|sierra] [--output output.h] [--threshold 128] [--invert]

选项:
--invert: 反转黑白颜色映射（默认：黑色=1，白色=0；反转后：白色=1，黑色=0）
"""

import argparse
import numpy as np
from PIL import Image
import os
import sys

class EPDConverter:
    def __init__(self, width=240, height=412):
        self.width = width
        self.height = height
        
    def load_image(self, image_path):
        """加载并预处理图像"""
        try:
            # 打开图像
            img = Image.open(image_path)
            
            # 转换为灰度图
            if img.mode != 'L':
                img = img.convert('L')
            
            # 调整图像尺寸以适应屏幕
            img = img.resize((self.width, self.height), Image.Resampling.LANCZOS)
            
            # 转换为numpy数组
            img_array = np.array(img, dtype=np.float32)
            
            print(f"图像加载成功: {self.width}x{self.height}")
            return img_array
            
        except Exception as e:
            print(f"加载图像失败: {e}")
            return None
    
    def floyd_steinberg_dither(self, img_array, threshold=128):
        """Floyd-Steinberg抖动算法"""
        print("应用Floyd-Steinberg抖动算法...")
        
        # 创建副本以避免修改原数组
        dithered = img_array.copy()
        height, width = dithered.shape
        
        for y in range(height):
            for x in range(width):
                old_pixel = dithered[y, x]
                new_pixel = 255 if old_pixel > threshold else 0
                dithered[y, x] = new_pixel
                
                error = old_pixel - new_pixel
                
                # 分散误差到相邻像素
                if x + 1 < width:
                    dithered[y, x + 1] += error * 7 / 16
                if y + 1 < height:
                    if x > 0:
                        dithered[y + 1, x - 1] += error * 3 / 16
                    dithered[y + 1, x] += error * 5 / 16
                    if x + 1 < width:
                        dithered[y + 1, x + 1] += error * 1 / 16
        
        return (dithered > threshold).astype(np.uint8) * 255
    
    def ordered_dither(self, img_array, threshold=128):
        """有序抖动 (Bayer矩阵)"""
        print("应用有序抖动算法...")
        
        # 4x4 Bayer矩阵
        bayer_matrix = np.array([
            [ 0,  8,  2, 10],
            [12,  4, 14,  6],
            [ 3, 11,  1,  9],
            [15,  7, 13,  5]
        ]) * 16
        
        height, width = img_array.shape
        result = np.zeros_like(img_array)
        
        for y in range(height):
            for x in range(width):
                bayer_value = bayer_matrix[y % 4, x % 4]
                adjusted_threshold = threshold + (bayer_value - 128)
                result[y, x] = 255 if img_array[y, x] > adjusted_threshold else 0
        
        return result.astype(np.uint8)
    
    def atkinson_dither(self, img_array, threshold=128):
        """Atkinson抖动算法"""
        print("应用Atkinson抖动算法...")
        
        dithered = img_array.copy()
        height, width = dithered.shape
        
        for y in range(height):
            for x in range(width):
                old_pixel = dithered[y, x]
                new_pixel = 255 if old_pixel > threshold else 0
                dithered[y, x] = new_pixel
                
                error = (old_pixel - new_pixel) / 8
                
                # Atkinson模式的误差分散
                if x + 1 < width:
                    dithered[y, x + 1] += error
                if x + 2 < width:
                    dithered[y, x + 2] += error
                if y + 1 < height:
                    if x > 0:
                        dithered[y + 1, x - 1] += error
                    dithered[y + 1, x] += error
                    if x + 1 < width:
                        dithered[y + 1, x + 1] += error
                if y + 2 < height:
                    dithered[y + 2, x] += error
        
        return (dithered > threshold).astype(np.uint8) * 255
    
    def sierra_dither(self, img_array, threshold=128):
        """Sierra抖动算法"""
        print("应用Sierra抖动算法...")
        
        dithered = img_array.copy()
        height, width = dithered.shape
        
        for y in range(height):
            for x in range(width):
                old_pixel = dithered[y, x]
                new_pixel = 255 if old_pixel > threshold else 0
                dithered[y, x] = new_pixel
                
                error = old_pixel - new_pixel
                
                # Sierra模式的误差分散
                if x + 1 < width:
                    dithered[y, x + 1] += error * 5 / 32
                if x + 2 < width:
                    dithered[y, x + 2] += error * 3 / 32
                if y + 1 < height:
                    if x > 1:
                        dithered[y + 1, x - 2] += error * 2 / 32
                    if x > 0:
                        dithered[y + 1, x - 1] += error * 4 / 32
                    dithered[y + 1, x] += error * 5 / 32
                    if x + 1 < width:
                        dithered[y + 1, x + 1] += error * 4 / 32
                    if x + 2 < width:
                        dithered[y + 1, x + 2] += error * 2 / 32
                if y + 2 < height:
                    if x > 0:
                        dithered[y + 2, x - 1] += error * 2 / 32
                    dithered[y + 2, x] += error * 3 / 32
                    if x + 1 < width:
                        dithered[y + 2, x + 1] += error * 2 / 32
        
        return (dithered > threshold).astype(np.uint8) * 255
    
    def apply_dithering(self, img_array, algorithm='floyd', threshold=128):
        """应用指定的抖动算法"""
        if algorithm == 'floyd':
            return self.floyd_steinberg_dither(img_array, threshold)
        elif algorithm == 'ordered':
            return self.ordered_dither(img_array, threshold)
        elif algorithm == 'atkinson':
            return self.atkinson_dither(img_array, threshold)
        elif algorithm == 'sierra':
            return self.sierra_dither(img_array, threshold)
        else:
            raise ValueError(f"不支持的抖动算法: {algorithm}")
    
    def image_to_1bpp_bytes(self, img_array, invert=False):
        """将图像转换为1bpp字节数组（适用于墨水屏）"""
        print("转换为1bpp数据...")
        
        # 确保图像是二值的（0或255）
        binary_img = (img_array > 128).astype(np.uint8)
        
        height, width = binary_img.shape
        
        # 计算需要的字节数 (每8个像素打包成1个字节)
        bytes_per_row = (width + 7) // 8
        total_bytes = bytes_per_row * height
        
        # 创建字节数组
        byte_data = []
        
        for y in range(height):
            for byte_idx in range(bytes_per_row):
                byte_val = 0
                for bit in range(8):
                    x = byte_idx * 8 + bit
                    if x < width:
                        # 根据invert参数决定黑白映射
                        if invert:
                            # 反转模式: 黑色像素 = 1, 白色像素 = 0
                            if binary_img[y, x] == 255:  # 白色像素设为1
                                byte_val |= (1 << (7 - bit))
                        else:
                            # 默认模式: 黑色像素 = 1, 白色像素 = 0 (墨水屏逻辑)
                            if binary_img[y, x] == 0:  # 黑色像素设为1
                                byte_val |= (1 << (7 - bit))
                byte_data.append(byte_val)
        
        invert_msg = "（颜色反转）" if invert else "（正常模式）"
        print(f"生成 {len(byte_data)} 字节的1bpp数据 {invert_msg}")
        return byte_data
    
    def save_as_c_header(self, byte_data, output_path, var_name="epd_image"):
        """保存为C头文件格式"""
        print(f"保存C头文件: {output_path}")
        
        with open(output_path, 'w') as f:
            f.write(f"/*\n")
            f.write(f" * 自动生成的墨水屏图像数据\n")
            f.write(f" * 尺寸: {self.width}x{self.height}\n")
            f.write(f" * 格式: 1bpp (1 bit per pixel)\n")
            f.write(f" * 数据长度: {len(byte_data)} 字节\n")
            f.write(f" */\n\n")
            f.write(f"#ifndef __EPD_IMAGE_DATA_H__\n")
            f.write(f"#define __EPD_IMAGE_DATA_H__\n\n")
            f.write(f"#include <stdint.h>\n\n")
            f.write(f"#define {var_name.upper()}_WIDTH  {self.width}\n")
            f.write(f"#define {var_name.upper()}_HEIGHT {self.height}\n")
            f.write(f"#define {var_name.upper()}_SIZE   {len(byte_data)}\n\n")
            f.write(f"static const uint8_t {var_name}[{len(byte_data)}] = {{\n")
            
            # 每行16个字节
            for i in range(0, len(byte_data), 16):
                f.write("    ")
                row_data = byte_data[i:i+16]
                hex_values = [f"0x{b:02X}" for b in row_data]
                f.write(", ".join(hex_values))
                if i + 16 < len(byte_data):
                    f.write(",")
                f.write("\n")
            
            f.write("};\n\n")
            f.write(f"#endif // __EPD_IMAGE_DATA_H__\n")
    
    def save_preview_image(self, img_array, output_path):
        """保存预览图像"""
        preview_img = Image.fromarray(img_array.astype(np.uint8), mode='L')
        preview_img.save(output_path)
        print(f"预览图像已保存: {output_path}")

def main():
    parser = argparse.ArgumentParser(description='将PNG图像转换为墨水屏数据')
    parser.add_argument('input', help='输入PNG文件路径')
    parser.add_argument('--algorithm', '-a', choices=['floyd', 'ordered', 'atkinson', 'sierra'], 
                       default='floyd', help='抖动算法 (默认: floyd)')
    parser.add_argument('--output', '-o', help='输出C头文件路径 (默认: input_epd.h)')
    parser.add_argument('--threshold', '-t', type=int, default=128, 
                       help='二值化阈值 (0-255, 默认: 128)')
    parser.add_argument('--preview', '-p', action='store_true', 
                       help='保存预览图像')
    parser.add_argument('--width', type=int, default=240, help='屏幕宽度 (默认: 240)')
    parser.add_argument('--height', type=int, default=412, help='屏幕高度 (默认: 412)')
    parser.add_argument('--invert', '-i', action='store_true', 
                       help='反转黑白颜色 (默认: 黑=1, 白=0)')
    
    args = parser.parse_args()
    
    # 检查输入文件
    if not os.path.exists(args.input):
        print(f"错误: 输入文件不存在: {args.input}")
        sys.exit(1)
    
    # 设置输出文件名
    if args.output is None:
        base_name = os.path.splitext(os.path.basename(args.input))[0]
        args.output = f"{base_name}_epd.h"
    
    # 创建转换器
    converter = EPDConverter(args.width, args.height)
    
    # 加载图像
    img_array = converter.load_image(args.input)
    if img_array is None:
        sys.exit(1)
    
    # 应用抖动算法
    try:
        dithered_img = converter.apply_dithering(img_array, args.algorithm, args.threshold)
    except ValueError as e:
        print(f"错误: {e}")
        sys.exit(1)
    
    # 转换为1bpp数据
    byte_data = converter.image_to_1bpp_bytes(dithered_img, args.invert)
    
    # 保存C头文件
    converter.save_as_c_header(byte_data, args.output)
    
    # 保存预览图像
    if args.preview:
        preview_path = os.path.splitext(args.output)[0] + "_preview.png"
        converter.save_preview_image(dithered_img, preview_path)
    
    print(f"\n转换完成!")
    print(f"算法: {args.algorithm}")
    print(f"阈值: {args.threshold}")
    print(f"颜色: {'反转' if args.invert else '正常'}")
    print(f"输出: {args.output}")
    print(f"数据大小: {len(byte_data)} 字节")

if __name__ == "__main__":
    main()
