#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Update device path in device_map.yaml file
Usage: ./update_device_map.py <device_path> [config_file_path]
"""

import sys
import os
import re
import shutil

def update_device_map(device_port, config_file=None):
    """Update device path in config file"""
    # Check device path format
    if not re.match(r'^/dev/(tty[A-Za-z0-9]+)', device_port):
        sys.stderr.write("ERROR: Invalid device path format '{}'\n".format(device_port))
        sys.stderr.write("Device path should be like /dev/ttyACM0 or /dev/ttyUSB0\n")
        return False

    # Determine config file path
    if not config_file:
        config_file = "../device_map/device_map.yaml"

    # Build full path
    if os.path.isabs(config_file):
        full_path = config_file
    elif config_file.startswith('.'):
        # Path relative to project root
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, "../.."))
        full_path = os.path.join(project_root, config_file)
    else:
        # Path relative to script directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        full_path = os.path.join(script_dir, config_file)

    # Check if config file exists
    if not os.path.isfile(full_path):
        sys.stderr.write("ERROR: Configuration file not found: {}\n".format(full_path))
        return False

    # Backup original file
    backup_path = full_path + ".bak"
    shutil.copyfile(full_path, backup_path)

    try:
        # Read file content with error handling
        try:
            with open(full_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except UnicodeDecodeError:
            # Fall back to system default encoding if utf-8 fails
            with open(full_path, 'r') as f:
                content = f.read()

        # Use regex to replace device path while preserving comments and format
        # 1. Replace program.device field
        content = re.sub(
            r'(\s+device:\s+)(/dev/tty[A-Za-z0-9]+|.*?)(\s+#.*?)?$', 
            r'\1{}\3'.format(device_port), 
            content, 
            flags=re.MULTILINE
        )
        
        # 2. Replace log.serial_port field
        content = re.sub(
            r'(\s+serial_port:\s+)(/dev/tty[A-Za-z0-9]+|.*?)(\s+#.*?)?$', 
            r'\1{}\3'.format(device_port), 
            content, 
            flags=re.MULTILINE
        )

        # Write back to file with error handling
        try:
            with open(full_path, 'w', encoding='utf-8') as f:
                f.write(content)
        except UnicodeEncodeError:
            # Fall back to system default encoding if utf-8 fails
            with open(full_path, 'w') as f:
                f.write(content)

        print("Successfully updated device path to {}".format(device_port))
        print("Config file: {}".format(full_path))
        print("Original file backed up to {}".format(backup_path))
        return True
        
    except Exception as e:
        sys.stderr.write("ERROR: Failed to update configuration file: {}\n".format(e))
        # Restore from backup if error occurs
        if os.path.exists(backup_path):
            shutil.copyfile(backup_path, full_path)
            sys.stderr.write("Restored configuration file from backup\n")
        return False

def main():
    # Check arguments
    if len(sys.argv) < 2:
        print("Usage: {} <device_path> [config_file_path]".format(sys.argv[0]))
        print("Example: {} /dev/ttyACM0 ../device_map/device_map.yaml".format(sys.argv[0]))
        sys.exit(1)

    # Device path
    device_port = sys.argv[1]
    
    # Optional config file path
    config_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    # Update config file
    success = update_device_map(device_port, config_file)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
