import re
import argparse

def line_startswith_keys(line, keys):
    for k in keys:
        if line.startswith(k):
            pattern = rf'{k}\((\w+)'
            match = re.search(pattern, line)
            return match.group(1) if match else None
    return None

def comments_section_get_by_keys(file_path, keys):
    current_comment = []
    extensions_dic = {}
    with open(file_path, 'r', encoding='UTF-8') as f:
        for line in f:
            line = line.strip()
            if line.startswith('#'):
                current_comment.append(line)
            else:
                name = line_startswith_keys(line, keys)
                if name is not None:
                    extensions_dic[name] = current_comment.copy()
                current_comment.clear()

    return extensions_dic

def save_comments_to_file(filepath,comments_dic):
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(f'# ListenAI Cmake扩展说明文档\n')
        for k,v in comments_dic.items():
            f.write(f'### {k}\n')
            for line in v:
                line = line.replace('#','').strip()
                f.write(f'{line}\n\n')

def main():
    parser = argparse.ArgumentParser(description='Extract comments from CMake files.')
    parser.add_argument('input_file', type=str, help='Path to the input CMake file')
    parser.add_argument('output_file', type=str, help='Path to the output Markdown file')
    args = parser.parse_args()

    cmake_keys= ['macro', 'function']
    dic = comments_section_get_by_keys(args.input_file, cmake_keys)
    save_comments_to_file(args.output_file, dic)

if __name__=='__main__':
    main()
