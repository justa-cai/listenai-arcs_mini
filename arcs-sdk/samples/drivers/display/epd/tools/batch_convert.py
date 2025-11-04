#!/usr/bin/env python3
"""
批量生成不同抖动算法的墨水屏图像数据
用于比较不同算法的效果
"""

import os
import subprocess
import sys


def run_conversion(title, input_file, algorithm, threshold=128, width=400, height=300):
    """运行转换并生成对应的文件"""
    # 确保输出目录存在
    output_dir = "output"
    os.makedirs(output_dir, exist_ok=True)

    output_file = os.path.join(output_dir, f"{title}_{algorithm}.h")
    preview_file = os.path.join(output_dir, f"{title}_{algorithm}_preview.png")

    print(f"\n=== 转换使用 {algorithm.upper()} 抖动算法 (阈值: {threshold}) ===")

    try:
        # 运行通用转换脚本
        cmd = [
            "python3", "convert_to_epd.py", input_file,
            "--algorithm", algorithm,
            "--output", output_file,
            "--threshold", str(threshold),
            "--preview",
            "--width", str(width),
            "--height", str(height)
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode == 0:
            print(f"✓ 成功生成: {output_file}")

            # 重命名预览文件
            default_preview = output_file.replace('.h', '_preview.png')
            if os.path.exists(default_preview):
                os.rename(default_preview, preview_file)
                print(f"✓ 预览图像: {preview_file}")

        else:
            print(f"✗ 转换失败: {result.stderr}")

    except Exception as e:
        print(f"✗ 执行错误: {e}")


def generate_comparison_html(title, input_file, width, height):
    """生成HTML比较页面"""
    # 从输入文件名提取基本信息
    base_name = os.path.splitext(os.path.basename(input_file))[0]

    html_content = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>墨水屏抖动算法比较</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }}
        .container {{ max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }}
        .algorithm-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin: 20px 0; }}
        .algorithm-card {{ border: 1px solid #ddd; border-radius: 8px; padding: 15px; background: #fafafa; }}
        .algorithm-card h3 {{ margin-top: 0; color: #333; }}
        .algorithm-card img {{ max-width: 100%; height: auto; border: 1px solid #ccc; border-radius: 4px; }}
        .original {{ text-align: center; margin: 20px 0; }}
        .original img {{ max-width: 400px; height: auto; border: 2px solid #333; border-radius: 8px; }}
        .stats {{ background: #e8f4f8; padding: 10px; border-radius: 4px; margin: 10px 0; font-size: 14px; }}
        h1 {{ color: #2c3e50; text-align: center; }}
        h2 {{ color: #34495e; border-bottom: 2px solid #3498db; padding-bottom: 5px; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>墨水屏抖动算法效果比较</h1>
        
        <div class="original">
            <h2>原始图像</h2>
            <img src="{input_file}" alt="原始图像">
            <p><em>{base_name} ({width}×{height})</em></p>
        </div>

        <h2>抖动算法比较</h2>
        <div class="algorithm-grid">
"""

    algorithms = [
        ("floyd", "Floyd-Steinberg", "高质量，自然过渡，适合照片"),
        ("ordered", "Ordered (Bayer)", "规则图案，适合简单图形"),
        ("atkinson", "Atkinson", "柔和效果，适合人像"),
        ("sierra", "Sierra", "高精度，细节丰富")
    ]

    # 计算数据大小 (1bpp = 1位每像素，所以总位数除以8得字节数)
    data_size_bytes = (width * height) // 8
    data_size_kb = data_size_bytes / 1024

    for algo, name, desc in algorithms:
        preview_file = f"output/{title}_{algo}_preview.png"
        header_file = f"output/{title}_{algo}.h"

        html_content += f"""
            <div class="algorithm-card">
                <h3>{name}</h3>
                <img src="{preview_file}" alt="{name} 效果" onerror="this.style.display='none'">
                <div class="stats">
                    <strong>特点:</strong> {desc}<br>
                    <strong>输出文件:</strong> {header_file}<br>
                    <strong>数据大小:</strong> {data_size_bytes:,} 字节 ({data_size_kb:.1f} KB)
                </div>
            </div>
"""

    html_content += f"""
        </div>

        <h2>使用说明</h2>
        <div class="stats">
            <p><strong>墨水屏规格:</strong> {width}×{height} 像素，1bpp单色显示</p>
            <p><strong>推荐算法:</strong> Floyd-Steinberg (floyd) - 提供最佳的整体效果</p>
            <p><strong>数据格式:</strong> C头文件，包含静态字节数组</p>
            <p><strong>在代码中使用:</strong></p>
            <pre style="background: #f8f8f8; padding: 10px; border-radius: 4px; overflow-x: auto;">
#include "{title}_floyd.h"

// 显示图像到墨水屏
lisa_display_write(display_dev, 0, 0, &desc, {title}_floyd_image);
            </pre>
        </div>
    </div>
</body>
</html>
"""

    # 确保输出目录存在
    output_dir = "output"
    os.makedirs(output_dir, exist_ok=True)

    # html_file = os.path.join(output_dir, "algorithm_comparison.html")
    html_file = os.path.join("algorithm_comparison.html")
    with open(html_file, "w", encoding="utf-8") as f:
        f.write(html_content)

    print(f"✓ 生成比较页面: {html_file}")


def main():
    print("批量生成墨水屏图像数据 - 不同抖动算法比较")
    print("=" * 50)

    # 转换参数配置
    # title = "Meisje_met_de_parel"
    # input_file = "Meisje_met_de_parel_240x416.png"  # 与 run_conversion 函数中保持一致
    # width = 240
    # height = 416
    title = "Earthrise"
    input_file = "Earthrise_400x300.png"  # 与 run_conversion 函数中保持一致
    width = 400
    height = 300

    # 检查输入文件
    if not os.path.exists(input_file):
        print(f"✗ 错误: 找不到输入文件 {input_file}")
        sys.exit(1)

    # 检查通用转换脚本
    if not os.path.exists("convert_to_epd.py"):
        print("✗ 错误: 找不到 convert_to_epd.py 脚本")
        sys.exit(1)

    # 转换不同算法
    algorithms = ["floyd", "ordered", "atkinson", "sierra"]

    for algo in algorithms:
        run_conversion(title, input_file, algo, threshold=128,
                       width=width, height=height)

    # 生成比较页面
    print(f"\n=== 生成比较页面 ===")
    generate_comparison_html(title, input_file, width, height)

    print(f"\n=== 完成! ===")
    print("生成的文件:")
    output_dir = "output"
    for algo in algorithms:
        header_file = os.path.join(output_dir, f"{title}_{algo}.h")
        preview_file = os.path.join(output_dir, f"{title}_{algo}_preview.png")
        if os.path.exists(header_file):
            print(f"  ✓ {header_file}")
        if os.path.exists(preview_file):
            print(f"  ✓ {preview_file}")

    # html_file = os.path.join(output_dir, "algorithm_comparison.html")
    html_file = os.path.join("algorithm_comparison.html")
    if os.path.exists(html_file):
        print(f"  ✓ {html_file}")

    print(f"\n可以打开 {html_file} 查看效果比较")


if __name__ == "__main__":
    main()
