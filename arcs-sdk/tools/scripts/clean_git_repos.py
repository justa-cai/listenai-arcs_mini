#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Clean up git repositories and directories that are not listed in .gitmodules file.
This script will find all git repositories and potential submodule directories in the project
and remove those that are not listed as submodules in the .gitmodules file.

In addition to removing the working directory, it also cleans up:
- .git/modules/<path> internal cache
- [submodule "<name>"] entries in .git/config
"""

import os
import sys
import shutil
import subprocess
import re
import argparse
import configparser
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
        return set()
        
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

def get_submodule_names_and_paths(gitmodules_path):
    """
    Parse .gitmodules file to extract submodule name-to-path mapping.

    Returns:
        A dict mapping submodule name -> normalized path
    """
    if not os.path.exists(gitmodules_path):
        return {}

    name_to_path = {}

    with open(gitmodules_path, 'r') as f:
        content = f.read()

    # Match each [submodule "name"] block with its path
    for match in re.finditer(
        r'^\[submodule\s+"([^"]+)"\]\s*\n((?:\s+\w+\s*=\s*.+\n)*)',
        content, re.MULTILINE
    ):
        name = match.group(1)
        block = match.group(2)
        path_match = re.search(r'^\s*path\s*=\s*(.+)$', block, re.MULTILINE)
        if path_match:
            path = os.path.normpath(path_match.group(1).strip())
            name_to_path[name] = path

    return name_to_path

def load_submodule_whitelist(base_dir):
    """Load whitelist paths from .gitmodules in base_dir. Paths are normalized."""
    gm_path = os.path.join(base_dir, '.gitmodules')
    return get_submodule_paths(gm_path)

def find_git_modules_dir(root_dir):
    """
    Find the .git/modules directory for the given root.
    Handles both normal repos (.git is a directory) and worktrees (.git is a file).
    """
    git_path = os.path.join(root_dir, '.git')
    if os.path.isdir(git_path):
        return os.path.join(git_path, 'modules')
    elif os.path.isfile(git_path):
        # worktree: .git file contains "gitdir: /path/to/actual/git/dir"
        with open(git_path, 'r') as f:
            gitdir = f.read().strip()
        if gitdir.startswith('gitdir:'):
            gitdir = gitdir[len('gitdir:'):].strip()
            if not os.path.isabs(gitdir):
                gitdir = os.path.normpath(os.path.join(root_dir, gitdir))
            return os.path.join(gitdir, 'modules')
    return None

def clean_git_modules_cache(root_dir, submodule_path):
    """
    Remove .git/modules/<submodule_path> cache for an orphaned submodule.

    Args:
        root_dir: Project root directory
        submodule_path: Relative path of the submodule (e.g. "modules/old_lib")
    """
    modules_dir = find_git_modules_dir(root_dir)
    if not modules_dir:
        return

    cache_path = os.path.join(modules_dir, submodule_path)
    if os.path.exists(cache_path):
        try:
            shutil.rmtree(cache_path)
            print(f"  [OK] Removed .git/modules/{submodule_path}")
        except Exception as e:
            print(f"  [FAILED] Failed to remove .git/modules/{submodule_path}: {e}")

def clean_git_config_entry(root_dir, submodule_path):
    """
    Remove [submodule "..."] entry from .git/config for an orphaned submodule.
    Matches by comparing the submodule's url/path against known patterns.

    Args:
        root_dir: Project root directory
        submodule_path: Relative path of the submodule
    """
    git_path = os.path.join(root_dir, '.git')

    # Resolve actual git dir for worktrees
    if os.path.isfile(git_path):
        with open(git_path, 'r') as f:
            gitdir = f.read().strip()
        if gitdir.startswith('gitdir:'):
            gitdir = gitdir[len('gitdir:'):].strip()
            if not os.path.isabs(gitdir):
                gitdir = os.path.normpath(os.path.join(root_dir, gitdir))
            config_path = os.path.join(gitdir, 'config')
        else:
            return
    elif os.path.isdir(git_path):
        config_path = os.path.join(git_path, 'config')
    else:
        return

    if not os.path.exists(config_path):
        return

    # Use git config to remove the submodule section
    # Try both the path as-is and as the submodule name
    # (submodule name often equals path, but not always)
    candidates = {submodule_path}

    # Also check .gitmodules for the actual name mapping
    gitmodules_path = os.path.join(root_dir, '.gitmodules')
    name_map = get_submodule_names_and_paths(gitmodules_path)
    # Reverse lookup: find names that had this path (from old .gitmodules in git history)
    # But since the submodule is already removed from .gitmodules, we try path as name
    for name, path in name_map.items():
        if path == submodule_path:
            candidates.add(name)

    for candidate in candidates:
        section = f'submodule.{candidate}'
        try:
            result = subprocess.run(
                ['git', '-C', root_dir, 'config', '--local', '--get', f'{section}.url'],
                capture_output=True, text=True
            )
            if result.returncode == 0:
                subprocess.run(
                    ['git', '-C', root_dir, 'config', '--local', '--remove-section', section],
                    capture_output=True, text=True
                )
                print(f"  [OK] Removed .git/config section [{section}]")
        except Exception as e:
            print(f"  [FAILED] Failed to clean .git/config for {candidate}: {e}")

def scan_unexpected_git_repos(root_dir):
    """
    Walk the tree from root_dir. For any git repo encountered under a base directory,
    compare against that base's .gitmodules whitelist. If a child repo path relative to base
    is not listed, mark it as unexpected. When a listed submodule is encountered, treat it
    as a new base and use its own .gitmodules for deeper comparisons.
    Returns list of paths (relative to root_dir) to remove.
    """
    # Maintain mapping of base absolute path -> whitelist set
    base_whitelists = {}
    root_abs = os.path.abspath(root_dir)
    base_whitelists[root_abs] = load_submodule_whitelist(root_abs)

    def find_base_for(path_abs):
        # choose deepest base that is a prefix of path_abs
        candidates = [b for b in base_whitelists.keys() if path_abs == b or path_abs.startswith(b + os.sep)]
        if not candidates:
            return root_abs
        return max(candidates, key=len)

    unexpected = []

    for dirpath, dirnames, filenames in os.walk(root_abs):
        # skip hidden directories at traversal level
        dirnames[:] = [d for d in dirnames if not d.startswith('.')]

        if not is_git_repo(dirpath):
            continue

        current_base = find_base_for(dirpath)
        rel_to_base = os.path.relpath(dirpath, current_base)

        # If this is exactly the base repo directory, allow and ensure its whitelist is loaded
        if rel_to_base == '.':
            # already loaded for root; for nested bases, load when we encounter them as listed modules below
            continue

        whitelist = base_whitelists.get(current_base, set())

        if rel_to_base in whitelist:
            # It's a declared submodule under current_base; treat it as a new base and load its whitelist
            sub_base_abs = dirpath
            if sub_base_abs not in base_whitelists:
                base_whitelists[sub_base_abs] = load_submodule_whitelist(sub_base_abs)
            # continue walking inside (allowed)
            continue
        else:
            # Not declared in the nearest base's .gitmodules -> unexpected
            rel_to_root = os.path.relpath(dirpath, root_abs)
            unexpected.append(rel_to_root)
            # no need to descend into this unexpected repo
            dirnames[:] = []

    return unexpected, base_whitelists[root_abs]

def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Clean up git repositories that are not listed in .gitmodules file.')
    parser.add_argument('-y', '--yes', action='store_true', help='Automatically confirm removal without prompting')
    parser.add_argument('--dry-run', action='store_true', help='Only show what would be removed, but do not actually remove anything')
    parser.add_argument('--root-path', type=str, help='Project root path containing .gitmodules', default=os.getcwd())
    args = parser.parse_args()
    
    root_dir = args.root_path
    # Get the project root directory (where .gitmodules is located)
    gitmodules_path = os.path.join(root_dir, '.gitmodules')
    
    # Get all submodule paths from root .gitmodules
    root_submodule_paths = get_submodule_paths(gitmodules_path)
    print(f"Found {len(root_submodule_paths)} submodules in .gitmodules:")
    for path in sorted(root_submodule_paths):
        print(f"  - {path}")

    # Scan with hierarchical .gitmodules
    dirs_to_remove, _ = scan_unexpected_git_repos(root_dir)
    
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
        
    # Remove the directories and clean up git internal state
    for dir_path in dirs_to_remove:
        full_path = os.path.join(root_dir, dir_path)
        print(f"Removing {dir_path}...")

        try:
            shutil.rmtree(full_path)
            print(f"  [OK] Removed working directory")
        except Exception as e:
            print(f"  [FAILED] Failed to remove working directory: {e}")

        # Clean .git/modules/<path> cache
        clean_git_modules_cache(root_dir, dir_path)

        # Clean .git/config [submodule "..."] entry
        clean_git_config_entry(root_dir, dir_path)

    print("\nCleanup completed.")

if __name__ == "__main__":
    main()
