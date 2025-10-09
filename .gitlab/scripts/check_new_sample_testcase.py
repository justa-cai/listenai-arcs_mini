import os
import fnmatch
import argparse
import sys

def find_files(directory, filename):
    """递归查找指定目录下的文件"""
    matches = []
    for root, dirnames, filenames in os.walk(directory):
        for name in fnmatch.filter(filenames, filename):
            matches.append(os.path.join(root, name))
    return matches

def check_cmakelists_and_yaml(directory, verbose=False):
    """检查CMakeLists.txt文件和testcase.yaml/sample.yaml文件"""
    cmake_files = find_files(directory, 'CMakeLists.txt')
    if verbose:
        print(f"Found {len(cmake_files)} CMakeLists.txt files in {directory}")
    
    result = True
    sdk_files_count = 0
    yaml_missing_count = 0
    
    for cmake_file in cmake_files:
        with open(cmake_file, 'r', encoding='utf-8') as f:
            content = f.read()
            # Check for either arcs-sdk or listenai-cmake packages
            if ('find_package(arcs-sdk REQUIRED HINTS $ENV{ARCS_SDK_BASE})' in content or
                'find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})' in content):
                sdk_files_count += 1
                dir_path = os.path.dirname(cmake_file)
                
                # 检查目录是否包含 'test' 子字符串 - 检查全路径
                is_test_dir = 'test' in dir_path.lower()
                yaml_filename = 'testcase.yaml' if is_test_dir else 'sample.yaml'
                yaml_file = os.path.join(dir_path, yaml_filename)
                
                if verbose:
                    print(f"Checking {cmake_file} -> {'test' if is_test_dir else 'normal'} directory")
                
                if not os.path.exists(yaml_file):
                    yaml_missing_count += 1
                    if is_test_dir:
                        print(f"{yaml_filename} not found in test directory: {dir_path}")
                    else:
                        print(f"{yaml_filename} not found in directory: {dir_path}")
                    result = False
    
    if verbose:
        print(f"Directory {directory}: Found {sdk_files_count} SDK CMakeLists.txt files, {yaml_missing_count} missing yaml files")
    
    return result

def main():
    parser = argparse.ArgumentParser(description='Check for CMakeLists.txt and testcase.yaml/sample.yaml files.')
    parser.add_argument('-d', '--directory', type=str, nargs='+', required=True,
                        help='Directory to search for CMakeLists.txt files (can specify multiple directories).')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Enable verbose output')
    args = parser.parse_args()
    
    if args.verbose:
        print(f"Starting check in directories: {', '.join(args.directory)}")
    
    all_valid = True
    for directory in args.directory:
        if os.path.isdir(directory):
            if args.verbose:
                print(f"\nChecking directory: {directory}")
            result = check_cmakelists_and_yaml(directory, verbose=args.verbose)
            all_valid = all_valid and result
        else:
            print(f"Warning: {directory} is not a valid directory, skipping")
    
    if args.verbose:
        print(f"\nSummary: Check {'PASSED' if all_valid else 'FAILED'}")
    
    if not all_valid:
        print(f"testcases check failed")
        sys.exit(-1)
    else:
        # Always print a success message regardless of verbose mode
        print("All directories have the correct yaml files. Check PASSED.")

if __name__ == "__main__":
    main()
    