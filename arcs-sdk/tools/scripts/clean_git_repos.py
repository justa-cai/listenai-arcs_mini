#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Clean up git repositories and directories that are not listed in .gitmodules file.
This script will find all git repositories and potential submodule directories in the project 
and remove those that are not listed as submodules in the .gitmodules file.
"""

import os
import sys
import shutil
import subprocess
import re
import argparse
from pathlib import Path

def get_submodule_paths(gitmodules_path):
    """
    Parse .gitmodules file to extract all submodule paths.
    
    Args:
        gitmodules_path: Path to .gitmodules file
        
    Returns:
        A set of normalized paths for all submodules
    """
    if not os.path.exists(gitmodules_path):
        print(f"Error: {gitmodules_path} does not exist")
        sys.exit(1)
        
    submodule_paths = set()
    
    with open(gitmodules_path, 'r') as f:
        content = f.read()
        
    # Find all path entries in the .gitmodules file
    path_matches = re.findall(r'^\s*path\s*=\s*(.+)$', content, re.MULTILINE)
    
    for path in path_matches:
        path = path.strip()
        # Normalize path to ensure consistent comparison
        normalized_path = os.path.normpath(path)
        submodule_paths.add(normalized_path)
        
    return submodule_paths

def is_git_repo(path):
    """
    Check if a directory is a git repository.
    
    Args:
        path: Path to the directory
        
    Returns:
        True if the directory is a git repository, False otherwise
    """
    # Check if .git directory exists
    git_dir = os.path.join(path, '.git')
    if os.path.exists(git_dir) and os.path.isdir(git_dir):
        return True
        
    # Check if .git file exists (for submodules)
    git_file = os.path.join(path, '.git')
    if os.path.exists(git_file) and os.path.isfile(git_file):
        return True
        
    return False

def find_potential_modules(root_dir):
    """
    Find all potential module directories in the given directory.
    This includes git repositories and directories that might be submodules.
    
    Args:
        root_dir: Root directory to search from
        
    Returns:
        A list of paths to potential module directories that are git repositories
    """
    potential_modules = []
    
    # Find all git repositories in the project
    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Skip the root .git directory and other hidden directories
        if os.path.basename(dirpath).startswith('.') and dirpath != root_dir:
            dirnames[:] = []  # Don't descend into hidden directories
            continue
            
        # Check if this is a git repository
        if is_git_repo(dirpath):
            # Get the relative path from the root directory
            rel_path = os.path.relpath(dirpath, root_dir)
            if rel_path == '.':
                continue  # Skip the root repository
            potential_modules.append(rel_path)
            
            # Don't descend into this directory further
            dirnames[:] = []
    
    return potential_modules

def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Clean up git repositories that are not listed in .gitmodules file.')
    parser.add_argument('-y', '--yes', action='store_true', help='Automatically confirm removal without prompting')
    parser.add_argument('--dry-run', action='store_true', help='Only show what would be removed, but do not actually remove anything')
    parser.add_argument('--root-path', action='store_true', help='Which path to clean up', default=os.getcwd())
    args = parser.parse_args()
    
    root_dir = args.root_path
    # Get the project root directory (where .gitmodules is located)
    gitmodules_path = os.path.join(root_dir, '.gitmodules')
    
    # Get all submodule paths from .gitmodules
    submodule_paths = get_submodule_paths(gitmodules_path)
    print(f"Found {len(submodule_paths)} submodules in .gitmodules:")
    for path in sorted(submodule_paths):
        print(f"  - {path}")
    
    # Find all potential module directories that are git repositories
    potential_modules = find_potential_modules(root_dir)
    print(f"\nFound {len(potential_modules)} potential module directories that are git repositories")
    
    # Find directories that are not in the submodules list
    dirs_to_remove = []
    for module in potential_modules:
        if module not in submodule_paths:
            dirs_to_remove.append(module)
    
    if not dirs_to_remove:
        print("\nNo unexpected module directories found. Nothing to clean up.")
        return
        
    print(f"\nFound {len(dirs_to_remove)} git repositories that are not in .gitmodules:")
    for dir_path in dirs_to_remove:
        print(f"  - {dir_path}")
        
    # Check if this is a dry run
    if args.dry_run:
        print("\nDry run: No repositories will be removed.")
        return
        
    # Ask for confirmation before removing (unless --yes flag is provided)
    if not args.yes:
        confirm = input("\nDo you want to remove these git repositories? [y/N]: ")
        if confirm.lower() != 'y':
            print("Operation cancelled.")
            return
    else:
        print("\nAutomatic confirmation enabled. Proceeding with removal...")
        
    # Remove the directories
    for dir_path in dirs_to_remove:
        full_path = os.path.join(root_dir, dir_path)
        print(f"Removing {full_path}...")
        
        try:
            shutil.rmtree(full_path)
            print(f"  [OK] Successfully removed {dir_path}")
        except Exception as e:
            print(f"  [FAILED] Failed to remove {dir_path}: {str(e)}")
    
    print("\nCleanup completed.")

if __name__ == "__main__":
    main()
