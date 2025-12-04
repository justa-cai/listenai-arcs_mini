import os
import re
from pathlib import Path

def generate_asset_files():
    # 定义路径
    assets_png_dir = Path("png/")
    output_c_file = "lisa_ui_assets.c"
    output_h_file = "lisa_ui_assets.h"
    
    # 获取所有PNG文件（保持文件系统顺序）
    png_files = []
    for file_path in sorted(assets_png_dir.rglob("*.png")):
        # 获取相对路径（相对于assets目录）
        rel_path = file_path.relative_to("png")
        # 生成变量名：img_png_ + 文件名（不含扩展名，转为小写，替换非字母数字为_）
        var_name = "img_png_" + re.sub(r'[^a-zA-Z0-9]', '_', file_path.stem.lower())
        png_files.append((var_name, str(rel_path)))
    
    # 生成.h文件
    with open(output_h_file, 'w', encoding='utf-8') as f:
        f.write("#ifndef LISA_UI_ASSETS_H\n")
        f.write("#define LISA_UI_ASSETS_H\n\n")
        f.write("#include \"lvgl/lvgl.h\"\n\n")
        
        # 声明所有图片描述符（按原始顺序）
        for var_name, _ in png_files:
            # f.write(f"extern lv_img_dsc_t lv_img_dsc_{var_name};\n")
            f.write(f"LISA_UI_ASSETS_IMG_DEC(LISA_UI_ASSETS_IMG_DSC({var_name}));\n")
        
        f.write("\nvoid lisa_ui_assets_init(void);\n\n")
        f.write("#endif // LISA_UI_ASSETS_H\n")
    
    # 生成.c文件
    with open(output_c_file, 'w', encoding='utf-8') as f:
        f.write("#include \"lisa_ui_assets.h\"\n\n")
        
        # 定义所有图片资源（按原始顺序）
        for var_name, rel_path in png_files:
            # 转换路径格式为LISA_UI_ASSETS_PNG_PATH格式
            path_in_code = rel_path.replace('\\', '/').replace('png/', '')
            f.write(f"UI_RES_IMG_NAME({var_name}, LISA_UI_ASSETS_PNG_PATH(\"{path_in_code}\"));\n")
        
        # 定义初始化函数（按原始顺序初始化）
        f.write("\nvoid lisa_ui_assets_init(void)\n{\n")
        for var_name, _ in png_files:
            f.write(f"    lv_img_png_src_init(UI_RES_IMG_PNG({var_name}));\n")
        f.write("}\n")

if __name__ == "__main__":
    generate_asset_files()
    print("lisa_ui_assets.c and lisa_ui_assets.h generated successfully!")